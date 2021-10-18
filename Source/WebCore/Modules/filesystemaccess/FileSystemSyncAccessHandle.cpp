/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "FileSystemSyncAccessHandle.h"

#include "BufferSource.h"
#include "ExceptionOr.h"
#include "FileSystemFileHandle.h"
#include "JSDOMPromiseDeferred.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

Ref<FileSystemSyncAccessHandle> FileSystemSyncAccessHandle::create(FileSystemFileHandle& source, FileSystemSyncAccessHandleIdentifier identifier)
{
    return adoptRef(*new FileSystemSyncAccessHandle(source, identifier));
}

FileSystemSyncAccessHandle::FileSystemSyncAccessHandle(FileSystemFileHandle& source, FileSystemSyncAccessHandleIdentifier identifier)
    : m_source(source)
    , m_identifier(identifier)
{
}

FileSystemSyncAccessHandle::~FileSystemSyncAccessHandle()
{
    if (!m_isClosed)
        m_source->close(m_identifier, [](auto) { });
}

void FileSystemSyncAccessHandle::truncate(unsigned long long size, DOMPromiseDeferred<void>&& promise)
{
    m_source->truncate(m_identifier, size, WTFMove(promise));
}

void FileSystemSyncAccessHandle::getSize(DOMPromiseDeferred<IDLUnsignedLongLong>&& promise)
{
    m_source->getSize(m_identifier, WTFMove(promise));
}

void FileSystemSyncAccessHandle::flush(DOMPromiseDeferred<void>&& promise)
{
    m_source->flush(m_identifier, WTFMove(promise));
}

void FileSystemSyncAccessHandle::close(DOMPromiseDeferred<void>&& promise)
{
    if (m_isClosed)
        return promise.reject(Exception { InvalidStateError });

    m_source->close(m_identifier, [weakThis = makeWeakPtr(*this), promise = WTFMove(promise)](auto result) mutable {
        if (weakThis)
            weakThis->didClose();

        promise.settle(WTFMove(result));
    });
}

void FileSystemSyncAccessHandle::didClose()
{
    m_isClosed = true;
}

} // namespace WebCore
