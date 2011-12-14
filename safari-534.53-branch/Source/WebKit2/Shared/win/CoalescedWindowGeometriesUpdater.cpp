/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CoalescedWindowGeometriesUpdater.h"

#include "WindowGeometry.h"
#include <wtf/PassOwnPtr.h>

namespace WebKit {

CoalescedWindowGeometriesUpdater::CoalescedWindowGeometriesUpdater()
{
}

CoalescedWindowGeometriesUpdater::~CoalescedWindowGeometriesUpdater()
{
}

void CoalescedWindowGeometriesUpdater::addPendingUpdate(const WindowGeometry& geometry)
{
    m_geometries.set(geometry.window, geometry);
}

static void setWindowRegion(HWND window, PassOwnPtr<HRGN> popRegion)
{
    OwnPtr<HRGN> region = popRegion;

    if (!::SetWindowRgn(window, region.get(), TRUE))
        return;

    // Windows owns the region now.
    region.leakPtr();
}

void CoalescedWindowGeometriesUpdater::updateGeometries(BringToTopOrNot bringToTopOrNot)
{
    HashMap<HWND, WindowGeometry> geometries;
    geometries.swap(m_geometries);

    HDWP deferWindowPos = ::BeginDeferWindowPos(geometries.size());

    for (HashMap<HWND, WindowGeometry>::const_iterator::Values it = geometries.begin().values(), end = geometries.end().values(); it != end; ++it) {
        const WindowGeometry& geometry = *it;

        if (!::IsWindow(geometry.window))
            continue;

        UINT flags = SWP_NOACTIVATE;
        if (geometry.visible)
            flags |= SWP_SHOWWINDOW;
        else
            flags |= SWP_HIDEWINDOW;

        HWND hwndInsertAfter;
        if (bringToTopOrNot == BringToTop)
            hwndInsertAfter = HWND_TOP;
        else {
            hwndInsertAfter = 0;
            flags |= SWP_NOOWNERZORDER | SWP_NOZORDER;
        }

        deferWindowPos = ::DeferWindowPos(deferWindowPos, geometry.window, hwndInsertAfter, geometry.frame.x(), geometry.frame.y(), geometry.frame.width(), geometry.frame.height(), flags);

        setWindowRegion(geometry.window, adoptPtr(::CreateRectRgn(geometry.clipRect.x(), geometry.clipRect.y(), geometry.clipRect.maxX(), geometry.clipRect.maxY())));
    }

    ::EndDeferWindowPos(deferWindowPos);
}

} // namespace WebKit
