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
#include "FileSystemDirectoryEntry.h"

#include "DOMException.h"
#include "DOMFileSystem.h"
#include "Document.h"
#include "ErrorCallback.h"
#include "FileSystemDirectoryReader.h"
#include "FileSystemEntryCallback.h"
#include "FileSystemFileEntry.h"
#include "ScriptExecutionContext.h"
#include "WindowEventLoop.h"

namespace WebCore {

Ref<FileSystemDirectoryEntry> FileSystemDirectoryEntry::create(ScriptExecutionContext& context, DOMFileSystem& filesystem, const String& virtualPath)
{
    auto result = adoptRef(*new FileSystemDirectoryEntry(context, filesystem, virtualPath));
    result->suspendIfNeeded();
    return result;
}

FileSystemDirectoryEntry::FileSystemDirectoryEntry(ScriptExecutionContext& context, DOMFileSystem& filesystem, const String& virtualPath)
    : FileSystemEntry(context, filesystem, virtualPath)
{
}

Ref<FileSystemDirectoryReader> FileSystemDirectoryEntry::createReader(ScriptExecutionContext& context)
{
    return FileSystemDirectoryReader::create(context, *this);
}

void FileSystemDirectoryEntry::getEntry(ScriptExecutionContext& context, const String& path, const Flags& flags, EntryMatchingFunction&& matches, RefPtr<FileSystemEntryCallback>&& successCallback, RefPtr<ErrorCallback>&& errorCallback)
{
    if (!successCallback && !errorCallback)
        return;

    filesystem().getEntry(context, *this, path, flags, [this, pendingActivity = makePendingActivity(*this), matches = WTFMove(matches), successCallback = WTFMove(successCallback), errorCallback = WTFMove(errorCallback)](auto&& result) mutable {
        RefPtr document = this->document();
        if (result.hasException()) {
            if (errorCallback && document) {
                document->eventLoop().queueTask(TaskSource::Networking, [errorCallback = WTFMove(errorCallback), exception = result.releaseException(), pendingActivity = WTFMove(pendingActivity)]() mutable {
                    errorCallback->handleEvent(DOMException::create(WTFMove(exception)));
                });
            }
            return;
        }
        auto entry = result.releaseReturnValue();
        if (!matches(entry)) {
            if (errorCallback && document) {
                document->eventLoop().queueTask(TaskSource::Networking, [errorCallback = WTFMove(errorCallback), pendingActivity = WTFMove(pendingActivity)]() mutable {
                    errorCallback->handleEvent(DOMException::create(Exception { ExceptionCode::TypeMismatchError, "Entry at given path does not match expected type"_s }));
                });
            }
            return;
        }
        if (successCallback && document) {
            document->eventLoop().queueTask(TaskSource::Networking, [successCallback = WTFMove(successCallback), entry = WTFMove(entry), pendingActivity = WTFMove(pendingActivity)]() mutable {
                successCallback->handleEvent(WTFMove(entry));
            });
        }
    });
}

void FileSystemDirectoryEntry::getFile(ScriptExecutionContext& context, const String& path, const Flags& flags, RefPtr<FileSystemEntryCallback>&& successCallback, RefPtr<ErrorCallback>&& errorCallback)
{
    getEntry(context, path, flags, [](auto& entry) { return entry.isFile(); }, WTFMove(successCallback), WTFMove(errorCallback));
}

void FileSystemDirectoryEntry::getDirectory(ScriptExecutionContext& context, const String& path, const Flags& flags, RefPtr<FileSystemEntryCallback>&& successCallback, RefPtr<ErrorCallback>&& errorCallback)
{
    getEntry(context, path, flags, [](auto& entry) { return entry.isDirectory(); }, WTFMove(successCallback), WTFMove(errorCallback));
}

} // namespace WebCore
