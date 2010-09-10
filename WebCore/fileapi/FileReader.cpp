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
#include "BlobURL.h"
#include "CrossThreadTask.h"
#include "File.h"
#include "Logging.h"
#include "ProgressEvent.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ScriptExecutionContext.h"
#include "TextResourceDecoder.h"
#include "ThreadableBlobRegistry.h"
#include "ThreadableLoader.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

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

void FileReader::readAsBinaryString(Blob* blob)
{
    if (!blob)
        return;

    LOG(FileAPI, "FileReader: reading as binary: %s %s\n", blob->url().string().utf8().data(), blob->isFile() ? static_cast<File*>(blob)->path().utf8().data() : "");

    readInternal(blob, ReadFileAsBinaryString);
}

void FileReader::readAsText(Blob* blob, const String& encoding)
{
    if (!blob)
        return;

    LOG(FileAPI, "FileReader: reading as text: %s %s\n", blob->url().string().utf8().data(), blob->isFile() ? static_cast<File*>(blob)->path().utf8().data() : "");

    if (!encoding.isEmpty())
        m_encoding = TextEncoding(encoding);
    readInternal(blob, ReadFileAsText);
}

void FileReader::readAsDataURL(Blob* blob)
{
    if (!blob)
        return;

    LOG(FileAPI, "FileReader: reading as data URL: %s %s\n", blob->url().string().utf8().data(), blob->isFile() ? static_cast<File*>(blob)->path().utf8().data() : "");

    m_fileType = blob->type();
    readInternal(blob, ReadFileAsDataURL);
}

static void delayedStart(ScriptExecutionContext*, FileReader* reader)
{
    reader->start();
}

void FileReader::readInternal(Blob* blob, ReadType type)
{
    // readAs*** methods() can be called multiple times. Only the last call before the actual reading happens is processed.
    if (m_state != None && m_state != Starting)
        return;

    if (m_state == None)
        scriptExecutionContext()->postTask(createCallbackTask(&delayedStart, this));

    m_blob = blob;
    m_readType = type;
    m_state = Starting;
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
    if (m_loader) {
        m_loader->cancel();
        cleanup();
    }
    m_state = Completed;
}

void FileReader::cleanup()
{
    m_loader = 0;
    ThreadableBlobRegistry::unregisterBlobURL(m_urlForReading);
    m_urlForReading = KURL();
}

void FileReader::start()
{
    m_state = Opening;

    // The blob is read by routing through the request handling layer given a temporary public url.
    m_urlForReading = BlobURL::createPublicURL(scriptExecutionContext()->securityOrigin());
    ThreadableBlobRegistry::registerBlobURL(m_urlForReading, m_blob->url());

    ResourceRequest request(m_urlForReading);
    request.setHTTPMethod("GET");

    ThreadableLoaderOptions options;
    options.sendLoadCallbacks = true;
    options.sniffContent = false;
    options.forcePreflight = false;
    options.allowCredentials = true;
    options.crossOriginRequestPolicy = DenyCrossOriginRequests;

    m_loader = ThreadableLoader::create(scriptExecutionContext(), this, request, options);
}

void FileReader::didReceiveResponse(const ResourceResponse& response)
{
    if (response.httpStatusCode() != 200) {
        failed(response.httpStatusCode());
        return;
    }

    m_state = Reading;
    fireEvent(eventNames().loadstartEvent);

    m_totalBytes = response.expectedContentLength();
}

void FileReader::didReceiveData(const char* data, int lengthReceived)
{
    ASSERT(data && lengthReceived);

    // Bail out if we have aborted the reading.
    if (m_state == Completed)
        return;

    switch (m_readType) {
    case ReadFileAsBinaryString:
        m_result += String(data, static_cast<unsigned>(lengthReceived));
        break;
    case ReadFileAsText:
    case ReadFileAsDataURL:
        m_rawData.append(data, static_cast<unsigned>(lengthReceived));
        m_isRawDataConverted = false;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    m_bytesLoaded += lengthReceived;

    // Fire the progress event at least every 50ms.
    double now = WTF::currentTimeMS();
    if (!m_lastProgressNotificationTimeMS)
        m_lastProgressNotificationTimeMS = now;
    else if (now - m_lastProgressNotificationTimeMS > progressNotificationIntervalMS) {
        fireEvent(eventNames().progressEvent);
        m_lastProgressNotificationTimeMS = now;
    }
}

void FileReader::didFinishLoading(unsigned long)
{
    m_state = Completed;

    fireEvent(eventNames().loadEvent);
    fireEvent(eventNames().loadendEvent);

    cleanup();
}

void FileReader::didFail(const ResourceError&)
{
    // Treat as internal error.
    failed(500);
}

void FileReader::failed(int httpStatusCode)
{
    m_state = Completed;

     m_error = FileError::create(httpStatusCodeToExceptionCode(httpStatusCode));
    fireEvent(eventNames().errorEvent);
    fireEvent(eventNames().loadendEvent);

    cleanup();
}

ExceptionCode FileReader::httpStatusCodeToExceptionCode(int httpStatusCode)
{
    switch (httpStatusCode) {
        case 403:
            return SECURITY_ERR;
        case 404:
            return NOT_FOUND_ERR;
        default:
            return NOT_READABLE_ERR;
    }
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
        convertToDataURL(m_rawData, m_fileType, m_result);

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

void FileReader::convertToDataURL(const Vector<char>& rawData, const String& fileType, ScriptString& result)
{
    result = "data:";

    if (!rawData.size())
        return;

    result += fileType;
    if (!fileType.isEmpty())
        result += ";";
    result += "base64,";

    Vector<char> out;
    base64Encode(rawData, out);
    out.append('\0');
    result += out.data();
}

} // namespace WebCore
 
#endif // ENABLE(BLOB)
