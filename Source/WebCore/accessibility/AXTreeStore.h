/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "AccessibilityObjectInterface.h"
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

template<typename T>
class AXTreeStore {
    WTF_MAKE_NONCOPYABLE(AXTreeStore);
    WTF_MAKE_FAST_ALLOCATED;
protected:
    AXTreeStore() = default;
    static void add(AXID, WeakPtr<T>);
    static void remove(AXID);
    static bool contains(AXID);

    static RefPtr<T> treeForID(AXID);
    static AXID generateNewID();

    const AXID m_id { generateNewID() };
    static Lock s_storeLock;
private:
    static HashMap<AXID, WeakPtr<T>>& map() WTF_REQUIRES_LOCK(s_storeLock);
};

template<typename T>
inline void AXTreeStore<T>::add(AXID axID, WeakPtr<T> t)
{
    Locker locker { s_storeLock };
    map().add(axID, t);
}

template<typename T>
inline void AXTreeStore<T>::remove(AXID axID)
{
    Locker locker { s_storeLock };
    map().remove(axID);
}

template<typename T>
inline bool AXTreeStore<T>::contains(AXID axID)
{
    Locker locker { s_storeLock };
    return map().contains(axID);
}

template<typename T>
inline RefPtr<T> AXTreeStore<T>::treeForID(AXID axID)
{
    Locker locker { s_storeLock };
    return map().get(axID).get();
}

template<typename T>
inline HashMap<AXID, WeakPtr<T>>& AXTreeStore<T>::map()
{
    static NeverDestroyed<HashMap<AXID, WeakPtr<T>>> map;
    return map;
}

template<typename T>
inline AXID AXTreeStore<T>::generateNewID()
{
    AXID axID;
    Locker locker { s_storeLock };
    do {
        axID = AXID::generate();
    } while (!axID.isValid() || map().contains(axID));
    return axID;
}

template<typename T>
Lock AXTreeStore<T>::s_storeLock;

} // namespace WebCore
