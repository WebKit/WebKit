/*
 * Copyright (C) 2014 Alex Christensen <achristensen@webkit.org>
 * All rights reserved.
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
#include "MediaPlayerPrivateMediaFoundation.h"

#if ENABLE(VIDEO) && USE(MEDIA_FOUNDATION) && USE(CAIRO)

#include "CairoOperations.h"

namespace WebCore {

void MediaPlayerPrivateMediaFoundation::Direct3DPresenter::paintCurrentFrame(GraphicsContext& context, const FloatRect& destRect)
{
    UINT width = m_destRect.right - m_destRect.left;
    UINT height = m_destRect.bottom - m_destRect.top;

    if (!width || !height)
        return;

    Locker locker { m_lock };

    if (!m_memSurface)
        return;

    D3DLOCKED_RECT lockedRect;
    if (SUCCEEDED(m_memSurface->LockRect(&lockedRect, nullptr, D3DLOCK_READONLY))) {
        void* data = lockedRect.pBits;
        int pitch = lockedRect.Pitch;
        D3DFORMAT format = D3DFMT_UNKNOWN;
        D3DSURFACE_DESC desc;
        if (SUCCEEDED(m_memSurface->GetDesc(&desc)))
            format = desc.Format;

        cairo_format_t cairoFormat = CAIRO_FORMAT_INVALID;

        switch (format) {
        case D3DFMT_A8R8G8B8:
            cairoFormat = CAIRO_FORMAT_ARGB32;
            break;
        case D3DFMT_X8R8G8B8:
            cairoFormat = CAIRO_FORMAT_RGB24;
            break;
        default:
            break;
        }

        ASSERT(cairoFormat != CAIRO_FORMAT_INVALID);

        if (cairoFormat != CAIRO_FORMAT_INVALID) {
            auto surface = adoptRef(cairo_image_surface_create_for_data(static_cast<unsigned char*>(data), cairoFormat, width, height, pitch));
            auto image = NativeImage::create(WTFMove(surface));
            FloatRect srcRect(0, 0, width, height);
            context.drawNativeImage(*image, destRect, srcRect);
        }
        m_memSurface->UnlockRect();
    }
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(MEDIA_FOUNDATION) && USE(CAIRO)
