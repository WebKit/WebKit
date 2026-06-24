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

#include "ClientOrigin.h"
#include "ContextDestructionObserverInlines.h"
#include "FileSystemStorageConnection.h"
#include "JSDOMConvertBoolean.h"
#include "JSDOMPromiseDeferred.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FileSystemHandle);

FileSystemHandle::FileSystemHandle(ScriptExecutionContext& context, FileSystemHandle::Kind kind, String&& name, FileSystemHandleGlobalIdentifier globalIdentifier, Markable<FileSystemHandleIdentifier> identifier, RefPtr<FileSystemStorageConnection>&& connection)
    : ActiveDOMObject(&context)
    , m_kind(kind)
    , m_name(WTF::move(name))
    , m_identifier(identifier)
    , m_connection(WTF::move(connection))
    , m_globalIdentifier(globalIdentifier)
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
    if (m_isUnresolved) {
        if (RefPtr connection = m_connection)
            connection->removeGlobalIdentifierReferences(ClientOrigin { m_origin }, { m_globalIdentifier });
        m_isUnresolved = false;
        resolveEnsureIdentifierCallbacks(false);
        return;
    }
    if (RefPtr connection = m_connection)
        connection->closeHandle(*m_identifier);
}

void FileSystemHandle::markAsUnresolved(ClientOrigin&& origin, Ref<FileSystemStorageConnection>&& connection)
{
    ASSERT(m_globalIdentifier.toRawValue());
    m_isUnresolved = true;
    m_origin = WTF::move(origin);
    connection->addGlobalIdentifierReference(ClientOrigin { m_origin }, m_globalIdentifier);
    m_connection = WTF::move(connection);
}

void FileSystemHandle::ensureIdentifier(CompletionHandler<void(bool)>&& callback)
{
    if (!m_isUnresolved)
        return callback(true);

    m_ensureIdentifierCallbacks.append(WTF::move(callback));
    if (m_ensureIdentifierCallbacks.size() > 1)
        return;

    RefPtr context = scriptExecutionContext();
    if (!context) {
        if (RefPtr connection = m_connection)
            connection->removeGlobalIdentifierReferences(ClientOrigin { m_origin }, { m_globalIdentifier });
        m_isUnresolved = false;
        resolveEnsureIdentifierCallbacks(false);
        return;
    }

    auto origin = clientOriginForContext(*context);
    protect(m_connection)->resolveGlobalIdentifier(WTF::move(origin), m_globalIdentifier, [this, protectedThis = Ref { *this }](auto result) mutable {
        if (m_isClosed) {
            if (!result.hasException())
                protect(m_connection)->closeHandle(result.returnValue());
            resolveEnsureIdentifierCallbacks(false);
            return;
        }

        if (result.hasException()) {
            m_isClosed = true;
            if (RefPtr connection = m_connection)
                connection->removeGlobalIdentifierReferences(ClientOrigin { m_origin }, { m_globalIdentifier });
            m_isUnresolved = false;
            resolveEnsureIdentifierCallbacks(false);
            return;
        }

        m_identifier = result.returnValue();
        m_isUnresolved = false;
        protect(m_connection)->removeGlobalIdentifierReferences(ClientOrigin { m_origin }, { m_globalIdentifier });
        resolveEnsureIdentifierCallbacks(true);
    });
}

void FileSystemHandle::resolveEnsureIdentifierCallbacks(bool success)
{
    for (auto& callback : std::exchange(m_ensureIdentifierCallbacks, { }))
        callback(success);
}

void FileSystemHandle::isSameEntry(FileSystemHandle& handle, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    if (isClosed())
        return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    if (m_kind != handle.kind() || m_name != handle.name())
        return promise.resolve(false);

    ensureIdentifier([this, protectedThis = Ref { *this }, handle = Ref { handle }, promise = WTF::move(promise)](bool success) mutable {
        if (!success)
            return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is invalid"_s });

        handle->ensureIdentifier([this, protectedThis = WTF::move(protectedThis), handle, promise = WTF::move(promise)](bool success) mutable {
            if (!success)
                return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is invalid"_s });

            protect(m_connection)->isSameEntry(*m_identifier, handle->identifier(), [promise = WTF::move(promise)](auto result) mutable {
                promise.settle(WTF::move(result));
            });
        });
    });
}

void FileSystemHandle::move(FileSystemHandle& destinationHandle, const String& newName, DOMPromiseDeferred<void>&& promise)
{
    if (isClosed())
        return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is closed"_s });

    if (destinationHandle.kind() != Kind::Directory)
        return promise.reject(Exception { ExceptionCode::TypeMismatchError });

    ensureIdentifier([this, protectedThis = Ref { *this }, destinationHandle = Ref { destinationHandle }, newName, promise = WTF::move(promise)](bool success) mutable {
        if (!success)
            return promise.reject(Exception { ExceptionCode::InvalidStateError, "Handle is invalid"_s });

        protect(m_connection)->move(*m_identifier, destinationHandle->identifier(), newName, [this, protectedThis = WTF::move(protectedThis), newName, promise = WTF::move(promise)](auto result) mutable {
            if (!result.hasException())
                m_name = newName;

            promise.settle(WTF::move(result));
        });
    });
}

void FileSystemHandle::stop()
{
    close();
}

} // namespace WebCore
