/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "openlisttheme.h"

#include <QString>
#include <QVariant>
#ifndef TOKEN_AUTH_ONLY
#include <QPixmap>
#include <QIcon>
#endif
#include <QCoreApplication>

#include "config.h"
#include "common/utility.h"
#include "version.h"

namespace OCC {

OpenListTheme::OpenListTheme()
    : Theme()
{
}

QString OpenListTheme::wizardUrlHint() const
{
    return QStringLiteral("https://demo.oplist.org");
}

} // namespace OCC
