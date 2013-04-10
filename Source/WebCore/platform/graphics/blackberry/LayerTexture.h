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

#ifndef LayerTexture_h
#define LayerTexture_h

#if USE(ACCELERATED_COMPOSITING)

#include "IntSize.h"

#include <BlackBerryPlatformGraphics.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Color;
class IntRect;
class TextureCacheCompositingThread;

// LayerTexture encapsulates a volatile texture - at any time, the underlying OpenGL
// texture may be deleted by the TextureCacheCompositingThread. The user must
// check using LayerTexture::isDirty() immediately before using it, every time.
// The only way to prevent eviction this is to call LayerTexture::protect().
// If the texture isDirty(), you must updateContents() before you can use it.
class LayerTexture : public RefCounted<LayerTexture> {
public:
    static PassRefPtr<LayerTexture> create(bool isColor = false)
    {
        return adoptRef(new LayerTexture(isColor));
    }

    ~LayerTexture();

    typedef BlackBerry::Platform::Graphics::Buffer* GpuHandle;
    typedef BlackBerry::Platform::Graphics::Buffer* HostType;

    GpuHandle textureId() const { return m_handle; }
    bool isDirty() const { return !m_handle; }
    bool hasTexture() const { return m_handle; }

    bool isColor() const { return m_isColor; }
    bool isOpaque() const { return m_isOpaque; }

    bool isProtected() const { return m_protectionCount > 0; }
    void protect() { ++m_protectionCount; }
    void unprotect() { --m_protectionCount; }

    // This will ensure the texture has backing with the specified size
    bool protect(const IntSize&, BlackBerry::Platform::Graphics::BufferType = BlackBerry::Platform::Graphics::BackedWhenNecessary);

    void updateContents(const HostType& contents, const IntRect& dirtyRect, const IntRect& tile, bool isOpaque);
    void setContentsToColor(const Color&);

    IntSize size() const { return m_size; }
    int width() const { return m_size.width(); }
    int height() const { return m_size.height(); }
    static int bytesPerPixel() { return 4; }

    size_t sizeInBytes() const { return m_bufferSizeInBytes; }

private:
    friend class TextureCacheCompositingThread;

    LayerTexture(bool isColor = false);
    void setTextureId(GpuHandle id)
    {
        m_handle = id;

        // We assume it is a newly allocated texture,
        // and thus empty, or 0, which would of course
        // be empty.
        m_size = IntSize();
        m_bufferSizeInBytes = 0;
    }

    void setSize(const IntSize& size) { m_size = size; }

    int m_protectionCount;
    GpuHandle m_handle;
    IntSize m_size;
    bool m_isColor;
    bool m_isOpaque;
    size_t m_bufferSizeInBytes;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // LayerTexture_h
