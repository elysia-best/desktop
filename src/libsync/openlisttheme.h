/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OPENLIST_THEME_H
#define OPENLIST_THEME_H

#include "theme.h"

namespace OCC {

/**
 * @brief The OpenListTheme class
 * @ingroup libsync
 */
class OpenListTheme : public Theme
{
    Q_OBJECT
public:
    OpenListTheme();

    [[nodiscard]] QString wizardUrlHint() const override;
};

} // namespace OCC

#endif // OPENLIST_THEME_H
