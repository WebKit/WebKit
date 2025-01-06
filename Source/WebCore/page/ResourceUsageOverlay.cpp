/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if ENABLE(RESOURCE_USAGE)

#include "ResourceUsageOverlay.h"

#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Page.h"
#include "PageOverlayController.h"
#include "PlatformMouseEvent.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ResourceUsageOverlay);

Ref<ResourceUsageOverlay> ResourceUsageOverlay::create(Page& page)
{
    return adoptRef(*new ResourceUsageOverlay(page));
}

ResourceUsageOverlay::ResourceUsageOverlay(Page& page)
    : m_page(page)
    , m_overlay(PageOverlay::create(*this, PageOverlay::OverlayType::View))
{
    ASSERT(isMainThread());
    // Let the event loop cycle before continuing with initialization.
    // This way we'll have access to the FrameView's dimensions.
    callOnMainThread([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->initialize();
    });
}

ResourceUsageOverlay::~ResourceUsageOverlay()
{
    ASSERT(isMainThread());
    platformDestroy();
    if (RefPtr page = m_page.get())
        page->pageOverlayController().uninstallPageOverlay(*m_overlay.copyRef(), PageOverlay::FadeMode::DoNotFade);
}

void ResourceUsageOverlay::initialize()
{
    RefPtr page = m_page.get();
    if (!page)
        return;
    auto* frameView = page->mainFrame().virtualView();
    if (!frameView)
        return;
    IntRect initialRect(frameView->width() / 2 - normalWidth / 2, frameView->height() - normalHeight - 20, normalWidth, normalHeight);

#if PLATFORM(IOS_FAMILY)
    // FIXME: The overlay should be stuck to the viewport instead of moving along with the page.
    initialRect.setY(20);
#endif

    RefPtr overlay = m_overlay;
    overlay->setFrame(initialRect);
    page->pageOverlayController().installPageOverlay(*overlay, PageOverlay::FadeMode::DoNotFade);
    platformInitialize();
}

bool ResourceUsageOverlay::mouseEvent(PageOverlay&, const PlatformMouseEvent& event)
{
    if (event.button() != MouseButton::Left)
        return false;

    RefPtr overlay = m_overlay;
    switch (event.type()) {
    case PlatformEvent::Type::MousePressed: {
        overlay->setShouldIgnoreMouseEventsOutsideBounds(false);
        m_dragging = true;
        IntPoint location = overlay->frame().location();
        m_dragPoint = event.position() + IntPoint(-location.x(), -location.y());
        return true;
    }
    case PlatformEvent::Type::MouseReleased:
        if (m_dragging) {
            overlay->setShouldIgnoreMouseEventsOutsideBounds(true);
            m_dragging = false;
            return true;
        }
        break;
    case PlatformEvent::Type::MouseMoved:
        if (m_dragging) {
            RefPtr page = m_page.get();
            if (!page)
                return false;
            IntRect newFrame = overlay->frame();

            // Move the new frame relative to the point where the drag was initiated.
            newFrame.setLocation(event.position());
            newFrame.moveBy(IntPoint(-m_dragPoint.x(), -m_dragPoint.y()));

            // Force the frame to stay inside the viewport entirely.
            if (newFrame.x() < 0)
                newFrame.setX(0);
            if (newFrame.y() < page->topContentInset())
                newFrame.setY(page->topContentInset());
            auto& frameView = *page->mainFrame().virtualView();
            if (newFrame.maxX() > frameView.width())
                newFrame.setX(frameView.width() - newFrame.width());
            if (newFrame.maxY() > frameView.height())
                newFrame.setY(frameView.height() - newFrame.height());

            overlay->setFrame(newFrame);
            overlay->setNeedsDisplay();
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

}

#endif
