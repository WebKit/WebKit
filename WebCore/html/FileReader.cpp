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

#if ENABLE(BLOB)

#include "FileReader.h"

#include "Base64.h"
#include "Blob.h"
#include "File.h"
#include "FileStreamProxy.h"
#include "Logging.h"
#include "ProgressEvent.h"
#include "ScriptExecutionContext.h"
#include "TextResourceDecoder.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

const unsigned bufferSize = 1024;
const double progressNotificationIntervalMS = 50;

FileReader::FileReader(ScriptExecutionContext* context)
    : ActiveDOMObject(context, this)
    , m_state(None)
    , m_readType(ReadFileAsBinaryString)
    , m_result("")
    , m_isRawDataConverted(false)
    , m_bytesLoaded(0)
    , m_totalBytes(0)
    , m_lastProgressNotificationTimeMS(0)
{
    m_buffer.resize(bufferSize);
}

FileReader::~FileReader()
{
    terminate();
}

bool FileReader::hasPendingActivity() const
{
    return (m_state != None && m_state != Completed) || ActiveDOMObject::hasPendingActivity();
}

bool FileReader::canSuspend() const
{
    // FIXME: It is not currently possible to suspend a FileReader, so pages with FileReader can not go into page cache.
    return false;
}

void FileReader::stop()
{
    terminate();
}

void FileReader::readAsBinaryString(Blob* fileBlob)
{
    if (!fileBlob)
        return;

    // FIXME: needs to handle non-file blobs.
    LOG(FileAPI, "FileReader: reading as binary: %s\n", fileBlob->path().utf8().data());

    readInternal(fileBlob, ReadFileAsBinaryString);
}

void FileReader::readAsText(Blob* fileBlob, const String& encoding)
{
    if (!fileBlob)
        return;

    // FIXME: needs to handle non-file blobs.
    LOG(FileAPI, "FileReader: reading as text: %s\n", fileBlob->path().utf8().data());

    if (!encoding.isEmpty())
        m_encoding = TextEncoding(encoding);
    readInternal(fileBlob, ReadFileAsText);
}

void FileReader::readAsDataURL(File* file)
{
    if (!file)
        return;

    LOG(FileAPI, "FileReader: reading as data URL: %s\n", file->path().utf8().data());

    m_fileType = file->type();
    readInternal(file, ReadFileAsDataURL);
}

void FileReader::readInternal(Blob* fileBlob, ReadType type)
{
    // readAs*** methods() can be called multiple times. Only the last call before the actual reading happens is processed.
    if (m_state != None && m_state != Starting)
        return;

    m_fileBlob = fileBlob;
    m_readType = type;
    m_state = Starting;

    // When FileStreamProxy is created, FileReader::didStart() will get notified on the File thread and we will start
    // opening and reading the file since then.
    if (!m_streamProxy.get())
        m_streamProxy = FileStreamProxy::create(scriptExecutionContext(), this);
}

void FileReader::abort()
{
    LOG(FileAPI, "FileReader: aborting\n");

    terminate();

    m_result = "";
    m_error = FileError::create(ABORT_ERR);

    fireEvent(eventNames().errorEvent);
    fireEvent(eventNames().abortEvent);
    fireEvent(eventNames().loadendEvent);
}

void FileReader::terminate()
{
    if (m_streamProxy) {
        m_streamProxy->stop();
        m_streamProxy = 0;
    }
    m_state = Completed;
}

void FileReader::didStart()
{
    m_state = Opening;

    ASSERT(m_fileBlob->items().size() == 1 && m_fileBlob->items().at(0)->toFileBlobItem());
    const FileRangeBlobItem* fileRangeItem = m_fileBlob->items().at(0)->toFileRangeBlobItem();
    double expectedModificationTime = fileRangeItem ? fileRangeItem->snapshotModificationTime() : 0;

    m_streamProxy->getSize(m_fileBlob->path(), expectedModificationTime);
}

void FileReader::didGetSize(long long size)
{
    // If the size is -1, it means the file has been moved or changed. Fail now.
    if (size == -1) {
        didFail(NOT_FOUND_ERR);
        return;
    }

    m_state = Reading;
    fireEvent(eventNames().loadstartEvent);

    ASSERT(m_fileBlob->items().size() == 1 && m_fileBlob->items().at(0)->toFileBlobItem());
    const FileRangeBlobItem* fileRangeItem = m_fileBlob->items().at(0)->toFileRangeBlobItem();
    long long start = fileRangeItem ? fileRangeItem->start() : 0;

    // The size passed back is the size of the whole file. If the underlying item is a sliced file, we need to use the slice length.
    m_totalBytes = fileRangeItem ? fileRangeItem->size() : size;

    m_streamProxy->openForRead(m_fileBlob->path(), start, m_totalBytes);
}

void FileReader::didOpen(ExceptionCode ec)
{
    if (ec) {
        didFail(ec);
        return;
    }
    
    m_streamProxy->read(m_buffer.data(), m_buffer.size());
}

void FileReader::didRead(int bytesRead)
{
    // Bail out if we have aborted the reading.
    if (m_state == Completed)
        return;

    // If bytesRead is -1, it means an error happens.
    if (bytesRead == -1) {
        didFail(NOT_READABLE_ERR);
        return;
    }

    // If bytesRead is 0, it means the reading is done.
    if (!bytesRead) {
        didFinish();
        return;
    }

    switch (m_readType) {
    case ReadFileAsBinaryString:
        m_result += String(m_buffer.data(), static_cast<unsigned>(bytesRead));
        break;
    case ReadFileAsText:
    case ReadFileAsDataURL:
        m_rawData.append(m_buffer.data(), static_cast<unsigned>(bytesRead));
        m_isRawDataConverted = false;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    m_bytesLoaded += bytesRead;

    // Fire the progress event at least every 50ms.
    double now = WTF::currentTimeMS();
    if (!m_lastProgressNotificationTimeMS)
        m_lastProgressNotificationTimeMS = now;
    else if (now - m_lastProgressNotificationTimeMS > progressNotificationIntervalMS) {
        fireEvent(eventNames().progressEvent);
        m_lastProgressNotificationTimeMS = now;
    }

    // Continue reading.
    m_streamProxy->read(m_buffer.data(), m_buffer.size());
}

void FileReader::didFinish()
{
    m_state = Completed;

    m_streamProxy->close();

    fireEvent(eventNames().loadEvent);
    fireEvent(eventNames().loadendEvent);
}

void FileReader::didFail(ExceptionCode ec)
{
    m_state = Completed;
    m_error = FileError::create(ec);

    m_streamProxy->close();

    fireEvent(eventNames().errorEvent);
    fireEvent(eventNames().loadendEvent);
}

void FileReader::fireEvent(const AtomicString& type)
{
    // FIXME: the current ProgressEvent uses "unsigned long" for total and loaded attributes. Need to talk with the spec writer to resolve the issue.
    dispatchEvent(ProgressEvent::create(type, true, static_cast<unsigned>(m_bytesLoaded), static_cast<unsigned>(m_totalBytes)));
}

FileReader::ReadyState FileReader::readyState() const
{
    switch (m_state) {
    case None:
    case Starting:
        return EMPTY;
    case Opening:
    case Reading:
        return LOADING;
    case Completed:
        return DONE;
    }
    ASSERT_NOT_REACHED();
    return EMPTY;
}

const ScriptString& FileReader::result()
{
    // If reading as binary string, we can return the result immediately.
    if (m_readType == ReadFileAsBinaryString)
        return m_result;

    // If we already convert the raw data received so far, we can return the result now.
    if (m_isRawDataConverted)
        return m_result;
    m_isRawDataConverted = true;

    if (m_readType == ReadFileAsText)
        convertToText();
    // For data URL, we only do the coversion until we receive all the raw data.
    else if (m_readType == ReadFileAsDataURL && m_state == Completed)
        convertToDataURL();

    return m_result;
}

void FileReader::convertToText()
{
    if (!m_rawData.size()) {
        m_result = "";
        return;
    }

    // Decode the data.
    // The File API spec says that we should use the supplied encoding if it is valid. However, we choose to ignore this
    // requirement in order to be consistent with how WebKit decodes the web content: always has the BOM override the
    // provided encoding.     
    // FIXME: consider supporting incremental decoding to improve the perf.
    if (!m_decoder)
        m_decoder = TextResourceDecoder::create("text/plain", m_encoding.isValid() ? m_encoding : UTF8Encoding());
    m_result = m_decoder->decode(&m_rawData.at(0), m_rawData.size());

    if (m_state == Completed && !m_error)
        m_result += m_decoder->flush();
}

void FileReader::convertToDataURL()
{
    m_result = "data:";

    if (!m_rawData.size())
        return;

    m_result += m_fileType;
    if (!m_fileType.isEmpty())
        m_result += ";";
    m_result += "base64,";

    Vector<char> out;
    base64Encode(m_rawData, out);
    out.append('\0');
    m_result += out.data();
}

} // namespace WebCore
 
#endif // ENABLE(BLOB)
