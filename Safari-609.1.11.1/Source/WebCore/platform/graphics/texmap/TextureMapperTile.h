/*
 Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies)

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

#ifndef TextureMapperTile_h
#define TextureMapperTile_h

#include "FloatRect.h"
#include "Image.h"
#include "TextureMapper.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class GraphicsLayer;

class TextureMapperTile {
public:
    inline RefPtr<BitmapTexture> texture() const { return m_texture; }
    inline FloatRect rect() const { return m_rect; }
    inline void setTexture(BitmapTexture* texture) { m_texture = texture; }
    inline void setRect(const FloatRect& rect) { m_rect = rect; }

    void updateContents(TextureMapper&, Image*, const IntRect&);
    void updateContents(TextureMapper&, GraphicsLayer*, const IntRect&, float scale = 1);
    WEBCORE_EXPORT virtual void paint(TextureMapper&, const TransformationMatrix&, float, const unsigned exposedEdges);
    virtual ~TextureMapperTile() = default;

    explicit TextureMapperTile(const FloatRect& rect)
        : m_rect(rect)
    {
    }
protected:
    RefPtr<BitmapTexture> m_texture;
private:
    FloatRect m_rect;
};

}

#endif
