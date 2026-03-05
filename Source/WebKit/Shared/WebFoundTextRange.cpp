/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebFoundTextRange.h"

#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>

namespace WebKit {

unsigned WebFoundTextRange::PDFData::hash() const
{
    return pairIntHash(pairIntHash(pairIntHash(startPage, endPage), startOffset), endOffset);
}

unsigned WebFoundTextRange::hash() const
{
    return WTF::switchOn(data,
        [] (const WebFoundTextRange::DOMData& domData) {
            return pairIntHash(domData.location, domData.length);
        },
        [] (const WebFoundTextRange::PDFData& pdfData) {
            return pdfData.hash();
        }
    );
}

bool WebFoundTextRange::operator==(const WebFoundTextRange& other) const
{
    if (pathToFrame.isHashTableDeletedValue())
        return other.pathToFrame.isHashTableDeletedValue();
    if (other.pathToFrame.isHashTableDeletedValue())
        return false;

    return data == other.data
        && pathToFrame == other.pathToFrame
        && order == other.order;
}

TextStream& operator<<(TextStream& ts, const WebFoundTextRange& range)
{
    WTF::switchOn(range.data,
        [&] (const WebFoundTextRange::DOMData& domData) {
            ts << "WebFoundTextRange"_s;
            ts.dumpProperty("DOMData"_s, domData);
        },
        [&] (const WebFoundTextRange::PDFData& pdfData) {
            ts << "WebFoundTextRange"_s;
            ts.dumpProperty("PDFData"_s, pdfData);
        }
    );
    ts.dumpProperty("order"_s, range.order);
    ts.dumpProperty("pathToFrame"_s, range.pathToFrame);
    return ts;
}

TextStream& operator<<(TextStream& ts, const WebFoundTextRange::DOMData& data)
{
    ts << "[location: " << data.location << ", length: " << data.length << "]";
    return ts;
}

TextStream& operator<<(TextStream& ts, const WebFoundTextRange::PDFData& data)
{
    ts << "[start page: " << data.startPage << ", start offset: " << data.startOffset << ", end page: " << data.endPage << ", end offset: " << data.endOffset << "]";
    return ts;
}

} // namespace WebKit
