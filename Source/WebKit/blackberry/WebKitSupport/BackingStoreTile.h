/*
 * Copyright (C) 2009, 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
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

#ifndef BackingStoreTile_h
#define BackingStoreTile_h

#include "BlackBerryPlatformIntRectRegion.h"
#include "BlackBerryPlatformPrimitives.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace BlackBerry {
namespace Platform {
namespace Graphics {
struct Buffer;
}
}

// Represents a fence that has been inserted into the command stream. You can wait on the corresponding platform sync
// object to make sure that previous commands have been completed.
// There is no reason to wait twice on the same platform sync object, so the only mechanism provided to access the sync
// object will also clear it to prevent anyone from waiting again.
// Fence is only refcounted on the compositing thread, so RefCounted will suffice, we don't need ThreadSafeRefCounted.
class Fence : public RefCounted<Fence> {
public:
    static PassRefPtr<Fence> create(void* platformSync = 0)
    {
        return adoptRef(new Fence(platformSync));
    }

    // Returns 0 if this fence is stale (someone already waited on it)
    // Otherwise returns the platform sync object and clears the sync
    // object so no-one waits again.
    void* takePlatformSync()
    {
        void* tmp = m_platformSync;
        m_platformSync = 0;
        return tmp;
    }

private:
    Fence(void* platformSync)
        : m_platformSync(platformSync)
    {
    }

    void* m_platformSync;
};

namespace WebKit {
class TileBuffer {
    public:
        TileBuffer(const Platform::IntSize&);
        ~TileBuffer();
        Platform::IntSize size() const;
        Platform::IntRect rect() const;

        bool isRendered() const;
        bool isRendered(const Platform::IntRectRegion& contents) const;
        void clearRenderedRegion();
        void clearRenderedRegion(const Platform::IntRectRegion&);
        void addRenderedRegion(const Platform::IntRectRegion&);
        Platform::IntRectRegion renderedRegion() const;
        Platform::IntRectRegion notRenderedRegion() const;

        Platform::Graphics::Buffer* nativeBuffer() const;

        Fence* fence() const { return m_fence.get(); }
        void setFence(PassRefPtr<Fence> fence) { m_fence = fence; }

    private:
        Platform::IntSize m_size;
        Platform::IntRectRegion m_renderedRegion;
        RefPtr<Fence> m_fence;
        mutable Platform::Graphics::Buffer* m_buffer;
};


class BackingStoreTile {
public:
    enum BufferingMode { SingleBuffered, DoubleBuffered };

    static BackingStoreTile* create(const Platform::IntSize& size, BufferingMode mode)
    {
        return new BackingStoreTile(size, mode);
    }

    ~BackingStoreTile();

    Platform::IntSize size() const;
    Platform::IntRect rect() const;

    TileBuffer* frontBuffer() const;
    TileBuffer* backBuffer() const;
    bool isDoubleBuffered() const { return m_bufferingMode == DoubleBuffered; }

    void reset();
    bool backgroundPainted() const { return m_backgroundPainted; }
    void paintBackground();

    bool isCommitted() const { return m_committed; }
    void setCommitted(bool committed) { m_committed = committed; }

    void clearShift() { m_horizontalShift = 0; m_verticalShift = 0; }
    int horizontalShift() const { return m_horizontalShift; }
    void setHorizontalShift(int shift) { m_horizontalShift = shift; }
    int verticalShift() const { return m_verticalShift; }
    void setVerticalShift(int shift) { m_verticalShift = shift; }

    void swapBuffers();

private:
    BackingStoreTile(const Platform::IntSize&, BufferingMode);

    mutable TileBuffer* m_frontBuffer;
    BufferingMode m_bufferingMode;
    bool m_checkered;
    bool m_committed;
    bool m_backgroundPainted;
    int m_horizontalShift;
    int m_verticalShift;
};

} // namespace WebKit
} // namespace BlackBerry

#endif // BackingStoreTile_h
