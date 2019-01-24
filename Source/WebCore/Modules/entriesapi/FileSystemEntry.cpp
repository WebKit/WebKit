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
#include "FileSystemEntry.h"

#include "DOMException.h"
#include "DOMFileSystem.h"
#include "ErrorCallback.h"
#include "FileSystemDirectoryEntry.h"
#include "FileSystemEntryCallback.h"
#include "ScriptExecutionContext.h"
#include <wtf/FileSystem.h>

namespace WebCore {

FileSystemEntry::FileSystemEntry(ScriptExecutionContext& context, DOMFileSystem& filesystem, const String& virtualPath)
    : ActiveDOMObject(&context)
    , m_filesystem(filesystem)
    , m_name(FileSystem::pathGetFileName(virtualPath))
    , m_virtualPath(virtualPath)
{
    suspendIfNeeded();
}

FileSystemEntry::~FileSystemEntry() = default;

DOMFileSystem& FileSystemEntry::filesystem() const
{
    return m_filesystem.get();
}

const char* FileSystemEntry::activeDOMObjectName() const
{
    return "FileSystemEntry";
}

bool FileSystemEntry::canSuspendForDocumentSuspension() const
{
    return !hasPendingActivity();
}

void FileSystemEntry::getParent(ScriptExecutionContext& context, RefPtr<FileSystemEntryCallback>&& successCallback, RefPtr<ErrorCallback>&& errorCallback)
{
    if (!successCallback && !errorCallback)
        return;

    filesystem().getParent(context, *this, [pendingActivity = makePendingActivity(*this), successCallback = WTFMove(successCallback), errorCallback = WTFMove(errorCallback)](auto&& result) {
        if (result.hasException()) {
            if (errorCallback)
                errorCallback->handleEvent(DOMException::create(result.releaseException()));
            return;
        }
        if (successCallback)
            successCallback->handleEvent(result.releaseReturnValue());
    });
}


} // namespace WebCore
