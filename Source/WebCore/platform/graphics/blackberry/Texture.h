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

#ifndef Texture_h
#define Texture_h

#if USE(ACCELERATED_COMPOSITING)

#include "IntSize.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

class SkBitmap;

namespace WebCore {

class Color;
class IntRect;
class TextureCacheCompositingThread;

// Texture encapsulates a volatile texture - at any time, the underlying OpenGL
// texture may be deleted by the TextureCacheCompositingThread. The user must
// check using Texture::isDirty() immediately before using it, every time.
// The only way to prevent eviction this is to call Texture::protect().
// If the texture isDirty(), you must updateContents() before you can use it.
class Texture : public RefCounted<Texture> {
public:
    static PassRefPtr<Texture> create(bool isColor = false)
    {
        return adoptRef(new Texture(isColor));
    }

    ~Texture();

    unsigned textureId() const { return m_textureId; }

    bool isDirty() const { return !m_textureId; }
    bool hasTexture() const { return m_textureId; }

    bool isColor() const { return m_isColor; }
    bool isOpaque() const { return m_isOpaque; }

    bool isProtected() const { return m_protectionCount > 0; }
    void protect() { ++m_protectionCount; }
    void unprotect() { --m_protectionCount; }
    bool protect(const IntSize&);

    void updateContents(const SkBitmap& contents, const IntRect& dirtyRect, const IntRect& tile, bool isOpaque);
    void setContentsToColor(const Color&);

    IntSize size() const { return m_size; }
    int width() const { return m_size.width(); }
    int height() const { return m_size.height(); }
    static int bytesPerPixel() { return 4; }

private:
    friend class TextureCacheCompositingThread;

    Texture(bool isColor = false);

    void setTextureId(unsigned id)
    {
        m_textureId = id;

        // We assume it is a newly allocated texture,
        // and thus empty, or 0, which would of course
        // be empty.
        m_size = IntSize();
    }

    int m_protectionCount;
    unsigned m_textureId;
    IntSize m_size;
    bool m_isColor;
    bool m_isOpaque;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // Texture_h
