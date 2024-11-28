/*
 * Copyright (C) 2024 Igalia S.L.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if USE(COORDINATED_GRAPHICS)
#include "FloatRect.h"
#include "IntRect.h"
#include <wtf/Vector.h>

namespace WebCore {
class BitmapTexture;
class CoordinatedTileBuffer;
class TextureMapper;

class CoordinatedBackingStoreTile {
public:
    explicit CoordinatedBackingStoreTile(float scale = 1);
    ~CoordinatedBackingStoreTile();

    BitmapTexture& texture() { ASSERT(canBePainted()); return *m_texture; }
    float scale() const { return m_scale; }
    const FloatRect& rect() { return m_rect; }

    struct Update {
        RefPtr<CoordinatedTileBuffer> buffer;
        IntRect sourceRect;
        IntRect tileRect;
        IntPoint bufferOffset;
    };
    void addUpdate(Update&&);

    void processPendingUpdates(TextureMapper&);

    bool canBePainted() const { return !!m_texture; }

private:
    RefPtr<BitmapTexture> m_texture;
    Vector<Update> m_updates;
    float m_scale { 1. };
    FloatRect m_rect;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
