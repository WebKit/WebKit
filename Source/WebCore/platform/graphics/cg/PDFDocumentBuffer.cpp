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

#include "config.h"
#include "PDFDocumentBuffer.h"

#if USE(CG)

#include "FloatSize.h"
#include "GraphicsContextCG.h"
#include "SharedBuffer.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PDFDocumentBuffer);

std::unique_ptr<PDFDocumentBuffer> PDFDocumentBuffer::create(const FloatSize& mediaBox)
{
    FloatSize size { ceilf(mediaBox.width()), ceilf(mediaBox.height()) };
    if (size.isEmpty() || !size.isExpressibleAsIntSize())
        return nullptr;

    RetainPtr data = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, 0));
    if (!data)
        return nullptr;

    RetainPtr dataConsumer = adoptCF(CGDataConsumerCreateWithCFData(data.get()));
    if (!dataConsumer)
        return nullptr;

    auto cgMediaBox = CGRectMake(0, 0, size.width(), size.height());

    RetainPtr pdfContext = adoptCF(CGPDFContextCreate(dataConsumer.get(), &cgMediaBox, nullptr));
    if (!pdfContext)
        return nullptr;

    return std::unique_ptr<PDFDocumentBuffer>(new PDFDocumentBuffer(WTF::move(data), makeUniqueRef<GraphicsContextCG>(pdfContext.get())));
}

PDFDocumentBuffer::PDFDocumentBuffer(RetainPtr<CFMutableDataRef>&& data, UniqueRef<GraphicsContextCG>&& context)
    : m_data(WTF::move(data))
    , m_context(WTF::move(context))
{
}

PDFDocumentBuffer::~PDFDocumentBuffer() = default;

GraphicsContext& PDFDocumentBuffer::context()
{
    return m_context.get();
}

RefPtr<SharedBuffer> PDFDocumentBuffer::takeData()
{
    RetainPtr data = std::exchange(m_data, nullptr);
    if (!data)
        return nullptr;
    CGPDFContextClose(protect(m_context->platformContext()));
    return SharedBuffer::create(data.get());
}

} // namespace WebCore

#endif // USE(CG)
