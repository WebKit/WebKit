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

class SkBitmap;

namespace WebCore {

class Color;
class IntRect;
class TileIndex;

class LayerTileData {
public:
    LayerTileData()
        : m_visible(false)
    {
    }

    bool isVisible() const { return m_visible; }

protected:
    bool m_visible;
};

class LayerTile : public LayerTileData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LayerTile();
    ~LayerTile();

    Texture* texture() const { return m_texture.get(); }

    bool isVisible() const { return m_visible; }
    void setVisible(bool);

    bool isDirty() const { return m_contentsDirty || !m_texture || m_texture->isDirty(); }

    bool hasTexture() const { return m_texture && m_texture->hasTexture(); }

    void setContents(const SkBitmap& contents, const IntRect& tileRect, const TileIndex&, bool isOpaque);
    void setContentsToColor(const Color&);
    void updateContents(const SkBitmap& contents, const IntRect& dirtyRect, const IntRect& tileRect, bool isOpaque);
    void discardContents();

    // The current texture is an accurate preview of this layer, but a more
    // detailed texture could be obtained by repainting the layer. Used when
    // zoom level changes.
    void setContentsDirty() { m_contentsDirty = true; }

private:
    void setTexture(PassRefPtr<Texture>);

    // Never assign to m_texture directly, use setTexture() above.
    RefPtr<Texture> m_texture;
    bool m_contentsDirty;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // Texture_h
