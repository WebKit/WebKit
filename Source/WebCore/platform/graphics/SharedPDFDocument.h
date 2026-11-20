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

#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class FloatRect;
class GraphicsContext;
class SharedBuffer;

class SharedPDFDocument : public ThreadSafeRefCounted<SharedPDFDocument> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(SharedPDFDocument, WEBCORE_EXPORT);
public:
    WEBCORE_EXPORT static Ref<SharedPDFDocument> create(Ref<SharedBuffer>&&);
    WEBCORE_EXPORT virtual ~SharedPDFDocument();

    SharedBuffer& data() const { return m_data; }
    RenderingResourceIdentifier renderingResourceIdentifier() const { return m_renderingResourceIdentifier; }

    virtual void drawPage(GraphicsContext&, unsigned pageIndex, const FloatRect& pageRect);

protected:
    WEBCORE_EXPORT explicit SharedPDFDocument(Ref<SharedBuffer>&&);

private:
    const Ref<SharedBuffer> m_data;
    const RenderingResourceIdentifier m_renderingResourceIdentifier { RenderingResourceIdentifier::generate() };
};

} // namespace WebCore
