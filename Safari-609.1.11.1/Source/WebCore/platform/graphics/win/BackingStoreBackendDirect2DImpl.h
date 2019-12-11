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

#if USE(DIRECT2D)

#include "BackingStoreBackendDirect2D.h"
#include <pal/HysteresisActivity.h>

interface ID2D1BitmapBrush;
interface IDXGISurface1;

namespace WebCore {

class IntSize;

class BackingStoreBackendDirect2DImpl final : public BackingStoreBackendDirect2D {
public:
    WEBCORE_EXPORT BackingStoreBackendDirect2DImpl(const IntSize&, float deviceScaleFactor, ID3D11Device1*);
    virtual ~BackingStoreBackendDirect2DImpl();

private:
    void scroll(const IntRect&, const IntSize&) override;
    ID2D1BitmapBrush* bitmapBrush() override;

    IntSize m_scrollSurfaceSize;
    COMPtr<ID2D1Bitmap> m_scrollSurface;
    COMPtr<IDXGISurface1> m_dxScrollSurface;
    COMPtr<ID2D1BitmapBrush> m_bitmapBrush;

    PAL::HysteresisActivity m_scrolledHysteresis;
};

} // namespace WebCore

#endif // USE(DIRECT2D)
