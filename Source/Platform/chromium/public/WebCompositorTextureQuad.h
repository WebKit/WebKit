/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebCompositorTextureQuad_h
#define WebCompositorTextureQuad_h

#if WEBKIT_IMPLEMENTATION
#include "FloatRect.h"
#endif
#include "WebCommon.h"
#include "WebCompositorQuad.h"
#include "WebFloatRect.h"
#if WEBKIT_IMPLEMENTATION
#include <wtf/PassOwnPtr.h>
#endif

namespace WebKit {

#pragma pack(push, 4)

class WebCompositorTextureQuad : public WebCompositorQuad {
public:
#if WEBKIT_IMPLEMENTATION
    static PassOwnPtr<WebCompositorTextureQuad> create(const WebCompositorSharedQuadState*, const WebCore::IntRect&, unsigned resourceId, bool premultipliedAlpha, const WebCore::FloatRect& uvRect, bool flipped);
    WebCore::FloatRect uvRect() const { return m_uvRect; }
#endif

    unsigned resourceId() const { return m_resourceId; }
    bool premultipliedAlpha() const { return  m_premultipliedAlpha; }
    bool flipped() const { return m_flipped; }

    void setNeedsBlending();

    static const WebCompositorTextureQuad* materialCast(const WebCompositorQuad*);
private:
#if WEBKIT_IMPLEMENTATION
    WebCompositorTextureQuad(const WebKit::WebCompositorSharedQuadState*, const WebCore::IntRect&, unsigned resourceId, bool premultipliedAlpha, const WebCore::FloatRect& uvRect, bool flipped);
#endif

    unsigned m_resourceId;
    bool m_premultipliedAlpha;
    WebFloatRect m_uvRect;
    bool m_flipped;
};

#pragma pack(pop)

}

#endif
