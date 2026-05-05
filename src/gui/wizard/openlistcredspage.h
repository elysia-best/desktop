/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_OPENLIST_CREDS_PAGE_H
#define MIRALL_OPENLIST_CREDS_PAGE_H

#include "wizard/abstractcredswizardpage.h"
#include "wizard/owncloudwizard.h"

#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>

class QProgressIndicator;

namespace OCC {

/**
 * @brief Login page for OpenList using username + password with pre-hashed password.
 */
class OpenListCredsPage : public AbstractCredentialsWizardPage
{
    Q_OBJECT
public:
    OpenListCredsPage(QWidget *parent);

    [[nodiscard]] AbstractCredentials *getCredentials() const override;

    void initializePage() override;
    void cleanupPage() override;
    bool validatePage() override;
    void setConnected();
    void setErrorString(const QString &err);

    [[nodiscard]] int nextId() const override;

Q_SIGNALS:
    void connectToOCUrl(const QString &);

public slots:
    void slotStyleChanged();

private:
    void startSpinner();
    void stopSpinner();
    void customizeStyle();

    QLineEdit *_leUsername = nullptr;
    QLineEdit *_lePassword = nullptr;
    QLineEdit *_leOtpCode = nullptr;
    QCheckBox *_otpCheckbox = nullptr;
    QProgressIndicator *_progressIndi = nullptr;
    OwncloudWizard *_ocWizard = nullptr;
    bool _connected = false;
};

} // namespace OCC

#endif
