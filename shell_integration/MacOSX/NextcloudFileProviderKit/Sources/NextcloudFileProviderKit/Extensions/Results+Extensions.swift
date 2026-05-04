//  SPDX-FileCopyrightText: 2024 OpenList contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Realm
import RealmSwift

extension Results where Element: RemoteFileChunk {
    func toUnmanagedResults() -> [RemoteFileChunk] {
        map { RemoteFileChunk(value: $0) }
    }
}

extension Results where Element: RealmItemMetadata {
    func toUnmanagedResults() -> [SendableItemMetadata] {
        map { SendableItemMetadata(value: $0) }
    }
}
