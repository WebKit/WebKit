/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)

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

#include "TextureMapperPlatformLayer.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class FloatRect;
class TextureMapper;

class TextureMapperBackingStore : public TextureMapperPlatformLayer {
public:
    virtual ~TextureMapperBackingStore() = default;

    void paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix&, float) override = 0;
    virtual void drawRepaintCounter(TextureMapper&, int, const Color&, const FloatRect&, const TransformationMatrix&) { }

protected:
    WEBCORE_EXPORT static unsigned calculateExposedTileEdges(const FloatRect& totalRect, const FloatRect& tileRect);
};

} // namespace WebCore
