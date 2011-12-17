/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef ContentLayerChromium_h
#define ContentLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "TiledLayerChromium.h"
#include "cc/CCTiledLayerImpl.h"

namespace WebCore {

class LayerTilerChromium;
class LayerTextureUpdater;

// A Layer that requires a GraphicsContext to render its contents.
class ContentLayerChromium : public TiledLayerChromium {
public:
    static PassRefPtr<ContentLayerChromium> create(CCLayerDelegate*);

    virtual ~ContentLayerChromium();

    virtual void paintContentsIfDirty();
    virtual void idlePaintContentsIfDirty();

protected:
    explicit ContentLayerChromium(CCLayerDelegate*);

private:
    virtual void cleanupResources();
    virtual bool drawsContent() const;
    virtual void createTextureUpdater(const CCLayerTreeHost*);
    virtual LayerTextureUpdater* textureUpdater() const { return m_textureUpdater.get(); }

    RefPtr<LayerTextureUpdater> m_textureUpdater;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
