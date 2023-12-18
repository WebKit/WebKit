/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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

#if USE(CAIRO)

#include <WebCore/IntSize.h>
#include <WebCore/RefPtrCairo.h>
#include <pal/HysteresisActivity.h>
#include <wtf/Noncopyable.h>

namespace WebCore {
class IntRect;
}

namespace WebKit {
class ShareableBitmap;
struct UpdateInfo;

class BackingStore {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(BackingStore);
public:
    BackingStore(const WebCore::IntSize&, float deviceScaleFactor);
    ~BackingStore();

    const WebCore::IntSize& size() const { return m_size; }
    float deviceScaleFactor() const { return m_deviceScaleFactor; }

    void paint(cairo_t*, const WebCore::IntRect&);
    void incorporateUpdate(UpdateInfo&&);

private:
    void scroll(const WebCore::IntRect&, const WebCore::IntSize&);

    WebCore::IntSize m_size;
    float m_deviceScaleFactor { 1 };
    RefPtr<cairo_surface_t> m_surface;
    RefPtr<cairo_surface_t> m_scrollSurface;
    PAL::HysteresisActivity m_scrolledHysteresis;
};

} // namespace WebKit

#endif // USE(CAIRO)
