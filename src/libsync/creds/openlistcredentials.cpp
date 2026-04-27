/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "creds/openlistcredentials.h"

#include "account.h"
#include "accessmanager.h"
#include "theme.h"

#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <qt6keychain/keychain.h>

namespace OCC {

Q_LOGGING_CATEGORY(lcOpenListCredentials, "openlist.sync.credentials", QtInfoMsg)

namespace {

constexpr char authTypeC[] = "openlist";
constexpr char userC[] = "user";
constexpr char tokenKeychainSuffix[] = ":token";

/** AccessManager subclass that injects the JWT token header. */
class OpenListCredentialsAccessManager : public AccessManager
{
public:
    explicit OpenListCredentialsAccessManager(const OpenListCredentials *cred, QObject *parent = nullptr)
        : AccessManager(parent)
        , _cred(cred)
    {
    }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override
    {
        QNetworkRequest req(request);
        if (!req.attribute(AbstractCredentials::DontAddCredentialsAttribute).toBool()) {
            if (_cred && !_cred->token().isEmpty()) {
                // OpenList uses the bare token, not a "Bearer" prefix
                req.setRawHeader("Authorization", _cred->token().toUtf8());
            }
        }
        return AccessManager::createRequest(op, req, outgoingData);
    }

private:
    QPointer<const OpenListCredentials> _cred;
};

} // namespace

OpenListCredentials::OpenListCredentials() = default;

void OpenListCredentials::setAccount(Account *account)
{
    AbstractCredentials::setAccount(account);
    if (_user.isEmpty()) {
        _user = _account->credentialSetting(QLatin1String(userC)).toString();
    }
}

QString OpenListCredentials::authType() const
{
    return QLatin1String(authTypeC);
}

QNetworkAccessManager *OpenListCredentials::createQNAM() const
{
    return new OpenListCredentialsAccessManager(this);
}

bool OpenListCredentials::ready() const
{
    return _ready;
}

QString OpenListCredentials::user() const
{
    return _user;
}

QString OpenListCredentials::password() const
{
    // The "password" abstraction here is the JWT token itself.
    return _token;
}

void OpenListCredentials::setToken(const QString &token)
{
    _token = token;
    _ready = !token.isEmpty();
}

void OpenListCredentials::fetchFromKeychain(const QString &appName)
{
    _wasFetched = true;

    if (!_account) {
        emit fetched();
        return;
    }

    _user = _account->credentialSetting(QLatin1String(userC)).toString();

    const auto key = keychainKey();
    auto *job = new QKeychain::ReadPasswordJob(Theme::instance()->appName(), this);
    if (!appName.isEmpty()) {
        job->setProperty("appName", appName);
    }
    job->setKey(key);
    job->setAutoDelete(false);
    connect(job, &QKeychain::ReadPasswordJob::finished, this, &OpenListCredentials::slotReadJobDone);
    job->start();
}

void OpenListCredentials::slotReadJobDone(QKeychain::Job *incomingJob)
{
    auto *job = qobject_cast<QKeychain::ReadPasswordJob *>(incomingJob);
    Q_ASSERT(job);

    if (job->error() == QKeychain::NoError) {
        _token = job->textData();
        _ready = !_token.isEmpty();
    } else {
        qCWarning(lcOpenListCredentials) << "Failed to read token from keychain:" << job->errorString();
        _token.clear();
        _ready = false;
    }

    job->deleteLater();
    emit fetched();
}

void OpenListCredentials::askFromUser()
{
    // GUI interaction is handled in the wizard/credential GUI layer.
    // Here we just signal completion; the GUI sets the token via setToken()
    // before calling persist().
    emit asked();
}

bool OpenListCredentials::stillValid(QNetworkReply *reply)
{
    if (!reply) {
        return false;
    }
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    // 401 means our token is no longer valid.
    return httpStatus != 401;
}

void OpenListCredentials::persist()
{
    if (!_account) {
        return;
    }

    _account->setCredentialSetting(QLatin1String(userC), _user);

    const auto key = keychainKey();
    auto *job = new QKeychain::WritePasswordJob(Theme::instance()->appName(), this);
    job->setKey(key);
    job->setTextData(_token);
    job->setAutoDelete(false);
    connect(job, &QKeychain::WritePasswordJob::finished, this, &OpenListCredentials::slotWriteJobDone);
    job->start();
}

void OpenListCredentials::slotWriteJobDone(QKeychain::Job *incomingJob)
{
    auto *job = qobject_cast<QKeychain::WritePasswordJob *>(incomingJob);
    Q_ASSERT(job);

    if (job->error() != QKeychain::NoError) {
        qCWarning(lcOpenListCredentials) << "Failed to persist token in keychain:" << job->errorString();
    }

    job->deleteLater();
    emit credentialsPersisted();
}

void OpenListCredentials::invalidateToken()
{
    _token.clear();
    _ready = false;
}

void OpenListCredentials::forgetSensitiveData()
{
    invalidateToken();

    if (!_account) {
        return;
    }

    const auto key = keychainKey();
    auto *job = new QKeychain::DeletePasswordJob(Theme::instance()->appName(), this);
    job->setKey(key);
    job->start();
}

QString OpenListCredentials::keychainKey() const
{
    return AbstractCredentials::keychainKey(
        _account ? _account->url().toString() : QString(),
        _user,
        _account ? _account->id() : QString()
    ) + QLatin1String(tokenKeychainSuffix);
}

} // namespace OCC
