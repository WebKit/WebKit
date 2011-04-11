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

#include "Clipboard.h"
#include "ClipboardMimeTypes.h"
#include "PlatformBridge.h"
#include "StringCallback.h"

namespace WebCore {

PassRefPtr<DataTransferItemChromium> DataTransferItemChromium::createFromPasteboard(PassRefPtr<Clipboard> owner, ScriptExecutionContext* context, const String& type)
{
    if (type == mimeTypeTextPlain || type == mimeTypeTextHTML)
        return adoptRef(new DataTransferItemChromium(owner, context, PasteboardSource, DataTransferItem::kindString, type, ""));
    return adoptRef(new DataTransferItemChromium(owner, context, PasteboardSource, DataTransferItem::kindFile, type, ""));
}

PassRefPtr<DataTransferItemChromium> DataTransferItemChromium::create(PassRefPtr<Clipboard> owner,
                                                                      ScriptExecutionContext* context,
                                                                      const String& data,
                                                                      const String& type)
{
    return adoptRef(new DataTransferItemChromium(owner, context, InternalSource, DataTransferItem::kindString, type, data));
}

DataTransferItemChromium::DataTransferItemChromium(PassRefPtr<Clipboard> owner, ScriptExecutionContext* context, DataSource source, const String& kind, const String& type, const String& data)
    : m_owner(owner)
    , m_context(context)
    , m_source(source)
    , m_kind(kind)
    , m_type(type)
    , m_data(data)
{
}

String DataTransferItemChromium::kind() const
{
    if (m_owner->policy() == ClipboardNumb)
        return String();
    return m_kind;
}

String DataTransferItemChromium::type() const
{
    if (m_owner->policy() == ClipboardNumb)
        return String();
    return m_type;
}

void DataTransferItemChromium::getAsString(PassRefPtr<StringCallback> callback)
{
    if ((m_owner->policy() != ClipboardReadable && m_owner->policy() != ClipboardWritable)
        || m_kind != kindString)
        return;
    if (m_source == InternalSource) {
        callback->scheduleCallback(m_context, m_data);
        return;
    }
    // This is ugly but there's no real alternative.
    if (m_type == mimeTypeTextPlain) {
        callback->scheduleCallback(m_context, PlatformBridge::clipboardReadPlainText(PasteboardPrivate::StandardBuffer));
        return;
    }
    if (m_type == mimeTypeTextHTML) {
        String html;
        KURL ignoredSourceURL;
        PlatformBridge::clipboardReadHTML(PasteboardPrivate::StandardBuffer, &html, &ignoredSourceURL);
        callback->scheduleCallback(m_context, html);
        return;
    }
    ASSERT_NOT_REACHED();
}

} // namespace WebCore

#endif // ENABLE(DATA_TRANSFER_ITEMS)
