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

#ifndef CCHeadsUpDisplayLayerImpl_h
#define CCHeadsUpDisplayLayerImpl_h

#include "CCFontAtlas.h"
#include "CCLayerImpl.h"
#include "CCScopedTexture.h"

class SkCanvas;

namespace WebCore {

class CCDebugRectHistory;
class CCFontAtlas;
class CCFrameRateCounter;

class CCHeadsUpDisplayLayerImpl : public CCLayerImpl {
public:
    static PassOwnPtr<CCHeadsUpDisplayLayerImpl> create(int id)
    {
        return adoptPtr(new CCHeadsUpDisplayLayerImpl(id));
    }
    virtual ~CCHeadsUpDisplayLayerImpl();

    void setFontAtlas(PassOwnPtr<CCFontAtlas>);

    virtual void willDraw(CCResourceProvider*) OVERRIDE;
    virtual void appendQuads(CCQuadSink&, const CCSharedQuadState*, bool& hadMissingTiles) OVERRIDE;
    virtual void didDraw(CCResourceProvider*) OVERRIDE;

    virtual void didLoseContext() OVERRIDE;

    virtual bool layerIsAlwaysDamaged() const OVERRIDE { return true; }

private:
    explicit CCHeadsUpDisplayLayerImpl(int);

    virtual const char* layerTypeAsString() const OVERRIDE { return "HeadsUpDisplayLayer"; }

    void drawHudContents(SkCanvas*);
    void drawFPSCounter(SkCanvas*, CCFrameRateCounter*, int top, int height);
    void drawFPSCounterText(SkCanvas*, CCFrameRateCounter*, int top, int width, int height);
    void drawDebugRects(SkCanvas*, CCDebugRectHistory*);

    OwnPtr<CCFontAtlas> m_fontAtlas;
    OwnPtr<CCScopedTexture> m_hudTexture;
    OwnPtr<SkCanvas> m_hudCanvas;
};

}

#endif // CCHeadsUpDisplayLayerImpl_h
