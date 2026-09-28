<p align="center">
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="/program_info/org.prismlauncher.PrismLauncher.logo-darkmode.svg">
  <source media="(prefers-color-scheme: light)" srcset="/program_info/org.prismlauncher.PrismLauncher.logo.svg">
  <img alt="Prism Launcher" src="/program_info/org.prismlauncher.PrismLauncher.logo.svg" width="40%">
</picture>
</p>

<p align="center">
  Prism Launcher is a custom launcher for Minecraft that allows you to easily manage multiple installations of Minecraft at once.<br />
  <br />This is a <b>fork</b> of the MultiMC Launcher and is <b>not</b> endorsed by it.
</p>

## Installation

<a href="https://repology.org/project/prismlauncher/versions">
    <img src="https://repology.org/badge/vertical-allrepos/prismlauncher.svg" alt="Packaging status" align="right">
</a>

- All downloads and instructions for Prism Launcher can be found on our [Website](https://prismlauncher.org/download).
- Last build status can be found in the [GitHub Actions](https://github.com/PrismLauncher/PrismLauncher/actions) tab (this also includes the pull requests status).

### Development Builds

Please understand that these builds are not intended for most users. There may be bugs, and other instabilities. You have been warned.

There are development builds available through:

- [GitHub Actions](https://github.com/PrismLauncher/PrismLauncher/actions) (includes builds from pull requests opened by contributors)
- [nightly.link](https://prismlauncher.org/nightly) (this will always point only to the latest version of develop)

These have debug information in the binaries, so their file sizes are relatively larger.

Prebuilt Development builds are provided for **Linux**, **Windows** and **macOS**.

On Linux, we also offer our own [Flatpak nightly repository](https://github.com/PrismLauncher/flatpak). Most software centers are able to install it by opening [this link](https://flatpak.prismlauncher.org/prismlauncher-nightly.flatpakref).

## Community & Support

Feel free to create a GitHub issue if you find a bug or want to suggest a new feature. We have multiple community spaces where other community members can help you:

- **Our Discord server:**

[![Prism Launcher Discord server](https://discordapp.com/api/guilds/1031648380885147709/widget.png?style=banner3)](https://prismlauncher.org/discord)

- **Our Matrix space:**

[![Prism Launcher Space](https://img.shields.io/matrix/prismlauncher:matrix.org?style=for-the-badge&label=Matrix%20Space&logo=matrix&color=purple)](https://prismlauncher.org/matrix)

- **Our Subreddit:**

[![r/PrismLauncher](https://img.shields.io/reddit/subreddit-subscribers/prismlauncher?style=for-the-badge&logo=reddit)](https://prismlauncher.org/reddit)

## Translations

The translation effort for Prism Launcher is hosted on [Weblate](https://hosted.weblate.org/projects/prismlauncher/launcher/) and information about translating Prism Launcher is available at <https://github.com/PrismLauncher/Translations>.

## Building

If you want to build Prism Launcher yourself, check the [build instructions](https://prismlauncher.org/wiki/development/build-instructions).

### Windows Build

Push this repository to GitHub; the **Windows x64 Build** workflow runs on pushes to `main`, `master`, and `release-11.x`, and on pull requests. To start one manually, open **Actions → Windows x64 Build** and select **Run workflow**.

After a successful run, open the workflow run and download **Actions → workflow run → Artifacts → VortexLauncher-Windows-x64**. The artifact contains `VortexLauncher-Windows-x64.zip` with `VortexLauncher.exe`.

#### Build locally on Windows x64

Install Visual Studio 2022 with **Desktop development with C++** (MSVC v143 and a Windows SDK), CMake 3.31.6, a Java 17 JDK, Qt 6.10.2 **MSVC 2022 64-bit** (including `qtimageformats` and `qtnetworkauth`), Git, and vcpkg. In an x64 Visual Studio Developer PowerShell, set the paths below to your installs. The checked-in vcpkg manifest and baseline install the remaining C++ dependencies during configure.

```powershell
# Run once if vcpkg is not already installed:
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
& C:\vcpkg\bootstrap-vcpkg.bat

# From the repository root; adjust these paths if needed:
git submodule update --init --recursive
$env:VCPKG_ROOT = 'C:\vcpkg'
$env:ARTIFACT_NAME = 'Windows-MSVC-Qt6'
$env:BUILD_PLATFORM = 'official'
$qtRoot = 'C:\Qt\6.10.2\msvc2022_64'

cmake --preset windows_msvc_x64 "-DQt6_DIR=$qtRoot\lib\cmake\Qt6"
cmake --build --preset windows_msvc_x64 --config Release --parallel 2
ctest --preset windows_msvc_x64 --build-config Release --output-on-failure

Remove-Item install, install-portable -Recurse -Force -ErrorAction SilentlyContinue
cmake --install build --config Release
New-Item -ItemType Directory install-portable -Force | Out-Null
Get-ChildItem install -Force | Copy-Item -Destination install-portable -Recurse -Force
cmake --install build --config Release --prefix install-portable --component portable
Set-ExecutionPolicy -Scope Process Bypass -Force
./scripts/package_windows_x64.ps1 -InstallDirectory install-portable -BuildType Release -OutputPath VortexLauncher-Windows-x64.zip -RunSmokeTests
```

The packaging command validates the x64 executable and bundled runtimes, then tests startup, shutdown, saved configuration/logs, and restart from a clean ZIP extraction. Run it in a logged-in desktop session; it has not been executed in this environment.

## Sponsors & Partners

We thank all the wonderful backers over at Open Collective! Support Prism Launcher by [becoming a backer](https://opencollective.com/prismlauncher).

[![OpenCollective Backers](https://opencollective.com/prismlauncher/backers.svg?width=890&limit=1000)](https://opencollective.com/prismlauncher#backers)

Thanks to JetBrains for providing us a few licenses for all their products, as part of their [Open Source program](https://www.jetbrains.com/opensource/).

<a href="https://jb.gg/OpenSource">
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://www.jetbrains.com/company/brand/img/logo_jb_dos_4.svg">
  <source media="(prefers-color-scheme: light)" srcset="https://resources.jetbrains.com/storage/products/company/brand/logos/jetbrains.svg">
  <img alt="JetBrains logo" src="https://resources.jetbrains.com/storage/products/company/brand/logos/jetbrains.svg" width="40%">
</picture>
</a>

Thanks to Weblate for hosting our translation efforts.

<a href="https://hosted.weblate.org/engage/prismlauncher/">
<img src="https://hosted.weblate.org/widgets/prismlauncher/-/open-graph.png" alt="Translation status" width="300" />
</a>

Thanks to Netlify for providing us their excellent web services, as part of their [Open Source program](https://www.netlify.com/open-source/).

<a href="https://www.netlify.com"> <img src="https://www.netlify.com/v3/img/components/netlify-color-accent.svg" alt="Deploys by Netlify" /> </a>

Thanks to the awesome people over at [MacStadium](https://www.macstadium.com/), for providing M1-Macs for development purposes!

<a href="https://www.macstadium.com"><img src="https://uploads-ssl.webflow.com/5ac3c046c82724970fc60918/5c019d917bba312af7553b49_MacStadium-developerlogo.png" alt="Powered by MacStadium" width="300"></a>

## Forking/Redistributing/Custom builds policy

You are free to fork, redistribute and provide custom builds as long as you follow the terms of the [license](LICENSE) (this is a legal responsibility), and if you made code changes rather than just packaging a custom build, please do the following as a basic courtesy:

- Make it clear that your fork is not Prism Launcher and is not endorsed by or affiliated with the Prism Launcher project (<https://prismlauncher.org>).
- Go through [CMakeLists.txt](CMakeLists.txt) and change Prism Launcher's API keys to your own or set them to empty strings (`""`) to disable them (this way the program will still compile but the functionality requiring those keys will be disabled).

If you have any questions or want any clarification on the above conditions please make an issue and ask us.

If you are just building Prism Launcher for your distribution, please make sure to set the `Launcher_BUILD_PLATFORM` to a slug representing your distribution. Examples are `archlinux`, `fedora` and `nixpkgs`.

Note that if you build this software without removing the provided API keys in [CMakeLists.txt](CMakeLists.txt) you are accepting the following terms and conditions:

- [Microsoft Identity Platform Terms of Use](https://docs.microsoft.com/en-us/legal/microsoft-identity-platform/terms-of-use)
- [CurseForge 3rd Party API Terms and Conditions](https://support.curseforge.com/en/support/solutions/articles/9000207405-curse-forge-3rd-party-api-terms-and-conditions)

If you do not agree with these terms and conditions, then remove the associated API keys from the [CMakeLists.txt](CMakeLists.txt) file by setting them to an empty string (`""`).

## License [![https://github.com/PrismLauncher/PrismLauncher/blob/develop/LICENSE](https://img.shields.io/github/license/PrismLauncher/PrismLauncher?label=License&logo=gnu&color=C4282D)](LICENSE)

All launcher code is available under the GPL-3.0-only license.

The logo and related assets are under the CC BY-SA 4.0 license.
