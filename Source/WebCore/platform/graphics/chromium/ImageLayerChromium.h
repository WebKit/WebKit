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


#ifndef ImageLayerChromium_h
#define ImageLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "ContentLayerChromium.h"
#include "PlatformImage.h"

#if USE(CG)
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

class Image;
class ImageLayerTextureUpdater;
class Region;

// A Layer that contains only an Image element.
class ImageLayerChromium : public TiledLayerChromium {
public:
    static PassRefPtr<ImageLayerChromium> create();
    virtual ~ImageLayerChromium();

    virtual bool drawsContent() const;
    virtual void paintContentsIfDirty(const Region& occludedScreenSpace);
    virtual bool needsContentsScale() const;

    void setContents(Image* image);

private:
    ImageLayerChromium();

    virtual void createTextureUpdater(const CCLayerTreeHost*);
    void setTilingOption(TilingOption);

    virtual LayerTextureUpdater* textureUpdater() const;
    virtual IntSize contentBounds() const;

    NativeImagePtr m_imageForCurrentFrame;
    RefPtr<Image> m_contents;

    RefPtr<ImageLayerTextureUpdater> m_textureUpdater;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
