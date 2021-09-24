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

#include "FileSystemStorageHandleIdentifier.h"
#include <WebCore/FileSystemHandleImpl.h>

namespace IPC {
class Connection;
}

namespace WebCore {
template<typename> class ExceptionOr;
class FileSystemDirectoryHandle;
class FileSystemFileHandle;
}

namespace WebKit {

class FileSystemStorageHandleProxy final : public WebCore::FileSystemHandleImpl {
public:
    static Ref<FileSystemStorageHandleProxy> create(FileSystemStorageHandleIdentifier, IPC::Connection&);
    void connectionClosed();

private:
    FileSystemStorageHandleProxy(FileSystemStorageHandleIdentifier, IPC::Connection&);

    // FileSystemHandleImpl
    std::optional<uint64_t> storageHandleIdentifier() { return m_identifier.toUInt64(); }
    void isSameEntry(FileSystemHandleImpl&, CompletionHandler<void(WebCore::ExceptionOr<bool>&&)>&&) final;
    void getFileHandle(const String& name, bool createIfNecessary, CompletionHandler<void(WebCore::ExceptionOr<Ref<WebCore::FileSystemHandleImpl>>&&)>&&) final;
    void getDirectoryHandle(const String& name, bool createIfNecessary, CompletionHandler<void(WebCore::ExceptionOr<Ref<WebCore::FileSystemHandleImpl>>&&)>&&) final;
    void removeEntry(const String& name, bool deleteRecursively, CompletionHandler<void(WebCore::ExceptionOr<void>&&)>&&) final;
    void resolve(FileSystemHandleImpl&, CompletionHandler<void(WebCore::ExceptionOr<Vector<String>>&&)>&&) final;

    FileSystemStorageHandleIdentifier m_identifier;
    RefPtr<IPC::Connection> m_connection;
};

} // namespace WebKit
