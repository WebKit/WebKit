/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebHTTPBody.h"

#include "FormData.h"
#include "WebFileInfo.h"

using namespace WebCore;

namespace WebKit {

class WebHTTPBodyPrivate : public FormData {
};

void WebHTTPBody::initialize()
{
    assign(static_cast<WebHTTPBodyPrivate*>(FormData::create().releaseRef()));
}

void WebHTTPBody::reset()
{
    assign(0);
}

void WebHTTPBody::assign(const WebHTTPBody& other)
{
    WebHTTPBodyPrivate* p = const_cast<WebHTTPBodyPrivate*>(other.m_private);
    if (p)
        p->ref();
    assign(p);
}

size_t WebHTTPBody::elementCount() const
{
    ASSERT(!isNull());
    return m_private->elements().size();
}

bool WebHTTPBody::elementAt(size_t index, Element& result) const
{
    ASSERT(!isNull());

    if (index >= m_private->elements().size())
        return false;

    const FormDataElement& element = m_private->elements()[index];

    switch (element.m_type) {
    case FormDataElement::data:
        result.type = Element::TypeData;
        result.data.assign(element.m_data.data(), element.m_data.size());
        result.filePath.reset();
#if ENABLE(BLOB_SLICE)
        result.fileStart = 0;
        result.fileLength = 0;
        result.fileInfo.modificationTime = 0.0;
#endif
        break;
    case FormDataElement::encodedFile:
        result.type = Element::TypeFile;
        result.data.reset();
        result.filePath = element.m_filename;
#if ENABLE(BLOB_SLICE)
        result.fileStart = element.m_fileStart;
        result.fileLength = element.m_fileLength;
        result.fileInfo.modificationTime = element.m_expectedFileModificationTime;
#else
        result.fileStart = 0;
        result.fileLength = -1;
        result.fileInfo.modificationTime = 0.0;
#endif
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    return true;
}

void WebHTTPBody::appendData(const WebData& data)
{
    ensureMutable();
    // FIXME: FormDataElement::m_data should be a SharedBuffer<char>.  Then we
    // could avoid this buffer copy.
    m_private->appendData(data.data(), data.size());
}

void WebHTTPBody::appendFile(const WebString& filePath)
{
    ensureMutable();
    m_private->appendFile(filePath);
}

void WebHTTPBody::appendFileRange(const WebString& filePath, long long fileStart, long long fileLength, const WebFileInfo& fileInfo)
{
#if ENABLE(BLOB_SLICE)
    ensureMutable();
    m_private->appendFileRange(filePath, fileStart, fileLength, fileInfo.modificationTime);
#endif
}

long long WebHTTPBody::identifier() const
{
    ASSERT(!isNull());
    return m_private->identifier();
}

void WebHTTPBody::setIdentifier(long long identifier)
{
    ensureMutable();
    return m_private->setIdentifier(identifier);
}

WebHTTPBody::WebHTTPBody(const PassRefPtr<FormData>& data)
    : m_private(static_cast<WebHTTPBodyPrivate*>(data.releaseRef()))
{
}

WebHTTPBody& WebHTTPBody::operator=(const PassRefPtr<FormData>& data)
{
    assign(static_cast<WebHTTPBodyPrivate*>(data.releaseRef()));
    return *this;
}

WebHTTPBody::operator PassRefPtr<FormData>() const
{
    return m_private;
}

void WebHTTPBody::assign(WebHTTPBodyPrivate* p)
{
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

void WebHTTPBody::ensureMutable()
{
    ASSERT(!isNull());
    if (!m_private->hasOneRef())
        assign(static_cast<WebHTTPBodyPrivate*>(m_private->copy().releaseRef()));
}

} // namespace WebKit
