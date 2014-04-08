/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "TelephoneNumberOverlayController.h"

#if ENABLE(TELEPHONE_NUMBER_DETECTION)

#include "WebPage.h"
#include <WebCore/NotImplemented.h>

using namespace WebCore;

namespace WebKit {
    
TelephoneNumberOverlayController::TelephoneNumberOverlayController(WebPage* webPage)
    : m_webPage(webPage)
    , m_telephoneNumberOverlay(nullptr)
{
    ASSERT(m_webPage);
}

void TelephoneNumberOverlayController::createOverlayIfNeeded()
{
    if (m_telephoneNumberOverlay) {
        m_telephoneNumberOverlay->setNeedsDisplay();
        return;
    }
    
    RefPtr<PageOverlay> overlay = PageOverlay::create(this);
    m_telephoneNumberOverlay = overlay.get();
    m_webPage->installPageOverlay(overlay.release(), PageOverlay::FadeMode::Fade);
    m_telephoneNumberOverlay->setNeedsDisplay();
}

void TelephoneNumberOverlayController::destroyOverlay()
{
    if (!m_telephoneNumberOverlay)
        return;

    m_webPage->uninstallPageOverlay(m_telephoneNumberOverlay, PageOverlay::FadeMode::DoNotFade);
}

void TelephoneNumberOverlayController::pageOverlayDestroyed(PageOverlay*)
{
    // Before the overlay is destroyed, it should have moved out of the WebPage,
    // at which point we already cleared our back pointer.
    ASSERT(!m_telephoneNumberOverlay);
}

void TelephoneNumberOverlayController::willMoveToWebPage(PageOverlay*, WebPage* webPage)
{
    if (webPage)
        return;

    ASSERT(m_telephoneNumberOverlay);
    m_telephoneNumberOverlay = 0;
}

void TelephoneNumberOverlayController::didMoveToWebPage(PageOverlay*, WebPage*)
{
}

void TelephoneNumberOverlayController::selectedTelephoneNumberRangesChanged(const Vector<RefPtr<Range>>& ranges)
{
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED > 1090
    m_currentSelectionRanges = ranges;
    
    clearHighlights();
    
    if (m_currentSelectionRanges.isEmpty())
        destroyOverlay();
    else
        createOverlayIfNeeded();
#else
    UNUSED_PARAM(ranges);
#endif
}

#if !PLATFORM(MAC)
void TelephoneNumberOverlayController::drawRect(PageOverlay*, WebCore::GraphicsContext&, const WebCore::IntRect&)
{
    notImplemented();
}

bool TelephoneNumberOverlayController::mouseEvent(PageOverlay*, const WebMouseEvent&)
{
    notImplemented();
    return false;
}
#endif

} // namespace WebKit

#endif // ENABLE(TELEPHONE_NUMBER_DETECTION)
