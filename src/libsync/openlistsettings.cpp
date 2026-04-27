/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "openlistsettings.h"

#include <QLoggingCategory>
#include <QVariantMap>

namespace OCC {

Q_LOGGING_CATEGORY(lcOpenListSettings, "openlist.sync.settings", QtInfoMsg)

OpenListSettings::OpenListSettings(const QVariantMap &settings)
    : _settings(settings)
    , _valid(!settings.isEmpty())
{
}

bool OpenListSettings::isValid() const
{
    return _valid;
}

QString OpenListSettings::version() const
{
    return _settings.value(QStringLiteral("version")).toString();
}

QString OpenListSettings::announcement() const
{
    return _settings.value(QStringLiteral("announcement")).toString();
}

bool OpenListSettings::allowRegistration() const
{
    return _settings.value(QStringLiteral("allow_registration"), false).toBool();
}

QByteArray OpenListSettings::uploadChecksumType() const
{
    // OpenList does not currently advertise a preferred checksum algorithm;
    // default to an empty value so the sync engine skips checksum validation.
    return {};
}

QList<int> OpenListSettings::httpErrorCodesThatResetFailingChunkedUploads() const
{
    // Standard set: reset chunked uploads on 412 (precondition failed) and 503.
    return {412, 503};
}

bool OpenListSettings::sharePublicLink() const
{
    // OpenList always supports public link sharing.
    return true;
}

qint64 OpenListSettings::maxChunkSize() const
{
    const auto val = _settings.value(QStringLiteral("max_upload_size"));
    if (!val.isValid()) {
        return 0;
    }
    return val.toLongLong();
}

bool OpenListSettings::webDavSupported() const
{
    // OpenList always exposes WebDAV at /dav/<username>/
    return true;
}

QString OpenListSettings::davPath(const QString &username) const
{
    return QStringLiteral("/dav/") + username + QLatin1Char('/');
}

} // namespace OCC
