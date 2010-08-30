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
#include "ExceptionCode.h"
#include "LineEnding.h"
#include "TextEncoding.h"
#include <wtf/text/AtomicString.h>

namespace WebCore {

static CString convertToCString(const String& text, const String& endingType, ExceptionCode& ec)
{
    DEFINE_STATIC_LOCAL(AtomicString, transparent, ("transparent"));
    DEFINE_STATIC_LOCAL(AtomicString, native, ("native"));

    ec = 0;

    if (endingType.isEmpty() || endingType == transparent)
        return UTF8Encoding().encode(text.characters(), text.length(), EntitiesForUnencodables);
    if (endingType == native)
        return normalizeLineEndingsToNative(UTF8Encoding().encode(text.characters(), text.length(), EntitiesForUnencodables));

    ec = SYNTAX_ERR;
    return CString();
}

bool BlobBuilder::append(const String& text, const String& endingType, ExceptionCode& ec)
{
    CString cstr = convertToCString(text, endingType, ec);
    if (ec)
        return false;

    m_items.append(StringBlobItem::create(cstr));
    return true;
}

bool BlobBuilder::append(const String& text, ExceptionCode& ec)
{
    return append(text, String(), ec);
}

bool BlobBuilder::append(PassRefPtr<Blob> blob)
{
    if (blob) {
        for (size_t i = 0; i < blob->items().size(); ++i)
            m_items.append(blob->items()[i]);
        return true;
    }
    return false;
}

PassRefPtr<Blob> BlobBuilder::getBlob(ScriptExecutionContext* scriptExecutionContext, const String& contentType) const
{
    return Blob::create(scriptExecutionContext, contentType, m_items);
}

} // namespace WebCore
