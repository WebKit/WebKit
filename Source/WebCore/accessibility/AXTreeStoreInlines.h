/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/AXIsolatedTree.h>

namespace WebCore {

template<typename T>
inline void AXTreeStore<T>::set(AXTreeID treeID, const AXTreeWeakPtr& tree)
{
    AX_ASSERT(isMainThread());

    switchOn(tree,
        [&] (const WeakPtr<AXObjectCache>& typedTree) {
            liveTreeMap().set(treeID, typedTree);
        }
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        , [&] (const ThreadSafeWeakPtr<AXIsolatedTree>& typedTree) {
            Locker locker { s_storeLock };
            isolatedTreeMap().set(treeID, typedTree.get());
        }
#endif
    );
}

template<typename T>
inline void AXTreeStore<T>::add(AXTreeID treeID, const AXTreeWeakPtr& tree)
{
    AX_ASSERT(isMainThread());

    switchOn(tree,
        [&] (const WeakPtr<AXObjectCache>& typedTree) {
            liveTreeMap().add(treeID, typedTree);
        }
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        , [&] (const ThreadSafeWeakPtr<AXIsolatedTree>& typedTree) {
            Locker locker { s_storeLock };
            isolatedTreeMap().add(treeID, typedTree.get());
        }
#endif
    );
}

template<typename T>
inline void AXTreeStore<T>::remove(AXTreeID treeID)
{
    if (isMainThread()) {
        liveTreeMap().remove(treeID);
        return;
    }
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    Locker locker { s_storeLock };
    isolatedTreeMap().remove(treeID);
#endif
}

template<typename T>
inline bool AXTreeStore<T>::contains(AXTreeID treeID)
{
    if (isMainThread())
        return liveTreeMap().contains(treeID);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    Locker locker { s_storeLock };
    return isolatedTreeMap().contains(treeID);
#endif
}

template<typename T>
inline WeakPtr<AXObjectCache> AXTreeStore<T>::axObjectCacheForID(std::optional<AXTreeID> treeID)
{
    return treeID ? liveTreeMap().get(*treeID) : nullptr;
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
template<typename T>
inline RefPtr<AXIsolatedTree> AXTreeStore<T>::isolatedTreeForID(std::optional<AXTreeID> treeID)
{
    if (!treeID)
        return nullptr;

    Locker locker { s_storeLock };
    return isolatedTreeMap().get(*treeID).get();
}
#endif

template<typename T>
inline HashMap<AXTreeID, WeakPtr<AXObjectCache>>& AXTreeStore<T>::liveTreeMap()
{
    AX_ASSERT(isMainThread());

    static NeverDestroyed<HashMap<AXTreeID, WeakPtr<AXObjectCache>>> map;
    return map;
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
template<typename T>
inline HashMap<AXTreeID, ThreadSafeWeakPtr<AXIsolatedTree>>& AXTreeStore<T>::isolatedTreeMap()
{
    static NeverDestroyed<HashMap<AXTreeID, ThreadSafeWeakPtr<AXIsolatedTree>>> map;
    return map;
}
#endif

template<typename T>
Lock AXTreeStore<T>::s_storeLock;

inline AXTreePtr axTreeForID(std::optional<AXTreeID> treeID)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isMainThread())
        return AXTreeStore<AXIsolatedTree>::isolatedTreeForID(treeID);
#endif
    return AXTreeStore<AXObjectCache>::axObjectCacheForID(treeID);
}

} // WebCore
