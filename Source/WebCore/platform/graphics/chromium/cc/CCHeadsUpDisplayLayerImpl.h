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

#include "cc/CCLayerImpl.h"
#include "cc/CCScopedTexture.h"

namespace WebCore {

class CCDebugRectHistory;
class CCFontAtlas;
class CCFrameRateCounter;
class GraphicsContext;

class CCHeadsUpDisplayLayerImpl : public CCLayerImpl {
public:
    static PassOwnPtr<CCHeadsUpDisplayLayerImpl> create(int id, PassOwnPtr<CCFontAtlas> fontAtlas)
    {
        return adoptPtr(new CCHeadsUpDisplayLayerImpl(id, fontAtlas));
    }
    virtual ~CCHeadsUpDisplayLayerImpl();

    virtual void willDraw(CCResourceProvider*) OVERRIDE;
    virtual void appendQuads(CCQuadSink&, const CCSharedQuadState*, bool& hadMissingTiles) OVERRIDE;
    virtual void didDraw(CCResourceProvider*) OVERRIDE;

    virtual void didLoseContext() OVERRIDE;

    virtual bool layerIsAlwaysDamaged() const OVERRIDE { return true; }

private:
    CCHeadsUpDisplayLayerImpl(int, PassOwnPtr<CCFontAtlas>);

    virtual const char* layerTypeAsString() const OVERRIDE { return "HeadsUpDisplayLayer"; }

    void drawHudContents(GraphicsContext*);
    void drawFPSCounter(GraphicsContext*, CCFrameRateCounter*, int top, int height);
    void drawFPSCounterText(GraphicsContext*, CCFrameRateCounter*, int top, int width, int height);
    void drawDebugRects(GraphicsContext*, CCDebugRectHistory*);

    OwnPtr<CCFontAtlas> m_fontAtlas;
    OwnPtr<CCScopedTexture> m_hudTexture;
};

}

#endif // CCHeadsUpDisplayLayerImpl_h
