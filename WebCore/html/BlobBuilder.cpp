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

#if ENABLE(FILE_WRITER)

#include "BlobBuilder.h"

#include "AtomicString.h"
#include "Blob.h"
#include "TextEncoding.h"
#include <wtf/text/CString.h>

namespace WebCore {

static bool getLineEndingTypeFromString(const AtomicString& typeString, LineEnding& endingType)
{
    DEFINE_STATIC_LOCAL(AtomicString, transparent, ("transparent"));
    DEFINE_STATIC_LOCAL(AtomicString, native, ("native"));

    if (typeString.isEmpty() || typeString == transparent) {
        endingType = EndingTransparent;
        return true;
    }
    if (typeString == native) {
        endingType = EndingNative;
        return true;
    }
    return false;
}

bool BlobBuilder::appendString(const String& text, const String& type, ExceptionCode& ec)
{
    ec = 0;
    LineEnding endingType;
    if (!getLineEndingTypeFromString(type, endingType)) {
        ec = SYNTAX_ERR;
        return false;
    }
    m_items.append(StringBlobItem::create(text, endingType, UTF8Encoding()));
    return true;
}

bool BlobBuilder::appendBlob(PassRefPtr<Blob> blob)
{
    if (blob) {
        for (size_t i = 0; i < blob->items().size(); ++i)
            m_items.append(blob->items()[i]);
        return true;
    }
    return false;
}

PassRefPtr<Blob> BlobBuilder::getBlob(const String& contentType) const
{
    return Blob::create(contentType, m_items);
}

} // namespace WebCore

#endif // ENABLE(FILE_WRITER)
