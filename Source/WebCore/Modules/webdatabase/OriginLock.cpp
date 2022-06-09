/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "OriginLock.h"

namespace WebCore {

String OriginLock::lockFileNameForPath(String originPath)
{
    return FileSystem::pathByAppendingComponent(originPath, ".lock"_s);
}

OriginLock::OriginLock(String originPath)
    : m_lockFileName(lockFileNameForPath(originPath).isolatedCopy())
#if USE(FILE_LOCK)
    , m_lockHandle(FileSystem::invalidPlatformFileHandle)
#endif
{
}

OriginLock::~OriginLock() = default;

void OriginLock::lock() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    m_mutex.lock();

#if USE(FILE_LOCK)
    m_lockHandle = FileSystem::openAndLockFile(m_lockFileName, FileSystem::FileOpenMode::Write);
    if (m_lockHandle == FileSystem::invalidPlatformFileHandle) {
        // The only way we can get here is if the directory containing the lock
        // has been deleted or we were given a path to a non-existant directory.
        // In that case, there's nothing we can do but cleanup and return.
        m_mutex.unlock();
        return;
    }
#endif
}

void OriginLock::unlock() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
#if USE(FILE_LOCK)
    // If the file descriptor was uninitialized, then that means the directory
    // containing the lock has been deleted before we opened the lock file, or
    // we were given a path to a non-existant directory. Which, in turn, means
    // that there's nothing to unlock.
    if (m_lockHandle == FileSystem::invalidPlatformFileHandle)
        return;

    FileSystem::unlockAndCloseFile(m_lockHandle);
    m_lockHandle = FileSystem::invalidPlatformFileHandle;
#endif

    m_mutex.unlock();
}

void OriginLock::deleteLockFile(String originPath)
{
    UNUSED_PARAM(originPath);
#if USE(FILE_LOCK)
    String lockFileName = OriginLock::lockFileNameForPath(originPath);
    FileSystem::deleteFile(lockFileName);
#endif
}

} // namespace WebCore
