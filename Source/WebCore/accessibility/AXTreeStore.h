/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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

#include <WebCore/AXCoreObject.h>
#include <WebCore/ActivityState.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
class AXIsolatedTree;
#endif
class AXObjectCache;

using AXTreePtr = Variant<std::nullptr_t, WeakPtr<AXObjectCache>
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    , RefPtr<AXIsolatedTree>
#endif
>;

using AXTreeWeakPtr = Variant<WeakPtr<AXObjectCache>
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    , ThreadSafeWeakPtr<AXIsolatedTree>
#endif
>;

AXTreePtr axTreeForID(AXID);
WEBCORE_EXPORT AXTreePtr findAXTree(Function<bool(AXTreePtr)>&&);

template<typename T>
class AXTreeStore {
    // For now, we just disable direct instantiations of this class because it is not
    // needed. Subclasses are expected to declare their own WTF_MAKE_TZONE_ALLOCATED.
    WTF_MAKE_TZONE_NON_HEAP_ALLOCATABLE(AXTreeStore);
    WTF_MAKE_NONCOPYABLE(AXTreeStore);
    friend WEBCORE_EXPORT AXTreePtr findAXTree(Function<bool(AXTreePtr)>&&);
public:
    AXID treeID() const { return m_id; }
    inline static WeakPtr<AXObjectCache> axObjectCacheForID(std::optional<AXID>);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    static RefPtr<AXIsolatedTree> isolatedTreeForID(std::optional<AXID>);
    static void applyPendingChangesForAllIsolatedTrees();
#endif

protected:
    AXTreeStore(AXID axID = generateNewID())
        : m_id(axID)
    { }

    inline static void set(AXID, const AXTreeWeakPtr&);
    inline static void add(AXID, const AXTreeWeakPtr&);
    inline static void remove(AXID);
    inline static bool contains(AXID);

    inline static AXID generateNewID();
    const AXID m_id;
    static Lock s_storeLock;
private:
    inline static HashMap<AXID, WeakPtr<AXObjectCache>>& liveTreeMap();
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    inline static HashMap<AXID, ThreadSafeWeakPtr<AXIsolatedTree>>& isolatedTreeMap() WTF_REQUIRES_LOCK(s_storeLock);
#endif
};

template<typename T>
inline AXID AXTreeStore<T>::generateNewID()
{
    ASSERT(isMainThread());

    std::optional<AXID> axID;
    do {
        axID = AXID::generate();
    } while (liveTreeMap().contains(*axID));
    return *axID;
}

} // namespace WebCore
