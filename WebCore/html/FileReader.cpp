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

#if ENABLE(FILE_READER)

#include "FileReader.h"

#include "Base64.h"
#include "Blob.h"
#include "File.h"
#include "FileStreamProxy.h"
#include "Logging.h"
#include "ProgressEvent.h"
#include "ScriptExecutionContext.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

const unsigned bufferSize = 1024;
const double progressNotificationIntervalMS = 50;

FileReader::FileReader(ScriptExecutionContext* context)
    : ActiveDOMObject(context, this)
    , m_state(Empty)
    , m_readType(ReadFileAsBinaryString)
    , m_result("")
    , m_isRawDataConverted(false)
    , m_bytesLoaded(0)
    , m_totalBytes(0)
    , m_lastProgressNotificationTimeMS(0)
    , m_alreadyStarted(false)
{
    m_buffer.resize(bufferSize);
}

FileReader::~FileReader()
{
    terminate();
}

bool FileReader::hasPendingActivity() const
{
    return m_state == Loading || ActiveDOMObject::hasPendingActivity();
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
    LOG(FileAPI, "FileReader: reading as binary: %s\n", fileBlob->path().utf8().data());

    readInternal(fileBlob, ReadFileAsBinaryString);
}

void FileReader::readAsText(Blob* fileBlob, const String& encoding)
{
    LOG(FileAPI, "FileReader: reading as text: %s\n", fileBlob->path().utf8().data());

    if (!encoding.isEmpty())
        m_encoding = TextEncoding(encoding);
    readInternal(fileBlob, ReadFileAsText);
}

void FileReader::readAsDataURL(File* file)
{
    LOG(FileAPI, "FileReader: reading as data URL: %s\n", file->path().utf8().data());

    m_fileType = file->type();
    readInternal(file, ReadFileAsDataURL);
}

void FileReader::readInternal(Blob* fileBlob, ReadType type)
{
    // readAs*** methods() can be called multiple times. Only the last call before the actual reading happens is processed.
    if (m_alreadyStarted)
        return;

    m_fileBlob = fileBlob;
    m_readType = type;

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
    m_state = Done;
}

void FileReader::didStart()
{
    m_alreadyStarted = true;
    m_streamProxy->openForRead(m_fileBlob.get());
}

void FileReader::didGetSize(long long size)
{
    m_state = Loading;
    fireEvent(eventNames().loadstartEvent);

    m_totalBytes = size;
    m_streamProxy->read(&m_buffer.at(0), m_buffer.size());
}

void FileReader::didRead(const char* data, int bytesRead)
{
    ASSERT(data && bytesRead);

    // Bail out if we have aborted the reading.
    if (m_state == Done)
      return;

    switch (m_readType) {
    case ReadFileAsBinaryString:
        m_result += String(data, static_cast<unsigned>(bytesRead));
        break;
    case ReadFileAsText:
    case ReadFileAsDataURL:
        m_rawData.append(data, static_cast<unsigned>(bytesRead));
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
    m_streamProxy->read(&m_buffer.at(0), m_buffer.size());
}

void FileReader::didFinish()
{
    m_state = Done;

    m_streamProxy->close();

    fireEvent(eventNames().loadEvent);
    fireEvent(eventNames().loadendEvent);
}

void FileReader::didFail(ExceptionCode ec)
{
    m_state = Done;
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
    else if (m_readType == ReadFileAsDataURL && m_state == Done)
        convertToDataURL();

    return m_result;
}

void FileReader::convertToText()
{
    if (!m_rawData.size()) {
        m_result = "";
        return;
    }

    // Try to determine the encoding if it is not provided.
    // FIXME: move the following logic to a more generic place.
    int offset = 0;
    if (!m_encoding.isValid()) {
        if (m_rawData.size() >= 2 && m_rawData[0] == '\xFE' && m_rawData[1] == '\xFF') {
            offset = 2;
            m_encoding = UTF16BigEndianEncoding();
        } else if (m_rawData.size() >= 2 && m_rawData[0] == '\xFF' && m_rawData[1] == '\xFE') {
            offset = 2;
            m_encoding = UTF16LittleEndianEncoding();
        } else if (m_rawData.size() >= 2 && m_rawData[0] == '\xEF' && m_rawData[1] == '\xBB' && m_rawData[2] == '\xBF') {
            offset = 3;
            m_encoding = UTF8Encoding();
        } else
            m_encoding = UTF8Encoding();
    }

    // Decode the data.
    // FIXME: consider supporting incremental decoding to improve the perf.
    m_result = m_encoding.decode(&m_rawData.at(0) + offset, m_rawData.size() - offset);
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
 
#endif // ENABLE(FILE_READER)
