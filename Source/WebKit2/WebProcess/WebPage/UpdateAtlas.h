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

#ifndef UpdateAtlas_h
#define UpdateAtlas_h

#include "ShareableSurface.h"

#if USE(UI_SIDE_COMPOSITING)
namespace WebCore {
class GraphicsContext;
class IntRect;
}

namespace WebKit {

class UpdateAtlas {
public:
    UpdateAtlas(int dimension, ShareableBitmap::Flags);

    PassRefPtr<ShareableSurface> surface() { return m_surface; }
    inline WebCore::IntSize size() const { return m_surface->size(); }

    // Returns a null pointer of there is no available buffer.
    PassOwnPtr<WebCore::GraphicsContext> beginPaintingOnAvailableBuffer(const WebCore::IntSize&, WebCore::IntPoint& offset);
    void didSwapBuffers();
    ShareableBitmap::Flags flags() const { return m_flags; }

private:
    void buildLayoutIfNeeded();
    WebCore::IntPoint offsetForIndex(int) const;
    int findAvailableIndex(const WebCore::IntSize&);

private:
    enum State {
        Available,
        Taken
    };

    Vector<State> m_bufferStates;
    Vector<int> m_layout;
    ShareableBitmap::Flags m_flags;
    RefPtr<ShareableSurface> m_surface;
};

}
#endif
#endif // UpdateAtlas_h
