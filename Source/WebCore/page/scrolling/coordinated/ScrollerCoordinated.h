/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)

#include "ScrollTypes.h"
#include "ScrollerImpAdwaita.h"
#include "ScrollerPairCoordinated.h"
#include "UserInterfaceLayoutDirection.h"
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class CoordinatedPlatformLayer;

class ScrollerCoordinated final : public CanMakeThreadSafeCheckedPtr<ScrollerCoordinated> {
    WTF_MAKE_TZONE_ALLOCATED(ScrollerCoordinated);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ScrollerCoordinated);
public:
    ScrollerCoordinated(ScrollerPairCoordinated&, ScrollbarOrientation);
    ~ScrollerCoordinated();

    void setScrollerImp(ScrollerImpAdwaita*);
    void setHostLayer(CoordinatedPlatformLayer*);
    void updateValues();
    void setEnabled(bool);
    void setOverlayScrollbarEnabled(bool);
    void setUseDarkAppearance(bool);
    void setHoveredAndPressedParts(ScrollbarPart hoveredPart, ScrollbarPart pressedPart);
    void setOpacity(float);
    void setScrollbarLayoutDirection(UserInterfaceLayoutDirection);

private:
    ThreadSafeWeakPtr<ScrollerPairCoordinated> m_pair;
    const ScrollbarOrientation m_orientation;
    Lock m_lock;
    RefPtr<ScrollerImpAdwaita> m_scrollerImp WTF_GUARDED_BY_LOCK(m_lock);
    bool m_needsUpdate WTF_GUARDED_BY_LOCK(m_lock) { true };
    RefPtr<CoordinatedPlatformLayer> m_hostLayer WTF_GUARDED_BY_LOCK(m_lock);
    AdwaitaScrollbarPainter::State m_state WTF_GUARDED_BY_LOCK(m_lock);
    ScrollerPairCoordinated::Values m_currentValue WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
