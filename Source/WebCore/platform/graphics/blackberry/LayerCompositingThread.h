/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
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


#ifndef LayerCompositingThread_h
#define LayerCompositingThread_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatQuad.h"
#include "LayerData.h"
#include "LayerRendererSurface.h"
#include "LayerTiler.h"

#include <BlackBerryPlatformGuardedPointer.h>
#include <GuardedPointerDeleter.h>

namespace BlackBerry {
namespace Platform {
namespace Graphics {
class Buffer;
}
}
}

namespace WebCore {

class LayerRenderer;

class LayerCompositingThread : public ThreadSafeRefCounted<LayerCompositingThread>, public LayerData, public BlackBerry::Platform::GuardedPointerBase {
public:
    static PassRefPtr<LayerCompositingThread> create(LayerType, PassRefPtr<LayerTiler>);

    // Thread safe
    void setPluginView(PluginView*);
#if ENABLE(VIDEO)
    void setMediaPlayer(MediaPlayer*);
#endif
    void clearAnimations();

    // Not thread safe

    // Returns true if we have an animation
    bool updateAnimations(double currentTime);
    void updateTextureContentsIfNeeded();
    void bindContentsTexture()
    {
        if (m_tiler)
            m_tiler->bindContentsTexture();
    }

    const LayerCompositingThread* rootLayer() const;
    void setSublayers(const Vector<RefPtr<LayerCompositingThread> >&);
    const Vector<RefPtr<LayerCompositingThread> >& getSublayers() const { return m_sublayers; }
    void setSuperlayer(LayerCompositingThread* superlayer) { m_superlayer = superlayer; }
    LayerCompositingThread* superlayer() const { return m_superlayer; }

    // The layer renderer must be set if the layer has been rendered
    void setLayerRenderer(LayerRenderer*);

    void setDrawTransform(const TransformationMatrix&);
    const TransformationMatrix& drawTransform() const { return m_drawTransform; }

    void setDrawOpacity(float opacity) { m_drawOpacity = opacity; }
    float drawOpacity() const { return m_drawOpacity; }

    void createLayerRendererSurface();
    LayerRendererSurface* layerRendererSurface() const { return m_layerRendererSurface.get(); }
    void clearLayerRendererSurface() { m_layerRendererSurface.clear(); }

    void setMaskLayer(LayerCompositingThread* maskLayer) { m_maskLayer = maskLayer; }
    LayerCompositingThread* maskLayer() const { return m_maskLayer.get(); }

    void setReplicaLayer(LayerCompositingThread* layer) { m_replicaLayer = layer; }
    LayerCompositingThread* replicaLayer() const { return m_replicaLayer.get(); }

    FloatRect getDrawRect() const { return m_drawRect; }
    const FloatQuad& getTransformedBounds() const { return m_transformedBounds; }
    FloatQuad getTransformedHolePunchRect() const;

    void deleteTextures();

    void drawTextures(int positionLocation, int texCoordLocation, const FloatRect& visibleRect);
    bool hasMissingTextures() const { return m_tiler ? m_tiler->hasMissingTextures() : false; }
    void drawMissingTextures(int positionLocation, int texCoordLocation, const FloatRect& visibleRect);
    void drawSurface(const TransformationMatrix&, LayerCompositingThread* mask, int positionLocation, int texCoordLocation);
    bool isDirty() const { return m_tiler ? m_tiler->hasDirtyTiles() : false; }

    void releaseTextureResources();

    // Layer visibility is determined by the LayerRenderer when drawing.
    // So we don't have an accurate value for visibility until it's too late,
    // but the attribute still is useful.
    bool isVisible() const { return m_visible; }
    void setVisible(bool);

    // This will cause a commit of the whole layer tree on the WebKit thread,
    // sometime after rendering is finished. Used when rendering results in a
    // need for commit, for example when a dirty layer becomes visible.
    void setNeedsCommit();

    // Normally you would schedule a commit from the webkit thread, but
    // this allows you to do it from the compositing thread.
    void scheduleCommit();

    // These two functions are used to update animated properties in LayerAnimation.
    void setOpacity(float opacity) { m_opacity = opacity; }
    void setTransform(const TransformationMatrix& matrix) { m_transform = matrix; }

    bool hasRunningAnimations() const { return !m_runningAnimations.isEmpty(); }

    bool hasVisibleHolePunchRect() const;

protected:
    virtual ~LayerCompositingThread();

private:
    LayerCompositingThread(LayerType, PassRefPtr<LayerTiler>);

    void updateTileContents(const IntRect& tile);

    void removeFromSuperlayer();

    size_t numSublayers() const { return m_sublayers.size(); }

    // Returns the index of the sublayer or -1 if not found.
    int indexOfSublayer(const LayerCompositingThread*);

    // This should only be called from removeFromSuperlayer.
    void removeSublayer(LayerCompositingThread*);

    LayerRenderer* m_layerRenderer;

    typedef Vector<RefPtr<LayerCompositingThread> > LayerList;
    LayerList m_sublayers;
    LayerCompositingThread* m_superlayer;

    // Vertex data for the bounds of this layer
    FloatQuad m_transformedBounds;
    // The bounding rectangle of the transformed layer
    FloatRect m_drawRect;

    OwnPtr<LayerRendererSurface> m_layerRendererSurface;

    RefPtr<LayerCompositingThread> m_maskLayer;
    RefPtr<LayerCompositingThread> m_replicaLayer;

    BlackBerry::Platform::Graphics::Buffer* m_pluginBuffer;

    // The global property values, after concatenation with parent values
    TransformationMatrix m_drawTransform;
    float m_drawOpacity;

    bool m_visible;
    bool m_commitScheduled;

    RefPtr<LayerTiler> m_tiler;
};

} // namespace WebCore

namespace WTF {

// LayerCompositingThread objects must be destroyed on the compositing thread.
// But it's possible for the last reference to be held by the WebKit thread.
// So we create a custom specialization of ThreadSafeRefCounted which calls a
// function that ensures the destructor is called on the correct thread, rather
// than calling delete directly.
template<>
inline void ThreadSafeRefCounted<WebCore::LayerCompositingThread>::deref()
{
    if (derefBase()) {
        // Delete on the compositing thread.
        BlackBerry::Platform::GuardedPointerDeleter::deleteOnThread(
                BlackBerry::Platform::userInterfaceThreadMessageClient(),
                static_cast<WebCore::LayerCompositingThread*>(this));
    }
}

} // namespace WTF


#endif // USE(ACCELERATED_COMPOSITING)

#endif
