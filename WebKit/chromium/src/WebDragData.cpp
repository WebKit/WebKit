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
#include "WebDragData.h"

#include "ChromiumDataObject.h"
#include "WebData.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebVector.h"

#include <wtf/PassRefPtr.h>

using namespace WebCore;

namespace WebKit {

class WebDragDataPrivate : public ChromiumDataObject {
};

void WebDragData::initialize()
{
    assign(static_cast<WebDragDataPrivate*>(ChromiumDataObject::create().releaseRef()));
}

void WebDragData::reset()
{
    assign(0);
}

void WebDragData::assign(const WebDragData& other)
{
    WebDragDataPrivate* p = const_cast<WebDragDataPrivate*>(other.m_private);
    if (p)
        p->ref();
    assign(p);
}

WebURL WebDragData::url() const
{
    ASSERT(!isNull());
    return m_private->url;
}

void WebDragData::setURL(const WebURL& url)
{
    ensureMutable();
    m_private->url = url;
}

WebString WebDragData::urlTitle() const
{
    ASSERT(!isNull());
    return m_private->urlTitle;
}

void WebDragData::setURLTitle(const WebString& urlTitle)
{
    ensureMutable();
    m_private->urlTitle = urlTitle;
}

WebString WebDragData::fileExtension() const
{
    ASSERT(!isNull());
    return m_private->fileExtension;
}

void WebDragData::setFileExtension(const WebString& fileExtension)
{
    ensureMutable();
    m_private->fileExtension = fileExtension;
}

bool WebDragData::hasFileNames() const
{
    ASSERT(!isNull());
    return !m_private->filenames.isEmpty();
}

void WebDragData::fileNames(WebVector<WebString>& fileNames) const
{
    ASSERT(!isNull());
    fileNames = m_private->filenames;
}

void WebDragData::setFileNames(const WebVector<WebString>& fileNames)
{
    ensureMutable();
    m_private->filenames.clear();
    m_private->filenames.append(fileNames.data(), fileNames.size());
}

void WebDragData::appendToFileNames(const WebString& fileName)
{
    ensureMutable();
    m_private->filenames.append(fileName);
}

WebString WebDragData::plainText() const
{
    ASSERT(!isNull());
    return m_private->plainText;
}

void WebDragData::setPlainText(const WebString& plainText)
{
    ensureMutable();
    m_private->plainText = plainText;
}

WebString WebDragData::htmlText() const
{
    ASSERT(!isNull());
    return m_private->textHtml;
}

void WebDragData::setHTMLText(const WebString& htmlText)
{
    ensureMutable();
    m_private->textHtml = htmlText;
}

WebURL WebDragData::htmlBaseURL() const
{
    ASSERT(!isNull());
    return m_private->htmlBaseUrl;
}

void WebDragData::setHTMLBaseURL(const WebURL& htmlBaseURL)
{
    ensureMutable();
    m_private->htmlBaseUrl = htmlBaseURL;
}

WebString WebDragData::fileContentFileName() const
{
    ASSERT(!isNull());
    return m_private->fileContentFilename;
}

void WebDragData::setFileContentFileName(const WebString& fileName)
{
    ensureMutable();
    m_private->fileContentFilename = fileName;
}

WebData WebDragData::fileContent() const
{
    ASSERT(!isNull());
    return WebData(m_private->fileContent);
}

void WebDragData::setFileContent(const WebData& fileContent)
{
    ensureMutable();
    m_private->fileContent = fileContent;
}

WebDragData::WebDragData(const WTF::PassRefPtr<WebCore::ChromiumDataObject>& data)
    : m_private(static_cast<WebDragDataPrivate*>(data.releaseRef()))
{
}

WebDragData& WebDragData::operator=(const WTF::PassRefPtr<WebCore::ChromiumDataObject>& data)
{
    assign(static_cast<WebDragDataPrivate*>(data.releaseRef()));
    return *this;
}

WebDragData::operator WTF::PassRefPtr<WebCore::ChromiumDataObject>() const
{
    return PassRefPtr<ChromiumDataObject>(const_cast<WebDragDataPrivate*>(m_private));
}

void WebDragData::assign(WebDragDataPrivate* p)
{
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

void WebDragData::ensureMutable()
{
    ASSERT(!isNull());
    if (!m_private->hasOneRef())
        assign(static_cast<WebDragDataPrivate*>(m_private->copy().releaseRef()));
}

} // namespace WebKit
