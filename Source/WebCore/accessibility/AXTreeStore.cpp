/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AXTreeStore.h"
#include "AXTreeStoreInlines.h"

#include "AXIsolatedTree.h"
#include "AXTreeStoreInlines.h"

namespace WebCore {

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
template<>
void AXTreeStore<AXIsolatedTree>::applyPendingChangesForAllIsolatedTrees()
{
    ASSERT(!isMainThread());

    Locker locker { AXTreeStore<AXIsolatedTree>::s_storeLock };
    auto& map = AXTreeStore<AXIsolatedTree>::isolatedTreeMap();
    for (const auto& axIDToTree : map) {
        if (RefPtr tree = axIDToTree.value.get()) {
            // Only applyPendingChanges for trees that aren't about to be destroyed.
            // When a tree is destroyed, it tries to remove itself from AXTreeStore,
            // which requires taking s_storeLock, which we hold. This would cause a deadlock.
            tree->applyPendingChangesUnlessQueuedForDestruction();
        }
    }
}
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

} // namespace WebCore
