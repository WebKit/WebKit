/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Watchpoint.h"

#include <wtf/CompilationThread.h>
#include <wtf/PassRefPtr.h>

namespace JSC {

Watchpoint::~Watchpoint()
{
    if (isOnList())
        remove();
}

WatchpointSet::WatchpointSet(WatchpointState state)
    : m_state(state)
    , m_setIsNotEmpty(false)
{
}

WatchpointSet::~WatchpointSet()
{
    // Remove all watchpoints, so that they don't try to remove themselves. Note that we
    // don't fire watchpoints on deletion. We assume that any code that is interested in
    // watchpoints already also separately has a mechanism to make sure that the code is
    // either keeping the watchpoint set's owner alive, or does some weak reference thing.
    while (!m_set.isEmpty())
        m_set.begin()->remove();
}

void WatchpointSet::add(Watchpoint* watchpoint)
{
    ASSERT(!isCompilationThread());
    ASSERT(state() != IsInvalidated);
    if (!watchpoint)
        return;
    m_set.push(watchpoint);
    m_setIsNotEmpty = true;
    m_state = IsWatched;
}

void WatchpointSet::fireAllSlow()
{
    ASSERT(state() == IsWatched);
    
    WTF::storeStoreFence();
    fireAllWatchpoints();
    m_state = IsInvalidated;
    WTF::storeStoreFence();
}

void WatchpointSet::fireAllWatchpoints()
{
    while (!m_set.isEmpty())
        m_set.begin()->fire();
}

void InlineWatchpointSet::add(Watchpoint* watchpoint)
{
    inflate()->add(watchpoint);
}

WatchpointSet* InlineWatchpointSet::inflateSlow()
{
    ASSERT(isThin());
    ASSERT(!isCompilationThread());
    WatchpointSet* fat = adoptRef(new WatchpointSet(decodeState(m_data))).leakRef();
    WTF::storeStoreFence();
    m_data = bitwise_cast<uintptr_t>(fat);
    return fat;
}

void InlineWatchpointSet::freeFat()
{
    ASSERT(isFat());
    fat()->deref();
}

} // namespace JSC

