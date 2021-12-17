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
#include "FileSystemHandle.h"

#include "FileSystemStorageConnection.h"
#include "JSDOMPromiseDeferred.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(FileSystemHandle);

FileSystemHandle::FileSystemHandle(ScriptExecutionContext& context, FileSystemHandle::Kind kind, String&& name, FileSystemHandleIdentifier identifier, Ref<FileSystemStorageConnection>&& connection)
    : ActiveDOMObject(&context)
    , m_kind(kind)
    , m_name(WTFMove(name))
    , m_identifier(identifier)
    , m_connection(WTFMove(connection))
{
}

FileSystemHandle::~FileSystemHandle()
{
    close();
}

void FileSystemHandle::close()
{
    if (m_isClosed)
        return;
    
    m_isClosed = true;
    m_connection->closeHandle(m_identifier);
}

void FileSystemHandle::isSameEntry(FileSystemHandle& handle, DOMPromiseDeferred<IDLBoolean>&& promise) const
{
    if (isClosed())
        return promise.reject(Exception { InvalidStateError, "Handle is closed" });

    if (m_kind != handle.kind() || m_name != handle.name())
        return promise.resolve(false);

    m_connection->isSameEntry(m_identifier, handle.identifier(), [promise = WTFMove(promise)](auto result) mutable {
        promise.settle(WTFMove(result));
    });
}

void FileSystemHandle::move(FileSystemHandle& destinationHandle, const String& newName, DOMPromiseDeferred<void>&& promise)
{
    if (isClosed())
        return promise.reject(Exception { InvalidStateError, "Handle is closed" });

    if (destinationHandle.kind() != Kind::Directory)
        return promise.reject(Exception { TypeMismatchError });

    m_connection->move(m_identifier, destinationHandle.identifier(), newName, [this, protectedThis = Ref { *this }, newName, promise = WTFMove(promise)](auto result) mutable {
        if (!result.hasException())
            m_name = newName;

        promise.settle(WTFMove(result));
    });
}

const char* FileSystemHandle::activeDOMObjectName() const
{
    return "FileSystemHandle";
}

void FileSystemHandle::stop()
{
    close();
}

} // namespace WebCore
