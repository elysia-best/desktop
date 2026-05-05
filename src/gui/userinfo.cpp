/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "userinfo.h"
#include "account.h"
#include "accountstate.h"
#include "networkjobs.h"
#include "folderman.h"
#include "creds/abstractcredentials.h"
#include <theme.h>

#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace OCC {

namespace {
    static const int defaultIntervalT = 30 * 1000;
    static const int failIntervalT = 5 * 1000;
}

UserInfo::UserInfo(AccountState *accountState, bool allowDisconnectedAccountState, bool fetchAvatarImage, QObject *parent)
    : QObject(parent)
    , _accountState(accountState)
    , _allowDisconnectedAccountState(allowDisconnectedAccountState)
    , _fetchAvatarImage(fetchAvatarImage)
{
    connect(accountState, &AccountState::stateChanged,
        this, &UserInfo::slotAccountStateChanged);
    connect(&_jobRestartTimer, &QTimer::timeout, this, &UserInfo::slotFetchInfo);
    _jobRestartTimer.setSingleShot(true);
}

void UserInfo::setActive(bool active)
{
    _active = active;
    slotAccountStateChanged();
}


void UserInfo::slotAccountStateChanged()
{
    if (canGetInfo()) {
        // Obviously assumes there will never be more than thousand of hours between last info
        // received and now, hence why we static_cast
        auto elapsed = static_cast<int>(_lastInfoReceived.msecsTo(QDateTime::currentDateTime()));
        if (_lastInfoReceived.isNull() || elapsed >= defaultIntervalT) {
            slotFetchInfo();
        } else {
            _jobRestartTimer.start(defaultIntervalT - elapsed);
        }
    } else {
        _jobRestartTimer.stop();
    }
}

void UserInfo::slotRequestFailed()
{
    _lastQuotaTotalBytes = 0;
    _lastQuotaUsedBytes = 0;
    _jobRestartTimer.start(failIntervalT);
}

bool UserInfo::canGetInfo() const
{
    if (!_accountState || !_active || !_accountState->account() || _accountState->account()->isPublicShareLink()) {
        return false;
    }
    AccountPtr account = _accountState->account();
    return (_accountState->isConnected() || _allowDisconnectedAccountState)
        && account->credentials()
        && account->credentials()->ready();
}

void UserInfo::slotFetchInfo()
{
    if (!canGetInfo()) {
        return;
    }

    if (_job) {
        // The previous job was not finished?  Then we cancel it!
        _job->deleteLater();
    }

    AccountPtr account = _accountState->account();

    const bool isOpenList = account->credentials()
        && account->credentials()->authType() == QLatin1String("openlist");

    _job = new JsonApiJob(account,
        isOpenList ? QLatin1String("api/me") : QLatin1String("ocs/v1.php/cloud/user"),
        this);
    _job->setTimeout(20 * 1000);
    connect(_job.data(), &JsonApiJob::jsonReceived, this, &UserInfo::slotUpdateLastInfo);
    connect(_job.data(), &AbstractNetworkJob::networkError, this, &UserInfo::slotRequestFailed);
    _job->start();
}

void UserInfo::slotUpdateLastInfo(const QJsonDocument &json)
{
    AccountPtr account = _accountState->account();

    const bool isOpenList = account->credentials()
        && account->credentials()->authType() == QLatin1String("openlist");

    QString userId;
    QString displayName;
    QJsonObject objData;

    if (isOpenList) {
        // OpenList response: { code, message, data: { id, username, ... } }
        objData = json.object().value("data"_L1).toObject();
        userId = objData.value("username"_L1).toString();
        displayName = objData.value("nickname"_L1).toString();
        if (displayName.isEmpty()) {
            displayName = userId;
        }
    } else {
        // Nextcloud response: { ocs: { data: { id, display-name, quota, ... } } }
        objData = json.object().value("ocs"_L1).toObject().value("data"_L1).toObject();
        userId = objData.value("id"_L1).toString();
        displayName = objData.value("display-name"_L1).toString();
    }

    if (!userId.isEmpty()) {
        if (QString::compare(account->davUser(), userId, Qt::CaseInsensitive) != 0) {
            // TODO: the error message should be in the UI
            qInfo() << "Authenticated with the wrong user! Please login with the account:" << account->prettyName();
            if (account->credentials()) {
                account->credentials()->askFromUser();
            }
            return;
        }
        account->setDavUser(userId);
    }

    if (!displayName.isEmpty()) {
        account->setDavDisplayName(displayName);
    }

    if (!isOpenList) {
        auto objQuota = objData.value("quota"_L1).toObject();
        qint64 used = objQuota.value("used"_L1).toDouble();
        qint64 total = objQuota.value("quota"_L1).toDouble();

        if(_lastInfoReceived.isNull() || _lastQuotaUsedBytes != used || _lastQuotaTotalBytes != total) {
            _lastQuotaUsedBytes = used;
            _lastQuotaTotalBytes = total;
            emit quotaUpdated(_lastQuotaTotalBytes, _lastQuotaUsedBytes);
        }
    }

    _jobRestartTimer.start(defaultIntervalT);
    _lastInfoReceived = QDateTime::currentDateTime();

    if(_fetchAvatarImage && !account->isPublicShareLink() && !isOpenList) {
        auto *job = new AvatarJob(account, account->davUser(), 128, this);
        job->setTimeout(20 * 1000);
        QObject::connect(job, &AvatarJob::avatarPixmap, this, &UserInfo::slotAvatarImage);
        job->start();
        return;
    }

    emit fetchedLastInfo(this);
}

void UserInfo::slotAvatarImage(const QImage &img)
{
    _accountState->account()->setAvatar(img);

    emit fetchedLastInfo(this);
}

} // namespace OCC
