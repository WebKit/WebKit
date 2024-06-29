/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(UNIFIED_PDF)

#include "PDFDocumentLayout.h"

namespace WTF {
class TextStream;
}

namespace WebKit {

struct PerPageInfo {
    PDFDocumentLayout::PageIndex pageIndex { 0 };
    WebCore::FloatRect pageBounds;

    bool operator==(const PerPageInfo&) const = default;
};

using PDFPageCoverage = Vector<PerPageInfo>;

struct PDFPageCoverageAndScales {
    PDFPageCoverage pages;
    float deviceScaleFactor { 1 };
    float pdfDocumentScale { 1 };
    float tilingScaleFactor { 1 };

    bool operator==(const PDFPageCoverageAndScales&) const = default;
};

WTF::TextStream& operator<<(WTF::TextStream&, const PerPageInfo&);
WTF::TextStream& operator<<(WTF::TextStream&, const PDFPageCoverageAndScales&);

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
