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

#include "config.h"
#include "UpdateAtlas.h"

#if USE(UI_SIDE_COMPOSITING)

#include "GraphicsContext.h"
#include "IntRect.h"
#include "MathExtras.h"
using namespace WebCore;

namespace WebKit {

UpdateAtlas::UpdateAtlas(int dimension, ShareableBitmap::Flags flags)
    : m_flags(flags)
{
    m_surface = ShareableSurface::create(IntSize(dimension, dimension), flags, ShareableSurface::SupportsGraphicsSurface);
}

static int nextPowerOfTwo(int number)
{
    // This is a fast trick to get nextPowerOfTwo for an integer.
    --number;
    number |= number >> 1;
    number |= number >> 2;
    number |= number >> 4;
    number |= number >> 8;
    number |= number >> 16;
    number++;
    return number;
}

void UpdateAtlas::buildLayoutIfNeeded()
{
    if (!m_layout.isEmpty())
        return;

    static const int MinTileSize = 32;
    static const int MaxTileSize = 512;

    // Divide our square to square power-of-two boxes.
    for (int cursor = 0; cursor < size().width(); ) {
        int remainder = size().width() - cursor;
        int dimension = std::min(remainder, std::min(MaxTileSize, std::max(MinTileSize, nextPowerOfTwo(remainder / 2))));
        cursor += dimension;
        m_layout.append(dimension);
    }

    m_bufferStates.resize(m_layout.size() * m_layout.size());
    for (int i = 0; i < m_bufferStates.size(); ++i)
        m_bufferStates[i] = Available;
}

int UpdateAtlas::findAvailableIndex(const WebCore::IntSize& size)
{
    int dimension = m_layout.size();
    int stride = dimension;
    int requiredDimension = std::max(size.width(), size.height());

    // Begin from the smallest buffer, until we reach the smallest available buffer that's big enough to contain our rect.
    for (int i = m_bufferStates.size() - 1; i >= 0; i -= (dimension + 1), --stride) {
        // Need a bigger buffer.
        if (m_layout[i / dimension] < requiredDimension)
            continue;

        // Check all buffers of current size, to find an available one.
        for (int offset = 0; offset < stride; ++offset) {
            int index = i - offset;
            if (m_bufferStates[index] == Available)
                return index;
        }
    }

    return -1;
}

void UpdateAtlas::didSwapBuffers()
{
    buildLayoutIfNeeded();
    for (int i = 0; i < m_bufferStates.size(); ++i)
        m_bufferStates[i] = Available;
}

PassOwnPtr<GraphicsContext> UpdateAtlas::beginPaintingOnAvailableBuffer(const WebCore::IntSize& size, IntPoint& offset)
{
    buildLayoutIfNeeded();
    int index = findAvailableIndex(size);

    // No available buffer was found, returning null.
    if (index < 0)
        return PassOwnPtr<GraphicsContext>();

    // FIXME: Use tri-state buffers, to allow faster updates.
    m_bufferStates[index] = Taken;
    offset = offsetForIndex(index);
    IntRect rect(IntPoint::zero(), size);
    OwnPtr<GraphicsContext> graphicsContext = m_surface->createGraphicsContext(IntRect(offset, size));

    if (flags() & ShareableBitmap::SupportsAlpha) {
        graphicsContext->setCompositeOperation(CompositeCopy);
        graphicsContext->fillRect(rect, Color::transparent, ColorSpaceDeviceRGB);
        graphicsContext->setCompositeOperation(CompositeSourceOver);
    }

    return graphicsContext.release();
}

IntPoint UpdateAtlas::offsetForIndex(int index) const
{
    IntPoint coord(index % m_layout.size(), index / m_layout.size());
    int x = 0;
    int y = 0;
    for (int i = 0; i < coord.x(); ++i)
        x += m_layout[i];
    for (int i = 0; i < coord.y(); ++i)
        y += m_layout[i];

    return IntPoint(x, y);
}

}
#endif
