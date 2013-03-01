/*
 * Copyright (C) 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef LayerTile_h
#define LayerTile_h

#if USE(ACCELERATED_COMPOSITING)

#include "Texture.h"

#include <wtf/RefPtr.h>

namespace WebCore {

class Color;
class IntRect;
class TileIndex;

class LayerTile {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LayerTile();
    ~LayerTile();

    Texture* texture() const { return m_texture.get(); }

    bool isVisible() const { return m_visible; }
    void setVisible(bool);

    bool isDirty() const { return m_contentsDirty || !m_texture || m_texture->isDirty(); }

    bool hasTexture() const { return m_texture && m_texture->hasTexture(); }

    void setContents(const Texture::HostType& contents, const IntRect& tileRect, const TileIndex&, bool isOpaque);
    void setContentsToColor(const Color&);
    void updateContents(const Texture::HostType& contents, const IntRect& dirtyRect, const IntRect& tileRect, bool isOpaque);
    void discardContents();

    // The current texture is an accurate preview of this layer, but a more
    // detailed texture could be obtained by repainting the layer. Used when
    // zoom level changes.
    void setContentsDirty() { m_contentsDirty = true; }

    enum RenderState { DoesNotNeedRender = 0, NeedsRender = 1, RenderPending = 2 };
    RenderState renderState() const { return static_cast<RenderState>(m_needsRender); }
    bool needsRender() { return m_needsRender == NeedsRender; }
    void setNeedsRender(bool needsRender) { m_needsRender = needsRender; }
    void setRenderPending() { m_needsRender = RenderPending; }
    void setRenderDone() { m_needsRender = DoesNotNeedRender; }

private:
    void setTexture(PassRefPtr<Texture>);

    // Never assign to m_texture directly, use setTexture() above.
    RefPtr<Texture> m_texture;
    bool m_contentsDirty : 1;
    bool m_visible : 1;
    unsigned m_needsRender : 2;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // Texture_h
