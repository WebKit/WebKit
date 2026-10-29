/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#include <wtf/Lock.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

OBJC_CLASS PDFDocument;

namespace WebCore {
class FloatRect;
class GraphicsContext;
class SharedMemory;
}

namespace WebKit {

// A PDF document that a web process sent the print process to draw the pages of as vectors. Parsed
// in the print process, so that the print process draws what it parsed itself.
class PrintPDFDocument : public ThreadSafeRefCounted<PrintPDFDocument> {
public:
    // Null if the data is not a PDF document.
    static RefPtr<PrintPDFDocument> create(WebCore::SharedMemory&);
    ~PrintPDFDocument();

    // Draws the part of the page in source into destination, as the page is displayed: its crop box,
    // rotated as the page asks, with the origin at its top left. Draws the links of the page too, into
    // a PDF, if asked to. Draws nothing for a page the document does not have, or into a context with
    // no CG context. Can be called on any thread.
    void drawPage(WebCore::GraphicsContext&, unsigned pageIndex, const WebCore::FloatRect& source, const WebCore::FloatRect& destination, bool includeLinks) const;

private:
    explicit PrintPDFDocument(RetainPtr<PDFDocument>&&);

    mutable Lock m_lock;
    const RetainPtr<PDFDocument> m_document;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
