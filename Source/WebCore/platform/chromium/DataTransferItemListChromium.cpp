/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "DataTransferItemListChromium.h"

#if ENABLE(DATA_TRANSFER_ITEMS)

#include "BlobURL.h"
#include "ChromiumDataObject.h"
#include "ClipboardChromium.h"
#include "ClipboardMimeTypes.h"
#include "ClipboardUtilitiesChromium.h"
#include "DataTransferItem.h"
#include "DataTransferItemChromium.h"
#include "ExceptionCode.h"
#include "File.h"
#include "KURL.h"
#include "PlatformSupport.h"
#include "ScriptExecutionContext.h"
#include "ThreadableBlobRegistry.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

PassRefPtr<DataTransferItemListChromium> DataTransferItemListChromium::create()
{
    return adoptRef(new DataTransferItemListChromium());
}

PassRefPtr<DataTransferItemListChromium> DataTransferItemListChromium::createFromPasteboard()
{
    RefPtr<DataTransferItemListChromium> list = create();
    uint64_t sequenceNumber = PlatformSupport::clipboardSequenceNumber(currentPasteboardBuffer());
    bool ignored;
    HashSet<String> types = PlatformSupport::clipboardReadAvailableTypes(currentPasteboardBuffer(), &ignored);
    for (HashSet<String>::const_iterator it = types.begin(); it != types.end(); ++it)
        list->m_itemList.append(DataTransferItemChromium::createFromPasteboard(*it, sequenceNumber));
    return list.release();
}

DataTransferItemListChromium::DataTransferItemListChromium()
{
}

size_t DataTransferItemListChromium::length() const
{
    return m_itemList.size();
}

PassRefPtr<DataTransferItemChromium> DataTransferItemListChromium::item(unsigned long index)
{
    if (index >= length())
        return 0;
    return m_itemList[index];
}

void DataTransferItemListChromium::deleteItem(unsigned long index)
{
    if (index >= length())
        return;
    m_itemList.remove(index);
}

void DataTransferItemListChromium::clear()
{
    m_itemList.clear();
}

void DataTransferItemListChromium::add(const String& data, const String& type, ExceptionCode& ec)
{
    if (!internalAddStringItem(DataTransferItemChromium::createFromString(type, data)))
        ec = NOT_SUPPORTED_ERR;
}

void DataTransferItemListChromium::add(PassRefPtr<File> file, ScriptExecutionContext* context)
{
    if (!file)
        return;

    m_itemList.append(DataTransferItemChromium::createFromFile(file));

    // FIXME: Allow multiple files to be dragged out at once if more than one file is added to the storage.
    KURL urlForDownload = BlobURL::createPublicURL(context->securityOrigin());
    ThreadableBlobRegistry::registerBlobURL(urlForDownload, file->url());

    StringBuilder downloadUrl;
    downloadUrl.append(file->type());
    downloadUrl.append(":");
    downloadUrl.append(encodeWithURLEscapeSequences(file->name()));
    downloadUrl.append(":");
    downloadUrl.append(urlForDownload.string());

    internalAddStringItem(DataTransferItemChromium::createFromString(mimeTypeDownloadURL, downloadUrl.toString()));
}

// FIXME: Make sure item is released correctly in case of failure.
bool DataTransferItemListChromium::internalAddStringItem(PassRefPtr<DataTransferItemChromium> item)
{
    ASSERT(item->kind() == DataTransferItem::kindString);
    for (size_t i = 0; i < m_itemList.size(); ++i)
        if (m_itemList[i]->kind() == DataTransferItem::kindString && m_itemList[i]->type() == item->type())
            return false;

    m_itemList.append(item);
    return true;
}

void DataTransferItemListChromium::internalAddFileItem(PassRefPtr<DataTransferItemChromium> item)
{
    ASSERT(item->kind() == DataTransferItem::kindFile);
    m_itemList.append(item);
}

} // namespace WebCore

#endif // ENABLE(DATA_TRANSFER_ITEMS)
