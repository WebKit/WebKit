/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "EmptyClients.h"
#include "SVGImage.h"

namespace WebCore {

class SVGImageChromeClient final : public EmptyChromeClient {
    WTF_MAKE_NONCOPYABLE(SVGImageChromeClient); WTF_MAKE_FAST_ALLOCATED;
public:
    SVGImageChromeClient(SVGImage* image)
        : m_image(image)
    {
    }
    
    bool isSVGImageChromeClient() const final { return true; }
    SVGImage* image() const { return m_image; }
    
private:
    void chromeDestroyed() final
    {
        m_image = nullptr;
    }
    
    void invalidateContentsAndRootView(const IntRect& r) final
    {
        // If m_image->m_page is null, we're being destroyed.
        if (!m_image || !m_image->m_page)
            return;

        auto* imageObserver = m_image->imageObserver();
        if (!imageObserver)
            return;

        imageObserver->imageFrameAvailable(*m_image, m_image->isAnimating() ? ImageAnimatingState::Yes : ImageAnimatingState::No, &r);
    }
    
    SVGImage* m_image;
};

} // namespace WebCore
