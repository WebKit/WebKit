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

#pragma once

#include "FloatRect.h"
#include "Image.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class BitmapTexture;
class GraphicsLayer;
class TextureMapper;

class TextureMapperTile {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RefPtr<BitmapTexture> texture() const;
    inline FloatRect rect() const { return m_rect; }
    void setTexture(BitmapTexture*);
    inline void setRect(const FloatRect& rect) { m_rect = rect; }

    void updateContents(Image*, const IntRect&);
    void updateContents(GraphicsLayer*, const IntRect&, float scale = 1);
    WEBCORE_EXPORT virtual void paint(TextureMapper&, const TransformationMatrix&, float, bool allEdgesExposed);
    virtual ~TextureMapperTile();

    explicit TextureMapperTile(const FloatRect&);
protected:
    RefPtr<BitmapTexture> m_texture;
private:
    FloatRect m_rect;
};

} // namespace WebCore
