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
#include "platform/WebDragData.h"

#include "ChromiumDataObject.h"
#include "ClipboardMimeTypes.h"
#include "platform/WebData.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include "platform/WebVector.h"

#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>

using namespace WebCore;

namespace WebKit {

class WebDragDataPrivate : public ChromiumDataObject {
};

void WebDragData::initialize()
{
    assign(static_cast<WebDragDataPrivate*>(ChromiumDataObject::create().leakRef()));
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

WebVector<WebDragData::Item> WebDragData::items() const
{
    Vector<Item> itemList;
    RefPtr<DOMStringList> types = m_private->types();
    if (types->contains(mimeTypeTextPlain)) {
        Item item;
        item.storageType = Item::StorageTypeString;
        item.stringType = String(mimeTypeTextPlain);
        bool ignored;
        item.stringData = m_private->getData(mimeTypeTextPlain, ignored);
        itemList.append(item);
    }
    if (types->contains(mimeTypeTextURIList)) {
        Item item;
        item.storageType = Item::StorageTypeString;
        item.stringType = String(mimeTypeTextURIList);
        bool ignored;
        item.stringData = m_private->getData(mimeTypeURL, ignored);
        item.title = m_private->urlTitle();
        itemList.append(item);
    }
    if (types->contains(mimeTypeTextHTML)) {
        Item item;
        item.storageType = Item::StorageTypeString;
        item.stringType = String(mimeTypeTextHTML);
        bool ignored;
        item.stringData = m_private->getData(mimeTypeTextHTML, ignored);
        item.baseURL = m_private->htmlBaseUrl();
        itemList.append(item);
    }
    if (types->contains(mimeTypeDownloadURL)) {
        Item item;
        item.storageType = Item::StorageTypeString;
        item.stringType = String(mimeTypeDownloadURL);
        bool ignored;
        item.stringData = m_private->getData(mimeTypeDownloadURL, ignored);
        itemList.append(item);
    }
    const HashMap<String, String>& customData = m_private->customData();
    for (HashMap<String, String>::const_iterator it = customData.begin(); it != customData.end(); ++it) {
        Item item;
        item.storageType = Item::StorageTypeString;
        item.stringType = it->first;
        item.stringData = it->second;
        itemList.append(item);
    }
    if (m_private->fileContent()) {
        Item item;
        item.storageType = Item::StorageTypeBinaryData;
        item.binaryData = m_private->fileContent();
        item.title = m_private->fileContentFilename();
        itemList.append(item);
    }
    // We don't handle filenames here, since they are never used for dragging out.
    return itemList;
}

void WebDragData::setItems(const WebVector<Item>& itemList)
{
    m_private->clearAll();
    for (size_t i = 0; i < itemList.size(); ++i)
        addItem(itemList[i]);
}

void WebDragData::addItem(const Item& item)
{
    switch (item.storageType) {
    case Item::StorageTypeString:
        m_private->setData(item.stringType, item.stringData);
        if (String(item.stringType) == mimeTypeTextURIList)
            m_private->setUrlTitle(item.title);
        else if (String(item.stringType) == mimeTypeTextHTML)
            m_private->setHtmlBaseUrl(item.baseURL);
        return;
    case Item::StorageTypeFilename:
        m_private->addFilename(item.filenameData);
        return;
    case Item::StorageTypeBinaryData:
        // This should never happen when dragging in.
        ASSERT_NOT_REACHED();
    }
}

WebString WebDragData::url() const
{
    ASSERT(!isNull());
    bool ignoredSuccess;
    return m_private->getData(mimeTypeURL, ignoredSuccess);
}

void WebDragData::setURL(const WebURL& url)
{
    ensureMutable();
    m_private->setData(mimeTypeURL, KURL(url).string());
}

WebString WebDragData::urlTitle() const
{
    ASSERT(!isNull());
    return m_private->urlTitle();
}

void WebDragData::setURLTitle(const WebString& urlTitle)
{
    ensureMutable();
    m_private->setUrlTitle(urlTitle);
}

WebString WebDragData::downloadMetadata() const
{
    ASSERT(!isNull());
    bool ignoredSuccess;
    return m_private->getData(mimeTypeDownloadURL, ignoredSuccess);
}

void WebDragData::setDownloadMetadata(const WebString& downloadMetadata)
{
    ensureMutable();
    m_private->setData(mimeTypeDownloadURL, downloadMetadata);
}

WebString WebDragData::fileExtension() const
{
    ASSERT(!isNull());
    return m_private->fileExtension();
}

void WebDragData::setFileExtension(const WebString& fileExtension)
{
    ensureMutable();
    m_private->setFileExtension(fileExtension);
}

bool WebDragData::containsFilenames() const
{
    ASSERT(!isNull());
    return m_private->containsFilenames();
}

void WebDragData::filenames(WebVector<WebString>& filenames) const
{
    ASSERT(!isNull());
    filenames = m_private->filenames();
}

void WebDragData::setFilenames(const WebVector<WebString>& filenames)
{
    ensureMutable();
    Vector<String> filenamesCopy;
    filenamesCopy.append(filenames.data(), filenames.size());
    m_private->setFilenames(filenamesCopy);
}

void WebDragData::appendToFilenames(const WebString& filename)
{
    ensureMutable();
    Vector<String> filenames = m_private->filenames();
    filenames.append(filename);
    m_private->setFilenames(filenames);
}

WebString WebDragData::plainText() const
{
    ASSERT(!isNull());
    bool ignoredSuccess;
    return m_private->getData(mimeTypeTextPlain, ignoredSuccess);
}

void WebDragData::setPlainText(const WebString& plainText)
{
    ensureMutable();
    m_private->setData(mimeTypeTextPlain, plainText);
}

WebString WebDragData::htmlText() const
{
    ASSERT(!isNull());
    bool ignoredSuccess;
    return m_private->getData(mimeTypeTextHTML, ignoredSuccess);
}

void WebDragData::setHTMLText(const WebString& htmlText)
{
    ensureMutable();
    m_private->setData(mimeTypeTextHTML, htmlText);
}

WebURL WebDragData::htmlBaseURL() const
{
    ASSERT(!isNull());
    return m_private->htmlBaseUrl();
}

void WebDragData::setHTMLBaseURL(const WebURL& htmlBaseURL)
{
    ensureMutable();
    m_private->setHtmlBaseUrl(htmlBaseURL);
}

WebString WebDragData::fileContentFilename() const
{
    ASSERT(!isNull());
    return m_private->fileContentFilename();
}

void WebDragData::setFileContentFilename(const WebString& filename)
{
    ensureMutable();
    m_private->setFileContentFilename(filename);
}

WebData WebDragData::fileContent() const
{
    ASSERT(!isNull());
    return WebData(m_private->fileContent());
}

void WebDragData::setFileContent(const WebData& fileContent)
{
    ensureMutable();
    m_private->setFileContent(fileContent);
}

WebVector<WebDragData::CustomData> WebDragData::customData() const
{
    ASSERT(!isNull());
    WebVector<CustomData> customData(static_cast<size_t>(m_private->customData().size()));
    HashMap<String, String>::const_iterator begin = m_private->customData().begin();
    HashMap<String, String>::const_iterator end = m_private->customData().end();
    size_t i = 0;
    for (HashMap<String, String>::const_iterator it = begin; it != end; ++it) {
        CustomData data = {it->first, it->second};
        customData[i++] = data;
    }
    return customData;
}

void WebDragData::setCustomData(const WebVector<WebDragData::CustomData>& customData)
{
    ensureMutable();
    HashMap<String, String>& customDataMap = m_private->customData();
    for (size_t i = 0; i < customData.size(); ++i)
        customDataMap.set(customData[i].type, customData[i].data);
}

WebDragData::WebDragData(const WTF::PassRefPtr<WebCore::ChromiumDataObject>& data)
    : m_private(static_cast<WebDragDataPrivate*>(data.leakRef()))
{
}

WebDragData& WebDragData::operator=(const WTF::PassRefPtr<WebCore::ChromiumDataObject>& data)
{
    assign(static_cast<WebDragDataPrivate*>(data.leakRef()));
    return *this;
}

WebDragData::operator WTF::PassRefPtr<WebCore::ChromiumDataObject>() const
{
    return PassRefPtr<ChromiumDataObject>(const_cast<WebDragDataPrivate*>(m_private));
}

void WebDragData::assign(WebDragDataPrivate* p)
{
    ASSERT(!p || p->storageMode() == ChromiumDataObject::Buffered);
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

void WebDragData::ensureMutable()
{
    ASSERT(!isNull());
    ASSERT(m_private->hasOneRef());
}

} // namespace WebKit
