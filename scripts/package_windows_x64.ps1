[CmdletBinding()]
param(
    [string]$InstallDirectory = "install-portable",
    [string]$OutputPath = "VortexLauncher-Windows-x64.zip",
    [string]$BuildType = "Release",
    [switch]$RunSmokeTests
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($env:OS -ne "Windows_NT") {
    throw "This packaging script must run on Windows; it does not create or rename a Linux binary."
}
if ($BuildType -ne "Release" -or ($env:BUILD_TYPE -and $env:BUILD_TYPE -ne "Release")) {
    throw "The Windows x64 release package must be produced from a Release configuration."
}

function Assert-PeX64File {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($stream.Length -lt 64) {
            throw "File is too short to be a valid Windows PE image: $Path"
        }
        $stream.Position = 0
        if ($reader.ReadUInt16() -ne 0x5A4D) {
            throw "File is not a Windows PE image (missing MZ signature): $Path"
        }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 64 -or $peOffset -gt ($stream.Length - 26)) {
            throw "Invalid PE header offset in $Path"
        }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "File is not a Windows PE image (missing PE signature): $Path"
        }
        $machine = $reader.ReadUInt16()
        $stream.Position = $peOffset + 24
        $optionalHeaderMagic = $reader.ReadUInt16()
        if ($machine -ne 0x8664 -or $optionalHeaderMagic -ne 0x020B) {
            throw "Expected x64 PE32+ image, got machine 0x$($machine.ToString('X4')) / optional header 0x$($optionalHeaderMagic.ToString('X4')): $Path"
        }
    } finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

$workspace = if ($env:GITHUB_WORKSPACE) { $env:GITHUB_WORKSPACE } else { (Get-Location).Path }
$installPath = if ([IO.Path]::IsPathRooted($InstallDirectory)) {
    $InstallDirectory
} else {
    Join-Path $workspace $InstallDirectory
}
$installPath = [IO.Path]::GetFullPath($installPath)
if (-not (Test-Path -LiteralPath $installPath -PathType Container)) {
    throw "Portable CMake install directory was not found: $installPath"
}

$launcherExe = Join-Path $installPath "VortexLauncher.exe"
if (-not (Test-Path -LiteralPath $launcherExe -PathType Leaf)) {
    throw "VortexLauncher.exe is missing from the portable install."
}

# Remove debug/linker byproducts that are not runtime dependencies.
$debugArtifacts = Get-ChildItem -LiteralPath $installPath -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -in @(".pdb", ".ilk", ".obj", ".lib", ".exp") }
$debugArtifacts | Remove-Item -Force
$forbiddenDirectories = Get-ChildItem -LiteralPath $installPath -Recurse -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -in @(".git", "CMakeFiles", "_deps", "build") }
if ($forbiddenDirectories) {
    throw "Build/source directories must not be included in the release package: $($forbiddenDirectories.FullName -join ', ')"
}
$forbiddenDevExtensions = @(
    ".cpp", ".c", ".cc", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".m", ".mm", ".rc",
    ".obj", ".o", ".a", ".lib", ".exp", ".ilk", ".pdb", ".pch", ".idb", ".map",
    ".so", ".dylib", ".cmake", ".sln", ".vcxproj", ".filters", ".props", ".targets", ".ninja"
)
$forbiddenDevNames = @(
    "CMakeCache.txt", "CMakeLists.txt", "build.ninja", "ninja_deps", "ninja_log",
    "compile_commands.json", "Makefile", "GNUmakefile"
)
$forbiddenDevFiles = Get-ChildItem -LiteralPath $installPath -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -in $forbiddenDevNames -or $_.Extension -in $forbiddenDevExtensions }
if ($forbiddenDevFiles) {
    throw "Build/source files must not be included in the release package: $($forbiddenDevFiles.FullName -join ', ')"
}

foreach ($requiredAsset in @("portable.txt", "jars\NewLaunch.jar", "jars\NewLaunchLegacy.jar", "jars\JavaCheck.jar")) {
    if (-not (Test-Path -LiteralPath (Join-Path $installPath $requiredAsset) -PathType Leaf)) {
        throw "Required installed launcher asset is missing: $requiredAsset"
    }
}

# CMake's Qt deployment step stages Qt DLLs and plugins, while the dependency-set
# intentionally excludes the MSVC CRT. Copy the redistributable x64 CRT from the
# Visual Studio toolchain so users do not need to install the compiler runtime.
$redistRoots = @()
if ($env:VCToolsRedistDir) {
    $redistRoots += $env:VCToolsRedistDir
}
$programFilesX86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
if ($programFilesX86) {
    $vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $vsInstall = & $vswhere -latest -products "*" -property installationPath | Select-Object -First 1
        if ($LASTEXITCODE -eq 0 -and $vsInstall) {
            $redistRoots += Join-Path $vsInstall "VC\Redist\MSVC"
        }
    }
}

$crtDirectory = $null
foreach ($root in ($redistRoots | Select-Object -Unique)) {
    if (-not (Test-Path -LiteralPath $root -PathType Container)) {
        continue
    }
    $candidate = Get-ChildItem -LiteralPath $root -Directory -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '[\\/]x64[\\/]Microsoft\.VC\d+\.CRT$' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if ($candidate) {
        $crtDirectory = $candidate.FullName
        break
    }
}
if (-not $crtDirectory) {
    throw "Could not locate the x64 Microsoft VC runtime redistributable under VCToolsRedistDir or Visual Studio."
}

$crtDlls = @(Get-ChildItem -LiteralPath $crtDirectory -File -Filter "*.dll")
if ($crtDlls.Count -eq 0) {
    throw "The MSVC redistributable directory contains no DLLs: $crtDirectory"
}
$crtDlls | Copy-Item -Destination $installPath -Force
foreach ($requiredCrt in @("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll")) {
    if (-not (Test-Path -LiteralPath (Join-Path $installPath $requiredCrt) -PathType Leaf)) {
        throw "Required MSVC runtime file was not staged: $requiredCrt"
    }
}

$debugRuntimePattern = '^(?:Qt\d+.*d|msvcp.*d|vcruntime.*d|ucrtbase.*d|concrt.*d|vcomp.*d)\.dll$'
$debugRuntimeFiles = Get-ChildItem -LiteralPath $installPath -Recurse -File -Filter "*.dll" |
    Where-Object { $_.Name -match $debugRuntimePattern }
if ($debugRuntimeFiles) {
    throw "Debug Qt/MSVC runtime DLLs are not permitted in the Release package: $($debugRuntimeFiles.FullName -join ', ')"
}

$installedFiles = Get-ChildItem -LiteralPath $installPath -Recurse -File
foreach ($file in $installedFiles) {
    $stream = [IO.File]::OpenRead($file.FullName)
    try {
        if ($stream.Length -ge 4) {
            $magic = [byte[]]::new(4)
            [void]$stream.Read($magic, 0, 4)
            if ($magic[0] -eq 0x7F -and $magic[1] -eq 0x45 -and $magic[2] -eq 0x4C -and $magic[3] -eq 0x46) {
                throw "Linux ELF file found in the Windows package: $($file.FullName)"
            }
        }
    } finally {
        $stream.Dispose()
    }
    if ($file.Extension -in @(".exe", ".dll")) {
        Assert-PeX64File -Path $file.FullName
    }
}

# Verify the import closure using the Visual Studio PE inspection tool. Every
# non-Windows DLL import must be bundled beside the launcher or in its plugins.
$dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
if (-not $dumpbin) {
    throw "dumpbin.exe is required to verify the packaged PE dependency closure."
}
$bundledDllNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($dll in ($installedFiles | Where-Object { $_.Extension -eq ".dll" })) {
    [void]$bundledDllNames.Add($dll.Name)
}
$system32Path = Join-Path $env:SystemRoot "System32"
$unresolvedDependencies = [System.Collections.Generic.List[string]]::new()
foreach ($binary in ($installedFiles | Where-Object { $_.Extension -in @(".exe", ".dll") })) {
    $dependencyOutput = & $dumpbin.Source /NOLOGO /DEPENDENTS $binary.FullName 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin failed while inspecting dependencies of $($binary.FullName): $($dependencyOutput -join ' ')"
    }
    foreach ($line in $dependencyOutput) {
        if ($line.ToString() -match '^\s*([A-Za-z0-9_.+-]+\.dll)\s*$') {
            $dependencyName = $Matches[1]
            if ($dependencyName -match '^(api-ms-win|ext-ms)-') {
                continue
            }
            if ($bundledDllNames.Contains($dependencyName)) {
                continue
            }
            if (Test-Path -LiteralPath (Join-Path $system32Path $dependencyName) -PathType Leaf) {
                continue
            }
            $unresolvedDependencies.Add("$($binary.Name) -> $dependencyName")
        }
    }
}
if ($unresolvedDependencies.Count -gt 0) {
    throw "Unbundled non-system DLL dependencies were detected: $($unresolvedDependencies -join ', ')"
}
Write-Host "Verified packaged PE imports for $(@($installedFiles | Where-Object { $_.Extension -in @('.exe', '.dll') }).Count) x64 binaries."

# Refuse to publish an incomplete Qt bundle. These checks catch missing Qt
# runtime, UI, image decoding, TLS, or Windows platform dependencies before upload.
foreach ($requiredQtDll in @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Network.dll")) {
    $found = Get-ChildItem -LiteralPath $installPath -File -Filter $requiredQtDll -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $found) {
        throw "Qt runtime DLL is missing from the staged package: $requiredQtDll"
    }
}
foreach ($requiredQtPlugin in @("qwindows.dll", "qschannelbackend.dll", "qwebp.dll")) {
    $found = Get-ChildItem -LiteralPath $installPath -File -Filter $requiredQtPlugin -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $found) {
        throw "Required Qt plugin is missing from the staged package: $requiredQtPlugin"
    }
}
if (-not (Test-Path -LiteralPath (Join-Path $installPath "qt.conf") -PathType Leaf)) {
    throw "Qt's local qt.conf is missing; the package could fall back to an external Qt installation."
}

$runnerTemp = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } elseif ($env:TEMP) { $env:TEMP } else { [IO.Path]::GetTempPath() }
$stagingParent = Join-Path $runnerTemp "VortexLauncher-Windows-x64-stage"
$packageDirectory = Join-Path $stagingParent "VortexLauncher-Windows-x64"
if (Test-Path -LiteralPath $stagingParent) {
    Remove-Item -LiteralPath $stagingParent -Recurse -Force
}
New-Item -ItemType Directory -Path $packageDirectory -Force | Out-Null
Get-ChildItem -LiteralPath $installPath -Force | Copy-Item -Destination $packageDirectory -Recurse -Force

$readme = @"
Vortex Launcher — Windows x64 portable package

Run VortexLauncher.exe from this folder. This package contains the Qt 6 runtime,
Qt plugins, and the x64 Microsoft Visual C++ runtime needed by the launcher.
No Qt SDK, compiler, CMake, or separate VC++ Redistributable installation is
required. The launcher can download compatible Java runtimes when needed.

Full Minecraft access requires a Microsoft account that owns Minecraft: Java
Edition. Local/offline profiles do not bypass account ownership requirements.
"@
[IO.File]::WriteAllText((Join-Path $packageDirectory "README-Windows.txt"), $readme, [Text.UTF8Encoding]::new($false))

$manifestPath = Join-Path $packageDirectory "manifest.txt"
$manifest = Get-ChildItem -LiteralPath $packageDirectory -Recurse -File |
    Where-Object { $_.FullName -ne $manifestPath } |
    ForEach-Object { $_.FullName.Substring($packageDirectory.Length + 1) } |
    Sort-Object
$manifest | Set-Content -LiteralPath $manifestPath -Encoding utf8

$outputFile = if ([IO.Path]::IsPathRooted($OutputPath)) {
    [IO.Path]::GetFullPath($OutputPath)
} else {
    [IO.Path]::GetFullPath((Join-Path $workspace $OutputPath))
}
$outputParent = Split-Path -Parent $outputFile
New-Item -ItemType Directory -Path $outputParent -Force | Out-Null
if (Test-Path -LiteralPath $outputFile) {
    Remove-Item -LiteralPath $outputFile -Force
}
Compress-Archive -Path $packageDirectory -DestinationPath $outputFile -CompressionLevel Optimal
$sha256 = (Get-FileHash -LiteralPath $outputFile -Algorithm SHA256).Hash.ToLowerInvariant()
"$sha256  $(Split-Path -Leaf $outputFile)" | Set-Content -LiteralPath "$outputFile.sha256" -Encoding ascii
Write-Host "Created $outputFile"
Write-Host "SHA256 $sha256"

if (-not $RunSmokeTests) {
    return
}

# Test the exact ZIP after extraction with a deliberately minimal PATH and no
# Qt environment variables, preventing the runner's developer Qt install from
# masking missing files in the release bundle.
$cleanTestRoot = Join-Path $runnerTemp "VortexLauncher-Windows-x64-clean-test"
if (Test-Path -LiteralPath $cleanTestRoot) {
    Remove-Item -LiteralPath $cleanTestRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $cleanTestRoot -Force | Out-Null
Expand-Archive -LiteralPath $outputFile -DestinationPath $cleanTestRoot -Force
$cleanPackage = Join-Path $cleanTestRoot "VortexLauncher-Windows-x64"
$cleanExe = Join-Path $cleanPackage "VortexLauncher.exe"
if (-not (Test-Path -LiteralPath $cleanExe -PathType Leaf)) {
    throw "VortexLauncher.exe was not present after extracting the release ZIP."
}

$qtEnvironmentNames = @("QTDIR", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QT_QPA_FONTDIR", "QT_QPA_PLATFORM")
$savedQtEnvironment = @{}
foreach ($name in $qtEnvironmentNames) {
    $value = [Environment]::GetEnvironmentVariable($name)
    if ($null -ne $value) {
        $savedQtEnvironment[$name] = $value
        [Environment]::SetEnvironmentVariable($name, $null)
    }
}
$originalPath = $env:PATH
$env:PATH = "$cleanPackage;$env:SystemRoot\System32;$env:SystemRoot"
try {
    $versionOutput = (& $cleanExe --version 2>&1 | Out-String).Trim()
    $versionExitCode = $LASTEXITCODE
    Write-Host $versionOutput
    if ($versionExitCode -ne 0) {
        throw "Packaged VortexLauncher.exe --version failed with exit code $versionExitCode."
    }

    $smokeData = Join-Path $cleanTestRoot "user-data"
    New-Item -ItemType Directory -Path $smokeData -Force | Out-Null
    $configPath = Join-Path $smokeData "VortexLauncher.cfg"
    $configText = "[General]`r`nConfigVersion=1.3`r`nIgnoreJavaWizard=true`r`nLanguage=en_US`r`n"
    [IO.File]::WriteAllText($configPath, $configText, [Text.UTF8Encoding]::new($false))

    if (-not ("VortexSmokeWin32" -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class VortexSmokeWin32 {
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool PostMessage(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam);
}
'@
    }

    function Invoke-VortexGuiSmokeRun {
        param(
            [Parameter(Mandatory = $true)][string]$Executable,
            [Parameter(Mandatory = $true)][string]$WorkingDirectory,
            [Parameter(Mandatory = $true)][string]$DataDirectory
        )

        $marker = Join-Path $DataDirectory "live.check"
        if (Test-Path -LiteralPath $marker) {
            Remove-Item -LiteralPath $marker -Force
        }
        $quotedDataDirectory = '"' + $DataDirectory.Replace('"', '\"') + '"'
        $arguments = "--dir $quotedDataDirectory --alive"
        $process = Start-Process -FilePath $Executable -ArgumentList $arguments -WorkingDirectory $WorkingDirectory -PassThru
        $deadline = (Get-Date).AddSeconds(60)
        while ((Get-Date) -lt $deadline -and -not (Test-Path -LiteralPath $marker)) {
            $process.Refresh()
            if ($process.HasExited) {
                throw "Packaged launcher exited before its UI startup marker (exit $($process.ExitCode))."
            }
            Start-Sleep -Seconds 1
        }
        if (-not (Test-Path -LiteralPath $marker)) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            throw "Packaged launcher did not create its startup marker within 60 seconds."
        }

        # This fork keeps the app alive when the last window closes; use the
        # actual Quit shortcut (Ctrl+Q), not WM_CLOSE/Alt+F4, for shutdown.
        $process.Refresh()
        $window = $process.MainWindowHandle
        if ($window -eq [IntPtr]::Zero) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            throw "The started launcher did not expose a main UI window handle."
        }
        [VortexSmokeWin32]::SetForegroundWindow($window) | Out-Null
        Start-Sleep -Milliseconds 250
        $keyMessages = @(
            @{ Message = 0x0100; Key = 0x11; LParam = 0x001D0001 }, # Ctrl down
            @{ Message = 0x0100; Key = 0x51; LParam = 0x00100001 }, # Q down
            @{ Message = 0x0101; Key = 0x51; LParam = 0xC0100001 }, # Q up
            @{ Message = 0x0101; Key = 0x11; LParam = 0xC01D0001 }  # Ctrl up
        )
        foreach ($keyMessage in $keyMessages) {
            $posted = [VortexSmokeWin32]::PostMessage(
                $window,
                [uint32]$keyMessage.Message,
                [IntPtr]::new([int]$keyMessage.Key),
                [IntPtr]::new([long]$keyMessage.LParam)
            )
            if (-not $posted) {
                Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
                throw "Could not send the normal Ctrl+Q shutdown sequence to the launcher window."
            }
        }
        if (-not $process.WaitForExit(20000)) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            throw "Packaged launcher did not exit after the normal Ctrl+Q shutdown shortcut."
        }
        if ($process.ExitCode -ne 0) {
            throw "Packaged launcher exited with code $($process.ExitCode) after normal close."
        }
    }

    Invoke-VortexGuiSmokeRun -Executable $cleanExe -WorkingDirectory $cleanPackage -DataDirectory $smokeData
    if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
        throw "Launcher configuration was not preserved after clean shutdown."
    }
    if (-not (Select-String -LiteralPath $configPath -Pattern '^Language=en_US$' -Quiet)) {
        throw "Launcher configuration did not retain the selected language after shutdown."
    }
    $launcherLogs = Get-ChildItem -LiteralPath (Join-Path $smokeData "logs") -File -Filter "*.log" -ErrorAction SilentlyContinue
    if (-not $launcherLogs) {
        throw "Launcher did not generate a log file during the clean-folder smoke test."
    }
    Invoke-VortexGuiSmokeRun -Executable $cleanExe -WorkingDirectory $cleanPackage -DataDirectory $smokeData
    Write-Host "Clean extracted-package UI startup, normal shutdown, saved-configuration, log, and restart smoke checks passed."
} finally {
    $env:PATH = $originalPath
    foreach ($name in $qtEnvironmentNames) {
        if ($savedQtEnvironment.ContainsKey($name)) {
            [Environment]::SetEnvironmentVariable($name, $savedQtEnvironment[$name])
        }
    }
}
