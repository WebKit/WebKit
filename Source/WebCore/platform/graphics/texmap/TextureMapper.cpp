/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "TextureMapper.h"

#include "TextureMapperImageBuffer.h"

#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER)

namespace WebCore {

PassRefPtr<BitmapTexture> TextureMapper::acquireTextureFromPool(const IntSize& size)
{
    RefPtr<BitmapTexture> selectedTexture;

    for (size_t i = 0; i < m_texturePool.size(); ++i) {
        RefPtr<BitmapTexture>& texture = m_texturePool[i];

        // If the surface has only one reference (the one in m_texturePool), we can safely reuse it.
        if (texture->refCount() > 1)
            continue;

        // We default to the first available texture.
        if (!selectedTexture) {
            selectedTexture = texture;
            continue;
        }

        IntSize textureSize = texture->size();
        IntSize selectedTextureSize = selectedTexture->size();

        // We prefer to pick a texture that's equal or larger than the requested size.
        if (textureSize.width() < size.width() || textureSize.height() < size.height())
            continue;

        // We select the new texture if the currently selected texture is smaller than the
        // required size, and the new texture has a smaller area.
        int textureArea = textureSize.width() * textureSize.height();
        int selectedTextureArea = selectedTextureSize.width() * selectedTextureSize.height();
        bool selectedTextureFitsSize =
                selectedTextureSize.width() >= size.width()
                && selectedTextureSize.height() >= size.height();

        if (selectedTextureFitsSize && selectedTextureArea <= textureArea)
            continue;

        selectedTexture = texture;
    }

    if (!selectedTexture) {
        selectedTexture = createTexture();
        m_texturePool.append(selectedTexture);
    }

    selectedTexture->reset(size, false);
    return selectedTexture;
}


PassOwnPtr<TextureMapper> TextureMapper::create(AccelerationMode mode)
{
    if (mode == SoftwareMode)
        return TextureMapperImageBuffer::create();
    return platformCreateAccelerated();
}

}
#endif
