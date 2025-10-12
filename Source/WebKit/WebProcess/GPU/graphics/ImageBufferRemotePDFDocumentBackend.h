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

#include <WebCore/NullImageBufferBackend.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class ImageBufferRemotePDFDocumentBackend final : public WebCore::NullImageBufferBackend {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(ImageBufferRemotePDFDocumentBackend);
    WTF_MAKE_NONCOPYABLE(ImageBufferRemotePDFDocumentBackend);
public:
    static unsigned calculateBytesPerRow(const WebCore::IntSize& backendSize);
    static size_t calculateMemoryCost(const Parameters&);

    static std::unique_ptr<ImageBufferRemotePDFDocumentBackend> create(const Parameters&);

    virtual ~ImageBufferRemotePDFDocumentBackend();

    static constexpr WebCore::RenderingMode renderingMode = WebCore::RenderingMode::PDFDocument;

private:
    using WebCore::NullImageBufferBackend::NullImageBufferBackend;

    String debugDescription() const final;
};

} // namespace WebKit
