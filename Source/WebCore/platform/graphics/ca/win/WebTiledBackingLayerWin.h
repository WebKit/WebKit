/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef WebTiledBackingLayerWin_h
#define WebTiledBackingLayerWin_h

#include "PlatformCALayerWinInternal.h"

namespace WebCore {

class WebTiledBackingLayerWin : public PlatformCALayerWinInternal {
public:
    WebTiledBackingLayerWin(PlatformCALayer*);
    ~WebTiledBackingLayerWin();

    void displayCallback(CACFLayerRef, CGContextRef) override;
    void setNeedsDisplayInRect(const FloatRect&) override;
    void setNeedsDisplay() override;

    bool isOpaque() const override;
    void setOpaque(bool) override;

    void setBounds(const FloatRect&) override;

    float contentsScale() const override;
    void setContentsScale(float) override;

    void setBorderWidth(float) override;

    void setBorderColor(const Color&) override;

    // WebTiledBackingLayer Features
    TileController* createTileController(PlatformCALayer* rootLayer);
    TiledBacking* tiledBacking();
    void invalidate();

private:
    RetainPtr<CACFLayerRef> m_tileParent;
    std::unique_ptr<TileController> m_tileController;
};

}

#endif // WebTiledBackingLayerWin_h
