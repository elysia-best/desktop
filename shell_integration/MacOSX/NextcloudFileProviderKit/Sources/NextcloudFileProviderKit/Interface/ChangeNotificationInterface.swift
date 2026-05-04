//  SPDX-FileCopyrightText: 2024 OpenList contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

public protocol ChangeNotificationInterface: Sendable {
    func notifyChange()
}
