/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "creds/abstractcredentials.h"
#include "accessmanager.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QString>

namespace QKeychain {
class Job;
class WritePasswordJob;
class ReadPasswordJob;
}

namespace OCC {

/**
 * @brief Credentials implementation for OpenList using JWT token authentication.
 *
 * Authentication flow:
 *  1. fetchFromKeychain() – loads the previously persisted JWT token from the
 *     system keychain and emits fetched() when done.
 *  2. askFromUser()       – prompts the user for username + password, POSTs to
 *     POST /api/auth/login, stores the returned JWT token, and emits asked().
 *  3. persist()           – writes the JWT token to the system keychain.
 *  4. forgetSensitiveData() – removes the token from the keychain and clears
 *     the in-memory copy.
 *
 * Every outgoing request gets an "Authorization: <token>" header injected by
 * the custom QNetworkAccessManager returned from createQNAM().
 *
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT OpenListCredentials : public AbstractCredentials
{
    Q_OBJECT

public:
    OpenListCredentials();

    [[nodiscard]] QString authType() const override;
    [[nodiscard]] QNetworkAccessManager *createQNAM() const override;
    [[nodiscard]] bool ready() const override;

    void fetchFromKeychain(const QString &appName = {}) override;
    void askFromUser() override;

    bool stillValid(QNetworkReply *reply) override;
    void persist() override;

    [[nodiscard]] QString user() const override;
    [[nodiscard]] QString password() const override;

    void invalidateToken() override;
    void forgetSensitiveData() override;

    void setAccount(Account *account) override;

    /** The JWT token returned by /api/auth/login. */
    [[nodiscard]] QString token() const { return _token; }
    void setToken(const QString &token);

private Q_SLOTS:
    void slotReadJobDone(QKeychain::Job *job);
    void slotWriteJobDone(QKeychain::Job *job);

private:
    QString keychainKey() const;

    QString _user;
    QString _token;
    bool _ready = false;
};

} // namespace OCC
