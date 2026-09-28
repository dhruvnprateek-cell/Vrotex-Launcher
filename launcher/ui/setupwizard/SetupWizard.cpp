#include "SetupWizard.h"

#include "JavaWizardPage.h"
#include "LanguageWizardPage.h"

#include <Application.h>
#include <FileSystem.h>
#include "translations/TranslationsModel.h"

#include <BuildConfig.h>
#include <QAbstractButton>
#include <QApplication>
#include <QPalette>
#include <QStringList>

SetupWizard::SetupWizard(QWidget* parent) : QWizard(parent)
{
    setObjectName(QStringLiteral("SetupWizard"));
    resize(620, 660);
    setMinimumSize(300, 400);
    setWizardStyle(QWizard::ClassicStyle);
    setOptions(QWizard::NoCancelButton | QWizard::IndependentPages | QWizard::HaveCustomButton1);
    setWindowIcon(APPLICATION->logo());

    const bool dark = QApplication::palette().color(QPalette::Window).lightness() < 128;
    const QString background = dark ? QStringLiteral("#10141d") : QStringLiteral("#f3f5fa");
    const QString surface = dark ? QStringLiteral("#171e2a") : QStringLiteral("#ffffff");
    const QString border = dark ? QStringLiteral("#293243") : QStringLiteral("#e2e6ef");
    const QString text = dark ? QStringLiteral("#eff3fb") : QStringLiteral("#1a2232");
    const QString muted = dark ? QStringLiteral("#9ba7ba") : QStringLiteral("#687386");
    const QString accent = dark ? QStringLiteral("#8e8aff") : QStringLiteral("#615ce8");
    QString style = QStringLiteral(R"QSS(
        QWizard, QWizardPage { background: %1; color: %4; }
        QWizard QLabel { color: %4; }
        QWizard QTableView, QWizard QTreeView, QWizard QListView { color: %4; background: %2; border: 1px solid %3; selection-background-color: %6; selection-color: %4; }
        QWizard QHeaderView::section { color: %5; background: %1; border: 0; border-bottom: 1px solid %3; padding: 6px 8px; }
        QWizard QLineEdit, QWizard QComboBox { color: %4; background: %2; border: 1px solid %3; border-radius: 7px; padding: 7px 9px; }
        QWizard QLineEdit:focus, QWizard QComboBox:focus { border: 1px solid %6; }
        QWizard QCheckBox, QWizard QRadioButton { color: %4; spacing: 7px; }
        QWizard QPushButton { color: %4; background: %2; border: 1px solid %3; border-radius: 8px; padding: 8px 14px; font-weight: 600; }
        QWizard QPushButton:hover { border-color: %6; }
        QWizard QPushButton:disabled { color: %5; background: %1; }
    )QSS");
    const QStringList colors = { background, surface, border, text, muted, accent };
    for (int token = colors.size(); token > 0; --token)
        style.replace(QStringLiteral("%") + QString::number(token), colors.at(token - 1));
    setStyleSheet(style);

    retranslate();

    connect(this, &QWizard::currentIdChanged, this, &SetupWizard::pageChanged);
}

void SetupWizard::retranslate()
{
    setButtonText(QWizard::NextButton, tr("&Next >"));
    setButtonText(QWizard::BackButton, tr("< &Back"));
    setButtonText(QWizard::FinishButton, tr("&Finish"));
    setButtonText(QWizard::CustomButton1, tr("&Refresh"));
    setWindowTitle(tr("%1 Quick Setup").arg(BuildConfig.LAUNCHER_DISPLAYNAME));
}

BaseWizardPage* SetupWizard::getBasePage(int id)
{
    if (id == -1)
        return nullptr;
    auto pagePtr = page(id);
    if (!pagePtr)
        return nullptr;
    return dynamic_cast<BaseWizardPage*>(pagePtr);
}

BaseWizardPage* SetupWizard::getCurrentBasePage()
{
    return getBasePage(currentId());
}

void SetupWizard::pageChanged(int id)
{
    auto basePagePtr = getBasePage(id);
    if (!basePagePtr) {
        return;
    }
    if (basePagePtr->wantsRefreshButton()) {
        setButtonLayout({ QWizard::CustomButton1, QWizard::Stretch, QWizard::BackButton, QWizard::NextButton, QWizard::FinishButton });
        auto customButton = button(QWizard::CustomButton1);
        connect(customButton, &QAbstractButton::clicked, [this]() {
            auto basePagePtr = getCurrentBasePage();
            if (basePagePtr) {
                basePagePtr->refresh();
            }
        });
    } else {
        setButtonLayout({ QWizard::Stretch, QWizard::BackButton, QWizard::NextButton, QWizard::FinishButton });
    }
}

void SetupWizard::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QWizard::changeEvent(event);
}

SetupWizard::~SetupWizard() {}
