/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include <wtf/ReadWriteLock.h>

#include <wtf/Locker.h>

namespace WTF {

void ReadWriteLock::readLock()
{
    auto locker = holdLock(m_lock);
    while (m_isWriteLocked || m_numWaitingWriters)
        m_cond.wait(m_lock);
    m_numReaders++;
}

void ReadWriteLock::readUnlock()
{
    auto locker = holdLock(m_lock);
    m_numReaders--;
    if (!m_numReaders)
        m_cond.notifyAll();
}

void ReadWriteLock::writeLock()
{
    auto locker = holdLock(m_lock);
    while (m_isWriteLocked || m_numReaders) {
        m_numWaitingWriters++;
        m_cond.wait(m_lock);
        m_numWaitingWriters--;
    }
    m_isWriteLocked = true;
}

void ReadWriteLock::writeUnlock()
{
    auto locker = holdLock(m_lock);
    m_isWriteLocked = false;
    m_cond.notifyAll();
}

} // namespace WTF

