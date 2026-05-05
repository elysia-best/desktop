/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "QProgressIndicator.h"

#include "creds/openlistcredentials.h"
#include "theme.h"
#include "account.h"
#include "configfile.h"
#include "wizard/openlistcredspage.h"
#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudwizard.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QProgressBar>

namespace OCC {

OpenListCredsPage::OpenListCredsPage(QWidget *parent)
    : AbstractCredentialsWizardPage()
{
    if (parent) {
        _ocWizard = qobject_cast<OwncloudWizard *>(parent);
    }

    setTitle(WizardCommon::titleTemplate().arg(tr("Connect to %1").arg(Theme::instance()->appNameGUI())));
    setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Enter your OpenList credentials")));

    auto *mainLayout = new QVBoxLayout(this);

    // Error label
    auto *errorLabel = new QLabel(this);
    errorLabel->setObjectName(QStringLiteral("errorLabel"));
    errorLabel->setVisible(false);
    errorLabel->setWordWrap(true);
    errorLabel->setStyleSheet(QStringLiteral("color: red;"));
    mainLayout->addWidget(errorLabel);

    // Form
    auto *formLayout = new QFormLayout();

    _leUsername = new QLineEdit(this);
    _leUsername->setPlaceholderText(tr("Username"));
    formLayout->addRow(tr("&Username:"), _leUsername);

    _lePassword = new QLineEdit(this);
    _lePassword->setEchoMode(QLineEdit::Password);
    _lePassword->setPlaceholderText(tr("Password"));
    formLayout->addRow(tr("&Password:"), _lePassword);

    // OTP checkbox + field
    _otpCheckbox = new QCheckBox(tr("Use two-factor authentication code"), this);
    formLayout->addRow(_otpCheckbox);

    _leOtpCode = new QLineEdit(this);
    _leOtpCode->setPlaceholderText(tr("6-digit code"));
    _leOtpCode->setMaxLength(10);
    _leOtpCode->setVisible(false);
    formLayout->addRow(tr("&2FA Code:"), _leOtpCode);

    connect(_otpCheckbox, &QCheckBox::toggled, _leOtpCode, &QLineEdit::setVisible);

    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();

    // Progress indicator
    _progressIndi = new QProgressIndicator(this);
    mainLayout->addWidget(_progressIndi);
    stopSpinner();

    registerField(QLatin1String("OCUser*"), _leUsername);
    registerField(QLatin1String("OCPasswd*"), _lePassword);
}

void OpenListCredsPage::initializePage()
{
    auto *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
    if (ocWizard) {
        QUrl url = ocWizard->account()->url();
        const QString user = url.userName();
        if (!user.isEmpty()) {
            _leUsername->setText(user);
        }
    }
    _leUsername->setFocus();
    customizeStyle();
}

void OpenListCredsPage::cleanupPage()
{
    _leUsername->clear();
    _lePassword->clear();
    _leOtpCode->clear();
    _otpCheckbox->setChecked(false);
    _connected = false;
}

bool OpenListCredsPage::validatePage()
{
    if (_leUsername->text().isEmpty()) {
        return false;
    }

    if (!_connected) {
        auto *errorLabel = findChild<QLabel *>(QStringLiteral("errorLabel"));
        if (errorLabel) {
            errorLabel->setVisible(false);
        }
        startSpinner();
        emit completeChanged();
        emit connectToOCUrl(field("OCUrl").toString().simplified());
        return false;
    }

    _connected = false;
    emit completeChanged();
    stopSpinner();

    return true;
}

void OpenListCredsPage::setConnected()
{
    _connected = true;
    stopSpinner();
}

void OpenListCredsPage::setErrorString(const QString &err)
{
    auto *errorLabel = findChild<QLabel *>(QStringLiteral("errorLabel"));
    if (errorLabel) {
        if (err.isEmpty()) {
            errorLabel->setVisible(false);
        } else {
            errorLabel->setVisible(true);
            errorLabel->setText(err);
        }
    }
    emit completeChanged();
    stopSpinner();
}

void OpenListCredsPage::startSpinner()
{
    _progressIndi->setVisible(true);
    _progressIndi->startAnimation();
}

void OpenListCredsPage::stopSpinner()
{
    _progressIndi->setVisible(false);
    _progressIndi->stopAnimation();
}

AbstractCredentials *OpenListCredsPage::getCredentials() const
{
    auto *creds = new OpenListCredentials();
    creds->setUser(_leUsername->text());
    creds->setPassword(_lePassword->text());
    if (_otpCheckbox->isChecked()) {
        creds->setOtpCode(_leOtpCode->text());
    }
    return creds;
}

int OpenListCredsPage::nextId() const
{
    return WizardCommon::Page_AdvancedSetup;
}

void OpenListCredsPage::slotStyleChanged()
{
    customizeStyle();
}

void OpenListCredsPage::customizeStyle()
{
    if (_progressIndi) {
        _progressIndi->setColor(QGuiApplication::palette().color(QPalette::Text));
    }
}

} // namespace OCC
