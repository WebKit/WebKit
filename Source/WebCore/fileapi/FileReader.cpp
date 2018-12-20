/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FileReader.h"

#include "EventNames.h"
#include "File.h"
#include "Logging.h"
#include "ProgressEvent.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/text/CString.h>

namespace WebCore {

// Fire the progress event at least every 50ms.
static const auto progressNotificationInterval = 50_ms;

Ref<FileReader> FileReader::create(ScriptExecutionContext& context)
{
    auto fileReader = adoptRef(*new FileReader(context));
    fileReader->suspendIfNeeded();
    return fileReader;
}

FileReader::FileReader(ScriptExecutionContext& context)
    : ActiveDOMObject(&context)
{
}

FileReader::~FileReader()
{
    if (m_loader)
        m_loader->cancel();
}

bool FileReader::canSuspendForDocumentSuspension() const
{
    // FIXME: It is not currently possible to suspend a FileReader, so pages with FileReader can not go into page cache.
    return false;
}

const char* FileReader::activeDOMObjectName() const
{
    return "FileReader";
}

void FileReader::stop()
{
    if (m_loader) {
        m_loader->cancel();
        m_loader = nullptr;
    }
    m_state = DONE;
}

ExceptionOr<void> FileReader::readAsArrayBuffer(Blob* blob)
{
    if (!blob)
        return { };

    LOG(FileAPI, "FileReader: reading as array buffer: %s %s\n", blob->url().string().utf8().data(), is<File>(*blob) ? downcast<File>(*blob).path().utf8().data() : "");

    return readInternal(*blob, FileReaderLoader::ReadAsArrayBuffer);
}

ExceptionOr<void> FileReader::readAsBinaryString(Blob* blob)
{
    if (!blob)
        return { };

    LOG(FileAPI, "FileReader: reading as binary: %s %s\n", blob->url().string().utf8().data(), is<File>(*blob) ? downcast<File>(*blob).path().utf8().data() : "");

    return readInternal(*blob, FileReaderLoader::ReadAsBinaryString);
}

ExceptionOr<void> FileReader::readAsText(Blob* blob, const String& encoding)
{
    if (!blob)
        return { };

    LOG(FileAPI, "FileReader: reading as text: %s %s\n", blob->url().string().utf8().data(), is<File>(*blob) ? downcast<File>(*blob).path().utf8().data() : "");

    m_encoding = encoding;
    return readInternal(*blob, FileReaderLoader::ReadAsText);
}

ExceptionOr<void> FileReader::readAsDataURL(Blob* blob)
{
    if (!blob)
        return { };

    LOG(FileAPI, "FileReader: reading as data URL: %s %s\n", blob->url().string().utf8().data(), is<File>(*blob) ? downcast<File>(*blob).path().utf8().data() : "");

    return readInternal(*blob, FileReaderLoader::ReadAsDataURL);
}

ExceptionOr<void> FileReader::readInternal(Blob& blob, FileReaderLoader::ReadType type)
{
    // If multiple concurrent read methods are called on the same FileReader, InvalidStateError should be thrown when the state is LOADING.
    if (m_state == LOADING)
        return Exception { InvalidStateError };

    setPendingActivity(this);

    m_blob = &blob;
    m_readType = type;
    m_state = LOADING;
    m_error = nullptr;

    m_loader = std::make_unique<FileReaderLoader>(m_readType, static_cast<FileReaderLoaderClient*>(this));
    m_loader->setEncoding(m_encoding);
    m_loader->setDataType(m_blob->type());
    m_loader->start(scriptExecutionContext(), blob);

    return { };
}

void FileReader::abort()
{
    LOG(FileAPI, "FileReader: aborting\n");

    if (m_aborting || m_state != LOADING)
        return;
    m_aborting = true;

    // Schedule to have the abort done later since abort() might be called from the event handler and we do not want the resource loading code to be in the stack.
    scriptExecutionContext()->postTask([this] (ScriptExecutionContext&) {
        ASSERT(m_state != DONE);

        stop();
        m_aborting = false;

        m_error = FileError::create(FileError::ABORT_ERR);

        fireEvent(eventNames().errorEvent);
        fireEvent(eventNames().abortEvent);
        fireEvent(eventNames().loadendEvent);

        // All possible events have fired and we're done, no more pending activity.
        unsetPendingActivity(this);
    });
}

void FileReader::didStartLoading()
{
    fireEvent(eventNames().loadstartEvent);
}

void FileReader::didReceiveData()
{
    auto now = MonotonicTime::now();
    if (std::isnan(m_lastProgressNotificationTime)) {
        m_lastProgressNotificationTime = now;
        return;
    }
    if (now - m_lastProgressNotificationTime > progressNotificationInterval) {
        fireEvent(eventNames().progressEvent);
        m_lastProgressNotificationTime = now;
    }
}

void FileReader::didFinishLoading()
{
    if (m_aborting)
        return;

    ASSERT(m_state != DONE);
    m_state = DONE;

    fireEvent(eventNames().progressEvent);
    fireEvent(eventNames().loadEvent);
    fireEvent(eventNames().loadendEvent);
    
    // All possible events have fired and we're done, no more pending activity.
    unsetPendingActivity(this);
}

void FileReader::didFail(int errorCode)
{
    // If we're aborting, do not proceed with normal error handling since it is covered in aborting code.
    if (m_aborting)
        return;

    ASSERT(m_state != DONE);
    m_state = DONE;

    m_error = FileError::create(static_cast<FileError::ErrorCode>(errorCode));
    fireEvent(eventNames().errorEvent);
    fireEvent(eventNames().loadendEvent);
    
    // All possible events have fired and we're done, no more pending activity.
    unsetPendingActivity(this);
}

void FileReader::fireEvent(const AtomicString& type)
{
    dispatchEvent(ProgressEvent::create(type, true, m_loader ? m_loader->bytesLoaded() : 0, m_loader ? m_loader->totalBytes() : 0));
}

Optional<Variant<String, RefPtr<JSC::ArrayBuffer>>> FileReader::result() const
{
    if (!m_loader || m_error)
        return WTF::nullopt;
    if (m_readType == FileReaderLoader::ReadAsArrayBuffer) {
        auto result = m_loader->arrayBufferResult();
        if (!result)
            return WTF::nullopt;
        return { result };
    }
    String result = m_loader->stringResult();
    if (result.isNull())
        return WTF::nullopt;
    return { WTFMove(result) };
}

} // namespace WebCore
