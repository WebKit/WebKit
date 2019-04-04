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
#include "FileSystemDirectoryReader.h"

#include "DOMException.h"
#include "DOMFileSystem.h"
#include "ErrorCallback.h"
#include "FileSystemDirectoryEntry.h"
#include "FileSystemEntriesCallback.h"
#include "ScriptExecutionContext.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MainThread.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(FileSystemDirectoryReader);

FileSystemDirectoryReader::FileSystemDirectoryReader(ScriptExecutionContext& context, FileSystemDirectoryEntry& directory)
    : ActiveDOMObject(&context)
    , m_directory(directory)
{
    suspendIfNeeded();
}

FileSystemDirectoryReader::~FileSystemDirectoryReader() = default;

const char* FileSystemDirectoryReader::activeDOMObjectName() const
{
    return "FileSystemDirectoryReader";
}

bool FileSystemDirectoryReader::canSuspendForDocumentSuspension() const
{
    return !hasPendingActivity();
}

// https://wicg.github.io/entries-api/#dom-filesystemdirectoryentry-readentries
void FileSystemDirectoryReader::readEntries(ScriptExecutionContext& context, Ref<FileSystemEntriesCallback>&& successCallback, RefPtr<ErrorCallback>&& errorCallback)
{
    if (m_isReading) {
        if (errorCallback)
            errorCallback->scheduleCallback(context, DOMException::create(Exception { InvalidStateError, "Directory reader is already reading"_s }));
        return;
    }

    if (m_error) {
        if (errorCallback)
            errorCallback->scheduleCallback(context, DOMException::create(*m_error));
        return;
    }

    if (m_isDone) {
        successCallback->scheduleCallback(context, { });
        return;
    }

    m_isReading = true;
    auto pendingActivity = makePendingActivity(*this);
    callOnMainThread([this, context = makeRef(context), successCallback = WTFMove(successCallback), errorCallback = WTFMove(errorCallback), pendingActivity = WTFMove(pendingActivity)]() mutable {
        m_isReading = false;
        m_directory->filesystem().listDirectory(context, m_directory, [this, successCallback = WTFMove(successCallback), errorCallback = WTFMove(errorCallback), pendingActivity = WTFMove(pendingActivity)](ExceptionOr<Vector<Ref<FileSystemEntry>>>&& result) {
            if (result.hasException()) {
                m_error = result.releaseException();
                if (errorCallback)
                    errorCallback->handleEvent(DOMException::create(*m_error));
                return;
            }
            m_isDone = true;
            successCallback->handleEvent(result.releaseReturnValue());
        });
    });
}

} // namespace WebCore
