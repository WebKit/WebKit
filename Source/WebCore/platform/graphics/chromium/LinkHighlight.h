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

#ifndef LinkHighlight_h
#define LinkHighlight_h

#include "ContentLayerChromium.h"
#include "Path.h"
#include <public/WebAnimationDelegate.h>
#include <wtf/RefPtr.h>

#if USE(ACCELERATED_COMPOSITING)

namespace WebCore {

class GraphicsLayerChromium;

class LinkHighlight : public RefCounted<LinkHighlight>, public ContentLayerDelegate, public WebKit::WebAnimationDelegate {
public:
    static PassRefPtr<LinkHighlight> create(GraphicsLayerChromium* parent, const Path&, int animationId, int groupId);
    virtual ~LinkHighlight();

    ContentLayerChromium* contentLayer();

    // ContentLayerDelegate implementation.
    virtual void paintContents(SkCanvas*, const IntRect& clipRect, FloatRect& opaque) OVERRIDE;

    // WebAnimationDelegate implementation.
    virtual void notifyAnimationStarted(double time) OVERRIDE;
    virtual void notifyAnimationFinished(double time) OVERRIDE;

private:
    LinkHighlight(GraphicsLayerChromium* parent, const Path&, int animationId, int groupId);

    RefPtr<ContentLayerChromium> m_contentLayer;
    GraphicsLayerChromium* m_parent;
    Path m_path;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif
