/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCHeadsUpDisplay_h
#define CCHeadsUpDisplay_h

#if USE(ACCELERATED_COMPOSITING)

#include "cc/CCFontAtlas.h"

namespace WebCore {

class CCDebugRectHistory;
class CCFrameRateCounter;
class CCLayerTreeHostImpl;
class GraphicsContext;
class ManagedTexture;
struct CCSettings;

// Class that handles drawing of composited render layers using GL.
class CCHeadsUpDisplay {
    WTF_MAKE_NONCOPYABLE(CCHeadsUpDisplay);
public:
    static PassOwnPtr<CCHeadsUpDisplay> create()
    {
        return adoptPtr(new CCHeadsUpDisplay());
    }

    ~CCHeadsUpDisplay();

    void setFontAtlas(PassOwnPtr<CCFontAtlas>);

    bool enabled(const CCSettings&) const;
    void draw(CCLayerTreeHostImpl*);

private:
    CCHeadsUpDisplay() { };

    void drawHudContents(GraphicsContext*, CCLayerTreeHostImpl*, const CCSettings&, const IntSize& hudSize);
    void drawFPSCounter(GraphicsContext*, CCFrameRateCounter*, int top, int height);
    void drawFPSCounterText(GraphicsContext*, CCFrameRateCounter*, int top, int width, int height);
    void drawDebugRects(GraphicsContext*, CCDebugRectHistory*, const CCSettings&);

    bool showPlatformLayerTree(const CCSettings&) const;
    bool showDebugRects(const CCSettings&) const;

    OwnPtr<ManagedTexture> m_hudTexture;
    OwnPtr<CCFontAtlas> m_fontAtlas;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif
