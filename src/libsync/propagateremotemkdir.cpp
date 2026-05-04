/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "propagateremotemkdir.h"
#include "owncloudpropagator_p.h"
#include "account.h"
#include "common/syncjournalfilerecord.h"

#include "deletejob.h"
#include "common/asserts.h"

#include "filesystem.h"
#include "csync/csync.h"

#include <QFile>
#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcPropagateRemoteMkdir, "openlist.sync.propagator.remotemkdir", QtInfoMsg)

PropagateRemoteMkdir::PropagateRemoteMkdir(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
    : PropagateItemJob(propagator, item)
{
    const auto path = _item->_file;
    const auto slashPosition = path.lastIndexOf('/');
    const auto parentPath = slashPosition >= 0 ? path.left(slashPosition) : QString();

    SyncJournalFileRecord parentRec;
    bool ok = propagator->_journal->getFileRecord(parentPath, &parentRec);
    if (!ok) {
        return;
    }
}

void PropagateRemoteMkdir::start()
{
    if (propagator()->_abortRequested)
        return;

    qCDebug(lcPropagateRemoteMkdir) << _item->_file;

    propagator()->_activeJobList.append(this);

    if (!_deleteExisting) {
        slotMkdir();
        return;
    }

    _job = new DeleteJob(propagator()->account(),
                         propagator()->fullRemotePath(_item->_file),
                         {},
                         this);
    connect(qobject_cast<DeleteJob *>(_job), &DeleteJob::finishedSignal, this, &PropagateRemoteMkdir::slotMkdir);
    _job->start();
}

void PropagateRemoteMkdir::slotStartMkcolJob()
{
    if (propagator()->_abortRequested)
        return;

    qCDebug(lcPropagateRemoteMkdir) << _item->_file;

    _job = new MkColJob(propagator()->account(),
        propagator()->fullRemotePath(_item->_file),
        this);
    connect(qobject_cast<MkColJob *>(_job), &MkColJob::finishedWithError, this, &PropagateRemoteMkdir::slotMkcolJobFinished);
    connect(qobject_cast<MkColJob *>(_job), &MkColJob::finishedWithoutError, this, &PropagateRemoteMkdir::slotMkcolJobFinished);
    _job->start();
}

void PropagateRemoteMkdir::abort(PropagatorJob::AbortType abortType)
{
    if (_job && _job->reply())
        _job->reply()->abort();

    if (abortType == AbortType::Asynchronous) {
        emit abortFinished();
    }
}

void PropagateRemoteMkdir::setDeleteExisting(bool enabled)
{
    _deleteExisting = enabled;
}

void PropagateRemoteMkdir::finalizeMkColJob(QNetworkReply::NetworkError err, const QString &jobHttpReasonPhraseString, const QString &jobPath)
{
    if (_item->_httpErrorCode == 405) {
        // This happens when the directory already exists. Nothing to do.
        qDebug(lcPropagateRemoteMkdir) << "Folder" << jobPath << "already exists.";
    } else if (err != QNetworkReply::NoError) {
        SyncFileItem::Status status = classifyError(err, _item->_httpErrorCode,
            &propagator()->_anotherSyncNeeded);
        done(status, _item->_errorString, errorCategoryFromNetworkError(err));
        return;
    } else if (_item->_httpErrorCode != 201) {
        // Normally we expect "201 Created"
        // If it is not the case, it might be because of a proxy or gateway intercepting the request, so we must
        // throw an error.
        done(SyncFileItem::NormalError,
            tr("Wrong HTTP code returned by server. Expected 201, but received \"%1 %2\".")
                .arg(_item->_httpErrorCode)
                .arg(jobHttpReasonPhraseString), ErrorCategory::GenericError);
        return;
    }

    propagator()->_activeJobList.append(this);
    auto propfindJob = new PropfindJob(propagator()->account(), jobPath, this);
    propfindJob->setProperties({QByteArrayLiteral("http://owncloud.org/ns:share-types"), QByteArrayLiteral("http://owncloud.org/ns:permissions"), QByteArrayLiteral("http://nextcloud.org/ns:is-mount-root")});
    connect(propfindJob, &PropfindJob::result, this, [this, jobPath](const QVariantMap &result){
        propagator()->_activeJobList.removeOne(this);
        _item->_remotePerm = RemotePermissions::fromServerString(result.value(QStringLiteral("permissions")).toString(),
                                                                 propagator()->account()->serverHasMountRootProperty() ? RemotePermissions::MountedPermissionAlgorithm::UseMountRootProperty : RemotePermissions::MountedPermissionAlgorithm::WildGuessMountedSubProperty,
                                                                 result);

        _item->_sharedByMe = !result.value(QStringLiteral("share-types")).toString().isEmpty();
        _item->_isShared = _item->_remotePerm.hasPermission(RemotePermissions::IsShared) || _item->_sharedByMe;
        _item->_lastShareStateFetchedTimestamp = QDateTime::currentMSecsSinceEpoch();

        success();
    });
    connect(propfindJob, &PropfindJob::finishedWithError, this, [this] (QNetworkReply *reply) {
        const auto err = reply ? reply->error() : QNetworkReply::NetworkError::UnknownNetworkError;
        propagator()->_activeJobList.removeOne(this);
        done(SyncFileItem::NormalError, {}, errorCategoryFromNetworkError(err));
    });
    propfindJob->start();
}

void PropagateRemoteMkdir::slotMkdir()
{
    if (!_item->_originalFile.isEmpty() && !_item->_renameTarget.isEmpty() && _item->_renameTarget != _item->_originalFile) {
        const auto existingFile = propagator()->fullLocalPath(propagator()->adjustRenamedPath(_item->_originalFile));
        const auto targetFile = propagator()->fullLocalPath(_item->_renameTarget);
        QString renameError;
        if (!FileSystem::rename(existingFile, targetFile, &renameError)) {
            done(SyncFileItem::NormalError, renameError, ErrorCategory::GenericError);
            return;
        }
        emit propagator()->touchedFile(existingFile);
        emit propagator()->touchedFile(targetFile);
    }

    const auto path = _item->_file;
    const auto slashPosition = path.lastIndexOf('/');
    const auto parentPath = slashPosition >= 0 ? path.left(slashPosition) : QString();

    SyncJournalFileRecord parentRec;
    bool ok = propagator()->_journal->getFileRecord(parentPath, &parentRec);
    if (!ok) {
        done(SyncFileItem::NormalError, {}, ErrorCategory::GenericError);
        return;
    }

    slotStartMkcolJob();
}

void PropagateRemoteMkdir::slotMkcolJobFinished()
{
    propagator()->_activeJobList.removeOne(this);

    ASSERT(_job);

    QNetworkReply::NetworkError err = _job->reply()->error();
    _item->_httpErrorCode = _job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_responseTimeStamp = _job->responseTimestamp();
    _item->_requestId = _job->requestId();

    _item->_fileId = _job->reply()->rawHeader("OC-FileId");

    qCInfo(lcPropagateRemoteMkdir()) << "mkcol job error string:" << _item->_errorString << _job->errorString();
    _item->_errorString = _job->errorString();

    const auto jobHttpReasonPhraseString = _job->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();

    const auto jobPath = _job->path();

    finalizeMkColJob(err, jobHttpReasonPhraseString, jobPath);
}

void PropagateRemoteMkdir::success()
{
    // Never save the etag on first mkdir.
    // Only fully propagated directories should have the etag set.
    auto itemCopy = *_item;
    itemCopy._etag.clear();

    // save the file id already so we can detect rename or remove
    const auto result = propagator()->updateMetadata(itemCopy);
    if (!result) {
        done(SyncFileItem::FatalError, tr("Error writing metadata to the database: %1").arg(result.error()), ErrorCategory::GenericError);
        return;
    } else if (*result == Vfs::ConvertToPlaceholderResult::Locked) {
        done(SyncFileItem::FatalError, tr("The file %1 is currently in use").arg(_item->_file), ErrorCategory::GenericError);
        return;
    }

    done(SyncFileItem::Success, {}, ErrorCategory::NoError);
}
}
