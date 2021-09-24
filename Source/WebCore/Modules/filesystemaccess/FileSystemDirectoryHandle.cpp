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
#include "FileSystemDirectoryHandle.h"

#include "FileSystemHandleImpl.h"
#include "JSDOMPromiseDeferred.h"
#include "JSFileSystemDirectoryHandle.h"
#include "JSFileSystemFileHandle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(FileSystemDirectoryHandle);

Ref<FileSystemDirectoryHandle> FileSystemDirectoryHandle::create(String&& name, Ref<FileSystemHandleImpl>&& impl)
{
    return adoptRef(*new FileSystemDirectoryHandle(WTFMove(name), WTFMove(impl)));
}

FileSystemDirectoryHandle::FileSystemDirectoryHandle(String&& name, Ref<FileSystemHandleImpl>&& impl)
    : FileSystemHandle(FileSystemHandle::Kind::Directory, WTFMove(name), WTFMove(impl))
{
}

void FileSystemDirectoryHandle::getFileHandle(const String& name, std::optional<FileSystemDirectoryHandle::GetFileOptions> options, DOMPromiseDeferred<IDLInterface<FileSystemFileHandle>>&& promise)
{
    bool createIfNecessary = options ? options->create : false;
    impl().getFileHandle(name, createIfNecessary, [name, promise = WTFMove(promise)](auto result) mutable {
        if (result.hasException())
            return promise.reject(result.releaseException());

        promise.resolve(FileSystemFileHandle::create(String { name }, result.releaseReturnValue()));
    });
}

void FileSystemDirectoryHandle::getDirectoryHandle(const String& name, std::optional<FileSystemDirectoryHandle::GetDirectoryOptions> options, DOMPromiseDeferred<IDLInterface<FileSystemDirectoryHandle>>&& promise)
{
    bool createIfNecessary = options ? options->create : false;
    impl().getDirectoryHandle(name, createIfNecessary, [name, promise = WTFMove(promise)](auto result) mutable {
        if (result.hasException())
            return promise.reject(result.releaseException());

        promise.resolve(FileSystemDirectoryHandle::create(String { name }, result.releaseReturnValue()));
    });
}

void FileSystemDirectoryHandle::removeEntry(const String& name, std::optional<FileSystemDirectoryHandle::RemoveOptions> options, DOMPromiseDeferred<void>&& promise)
{
    bool deleteRecursively = options ? options->recursive : false;
    impl().removeEntry(name, deleteRecursively, [promise = WTFMove(promise)](auto result) mutable {
        promise.settle(WTFMove(result));
    });
}

void FileSystemDirectoryHandle::resolve(const FileSystemHandle& handle, DOMPromiseDeferred<IDLSequence<IDLUSVString>>&& promise)
{
    impl().resolve(handle.impl(), [promise = WTFMove(promise)](auto result) mutable {
        if (result.hasException())
            return promise.reject(result.releaseException());

        promise.resolve(result.releaseReturnValue());
    });
}

} // namespace WebCore


