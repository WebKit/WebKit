/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if USE(DIRECT2D)

#include "COMPtr.h"
#include "IntRect.h"
#include <wincodec.h>
#include <wtf/Noncopyable.h>

interface ID2D1Bitmap;
interface ID2D1BitmapBrush;
interface ID2D1DCRenderTarget;

namespace WebCore {

class BackingStoreBackendDirect2D {
    WTF_MAKE_NONCOPYABLE(BackingStoreBackendDirect2D);
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~BackingStoreBackendDirect2D() = default;

    ID2D1DCRenderTarget* renderTarget() const { return m_renderTarget.get(); }
    ID2D1Bitmap* surface() const { return m_surface.get(); }
    const IntSize& size() const { return m_size; }

    virtual void scroll(const IntRect& scrollRect, const IntSize& scrollOffset) = 0;
    virtual ID2D1BitmapBrush* bitmapBrush() = 0;

protected:
    BackingStoreBackendDirect2D(const IntSize& size)
        : m_size(size)
    {
    }

    COMPtr<ID2D1DCRenderTarget> m_renderTarget;
    COMPtr<ID2D1Bitmap> m_surface;
    IntSize m_size;
};


} // namespace WebCore

#endif // USE(DIRECT2D)
