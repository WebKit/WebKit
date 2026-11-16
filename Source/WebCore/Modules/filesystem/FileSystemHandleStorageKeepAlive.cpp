/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "FileSystemHandleStorageKeepAlive.h"

#include "FileSystemStorageConnection.h"
#include <wtf/CrossThreadCopier.h>

namespace WebCore {

Ref<FileSystemHandleStorageKeepAlive> FileSystemHandleStorageKeepAlive::create(ClientOrigin&& origin, Vector<FileSystemHandleGlobalIdentifier>&& globalIdentifiers, Ref<FileSystemStorageConnection>&& connection)
{
    return adoptRef(*new FileSystemHandleStorageKeepAlive(WTF::move(origin), WTF::move(globalIdentifiers), WTF::move(connection)));
}

FileSystemHandleStorageKeepAlive::FileSystemHandleStorageKeepAlive(ClientOrigin&& origin, Vector<FileSystemHandleGlobalIdentifier>&& globalIdentifiers, Ref<FileSystemStorageConnection>&& connection)
    : m_origin(crossThreadCopy(WTF::move(origin)))
    , m_globalIdentifiers(WTF::move(globalIdentifiers))
    , m_connection(WTF::move(connection))
{
}

FileSystemHandleStorageKeepAlive::~FileSystemHandleStorageKeepAlive()
{
    if (m_globalIdentifiers.isEmpty())
        return;
    if (RefPtr connection = m_connection)
        connection->removeGlobalIdentifierReferences(ClientOrigin { m_origin }, WTF::move(m_globalIdentifiers));
}

} // namespace WebCore
