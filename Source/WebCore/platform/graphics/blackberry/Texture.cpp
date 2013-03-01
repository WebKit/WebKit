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

#include "config.h"
#include "Texture.h"

#if USE(ACCELERATED_COMPOSITING)

#include "Color.h"
#include "IntRect.h"
#include "TextureCacheCompositingThread.h"

#include <BlackBerryPlatformGraphicsContext.h>
#include <GLES2/gl2.h>
#include <wtf/CurrentTime.h>
#include <wtf/OwnArrayPtr.h>

namespace WebCore {

Texture::Texture(bool isColor)
    : m_protectionCount(0)
    , m_handle(0)
    , m_isColor(isColor)
    , m_isOpaque(false)
    , m_bufferSizeInBytes(0)
{
    textureCacheCompositingThread()->install(this, IntSize(), BlackBerry::Platform::Graphics::BackedWhenNecessary);
}

Texture::~Texture()
{
    textureCacheCompositingThread()->textureDestroyed(this);
}

void Texture::updateContents(const Texture::HostType& contents, const IntRect& dirtyRect, const IntRect& tile, bool isOpaque)
{
    if (m_handle)
        BlackBerry::Platform::Graphics::destroyBuffer(m_handle);
    m_handle = contents;
    size_t newBufferSizeInBytes = BlackBerry::Platform::Graphics::bufferSizeInBytes(m_handle);
    textureCacheCompositingThread()->textureSizeInBytesChanged(this, newBufferSizeInBytes - m_bufferSizeInBytes);
    m_bufferSizeInBytes = newBufferSizeInBytes;
}

void Texture::setContentsToColor(const Color& color)
{
    m_isOpaque = !color.hasAlpha();
    RGBA32 rgba = color.rgb();

    if (m_handle)
        BlackBerry::Platform::Graphics::destroyBuffer(m_handle);

    m_handle = BlackBerry::Platform::Graphics::createBuffer(IntSize(1, 1), BlackBerry::Platform::Graphics::BackedWhenNecessary);
    BlackBerry::Platform::Graphics::PlatformGraphicsContext* gc = lockBufferDrawable(m_handle);
    gc->setFillColor(rgba);
    gc->addFillRect(BlackBerry::Platform::FloatRect(0, 0, 1, 1));
    releaseBufferDrawable(m_handle);

    IntSize oldSize = m_size;
    m_size = IntSize(1, 1);
    if (m_size != oldSize)
        textureCacheCompositingThread()->textureResized(this, oldSize);
}

bool Texture::protect(const IntSize& size, BlackBerry::Platform::Graphics::BufferType type)
{
    if (!hasTexture()) {
        // We may have been evicted by the TextureCacheCompositingThread,
        // attempt to install us again.
        if (!textureCacheCompositingThread()->install(this, size, type))
            return false;
    }

    ++m_protectionCount;

    if (m_size == size)
        return true;

    textureCacheCompositingThread()->resizeTexture(this, size, type);

    return true;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
