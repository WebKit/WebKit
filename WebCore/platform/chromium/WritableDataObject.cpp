/*
 * Copyright (c) 2010 Google Inc. All rights reserved.
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
#include "WritableDataObject.h"

#include "ChromiumBridge.h"
#include "ClipboardMimeTypes.h"

namespace WebCore {

PassRefPtr<WritableDataObject> WritableDataObject::create(bool isForDragging)
{
    return adoptRef(new WritableDataObject(isForDragging));
}

WritableDataObject::WritableDataObject(bool isForDragging)
    : m_isForDragging(isForDragging)
{
}

void WritableDataObject::clearData(const String& type)
{
    m_dataMap.remove(type);
    if (type == mimeTypeTextURIList)
        m_urlTitle = "";
    else if (type == mimeTypeTextHTML)
        m_htmlBaseURL = KURL();
}

void WritableDataObject::clearAllExceptFiles()
{
    // FIXME: The spec does not provide a way to populate FileList currently. In
    // fact, the spec explicitly states that dragging files can only happen from
    // outside a browsing context.
    clearAll();
}

void WritableDataObject::clearAll()
{
    m_dataMap.clear();
    m_urlTitle = "";
    m_htmlBaseURL = KURL();
    m_fileContentFilename = "";
    if (m_fileContent)
        m_fileContent->clear();
    m_fileExtension = "";
}

bool WritableDataObject::setData(const String& type, const String& data)
{
    if (!m_isForDragging) {
        ChromiumBridge::clipboardWriteData(type, data, "");
        return true;
    }
    m_dataMap.set(type, data);
    if (type == mimeTypeTextURIList)
        m_urlTitle = "";
    else if (type == mimeTypeTextHTML)
        m_htmlBaseURL = KURL();
    return true;
}

void WritableDataObject::setURL(const String& url, const String& title)
{
    setData(mimeTypeTextURIList, url);
    m_urlTitle = title;
}

void WritableDataObject::setHTML(const String& html, const KURL& baseURL)
{
    setData(mimeTypeTextHTML, html);
    m_htmlBaseURL = baseURL;
}

// Accessors used when transferring drag data from the renderer to the
// browser.
HashMap<String, String> WritableDataObject::dataMap() const
{
    return m_dataMap;
}

String WritableDataObject::urlTitle() const
{
    return m_urlTitle;
}

KURL WritableDataObject::htmlBaseURL() const
{
    return m_htmlBaseURL;
}

// Used for transferring file data from the renderer to the browser.
String WritableDataObject::fileExtension() const
{
    return m_fileExtension;
}

String WritableDataObject::fileContentFilename() const
{
    return m_fileContentFilename;
}

PassRefPtr<SharedBuffer> WritableDataObject::fileContent() const
{
    return m_fileContent;
}

void WritableDataObject::setFileExtension(const String& fileExtension)
{
    m_fileExtension = fileExtension;
}

void WritableDataObject::setFileContentFilename(const String& fileContentFilename)
{
    m_fileContentFilename = fileContentFilename;
}

void WritableDataObject::setFileContent(PassRefPtr<SharedBuffer> fileContent)
{
    m_fileContent = fileContent;
}


} // namespace WebCore
