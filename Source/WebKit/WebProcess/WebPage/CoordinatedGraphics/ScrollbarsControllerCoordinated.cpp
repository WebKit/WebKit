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

#include "config.h"
#include "ScrollbarsControllerCoordinated.h"

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)

#include <WebCore/ScrollableArea.h>
#include <WebCore/ScrollbarTheme.h>
#include <WebCore/ScrollbarsControllerInlines.h>
#include <WebCore/ScrollingCoordinator.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollbarsControllerCoordinated);

ScrollbarsControllerCoordinated::ScrollbarsControllerCoordinated(WebCore::ScrollableArea& scrollableArea, WebCore::ScrollingCoordinator* coordinator)
    : WebCore::ScrollbarsControllerGeneric(scrollableArea)
    , m_coordinator(ThreadSafeWeakPtr<WebCore::ScrollingCoordinator>(coordinator))
{
    if (auto scrollingCoordinator = m_coordinator.get())
        scrollingCoordinator->setScrollbarWidth(scrollableArea, scrollableArea.scrollbarWidthStyle());
}

void ScrollbarsControllerCoordinated::scrollbarLayoutDirectionChanged(WebCore::UserInterfaceLayoutDirection scrollbarLayoutDirection)
{
    WebCore::ScrollbarsControllerGeneric::scrollbarLayoutDirectionChanged(scrollbarLayoutDirection);

    if (RefPtr scrollingCoordinator = m_coordinator.get())
        scrollingCoordinator->setScrollbarLayoutDirection(checkedScrollableArea(), scrollbarLayoutDirection);
}

bool ScrollbarsControllerCoordinated::shouldDrawIntoScrollbarLayer(WebCore::Scrollbar& scrollbar) const
{
    return scrollbar.isCustomScrollbar() || scrollbar.isMockScrollbar();
}

void ScrollbarsControllerCoordinated::updateScrollbarEnabledState(WebCore::Scrollbar& scrollbar)
{
    if (auto scrollingCoordinator = m_coordinator.get())
        scrollingCoordinator->setScrollbarEnabled(scrollbar);
}

void ScrollbarsControllerCoordinated::updateScrollbarStyle()
{
    auto& theme = WebCore::ScrollbarTheme::theme();
    if (theme.isMockTheme())
        return;

    // The different scrollbar styles have different thicknesses, so we must re-set the
    // frameRect to the new thickness, and the re-layout below will ensure the position
    // and length are properly updated.
    updateScrollbarsThickness();

    checkedScrollableArea()->scrollbarStyleChanged(theme.usesOverlayScrollbars() ? WebCore::ScrollbarStyle::Overlay : WebCore::ScrollbarStyle::AlwaysVisible, true);
}

void ScrollbarsControllerCoordinated::scrollbarOpacityChanged()
{
    if (auto scrollingCoordinator = m_coordinator.get())
        scrollingCoordinator->setScrollbarOpacity(checkedScrollableArea());
}

void ScrollbarsControllerCoordinated::hoveredPartChanged(WebCore::Scrollbar& scrollbar)
{
    if (auto scrollingCoordinator = m_coordinator.get())
        scrollingCoordinator->setHoveredAndPressedScrollbarParts(checkedScrollableArea());
}

void ScrollbarsControllerCoordinated::pressedPartChanged(WebCore::Scrollbar& scrollbar)
{
    if (auto scrollingCoordinator = m_coordinator.get())
        scrollingCoordinator->setHoveredAndPressedScrollbarParts(checkedScrollableArea());
}

}
#endif // USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
