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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(CG)

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class FloatSize;
class GraphicsContext;
class GraphicsContextCG;
class SharedBuffer;

// A PDF document that is drawn through a GraphicsContext. The document is finished and its
// data is handed out by takeData(). The context draws in PDF points: it has no base transform.
class PDFDocumentBuffer {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(PDFDocumentBuffer, WEBCORE_EXPORT);
    WTF_MAKE_NONCOPYABLE(PDFDocumentBuffer);
public:
    // The mediaBox size is rounded up to whole points. Returns nullptr if it is empty.
    WEBCORE_EXPORT static std::unique_ptr<PDFDocumentBuffer> create(const FloatSize& mediaBox);
    WEBCORE_EXPORT ~PDFDocumentBuffer();

    WEBCORE_EXPORT GraphicsContext& context();

    // Closes the PDF context. The context must not be drawn to afterwards.
    WEBCORE_EXPORT RefPtr<SharedBuffer> takeData();

private:
    PDFDocumentBuffer(RetainPtr<CFMutableDataRef>&&, UniqueRef<GraphicsContextCG>&&);

    RetainPtr<CFMutableDataRef> m_data;
    const UniqueRef<GraphicsContextCG> m_context;
};

} // namespace WebCore

#endif // USE(CG)
