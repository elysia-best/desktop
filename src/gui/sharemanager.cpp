/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharemanager.h"
#include "account.h"
#include "folderman.h"
#include "accountstate.h"
#include "networkjobs.h"

#include <QUrl>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcUserGroupShare, "openlist.gui.usergroupshare", QtInfoMsg)

namespace OCC {

/**
 * When a share is modified, we need to tell the folders so they can adjust overlay icons
 */
static void updateFolder(const AccountPtr &account, QStringView path)
{
    for (auto f : std::as_const(FolderMan::instance()->map())) {
        if (f->accountState()->account() != account)
            continue;
        auto folderPath = f->remotePath();
        if (path.startsWith(folderPath) && (path == folderPath || folderPath.endsWith('/') || path[folderPath.size()] == '/')) {
            // Workaround the fact that the server does not invalidate the etags of parent directories
            // when something is shared.
            auto relative = path.mid(f->remotePathTrailingSlash().length());
            f->journalDb()->schedulePathForRemoteDiscovery(relative.toString());

            // Schedule a sync so it can update the remote permission flag and let the socket API
            // know about the shared icon.
            f->scheduleThisFolderSoon();
        }
    }
}

static QUrl openListApiUrl(const AccountPtr &account, const QString &path)
{
    return Utility::concatUrlPath(account->url(), path);
}

static QNetworkRequest makeJsonRequest()
{
    QNetworkRequest req;
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return req;
}

static QBuffer *makeJsonBody(const QJsonObject &obj)
{
    auto *buf = new QBuffer;
    buf->setData(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    return buf;
}


Share::Share(AccountPtr account,
    const QString &id,
    const QString &uidOwner,
    const QString &uidFileOwner,
    const QString &ownerDisplayName,
    const QString &path,
    const ShareType shareType,
    bool isPasswordSet,
    const Permissions permissions,
    const ShareePtr shareWith)
    : _account(account)
    , _id(id)
    , _uidOwner(uidOwner)
    , _uidFileOwner(uidFileOwner)
    , _ownerDisplayName(ownerDisplayName)
    , _path(path)
    , _shareType(shareType)
    , _isPasswordSet(isPasswordSet)
    , _permissions(permissions)
    , _shareWith(shareWith)
{
}

AccountPtr Share::account() const
{
    return _account;
}

QString Share::path() const
{
    return _path;
}

QString Share::getId() const
{
    return _id;
}

QString Share::getUidOwner() const
{
    return _uidOwner;
}

QString Share::getUidFileOwner() const
{
    return _uidFileOwner;
}

QString Share::getOwnerDisplayName() const
{
    return _ownerDisplayName;
}

Share::ShareType Share::getShareType() const
{
    return _shareType;
}

ShareePtr Share::getShareWith() const
{
    return _shareWith;
}

void Share::setPassword(const QString &password)
{
    QJsonObject body;
    body[QStringLiteral("id")] = getId();
    body[QStringLiteral("password")] = password;
    auto *job = _account->sendRequest("PUT", openListApiUrl(_account, QStringLiteral("/api/share/create")),
        makeJsonRequest(), makeJsonBody(body));
    connect(job, &SimpleNetworkJob::finishedSignal, this, [this, password](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            slotSetPasswordError(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                reply->errorString());
            return;
        }
        slotPasswordSet(QJsonDocument(), password);
    });
}

bool Share::isPasswordSet() const
{
    return _isPasswordSet;
}

void Share::setPermissions(Permissions permissions)
{
    QJsonObject body;
    body[QStringLiteral("id")] = getId();
    body[QStringLiteral("permissions")] = static_cast<int>(permissions);
    auto *job = _account->sendRequest("PUT", openListApiUrl(_account, QStringLiteral("/api/share/create")),
        makeJsonRequest(), makeJsonBody(body));
    connect(job, &SimpleNetworkJob::finishedSignal, this, [this, permissions](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            slotOcsError(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                reply->errorString());
            return;
        }
        slotPermissionsSet(QJsonDocument(), permissions);
    });
}

void Share::slotPermissionsSet(const QJsonDocument &, const QVariant &value)
{
    _permissions = (Permissions)value.toInt();
    emit permissionsSet();
}

Share::Permissions Share::getPermissions() const
{
    return _permissions;
}

void Share::deleteShare()
{
    QJsonObject body;
    body[QStringLiteral("id")] = getId();
    auto *job = _account->sendRequest("DELETE", openListApiUrl(_account, QStringLiteral("/api/share/delete")),
        makeJsonRequest(), makeJsonBody(body));
    connect(job, &SimpleNetworkJob::finishedSignal, this, [this](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            slotOcsError(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                reply->errorString());
            return;
        }
        slotDeleted();
    });
}

bool Share::isShareTypeUserGroupEmailRoomOrRemote(const ShareType type)
{
    return (type == Share::TypeUser || type == Share::TypeGroup || type == Share::TypeEmail || type == Share::TypeRoom
        || type == Share::TypeRemote);
}

void Share::slotDeleted()
{
    updateFolder(_account, _path);
    emit shareDeleted();
}

void Share::slotOcsError(int statusCode, const QString &message)
{
    emit serverError(statusCode, message);
}

void Share::slotPasswordSet(const QJsonDocument &, const QVariant &value)
{
    _isPasswordSet = !value.toString().isEmpty();
    emit passwordSet();
}

void Share::slotSetPasswordError(int statusCode, const QString &message)
{
    emit passwordSetError(statusCode, message);
}

QUrl LinkShare::getLink() const
{
    return _url;
}

QUrl LinkShare::getDirectDownloadLink() const
{
    QUrl url = _url;
    url.setPath(url.path() + "/download");
    return url;
}

QDate LinkShare::getExpireDate() const
{
    return _expireDate;
}

LinkShare::LinkShare(AccountPtr account,
    const QString &id,
    const QString &uidOwner,
    const QString &uidFileOwner,
    const QString &ownerDisplayName,
    const QString &path,
    const QString &name,
    const QString &token,
    Permissions permissions,
    bool isPasswordSet,
    const QUrl &url,
    const QDate &expireDate,
    const QString &note,
    const QString &label,
    const bool hideDownload)
    : Share(account, id, uidOwner, uidFileOwner, ownerDisplayName, path, Share::TypeLink, isPasswordSet, permissions)
    , _name(name)
    , _token(token)
    , _note(note)
    , _expireDate(expireDate)
    , _url(url)
    , _label(label)
    , _hideDownload(hideDownload)
{
}

bool LinkShare::getPublicUpload() const
{
    return _permissions & SharePermissionCreate;
}

bool LinkShare::getShowFileListing() const
{
    return _permissions & SharePermissionRead;
}

QString LinkShare::getName() const
{
    return _name;
}

QString LinkShare::getNote() const
{
    return _note;
}

QString LinkShare::getLabel() const
{
    return _label;
}

bool LinkShare::getHideDownload() const
{
    return _hideDownload;
}

void LinkShare::setName(const QString &name)
{
    QJsonObject body;
    body[QStringLiteral("id")] = getId();
    body[QStringLiteral("name")] = name;
    auto *job = _account->sendRequest("PUT", openListApiUrl(_account, QStringLiteral("/api/share/create")),
        makeJsonRequest(), makeJsonBody(body));
    connect(job, &SimpleNetworkJob::finishedSignal, this, [this, name](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            slotOcsError(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                reply->errorString());
            return;
        }
        slotNameSet(QJsonDocument(), name);
    });
}

void LinkShare::setNote(const QString &note)
{
    QJsonObject body;
    body[QStringLiteral("id")] = getId();
    body[QStringLiteral("note")] = note;
    auto *job = _account->sendRequest("PUT", openListApiUrl(_account, QStringLiteral("/api/share/create")),
        makeJsonRequest(), makeJsonBody(body));
    connect(job, &SimpleNetworkJob::finishedSignal, this, [this, note](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            slotOcsError(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                reply->errorString());
            return;
        }
        slotNoteSet(QJsonDocument(), note);
    });
}

void LinkShare::slotNoteSet(const QJsonDocument &, const QVariant &note)
{
    _note = note.toString();
    emit noteSet();
}

QString LinkShare::getToken() const
{
    return _token;
}

void LinkShare::setExpireDate(const QDate &date)
{
    QJsonObject body;
    body[QStringLiteral("id")] = getId();
    body[QStringLiteral("expiration")] = date.toString(Qt::ISODate);
    auto *job = _account->sendRequest("PUT", openListApiUrl(_account, QStringLiteral("/api/share/create")),
        makeJsonRequest(), makeJsonBody(body));
    connect(job, &SimpleNetworkJob::finishedSignal, this, [this, date](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            slotOcsError(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                reply->errorString());
            return;
        }
        const auto data = QJsonDocument::fromJson(reply->readAll()).object();
        slotExpireDateSet(QJsonDocument(data), date);
    });
}

void LinkShare::setLabel(const QString &label)
{
    QJsonObject body;
    body[QStringLiteral("id")] = getId();
    body[QStringLiteral("label")] = label;
    auto *job = _account->sendRequest("PUT", openListApiUrl(_account, QStringLiteral("/api/share/create")),
        makeJsonRequest(), makeJsonBody(body));
    connect(job, &SimpleNetworkJob::finishedSignal, this, [this, label](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            slotOcsError(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                reply->errorString());
            return;
        }
        slotLabelSet(QJsonDocument(), label);
    });
}

void LinkShare::setHideDownload(const bool hideDownload)
{
    QJsonObject body;
    body[QStringLiteral("id")] = getId();
    body[QStringLiteral("hide_download")] = hideDownload ? 1 : 0;
    auto *job = _account->sendRequest("PUT", openListApiUrl(_account, QStringLiteral("/api/share/create")),
        makeJsonRequest(), makeJsonBody(body));
    connect(job, &SimpleNetworkJob::finishedSignal, this, [this, hideDownload](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            slotOcsError(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                reply->errorString());
            return;
        }
        slotHideDownloadSet(QJsonDocument(), hideDownload);
    });
}

void LinkShare::slotExpireDateSet(const QJsonDocument &reply, const QVariant &value)
{
    const auto data = reply.object();

    if (data.value("expiration"_L1).isString()) {
        _expireDate = QDateTime::fromString(data.value("expiration"_L1).toString(), "yyyy-MM-dd hh:mm:ss").date();
    } else {
        _expireDate = value.toDate();
    }
    emit expireDateSet();
}

void LinkShare::slotNameSet(const QJsonDocument &, const QVariant &value)
{
    _name = value.toString();
    emit nameSet();
}

void LinkShare::slotLabelSet(const QJsonDocument &, const QVariant &label)
{
    if (_label != label.toString()) {
        _label = label.toString();
        emit labelSet();
    }
}

void LinkShare::slotHideDownloadSet(const QJsonDocument &jsonDoc, const QVariant &hideDownload)
{
    Q_UNUSED(jsonDoc);
    if (!hideDownload.isValid()) {
        return;
    }
    _hideDownload = hideDownload.toBool();
    emit hideDownloadSet();
}

UserGroupShare::UserGroupShare(AccountPtr account,
    const QString &id,
    const QString &uidOwner,
    const QString &uidFileOwner,
    const QString &ownerDisplayName,
    const QString &path,
    const ShareType shareType,
    bool isPasswordSet,
    const Permissions permissions,
    const ShareePtr shareWith,
    const QDate &expireDate,
    const QString &note)
    : Share(account, id, uidOwner, uidFileOwner, ownerDisplayName, path, shareType, isPasswordSet, permissions, shareWith)
    , _note(note)
    , _expireDate(expireDate)
{
    Q_ASSERT(Share::isShareTypeUserGroupEmailRoomOrRemote(shareType));
    Q_ASSERT(shareWith);
}

void UserGroupShare::setNote(const QString &note)
{
    QJsonObject body;
    body[QStringLiteral("id")] = getId();
    body[QStringLiteral("note")] = note;
    auto *job = _account->sendRequest("PUT", openListApiUrl(_account, QStringLiteral("/api/share/create")),
        makeJsonRequest(), makeJsonBody(body));
    connect(job, &SimpleNetworkJob::finishedSignal, this, [this, note](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            emit noteSetError();
            return;
        }
        slotNoteSet(QJsonDocument(), note);
    });
}

QString UserGroupShare::getNote() const
{
    return _note;
}

void UserGroupShare::slotNoteSet(const QJsonDocument &, const QVariant &note)
{
    _note = note.toString();
    emit noteSet();
}

QDate UserGroupShare::getExpireDate() const
{
    return _expireDate;
}

void UserGroupShare::setExpireDate(const QDate &date)
{
    if (_expireDate == date) {
        emit expireDateSet();
        return;
    }

    QJsonObject body;
    body[QStringLiteral("id")] = getId();
    body[QStringLiteral("expiration")] = date.toString(Qt::ISODate);
    auto *job = _account->sendRequest("PUT", openListApiUrl(_account, QStringLiteral("/api/share/create")),
        makeJsonRequest(), makeJsonBody(body));
    connect(job, &SimpleNetworkJob::finishedSignal, this, [this, date](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            slotOcsError(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                reply->errorString());
            return;
        }
        const auto data = QJsonDocument::fromJson(reply->readAll()).object();
        slotExpireDateSet(QJsonDocument(data), date);
    });
}

void UserGroupShare::slotExpireDateSet(const QJsonDocument &reply, const QVariant &value)
{
    const auto data = reply.object();

    if (data.value("expiration"_L1).isString()) {
        _expireDate = QDateTime::fromString(data.value("expiration"_L1).toString(), "yyyy-MM-dd hh:mm:ss").date();
    } else {
        _expireDate = value.toDate();
    }
    emit expireDateSet();
}

ShareManager::ShareManager(AccountPtr account, QObject *parent)
    : QObject(parent)
    , _account(account)
{
}

void ShareManager::createLinkShare(const QString &path,
    const QString &name,
    const QString &password)
{
    QJsonObject body;
    body[QStringLiteral("path")] = path;
    body[QStringLiteral("share_type")] = static_cast<int>(Share::TypeLink);
    if (!name.isEmpty())
        body[QStringLiteral("name")] = name;
    if (!password.isEmpty())
        body[QStringLiteral("password")] = password;
    auto *job = _account->sendRequest("POST", openListApiUrl(_account, QStringLiteral("/api/share/create")),
        makeJsonRequest(), makeJsonBody(body));
    connect(job, &SimpleNetworkJob::finishedSignal, this, [this](QNetworkReply *reply) {
        const auto httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            if (httpCode == 403) {
                emit linkShareRequiresPassword(reply->errorString());
                return;
            }
            slotOcsError(httpCode, reply->errorString());
            return;
        }
        const auto json = QJsonDocument::fromJson(reply->readAll());
        slotLinkShareCreated(json);
    });
}

void ShareManager::createSecureFileDropShare(const QString &path, const QString &name, const QString &password)
{
    QJsonObject body;
    body[QStringLiteral("path")] = path;
    body[QStringLiteral("share_type")] = static_cast<int>(Share::TypeLink);
    body[QStringLiteral("name")] = name;
    if (!password.isEmpty())
        body[QStringLiteral("password")] = password;
    auto *job = _account->sendRequest("POST", openListApiUrl(_account, QStringLiteral("/api/share/create")),
        makeJsonRequest(), makeJsonBody(body));
    connect(job, &SimpleNetworkJob::finishedSignal, this, [this](QNetworkReply *reply) {
        const auto httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            if (httpCode == 403) {
                emit linkShareRequiresPassword(reply->errorString());
                return;
            }
            slotOcsError(httpCode, reply->errorString());
            return;
        }
        const auto json = QJsonDocument::fromJson(reply->readAll());
        slotLinkShareCreated(json);
    });
}

void ShareManager::slotLinkShareCreated(const QJsonDocument &reply)
{
    const auto data = reply.object();
    QSharedPointer<LinkShare> share(parseLinkShare(data));

    emit linkShareCreated(share);

    updateFolder(_account, share->path());
}


void ShareManager::createShare(const QString &path,
    const Share::ShareType shareType,
    const QString shareWith,
    const Share::Permissions desiredPermissions,
    const QString &password)
{
    // First fetch shares for this path to determine existing permissions
    auto *listJob = _account->sendRequest("GET",
        openListApiUrl(_account, QStringLiteral("/api/share/list?path=") + path),
        makeJsonRequest());
    connect(listJob, &SimpleNetworkJob::finishedSignal, this,
        [=, this](QNetworkReply *reply) {
            Share::Permissions existingPermissions = SharePermissionAll;
            if (reply->error() == QNetworkReply::NoError) {
                const auto doc = QJsonDocument::fromJson(reply->readAll());
                const auto dataArray = doc.object().value("shares"_L1).toArray();
                for (const auto &element : dataArray) {
                    auto map = element.toObject();
                    if (map["path"_L1] == path)
                        existingPermissions = Share::Permissions(map["permissions"_L1].toInt());
                }
            }

            auto validPermissions = desiredPermissions;
            if (validPermissions == SharePermissionAll) {
                validPermissions = existingPermissions;
            }
            if (existingPermissions != SharePermissionAll) {
                validPermissions &= existingPermissions;
            }

            // Create the share
            QJsonObject body;
            body[QStringLiteral("path")] = path;
            body[QStringLiteral("share_type")] = static_cast<int>(shareType);
            body[QStringLiteral("share_with")] = shareWith;
            body[QStringLiteral("permissions")] = static_cast<int>(validPermissions);
            if (!password.isEmpty())
                body[QStringLiteral("password")] = password;

            auto *createJob = _account->sendRequest("POST", openListApiUrl(_account, QStringLiteral("/api/share/create")),
                makeJsonRequest(), makeJsonBody(body));
            connect(createJob, &SimpleNetworkJob::finishedSignal, this, [this](QNetworkReply *creply) {
                if (creply->error() != QNetworkReply::NoError) {
                    slotOcsError(creply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                        creply->errorString());
                    return;
                }
                const auto json = QJsonDocument::fromJson(creply->readAll());
                slotShareCreated(json);
            });
        });
}

void ShareManager::slotShareCreated(const QJsonDocument &reply)
{
    auto data = reply.object();
    SharePtr share(parseShare(data));

    emit shareCreated(share);

    updateFolder(_account, share->path());
}

void ShareManager::fetchShares(const QString &path)
{
    auto *job = _account->sendRequest("GET",
        openListApiUrl(_account, QStringLiteral("/api/share/list?path=") + path),
        makeJsonRequest());
    connect(job, &SimpleNetworkJob::finishedSignal, this, [this](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            slotOcsError(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                reply->errorString());
            return;
        }
        const auto json = QJsonDocument::fromJson(reply->readAll());
        slotSharesFetched(json);
    });
}

void ShareManager::fetchSharedWithMe(const QString &path)
{
    auto *job = _account->sendRequest("GET",
        openListApiUrl(_account, QStringLiteral("/api/share/list?shared_with_me=true&path=") + path),
        makeJsonRequest());
    connect(job, &SimpleNetworkJob::finishedSignal, this, [this](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            slotOcsError(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                reply->errorString());
            return;
        }
        const auto json = QJsonDocument::fromJson(reply->readAll());
        slotSharedWithMeFetched(json);
    });
}

const QList<SharePtr> ShareManager::parseShares(const QJsonDocument &reply) const
{
    qDebug() << reply;
    const auto tmpShares = reply.object().value("shares"_L1).toArray();
    qCDebug(lcUserGroupShare) << "Fetched" << tmpShares.count() << "shares";

    QList<SharePtr> shares;

    for (const auto &share : tmpShares) {
        auto data = share.toObject();

        auto shareType = data.value("share_type"_L1).toInt();

        SharePtr newShare;

        if (shareType == Share::TypeLink) {
            newShare = parseLinkShare(data);
        } else if (Share::isShareTypeUserGroupEmailRoomOrRemote(static_cast <Share::ShareType>(shareType))) {
            newShare = parseUserGroupShare(data);
        } else {
            newShare = parseShare(data);
        }

        shares.append(SharePtr(newShare));
    }

    qCDebug(lcSharing) << "Sending " << shares.count() << "shares";
    return shares;
}

void ShareManager::slotSharesFetched(const QJsonDocument &reply)
{
    const auto shares = parseShares(reply);
    emit sharesFetched(shares);
}

void ShareManager::slotSharedWithMeFetched(const QJsonDocument &reply)
{
    const auto shares = parseShares(reply);
    emit sharedWithMeFetched(shares);
}

QSharedPointer<UserGroupShare> ShareManager::parseUserGroupShare(const QJsonObject &data) const
{
    ShareePtr sharee(new Sharee(data.value("share_with"_L1).toString(),
        data.value("share_with_displayname"_L1).toString(),
        static_cast<Sharee::Type>(data.value("share_type"_L1).toInt())));

    QDate expireDate;
    if (data.value("expiration"_L1).isString()) {
        expireDate = QDateTime::fromString(data.value("expiration"_L1).toString(), "yyyy-MM-dd hh:mm:ss").date();
    }

    QString note;
    if (data.value("note"_L1).isString()) {
        note = data.value("note"_L1).toString();
    }

    return QSharedPointer<UserGroupShare>(new UserGroupShare(_account,
        data.value("id"_L1).toVariant().toString(), // "id" used to be an integer, support both
        data.value("uid_owner"_L1).toVariant().toString(),
        data.value("uid_file_owner"_L1).toVariant().toString(),
        data.value("displayname_owner"_L1).toVariant().toString(),
        data.value("path"_L1).toString(),
        static_cast<Share::ShareType>(data.value("share_type"_L1).toInt()),
        !data.value("password"_L1).toString().isEmpty(),
        static_cast<Share::Permissions>(data.value("permissions"_L1).toInt()),
        sharee,
        expireDate,
        note));
}

QSharedPointer<LinkShare> ShareManager::parseLinkShare(const QJsonObject &data) const
{
    QUrl url;

    // From ownCloud server 8.2 the url field is always set for public shares
    if (data.contains("url"_L1)) {
        url = QUrl(data.value("url"_L1).toString());
    } else if (_account->serverVersionInt() >= Account::makeServerVersion(8, 0, 0)) {
        // From ownCloud server version 8 on, a different share link scheme is used.
        url = QUrl(Utility::concatUrlPath(_account->url(), QLatin1String("index.php/s/") + data.value("token"_L1).toString())).toString();
    } else {
        QUrlQuery queryArgs;
        queryArgs.addQueryItem(u"service"_s, u"files"_s);
        queryArgs.addQueryItem(u"t"_s, data.value("token"_L1).toString());
        url = QUrl(Utility::concatUrlPath(_account->url(), u"public.php"_s, queryArgs).toString());
    }

    QDate expireDate;
    if (data.value("expiration"_L1).isString()) {
        expireDate = QDateTime::fromString(data.value("expiration"_L1).toString(), "yyyy-MM-dd hh:mm:ss").date();
    }
    
    QString note;
    if (data.value("note"_L1).isString()) {
        note = data.value("note"_L1).toString();
    }

    return QSharedPointer<LinkShare>(new LinkShare(_account,
        data.value("id"_L1).toVariant().toString(), // "id" used to be an integer, support both
        data.value("uid_owner"_L1).toString(),
        data.value("uid_file_owner"_L1).toString(),
        data.value("displayname_owner"_L1).toString(),
        data.value("path"_L1).toString(),
        data.value("name"_L1).toString(),
        data.value("token"_L1).toString(),
        (Share::Permissions)data.value("permissions"_L1).toInt(),
        data.value("share_with"_L1).isString(), // has password?
        url,
        expireDate,
        note,
        data.value("label"_L1).toString(),
        data.value("hide_download"_L1).toInt() == 1));
}

SharePtr ShareManager::parseShare(const QJsonObject &data) const
{
    ShareePtr sharee(new Sharee(data.value("share_with"_L1).toString(),
        data.value("share_with_displayname"_L1).toString(),
        (Sharee::Type)data.value("share_type"_L1).toInt()));

    return SharePtr(new Share(_account,
        data.value("id"_L1).toVariant().toString(), // "id" used to be an integer, support both
        data.value("uid_owner"_L1).toVariant().toString(),
        data.value("uid_file_owner"_L1).toVariant().toString(),
        data.value("displayname_owner"_L1).toVariant().toString(),
        data.value("path"_L1).toString(),
        (Share::ShareType)data.value("share_type"_L1).toInt(),
        !data.value("password"_L1).toString().isEmpty(),
        (Share::Permissions)data.value("permissions"_L1).toInt(),
        sharee));
}

void ShareManager::slotOcsError(int statusCode, const QString &message)
{
    emit serverError(statusCode, message);
}

}
