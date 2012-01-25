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
#include "DataTransferItemChromium.h"
#include "ExceptionCode.h"
#include "File.h"
#include "KURL.h"
#include "ScriptExecutionContext.h"
#include "ThreadableBlobRegistry.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

PassRefPtr<DataTransferItemListChromium> DataTransferItemListChromium::create(PassRefPtr<Clipboard> owner, ScriptExecutionContext* context)
{
    return adoptRef(new DataTransferItemListChromium(owner, context));
}

DataTransferItemListChromium::DataTransferItemListChromium(PassRefPtr<Clipboard> owner, ScriptExecutionContext* context)
    : m_owner(owner)
    , m_context(context)
{
}

size_t DataTransferItemListChromium::length() const
{
    clipboardChromium()->mayUpdateItems(m_items);
    return m_items.size();
}

PassRefPtr<DataTransferItem> DataTransferItemListChromium::item(unsigned long index)
{
    clipboardChromium()->mayUpdateItems(m_items);
    if (index >= length())
        return 0;
    return m_items[index];
}

void DataTransferItemListChromium::deleteItem(unsigned long index, ExceptionCode& ec)
{
    clipboardChromium()->mayUpdateItems(m_items);
    if (index >= length())
        return;
    RefPtr<DataTransferItem> item = m_items[index];

    RefPtr<ChromiumDataObject> dataObject = clipboardChromium()->dataObject();
    if (!dataObject)
        return;

    if (item->kind() == DataTransferItem::kindString) {
        m_items.remove(index);
        dataObject->clearData(item->type());
        return;
    }

    ASSERT(item->kind() == DataTransferItem::kindFile);
    RefPtr<Blob> blob = item->getAsFile();
    if (!blob || !blob->isFile())
        return;

    m_items.clear();
    const Vector<String>& filenames = dataObject->filenames();
    Vector<String> copiedFilenames;
    for (size_t i = 0; i < filenames.size(); ++i) {
        if (filenames[i] != static_cast<File*>(blob.get())->path())
            copiedFilenames.append(static_cast<File*>(blob.get())->path());
    }
    dataObject->setFilenames(copiedFilenames);
}

void DataTransferItemListChromium::clear()
{
    m_items.clear();
    clipboardChromium()->clearAllData();
}

void DataTransferItemListChromium::add(const String& data, const String& type, ExceptionCode& ec)
{
    RefPtr<ChromiumDataObject> dataObject = clipboardChromium()->dataObject();
    if (!dataObject)
        return;

    bool success = false;
    dataObject->getData(type, success);
    if (success) {
        // Adding data with the same type should fail with NOT_SUPPORTED_ERR.
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    m_items.clear();
    dataObject->setData(type, data);
}

void DataTransferItemListChromium::add(PassRefPtr<File> file)
{
    if (!file)
        return;

    RefPtr<ChromiumDataObject> dataObject = clipboardChromium()->dataObject();
    if (!dataObject)
        return;

    m_items.clear();

    // Updating dataObject's fileset so that we will be seeing the consistent list even if we reconstruct the items later.
    dataObject->addFilename(file->path());

    // FIXME: Allow multiple files to be dragged out at once if more than one file is added to the storage. For now we manually set up drag-out download URL only if it has not been set yet.
    bool success = false;
    dataObject->getData(mimeTypeDownloadURL, success);
    if (success)
        return;

    KURL urlForDownload = BlobURL::createPublicURL(m_context->securityOrigin());
    ThreadableBlobRegistry::registerBlobURL(urlForDownload, file->url());

    StringBuilder downloadUrl;
    downloadUrl.append(file->type());
    downloadUrl.append(":");
    downloadUrl.append(encodeWithURLEscapeSequences(file->name()));
    downloadUrl.append(":");
    downloadUrl.append(urlForDownload.string());
    dataObject->setData(mimeTypeDownloadURL, downloadUrl.toString());
}

ClipboardChromium* DataTransferItemListChromium::clipboardChromium() const
{
    return static_cast<ClipboardChromium*>(m_owner.get());
}

} // namespace WebCore

#endif // ENABLE(DATA_TRANSFER_ITEMS)
