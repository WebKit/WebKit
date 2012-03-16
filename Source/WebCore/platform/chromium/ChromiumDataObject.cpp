/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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

#include "ClipboardMimeTypes.h"
#include "ClipboardUtilitiesChromium.h"
#include "DataTransferItem.h"
#include "DataTransferItemListChromium.h"
#include "ExceptionCodePlaceholder.h"

namespace WebCore {

static PassRefPtr<DataTransferItemChromium> findItem(PassRefPtr<DataTransferItemListChromium> itemList, const String& type)
{
    for (size_t i = 0; i < itemList->length(); ++i) {
        if (itemList->item(i)->kind() == DataTransferItem::kindString && itemList->item(i)->type() == type)
            return itemList->item(i);
    }
    return 0;
}

PassRefPtr<ChromiumDataObject> ChromiumDataObject::createFromPasteboard()
{
    return adoptRef(new ChromiumDataObject(DataTransferItemListChromium::createFromPasteboard()));
}

PassRefPtr<ChromiumDataObject> ChromiumDataObject::create()
{
    return adoptRef(new ChromiumDataObject(DataTransferItemListChromium::create()));
}

PassRefPtr<ChromiumDataObject> ChromiumDataObject::copy() const
{
    return adoptRef(new ChromiumDataObject(*this));
}

PassRefPtr<DataTransferItemListChromium> ChromiumDataObject::items() const
{
    return m_itemList;
}

void ChromiumDataObject::clearData(const String& type)
{
    for (size_t i = 0; i < m_itemList->length(); ++i) {
        if (m_itemList->item(i)->kind() == DataTransferItem::kindString && m_itemList->item(i)->type() == type) {
            // Per the spec, type must be unique among all items of kind 'string'.
            m_itemList->deleteItem(i);
            return;
        }
    }
}

void ChromiumDataObject::clearAll()
{
    m_itemList->clear();
}

void ChromiumDataObject::clearAllExceptFiles()
{
    for (size_t i = 0; i < m_itemList->length(); ) {
        if (m_itemList->item(i)->kind() != DataTransferItem::kindFile) {
            m_itemList->deleteItem(i);
            continue;
        }
        ++i;
    }
}

HashSet<String> ChromiumDataObject::types() const
{
    HashSet<String> results;
    bool containsFiles = false;
    for (size_t i = 0; i < m_itemList->length(); ++i) {
        if (m_itemList->item(i)->kind() == DataTransferItem::kindString)
            results.add(m_itemList->item(i)->type());
        else if (m_itemList->item(i)->kind() == DataTransferItem::kindFile)
            containsFiles = true;
        else
            ASSERT_NOT_REACHED();
    }
    if (containsFiles)
        results.add(mimeTypeFiles);
    return results;
}

String ChromiumDataObject::getData(const String& type) const
{
    for (size_t i = 0; i < m_itemList->length(); ++i)  {
        if (m_itemList->item(i)->kind() == DataTransferItem::kindString && m_itemList->item(i)->type() == type)
            return m_itemList->item(i)->internalGetAsString();
    }
    return String();
}

bool ChromiumDataObject::setData(const String& type, const String& data)
{
    clearData(type);
    m_itemList->add(data, type, ASSERT_NO_EXCEPTION);
    return true;
}

void ChromiumDataObject::urlAndTitle(String& url, String* title) const
{
    RefPtr<DataTransferItemChromium> item = findItem(m_itemList, mimeTypeTextURIList);
    if (!item)
        return;
    url = convertURIListToURL(item->internalGetAsString());
    if (title)
        *title = item->title();
}

void ChromiumDataObject::setURLAndTitle(const String& url, const String& title)
{
    clearData(mimeTypeTextURIList);
    m_itemList->internalAddStringItem(DataTransferItemChromium::createFromURL(url, title));
}

void ChromiumDataObject::htmlAndBaseURL(String& html, KURL& baseURL) const
{
    RefPtr<DataTransferItemChromium> item = findItem(m_itemList, mimeTypeTextHTML);
    if (!item)
        return;
    html = item->internalGetAsString();
    baseURL = item->baseURL();
}

void ChromiumDataObject::setHTMLAndBaseURL(const String& html, const KURL& baseURL)
{
    clearData(mimeTypeTextHTML);
    m_itemList->internalAddStringItem(DataTransferItemChromium::createFromHTML(html, baseURL));
}

bool ChromiumDataObject::containsFilenames() const
{
    for (size_t i = 0; i < m_itemList->length(); ++i)
        if (m_itemList->item(i)->isFilename())
            return true;
    return false;
}

Vector<String> ChromiumDataObject::filenames() const
{
    Vector<String> results;
    for (size_t i = 0; i < m_itemList->length(); ++i)
        if (m_itemList->item(i)->isFilename())
            results.append(static_cast<File*>(m_itemList->item(i)->getAsFile().get())->path());
    return results;
}

void ChromiumDataObject::addFilename(const String& filename)
{
    m_itemList->internalAddFileItem(DataTransferItemChromium::createFromFile(File::create(filename)));
}

void ChromiumDataObject::addSharedBuffer(const String& name, PassRefPtr<SharedBuffer> buffer)
{
    m_itemList->internalAddFileItem(DataTransferItemChromium::createFromSharedBuffer(name, buffer));
}

ChromiumDataObject::ChromiumDataObject(PassRefPtr<DataTransferItemListChromium> itemList)
    : m_itemList(itemList)
{
}

ChromiumDataObject::ChromiumDataObject(const ChromiumDataObject& other)
    : RefCounted<ChromiumDataObject>()
    , m_itemList(other.m_itemList)
{
}

} // namespace WebCore
