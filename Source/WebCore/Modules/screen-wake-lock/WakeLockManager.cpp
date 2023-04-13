/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WakeLockManager.h"

#include "Document.h"
#include "SleepDisabler.h"
#include "VisibilityState.h"
#include "WakeLockSentinel.h"

namespace WebCore {

WakeLockManager::WakeLockManager(Document& document)
    : m_document(document)
{
    m_document.registerForVisibilityStateChangedCallbacks(*this);
}

WakeLockManager::~WakeLockManager()
{
    m_document.unregisterForVisibilityStateChangedCallbacks(*this);
}

void WakeLockManager::addWakeLock(Ref<WakeLockSentinel>&& lock)
{
    auto type = lock->type();
    auto& locks = m_wakeLocks.ensure(type, [] { return Vector<RefPtr<WakeLockSentinel>>(); }).iterator->value;
    ASSERT(!locks.contains(lock.ptr()));
    locks.append(WTFMove(lock));

    if (locks.size() != 1)
        return;

    switch (type) {
    case WakeLockType::Screen:
        m_screenLockDisabler = makeUnique<SleepDisabler>("Screen Wake Lock"_s, PAL::SleepDisabler::Type::Display);
        break;
    }
}

void WakeLockManager::removeWakeLock(WakeLockSentinel& lock)
{
    auto it = m_wakeLocks.find(lock.type());
    if (it == m_wakeLocks.end())
        return;
    auto& locks = it->value;
    locks.removeFirst(&lock);
    ASSERT(!locks.contains(&lock));

    if (!locks.isEmpty())
        return;

    m_wakeLocks.remove(it);
    switch (lock.type()) {
    case WakeLockType::Screen:
        m_screenLockDisabler = nullptr;
        break;
    }
}

// https://www.w3.org/TR/screen-wake-lock/#handling-document-loss-of-visibility
void WakeLockManager::visibilityStateChanged()
{
    if (m_document.visibilityState() != VisibilityState::Hidden)
        return;

    releaseAllLocks(WakeLockType::Screen);
}

void WakeLockManager::releaseAllLocks(WakeLockType type)
{
    auto it = m_wakeLocks.find(type);
    if (it == m_wakeLocks.end())
        return;

    auto& locks = it->value;
    while (!locks.isEmpty()) {
        RefPtr lock = *locks.begin();
        lock->release(*this);
    }
}

} // namespace WebCore
