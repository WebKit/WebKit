/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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

#pragma once

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)

#include <WebCore/ScrollbarsControllerGeneric.h>
#include <WebCore/UserInterfaceLayoutDirection.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebCore {
class ScrollingCoordinator;
class Color;
}

namespace WebKit {

class ScrollbarsControllerCoordinated final : public WebCore::ScrollbarsControllerGeneric {
    WTF_MAKE_TZONE_ALLOCATED(ScrollbarsControllerCoordinated);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ScrollbarsControllerCoordinated);
public:
    ScrollbarsControllerCoordinated(WebCore::ScrollableArea&, WebCore::ScrollingCoordinator*);
    bool shouldDrawIntoScrollbarLayer(WebCore::Scrollbar&) const final;
    void updateScrollbarEnabledState(WebCore::Scrollbar&) final;
    void scrollbarLayoutDirectionChanged(WebCore::UserInterfaceLayoutDirection) final;
    void scrollbarOpacityChanged() final;
    void updateScrollbarStyle() final;
    bool isScrollbarsControllerCoordinated() const final { return true; }
    void hoveredPartChanged(WebCore::Scrollbar&) final;
    void pressedPartChanged(WebCore::Scrollbar&) final;

private:
    ThreadSafeWeakPtr<WebCore::ScrollingCoordinator> m_coordinator;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::ScrollbarsControllerCoordinated)
    static bool isType(const WebCore::ScrollbarsController& controller) { return controller.isScrollbarsControllerCoordinated(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
