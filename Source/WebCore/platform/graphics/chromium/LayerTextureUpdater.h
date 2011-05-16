/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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


#ifndef LayerTextureUpdater_h
#define LayerTextureUpdater_h

#if USE(ACCELERATED_COMPOSITING)

namespace WebCore {

class GraphicsContext3D;
class IntRect;
class IntSize;
class LayerTexture;

class LayerTextureUpdater {
public:
    explicit LayerTextureUpdater(GraphicsContext3D* context) : m_context(context) { }
    virtual ~LayerTextureUpdater() { }

    enum Orientation {
        BottomUpOrientation,
        TopDownOrientation
    };
    // Returns the orientation of the texture uploaded by this interface.
    virtual Orientation orientation() = 0;
    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, int borderTexels) = 0;
    virtual void updateTextureRect(LayerTexture*, const IntRect& sourceRect, const IntRect& destRect) = 0;

protected:
    GraphicsContext3D* context() const { return m_context; }

private:
    // The graphics context with which to update textures.
    // It is assumed that the textures are either created in the same context
    // or shared with this context.
    GraphicsContext3D* m_context;
};

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
#endif // LayerTextureUpdater_h

