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

#ifndef CCTiledLayerImpl_h
#define CCTiledLayerImpl_h

#include "LayerTextureUpdater.h"
#include "TransformationMatrix.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCLayerTilingData.h"

namespace WebCore {

class DrawableTile;

class CCTiledLayerImpl : public CCLayerImpl {
public:
    static PassOwnPtr<CCTiledLayerImpl> create(int id)
    {
        return adoptPtr(new CCTiledLayerImpl(id));
    }
    virtual ~CCTiledLayerImpl();

    virtual void appendQuads(CCQuadCuller&, const CCSharedQuadState*, bool& hadMissingTiles) OVERRIDE;

    virtual void bindContentsTexture(LayerRendererChromium*) OVERRIDE;

    virtual void dumpLayerProperties(TextStream&, int indent) const OVERRIDE;

    void setSkipsDraw(bool skipsDraw) { m_skipsDraw = skipsDraw; }
    void setTilingData(const CCLayerTilingData& tiler);
    void pushTileProperties(int, int, Platform3DObject textureId, const IntRect& opaqueRect);

    void setContentsSwizzled(bool contentsSwizzled) { m_contentsSwizzled = contentsSwizzled; }
    bool contentsSwizzled() const { return m_contentsSwizzled; }

    virtual Region visibleContentOpaqueRegion() const OVERRIDE;
    virtual void didLoseContext() OVERRIDE;

protected:
    explicit CCTiledLayerImpl(int id);
    // Exposed for testing.
    bool hasTileAt(int, int) const;
    bool hasTextureIdForTileAt(int, int) const;

    virtual TransformationMatrix quadTransform() const OVERRIDE;

private:

    virtual const char* layerTypeAsString() const OVERRIDE { return "ContentLayer"; }

    DrawableTile* tileAt(int, int) const;
    DrawableTile* createTile(int, int);

    bool m_skipsDraw;
    bool m_contentsSwizzled;

    OwnPtr<CCLayerTilingData> m_tiler;
};

}

#endif // CCTiledLayerImpl_h
