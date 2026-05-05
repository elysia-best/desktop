/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "creds/openlistcredentials.h"

#include "account.h"
#include "accessmanager.h"
#include "theme.h"
#include "common/utility.h"

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
constexpr char openlistUserC[] = "user";
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
    const OpenListCredentials *_cred;
};

} // namespace

OpenListCredentials::OpenListCredentials() = default;

void OpenListCredentials::setAccount(Account *account)
{
    AbstractCredentials::setAccount(account);
    if (_user.isEmpty()) {
        _user = _account->credentialSetting(QLatin1String(openlistUserC)).toString();
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
    return _password;
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

    _user = _account->credentialSetting(QLatin1String(openlistUserC)).toString();

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
    if (!_account) {
        qCWarning(lcOpenListCredentials) << "No account set, cannot login.";
        emit asked();
        return;
    }

    if (_password.isEmpty()) {
        qCWarning(lcOpenListCredentials) << "No password set, cannot perform login.";
        emit asked();
        return;
    }

    // Build the JSON request body
    QJsonObject body;
    body.insert(QStringLiteral("username"), _user);
    body.insert(QStringLiteral("password"), Utility::sha256Hash(_password));
    if (!_otpCode.isEmpty()) {
        body.insert(QStringLiteral("otp_code"), _otpCode);
    }

    QNetworkRequest req;
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setAttribute(AbstractCredentials::DontAddCredentialsAttribute, true);

    const auto loginUrl = Utility::concatUrlPath(_account->url().toString(), QStringLiteral("/api/auth/login/hash"));
    qCInfo(lcOpenListCredentials) << "Logging in at" << loginUrl << "for user" << _user;

    auto *reply = _account->sendRawRequest("POST", loginUrl, req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, &OpenListCredentials::slotLoginReplyFinished);
}

void OpenListCredentials::slotLoginReplyFinished()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        qCWarning(lcOpenListCredentials) << "slotLoginReplyFinished called without a valid reply";
        _ready = false;
        emit asked();
        return;
    }
    reply->deleteLater();

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto responseData = reply->readAll();

    if (httpStatus == 200) {
        const auto json = QJsonDocument::fromJson(responseData);
        const auto data = json.object().value(QStringLiteral("data")).toObject();
        const auto token = data.value(QStringLiteral("token")).toString();

        if (!token.isEmpty()) {
            qCInfo(lcOpenListCredentials) << "Login successful for user" << _user;
            _token = token;
            _ready = true;
            _otpCode.clear(); // one-time use
            persist();
            emit asked();
            return;
        }
    }

    // Try to extract error message from response
    QString errorMsg;
    const auto json = QJsonDocument::fromJson(responseData);
    const auto message = json.object().value(QStringLiteral("message")).toString();
    if (!message.isEmpty()) {
        errorMsg = message;
    } else if (!json.object().value(QStringLiteral("error")).toString().isEmpty()) {
        errorMsg = json.object().value(QStringLiteral("error")).toString();
    } else {
        errorMsg = reply->errorString();
    }

    qCWarning(lcOpenListCredentials) << "Login failed for user" << _user << "- HTTP" << httpStatus << errorMsg;
    _ready = false;
    _token.clear();
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

    _account->setCredentialSetting(QLatin1String(openlistUserC), _user);

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
    _password.clear();
    _otpCode.clear();
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
