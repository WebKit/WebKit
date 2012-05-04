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
#include "DataTransferItem.h"
#include "DraggedIsolatedFileSystem.h"
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
    for (size_t i = 0; i < m_private->length(); ++i) {
        ChromiumDataObjectItem* originalItem = m_private->item(i).get();
        WebDragData::Item item;
        if (originalItem->kind() == DataTransferItem::kindString) {
            item.storageType = Item::StorageTypeString;
            item.stringType = originalItem->type();
            item.stringData = originalItem->internalGetAsString();
        } else if (originalItem->kind() == DataTransferItem::kindFile) {
            if (originalItem->sharedBuffer()) {
                item.storageType = Item::StorageTypeBinaryData;
                item.binaryData = originalItem->sharedBuffer();
            } else if (originalItem->isFilename()) {
                item.storageType = Item::StorageTypeFilename;
                RefPtr<WebCore::Blob> blob = originalItem->getAsFile();
                if (blob->isFile()) {
                    File* file = static_cast<File*>(blob.get());
                    item.filenameData = file->path();
                    item.displayNameData = file->name();
                } else
                    ASSERT_NOT_REACHED();
            } else
                ASSERT_NOT_REACHED();
        } else
            ASSERT_NOT_REACHED();
        item.title = originalItem->title();
        item.baseURL = originalItem->baseURL();
        itemList.append(item);
    }
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
    ensureMutable();
    switch (item.storageType) {
    case Item::StorageTypeString:
        if (String(item.stringType) == mimeTypeTextURIList)
            m_private->setURLAndTitle(item.stringData, item.title);
        else if (String(item.stringType) == mimeTypeTextHTML)
            m_private->setHTMLAndBaseURL(item.stringData, item.baseURL);
        else
            m_private->setData(item.stringType, item.stringData);
        return;
    case Item::StorageTypeFilename:
        m_private->addFilename(item.filenameData, item.displayNameData);
        return;
    case Item::StorageTypeBinaryData:
        // This should never happen when dragging in.
        ASSERT_NOT_REACHED();
    }
}

WebString WebDragData::filesystemId() const
{
#if ENABLE(FILE_SYSTEM)
    ASSERT(!isNull());
    DraggedIsolatedFileSystem* filesystem = DraggedIsolatedFileSystem::from(m_private);
    if (filesystem)
        return filesystem->filesystemId();
#endif
    return WebString();
}

void WebDragData::setFilesystemId(const WebString& filesystemId)
{
#if ENABLE(FILE_SYSTEM)
    // The ID is an opaque string, given by and validated by chromium port.
    ensureMutable();
    DraggedIsolatedFileSystem::provideTo(m_private, DraggedIsolatedFileSystem::supplementName(), DraggedIsolatedFileSystem::create(filesystemId));
#endif
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
