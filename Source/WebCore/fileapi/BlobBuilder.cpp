/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "BlobBuilder.h"

#include "Blob.h"
#include "LineEnding.h"
#include "TextEncoding.h"
#include <runtime/ArrayBuffer.h>
#include <runtime/ArrayBufferView.h>
#include <wtf/text/CString.h>

namespace WebCore {

BlobBuilder::BlobBuilder(BlobLineEndings endings)
    : m_endings(endings)
{
}

void BlobBuilder::append(RefPtr<ArrayBuffer>&& arrayBuffer)
{
    if (!arrayBuffer)
        return;
    m_appendableData.append(static_cast<const char*>(arrayBuffer->data()), arrayBuffer->byteLength());
}

void BlobBuilder::append(RefPtr<ArrayBufferView>&& arrayBufferView)
{
    if (!arrayBufferView)
        return;
    m_appendableData.append(static_cast<const char*>(arrayBufferView->baseAddress()), arrayBufferView->byteLength());
}

void BlobBuilder::append(RefPtr<Blob>&& blob)
{
    if (!blob)
        return;
    if (!m_appendableData.isEmpty())
        m_items.append(BlobPart(WTFMove(m_appendableData)));
    m_items.append(BlobPart(blob->url()));
}

void BlobBuilder::append(const String& text)
{
    CString utf8Text = UTF8Encoding().encode(text, EntitiesForUnencodables);

    if (m_endings == BlobLineEndings::Native)
        normalizeLineEndingsToNative(utf8Text, m_appendableData);
    else {
        ASSERT(m_endings == BlobLineEndings::Transparent);
        m_appendableData.append(utf8Text.data(), utf8Text.length());
    }
}

Vector<BlobPart> BlobBuilder::finalize()
{
    if (!m_appendableData.isEmpty())
        m_items.append(BlobPart(WTFMove(m_appendableData)));
    return WTFMove(m_items);
}

} // namespace WebCore
