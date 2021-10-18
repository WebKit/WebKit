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

#pragma once

#include "FileSystemHandleIdentifier.h"
#include "FileSystemStorageConnection.h"
#include "WorkerFileSystemStorageConnectionCallbackIdentifier.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class WorkerGlobalScope;
class WorkerThread;

class WorkerFileSystemStorageConnection final : public FileSystemStorageConnection, public CanMakeWeakPtr<WorkerFileSystemStorageConnection, WeakPtrFactoryInitialization::Eager>  {
public:
    static Ref<WorkerFileSystemStorageConnection> create(WorkerGlobalScope&, Ref<FileSystemStorageConnection>&&);
    ~WorkerFileSystemStorageConnection();

    FileSystemStorageConnection* mainThreadConnection() const { return m_mainThreadConnection.get(); }
    void connectionClosed();
    void scopeClosed();
    using CallbackIdentifier = WorkerFileSystemStorageConnectionCallbackIdentifier;
    void didIsSameEntry(CallbackIdentifier, ExceptionOr<bool>&&);
    void didGetHandle(CallbackIdentifier, ExceptionOr<FileSystemHandleIdentifier>&&);
    void didRemoveEntry(CallbackIdentifier, ExceptionOr<void>&&);
    void didResolve(CallbackIdentifier, ExceptionOr<Vector<String>>&&);

private:
    WorkerFileSystemStorageConnection(WorkerGlobalScope&, Ref<FileSystemStorageConnection>&&);

    // FileSystemStorageConnection
    void isSameEntry(FileSystemHandleIdentifier, FileSystemHandleIdentifier, FileSystemStorageConnection::SameEntryCallback&&) final;
    void getFileHandle(FileSystemHandleIdentifier, const String& name, bool createIfNecessary, FileSystemStorageConnection::GetHandleCallback&&) final;
    void getDirectoryHandle(FileSystemHandleIdentifier, const String& name, bool createIfNecessary, FileSystemStorageConnection::GetHandleCallback&&) final;
    void removeEntry(FileSystemHandleIdentifier, const String& name, bool deleteRecursively, FileSystemStorageConnection::RemoveEntryCallback&&) final;
    void resolve(FileSystemHandleIdentifier, FileSystemHandleIdentifier, FileSystemStorageConnection::ResolveCallback&&) final;

    WeakPtr<WorkerGlobalScope> m_scope;
    RefPtr<FileSystemStorageConnection> m_mainThreadConnection;
    HashMap<CallbackIdentifier, FileSystemStorageConnection::SameEntryCallback> m_sameEntryCallbacks;
    HashMap<CallbackIdentifier, FileSystemStorageConnection::GetHandleCallback> m_getHandleCallbacks;
    HashMap<CallbackIdentifier, FileSystemStorageConnection::RemoveEntryCallback> m_removeEntryCallbacks;
    HashMap<CallbackIdentifier, FileSystemStorageConnection::ResolveCallback> m_resolveCallbacks;
};

} // namespace WebCore
