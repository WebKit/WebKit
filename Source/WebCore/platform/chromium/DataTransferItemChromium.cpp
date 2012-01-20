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
#include "DataTransferItemChromium.h"

#if ENABLE(DATA_TRANSFER_ITEMS)

#include "Blob.h"
#include "Clipboard.h"
#include "ClipboardChromium.h"
#include "ClipboardMimeTypes.h"
#include "ClipboardUtilitiesChromium.h"
#include "DataTransferItemListChromium.h"
#include "File.h"
#include "PlatformSupport.h"
#include "SharedBuffer.h"
#include "StringCallback.h"

namespace WebCore {

PassRefPtr<DataTransferItemChromium> DataTransferItemChromium::createFromPasteboard(PassRefPtr<Clipboard> owner, ScriptExecutionContext* context, const String& type)
{
    if (type == mimeTypeTextPlain || type == mimeTypeTextHTML)
        return adoptRef(new DataTransferItemChromium(owner, context, PasteboardSource, DataTransferItem::kindString, type, String()));
    return adoptRef(new DataTransferItemChromium(owner, context, PasteboardSource, DataTransferItem::kindFile, type, ""));
}

PassRefPtr<DataTransferItem> DataTransferItem::create(PassRefPtr<Clipboard> owner,
                                                      ScriptExecutionContext* context,
                                                      const String& data,
                                                      const String& type)
{
    return adoptRef(new DataTransferItemChromium(owner, context, DataTransferItemChromium::InternalSource, kindString, type, data));
}

PassRefPtr<DataTransferItem> DataTransferItem::create(PassRefPtr<Clipboard> owner,
                                                      ScriptExecutionContext* context,
                                                      PassRefPtr<File> file)
{
    return adoptRef(new DataTransferItemChromium(owner, context, DataTransferItemChromium::InternalSource, file));
}

DataTransferItemChromium::DataTransferItemChromium(PassRefPtr<Clipboard> owner, ScriptExecutionContext* context, DataSource source, const String& kind, const String& type, const String& data)
    : DataTransferItem(owner, kind, type)
    , m_context(context)
    , m_source(source)
    , m_data(data)
{
}

DataTransferItemChromium::DataTransferItemChromium(PassRefPtr<Clipboard> owner, ScriptExecutionContext* context, DataSource source, PassRefPtr<File> file)
    : DataTransferItem(owner, DataTransferItem::kindFile, file.get() ? file->type() : String())
    , m_context(context)
    , m_source(source)
    , m_file(file)
{
}

void DataTransferItemChromium::getAsString(PassRefPtr<StringCallback> callback)
{
    if ((owner()->policy() != ClipboardReadable && owner()->policy() != ClipboardWritable)
        || kind() != kindString)
        return;

    if (clipboardChromium()->storageHasUpdated())
        return;

    if (m_source == InternalSource) {
        callback->scheduleCallback(m_context, m_data);
        return;
    }

    ASSERT(m_source == PasteboardSource);

    // This is ugly but there's no real alternative.
    if (type() == mimeTypeTextPlain) {
        callback->scheduleCallback(m_context, PlatformSupport::clipboardReadPlainText(currentPasteboardBuffer()));
        return;
    }
    if (type() == mimeTypeTextHTML) {
        String html;
        KURL ignoredSourceURL;
        unsigned ignored;
        PlatformSupport::clipboardReadHTML(currentPasteboardBuffer(), &html, &ignoredSourceURL, &ignored, &ignored);
        callback->scheduleCallback(m_context, html);
        return;
    }
    ASSERT_NOT_REACHED();
}

PassRefPtr<Blob> DataTransferItemChromium::getAsFile()
{
    if (kind() != kindFile || clipboardChromium()->storageHasUpdated())
        return 0;

    if (m_source == InternalSource)
        return m_file;

    ASSERT(m_source == PasteboardSource);
    if (type() == mimeTypeImagePng) {
        // FIXME: This is pretty inefficient. We copy the data from the browser
        // to the renderer. We then place it in a blob in WebKit, which
        // registers it and copies it *back* to the browser. When a consumer
        // wants to read the data, we then copy the data back into the renderer.
        // https://bugs.webkit.org/show_bug.cgi?id=58107 has been filed to track
        // improvements to this code (in particular, add a registerClipboardBlob
        // method to the blob registry; that way the data is only copied over
        // into the renderer when it's actually read, not when the blob is
        // initially constructed).
        RefPtr<SharedBuffer> data = PlatformSupport::clipboardReadImage(PasteboardPrivate::StandardBuffer);
        RefPtr<RawData> rawData = RawData::create();
        rawData->mutableData()->append(data->data(), data->size());
        OwnPtr<BlobData> blobData = BlobData::create();
        blobData->appendData(rawData, 0, -1);
        blobData->setContentType(mimeTypeImagePng);
        return Blob::create(blobData.release(), data->size());
    }
    return 0;
}

ClipboardChromium* DataTransferItemChromium::clipboardChromium()
{
    return static_cast<ClipboardChromium*>(owner());
}

} // namespace WebCore

#endif // ENABLE(DATA_TRANSFER_ITEMS)
