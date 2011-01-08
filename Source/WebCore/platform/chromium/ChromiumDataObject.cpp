/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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
#include "ChromiumDataObject.h"

namespace WebCore {

ChromiumDataObject::ChromiumDataObject(PassRefPtr<ChromiumDataObjectLegacy> data)
    : RefCounted<ChromiumDataObject>()
    , m_legacyData(data)
{
}

ChromiumDataObject::ChromiumDataObject(PassRefPtr<ReadableDataObject> data)
    : RefCounted<ChromiumDataObject>()
    , m_readableData(data)
{
}

ChromiumDataObject::ChromiumDataObject(PassRefPtr<WritableDataObject> data)
    : RefCounted<ChromiumDataObject>()
    , m_writableData(data)
{
}

PassRefPtr<ChromiumDataObject> ChromiumDataObject::create(PassRefPtr<ChromiumDataObjectLegacy> data)
{
    return adoptRef(new ChromiumDataObject(data));
}

PassRefPtr<ChromiumDataObject> ChromiumDataObject::createReadable(Clipboard::ClipboardType clipboardType)
{
    return adoptRef(new ChromiumDataObject(ReadableDataObject::create(clipboardType)));
}

PassRefPtr<ChromiumDataObject> ChromiumDataObject::createWritable(Clipboard::ClipboardType clipboardType)
{
    return adoptRef(new ChromiumDataObject(WritableDataObject::create(clipboardType)));
}

void ChromiumDataObject::clearData(const String& type)
{
    if (m_legacyData)
        m_legacyData->clearData(type);
    else
        m_writableData->clearData(type);
}

void ChromiumDataObject::clearAll()
{
    if (m_legacyData)
        m_legacyData->clearAll();
    else
        m_writableData->clearAll();
}

void ChromiumDataObject::clearAllExceptFiles()
{
    if (m_legacyData)
        m_legacyData->clearAllExceptFiles();
    else
        m_writableData->clearAllExceptFiles();
}

bool ChromiumDataObject::hasData() const
{
    if (m_legacyData)
        return m_legacyData->hasData();
    return m_readableData->hasData();
}

HashSet<String> ChromiumDataObject::types() const
{
    if (m_legacyData)
        return m_legacyData->types();
    return m_readableData->types();
}

String ChromiumDataObject::getData(const String& type, bool& success)
{
    if (m_legacyData)
        return m_legacyData->getData(type, success);
    return m_readableData->getData(type, success);
}

bool ChromiumDataObject::setData(const String& type, const String& data)
{
    if (m_legacyData)
        return m_legacyData->setData(type, data);
    return m_writableData->setData(type, data);
}

String ChromiumDataObject::urlTitle() const
{
    if (m_legacyData)
        return m_legacyData->urlTitle();
    return m_readableData->urlTitle();
}

void ChromiumDataObject::setUrlTitle(const String& urlTitle)
{
    if (m_legacyData)
        m_legacyData->setUrlTitle(urlTitle);
    else
        m_writableData->setUrlTitle(urlTitle);
}

KURL ChromiumDataObject::htmlBaseUrl() const
{
    if (m_legacyData)
        return m_legacyData->htmlBaseUrl();
    return m_readableData->htmlBaseUrl();
}

void ChromiumDataObject::setHtmlBaseUrl(const KURL& url)
{
    if (m_legacyData)
        m_legacyData->setHtmlBaseUrl(url);
    else
        m_writableData->setHtmlBaseUrl(url);
}

bool ChromiumDataObject::containsFilenames() const
{
    if (m_legacyData)
        return m_legacyData->containsFilenames();
    return m_readableData->containsFilenames();
}

Vector<String> ChromiumDataObject::filenames() const
{
    if (m_legacyData)
        return m_legacyData->filenames();
    return m_readableData->filenames();
}

void ChromiumDataObject::setFilenames(const Vector<String>& filenames)
{
    if (m_legacyData)
        m_legacyData->setFilenames(filenames);
    else
        ASSERT_NOT_REACHED();
}

String ChromiumDataObject::fileExtension() const
{
    if (m_legacyData)
        return m_legacyData->fileExtension();
    return m_writableData->fileExtension();
}

void ChromiumDataObject::setFileExtension(const String& fileExtension)
{
    if (m_legacyData)
        m_legacyData->setFileExtension(fileExtension);
    else
        m_writableData->setFileExtension(fileExtension);
}

String ChromiumDataObject::fileContentFilename() const
{
    if (m_legacyData)
        return m_legacyData->fileContentFilename();
    return m_writableData->fileContentFilename();
}

void ChromiumDataObject::setFileContentFilename(const String& fileContentFilename)
{
    if (m_legacyData)
          m_legacyData->setFileContentFilename(fileContentFilename);
    else
          m_writableData->setFileContentFilename(fileContentFilename);
}

PassRefPtr<SharedBuffer> ChromiumDataObject::fileContent() const
{
    if (m_legacyData)
        return m_legacyData->fileContent();
    return m_writableData->fileContent();
}

void ChromiumDataObject::setFileContent(PassRefPtr<SharedBuffer> fileContent)
{
    if (m_legacyData)
        m_legacyData->setFileContent(fileContent);
    else
        m_writableData->setFileContent(fileContent);
}

}

