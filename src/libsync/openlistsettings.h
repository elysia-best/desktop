/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "owncloudlib.h"

#include <QVariantMap>
#include <QStringList>
#include <QColor>
#include <QUrl>

namespace OCC {

/**
 * @brief OpenListSettings holds the public server settings retrieved from
 *        GET /api/public/settings on an OpenList server.
 *
 * This class replaces the Nextcloud-specific Capabilities class and surfaces
 * only the features that OpenList actually provides.
 *
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT OpenListSettings
{
public:
    OpenListSettings() = default;
    explicit OpenListSettings(const QVariantMap &settings);

    /** Whether valid settings have been loaded (i.e. the server was reachable). */
    [[nodiscard]] bool isValid() const;

    /** The server version string returned by the settings endpoint. */
    [[nodiscard]] QString version() const;

    /** Human-readable server title/name. */
    [[nodiscard]] QString announcement() const;

    /** Whether registration of new users is allowed by the server. */
    [[nodiscard]] bool allowRegistration() const;

    /**
     * Preferred checksum algorithm reported by the server.
     * Returns an empty byte-array if none is specified (no checksum enforcement).
     */
    [[nodiscard]] QByteArray uploadChecksumType() const;

    /**
     * List of HTTP status codes that should reset a failing chunked upload,
     * mirroring the Nextcloud capability of the same name.
     */
    [[nodiscard]] QList<int> httpErrorCodesThatResetFailingChunkedUploads() const;

    /**
     * Whether sharing (public-link generation) is enabled.
     * OpenList supports link-sharing via /api/fs/link.
     */
    [[nodiscard]] bool sharePublicLink() const;

    /**
     * Maximum upload chunk size in bytes.
     * Returns 0 if the server does not advertise a limit.
     */
    [[nodiscard]] qint64 maxChunkSize() const;

    /** Whether the server supports WebDAV-based file sync. */
    [[nodiscard]] bool webDavSupported() const;

    /** The root WebDAV path for the authenticated user, e.g. /dav/<username>/. */
    [[nodiscard]] QString davPath(const QString &username) const;

private:
    QVariantMap _settings;
    bool _valid = false;
};

} // namespace OCC
