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

#include "FileReaderSync.h"

#include "Base64.h"
#include "Blob.h"
#include "BlobURL.h"
#include "FileReader.h"
#include "ResourceRequest.h"
#include "ScriptExecutionContext.h"
#include "TextEncoding.h"
#include "TextResourceDecoder.h"
#include "ThreadableBlobRegistry.h"
#include "ThreadableLoader.h"

namespace WebCore {

class FileReaderSyncLoader : public ThreadableLoaderClient {
public:
    // If the output result is provided, use it. Otherwise, save it as the raw data.
    FileReaderSyncLoader(ScriptString* result);

    // Returns the http status code.
    void start(ScriptExecutionContext*, const ResourceRequest&, ExceptionCode&);

    // ThreadableLoaderClient
    virtual void didReceiveResponse(const ResourceResponse&);
    virtual void didReceiveData(const char*, int);
    virtual void didFinishLoading(unsigned long identifier);
    virtual void didFail(const ResourceError&);

    const Vector<char>& rawData() const { return m_rawData; }

private:
    // The output result. The caller provides this in order to load the binary data directly.
    ScriptString* m_result;

    // The raw data. The caller does not provide the above output result and we need to save it here.
    Vector<char> m_rawData;

    int m_httpStatusCode;    
};

FileReaderSyncLoader::FileReaderSyncLoader(ScriptString* result)
    : m_result(result)
    , m_httpStatusCode(0)
{
}

void FileReaderSyncLoader::start(ScriptExecutionContext* scriptExecutionContext, const ResourceRequest& request, ExceptionCode& ec)
{
    ThreadableLoaderOptions options;
    options.sendLoadCallbacks = true;
    options.sniffContent = false;
    options.forcePreflight = false;
    options.allowCredentials = true;
    options.crossOriginRequestPolicy = DenyCrossOriginRequests;

    ThreadableLoader::loadResourceSynchronously(scriptExecutionContext, request, *this, options);

    ec = (m_httpStatusCode == 200) ? 0 : FileReader::httpStatusCodeToExceptionCode(m_httpStatusCode);
}

void FileReaderSyncLoader::didReceiveResponse(const ResourceResponse& response)
{
    m_httpStatusCode = response.httpStatusCode();
}

void FileReaderSyncLoader::didReceiveData(const char* data, int lengthReceived)
{
    if (m_result)
        *m_result += String(data, static_cast<unsigned>(lengthReceived));
    else
        m_rawData.append(data, static_cast<unsigned>(lengthReceived));
}

void FileReaderSyncLoader::didFinishLoading(unsigned long)
{
}

void FileReaderSyncLoader::didFail(const ResourceError&)
{
    // Treat as internal error.
    m_httpStatusCode = 500;
}

FileReaderSync::FileReaderSync()
    : m_result("")
{
}

const ScriptString& FileReaderSync::readAsBinaryString(ScriptExecutionContext* scriptExecutionContext, Blob* blob, ExceptionCode& ec)
{
    if (!blob)
        return m_result;

    read(scriptExecutionContext, blob, ReadAsBinaryString, ec);
    return m_result;
}

const ScriptString& FileReaderSync::readAsText(ScriptExecutionContext* scriptExecutionContext, Blob* blob, const String& encoding, ExceptionCode& ec)
{
    if (!blob)
        return m_result;

    m_encoding = encoding;
    read(scriptExecutionContext, blob, ReadAsText, ec);
    return m_result;
}

const ScriptString& FileReaderSync::readAsDataURL(ScriptExecutionContext* scriptExecutionContext, Blob* blob, ExceptionCode& ec)
{
    if (!blob)
        return m_result;

    read(scriptExecutionContext, blob, ReadAsDataURL, ec);
    return m_result;
}

void FileReaderSync::read(ScriptExecutionContext* scriptExecutionContext, Blob* blob, ReadType readType, ExceptionCode& ec)
{
    // The blob is read by routing through the request handling layer given the temporary public url.
    KURL urlForReading = BlobURL::createPublicURL(scriptExecutionContext->securityOrigin());
    ThreadableBlobRegistry::registerBlobURL(urlForReading, blob->url());

    ResourceRequest request(urlForReading);
    request.setHTTPMethod("GET");

    FileReaderSyncLoader loader((readType == ReadAsBinaryString) ? &m_result : 0);
    loader.start(scriptExecutionContext, request, ec);
    ThreadableBlobRegistry::unregisterBlobURL(urlForReading);
    if (ec)
        return;

    switch (readType) {
    case ReadAsBinaryString:
        // Nothing to do since we need no conversion.
        return;
    case ReadAsText:
        convertToText(loader.rawData().data(), loader.rawData().size(), m_result);
        return;
    case ReadAsDataURL:
        FileReader::convertToDataURL(loader.rawData(), blob->type(), m_result);
        return;
    }

    ASSERT_NOT_REACHED();
}

void FileReaderSync::convertToText(const char* data, int size, ScriptString& result)
{
    if (!size)
        return;

    // Decode the data.
    // The File API spec says that we should use the supplied encoding if it is valid. However, we choose to ignore this
    // requirement in order to be consistent with how WebKit decodes the web content: always have the BOM override the
    // provided encoding.     
    // FIXME: consider supporting incremental decoding to improve the perf.
    RefPtr<TextResourceDecoder> decoder = TextResourceDecoder::create("text/plain", m_encoding.isEmpty() ? UTF8Encoding() : TextEncoding(m_encoding));
    result = decoder->decode(data, size);
    result += decoder->flush();
}

} // namespace WebCore
 
#endif // ENABLE(BLOB)
