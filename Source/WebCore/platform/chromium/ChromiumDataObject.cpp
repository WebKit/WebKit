/*
 * Copyright (c) 2008, 2009, 2012 Google Inc. All rights reserved.
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
#include "ExceptionCode.h"
#include "ExceptionCodePlaceholder.h"
#include "PlatformSupport.h"

namespace WebCore {

PassRefPtr<ChromiumDataObject> ChromiumDataObject::createFromPasteboard()
{
    RefPtr<ChromiumDataObject> dataObject = create();
    uint64_t sequenceNumber = PlatformSupport::clipboardSequenceNumber(currentPasteboardBuffer());
    bool ignored;
    HashSet<String> types = PlatformSupport::clipboardReadAvailableTypes(currentPasteboardBuffer(), &ignored);
    for (HashSet<String>::const_iterator it = types.begin(); it != types.end(); ++it)
        dataObject->m_itemList.append(ChromiumDataObjectItem::createFromPasteboard(*it, sequenceNumber));
    return dataObject.release();
}

PassRefPtr<ChromiumDataObject> ChromiumDataObject::create()
{
    return adoptRef(new ChromiumDataObject());
}

PassRefPtr<ChromiumDataObject> ChromiumDataObject::copy() const
{
    return adoptRef(new ChromiumDataObject(*this));
}

size_t ChromiumDataObject::length() const
{
    return m_itemList.size();
}

PassRefPtr<ChromiumDataObjectItem> ChromiumDataObject::item(unsigned long index)
{
    if (index >= length())
        return 0;
    return m_itemList[index];
}

void ChromiumDataObject::deleteItem(unsigned long index)
{
    if (index >= length())
        return;
    m_itemList.remove(index);
}

void ChromiumDataObject::clearAll()
{
    m_itemList.clear();
}

void ChromiumDataObject::add(const String& data, const String& type, ExceptionCode& ec)
{
    if (!internalAddStringItem(ChromiumDataObjectItem::createFromString(type, data)))
        ec = NOT_SUPPORTED_ERR;
}

void ChromiumDataObject::add(PassRefPtr<File> file, ScriptExecutionContext* context)
{
    if (!file)
        return;

    m_itemList.append(ChromiumDataObjectItem::createFromFile(file));
}

void ChromiumDataObject::clearData(const String& type)
{
    for (size_t i = 0; i < m_itemList.size(); ++i) {
        if (m_itemList[i]->kind() == DataTransferItem::kindString && m_itemList[i]->type() == type) {
            // Per the spec, type must be unique among all items of kind 'string'.
            m_itemList.remove(i);
            return;
        }
    }
}

void ChromiumDataObject::clearAllExceptFiles()
{
    for (size_t i = 0; i < m_itemList.size(); ) {
        if (m_itemList[i]->kind() != DataTransferItem::kindFile) {
            m_itemList.remove(i);
            continue;
        }
        ++i;
    }
}

HashSet<String> ChromiumDataObject::types() const
{
    HashSet<String> results;
    bool containsFiles = false;
    for (size_t i = 0; i < m_itemList.size(); ++i) {
        if (m_itemList[i]->kind() == DataTransferItem::kindString)
            results.add(m_itemList[i]->type());
        else if (m_itemList[i]->kind() == DataTransferItem::kindFile)
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
    for (size_t i = 0; i < m_itemList.size(); ++i)  {
        if (m_itemList[i]->kind() == DataTransferItem::kindString && m_itemList[i]->type() == type)
            return m_itemList[i]->internalGetAsString();
    }
    return String();
}

bool ChromiumDataObject::setData(const String& type, const String& data)
{
    clearData(type);
    add(data, type, ASSERT_NO_EXCEPTION);
    return true;
}

void ChromiumDataObject::urlAndTitle(String& url, String* title) const
{
    RefPtr<ChromiumDataObjectItem> item = findStringItem(mimeTypeTextURIList);
    if (!item)
        return;
    url = convertURIListToURL(item->internalGetAsString());
    if (title)
        *title = item->title();
}

void ChromiumDataObject::setURLAndTitle(const String& url, const String& title)
{
    clearData(mimeTypeTextURIList);
    internalAddStringItem(ChromiumDataObjectItem::createFromURL(url, title));
}

void ChromiumDataObject::htmlAndBaseURL(String& html, KURL& baseURL) const
{
    RefPtr<ChromiumDataObjectItem> item = findStringItem(mimeTypeTextHTML);
    if (!item)
        return;
    html = item->internalGetAsString();
    baseURL = item->baseURL();
}

void ChromiumDataObject::setHTMLAndBaseURL(const String& html, const KURL& baseURL)
{
    clearData(mimeTypeTextHTML);
    internalAddStringItem(ChromiumDataObjectItem::createFromHTML(html, baseURL));
}

bool ChromiumDataObject::containsFilenames() const
{
    for (size_t i = 0; i < m_itemList.size(); ++i)
        if (m_itemList[i]->isFilename())
            return true;
    return false;
}

Vector<String> ChromiumDataObject::filenames() const
{
    Vector<String> results;
    for (size_t i = 0; i < m_itemList.size(); ++i)
        if (m_itemList[i]->isFilename())
            results.append(static_cast<File*>(m_itemList[i]->getAsFile().get())->path());
    return results;
}

void ChromiumDataObject::addFilename(const String& filename, const String& displayName)
{
    internalAddFileItem(ChromiumDataObjectItem::createFromFile(File::createWithName(filename, displayName)));
}

void ChromiumDataObject::addSharedBuffer(const String& name, PassRefPtr<SharedBuffer> buffer)
{
    internalAddFileItem(ChromiumDataObjectItem::createFromSharedBuffer(name, buffer));
}

ChromiumDataObject::ChromiumDataObject()
{
}

ChromiumDataObject::ChromiumDataObject(const ChromiumDataObject& other)
    : RefCounted<ChromiumDataObject>()
    , m_itemList(other.m_itemList)
{
}

PassRefPtr<ChromiumDataObjectItem> ChromiumDataObject::findStringItem(const String& type) const
{
    for (size_t i = 0; i < m_itemList.size(); ++i) {
        if (m_itemList[i]->kind() == DataTransferItem::kindString && m_itemList[i]->type() == type)
            return m_itemList[i];
    }
    return 0;
}

bool ChromiumDataObject::internalAddStringItem(PassRefPtr<ChromiumDataObjectItem> item)
{
    ASSERT(item->kind() == DataTransferItem::kindString);
    for (size_t i = 0; i < m_itemList.size(); ++i)
        if (m_itemList[i]->kind() == DataTransferItem::kindString && m_itemList[i]->type() == item->type())
            return false;

    m_itemList.append(item);
    return true;
}

void ChromiumDataObject::internalAddFileItem(PassRefPtr<ChromiumDataObjectItem> item)
{
    ASSERT(item->kind() == DataTransferItem::kindFile);
    m_itemList.append(item);
}

} // namespace WebCore
