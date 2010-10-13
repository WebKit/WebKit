/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "FindController.h"

#include "FindPageOverlay.h"
#include "WebPage.h"
#include <WebCore/Frame.h>
#include <WebCore/Page.h>

using namespace WebCore;

namespace WebKit {

FindController::FindController(WebPage* webPage)
    : m_webPage(webPage)
    , m_findPageOverlay(0)
{
}

static Frame* frameWithSelection(Page* page)
{
    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->selection()->isRange())
            return frame;
    }

    return 0;
}

void FindController::findString(const String& string, FindDirection findDirection, FindOptions findOptions, unsigned maxNumMatches)
{
    TextCaseSensitivity caseSensitivity = findOptions & FindOptionsCaseInsensitive ? TextCaseInsensitive : TextCaseSensitive;
    bool found = m_webPage->corePage()->findString(string, caseSensitivity,
                                                   findDirection == FindDirectionForward ? WebCore::FindDirectionForward : WebCore::FindDirectionBackward,
                                                   findOptions & FindOptionsWrapAround);

    Frame* selectedFrame = frameWithSelection(m_webPage->corePage());

    bool shouldShowOverlay = false;

    if (!found) {
        // We didn't find the string, clear all text matches.
        m_webPage->corePage()->unmarkAllTextMatches();

        // And clear the selection.
        if (selectedFrame)
            selectedFrame->selection()->clear();
    } else {
        shouldShowOverlay = findOptions & FindOptionsShowOverlay;

        if (shouldShowOverlay) {
            unsigned numMatches = m_webPage->corePage()->markAllMatchesForText(string, caseSensitivity, false, maxNumMatches);

            // Check if we have more matches than allowed.
            if (numMatches > maxNumMatches)
                shouldShowOverlay = false;
        }
    }

    if (!shouldShowOverlay) {
        if (m_findPageOverlay) {
            // Get rid of the overlay.
            m_webPage->uninstallPageOverlay();
        }
        
        ASSERT(!m_findPageOverlay);
        return;
    }

    if (!m_findPageOverlay) {
        OwnPtr<FindPageOverlay> findPageOverlay = FindPageOverlay::create(this);
        m_findPageOverlay = findPageOverlay.get();
        m_webPage->installPageOverlay(findPageOverlay.release());
    } else {
        // The page overlay needs to be repainted.
        m_findPageOverlay->setNeedsDisplay();
    }
}

void FindController::hideFindUI()
{
    // FIXME: Implement.
}

void FindController::findPageOverlayDestroyed()
{
    ASSERT(m_findPageOverlay);
    m_findPageOverlay = 0;
}

} // namespace WebKit
