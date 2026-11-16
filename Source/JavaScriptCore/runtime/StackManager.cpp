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
#include "StackManager.h"

#include "StackManagerInlines.h"

namespace JSC {

CONCURRENT_SAFE void StackManager::requestStop()
{
    Locker lock { m_mirrorLock };
    m_trapAwareSoftStackLimit.storeRelaxed(stopRequestMarker());
    for (auto& mirror : m_mirrors)
        mirror.m_trapAwareSoftStackLimit.storeRelaxed(stopRequestMarker());
}

CONCURRENT_SAFE void StackManager::cancelStop()
{
    if (Options::forceTrapAwareStackChecks()) [[unlikely]]
        return;

    Locker lock { m_mirrorLock };
    m_trapAwareSoftStackLimit.storeRelaxed(m_softStackLimit);
    for (auto& mirror : m_mirrors)
        mirror.m_trapAwareSoftStackLimit.storeRelaxed(m_softStackLimit);
}

void StackManager::setStackSoftLimit(void* newStackLimit)
{
    m_softStackLimit = newStackLimit;

    Locker lock { m_mirrorLock };
#if !ENABLE(C_LOOP)
    if (!hasStopRequest())
        m_trapAwareSoftStackLimit.storeRelaxed(newStackLimit);
    void* newTrapAwareSoftStackLimit = trapAwareSoftStackLimit();
#endif
    for (auto& mirror : m_mirrors) {
#if !ENABLE(C_LOOP)
        mirror.m_trapAwareSoftStackLimit.storeRelaxed(newTrapAwareSoftStackLimit);
#endif
        mirror.m_softStackLimit = newStackLimit;
    }
}

#if ENABLE(C_LOOP)
void StackManager::setCLoopStackLimit(void* newStackLimit)
{
    m_cloopStackLimit = newStackLimit;

    Locker lock { m_mirrorLock };
    if (!hasStopRequest())
        m_trapAwareSoftStackLimit.storeRelaxed(newStackLimit);

    void* newTrapAwareSoftStackLimit = trapAwareSoftStackLimit();
    for (auto& mirror : m_mirrors) {
        mirror.m_trapAwareSoftStackLimit.storeRelaxed(newTrapAwareSoftStackLimit);
        mirror.m_softStackLimit = newStackLimit;
    }
}
#endif

void StackManager::registerMirror(StackManager::Mirror& mirror)
{
    Locker lock { m_mirrorLock };
    mirror.m_trapAwareSoftStackLimit.storeRelaxed(trapAwareSoftStackLimit());
    mirror.m_softStackLimit = m_softStackLimit;
    m_mirrors.append(&mirror);
}

void StackManager::unregisterMirror(StackManager::Mirror& mirror)
{
    Locker lock { m_mirrorLock };
    m_mirrors.remove(&mirror);
}

} // namespace JSC
