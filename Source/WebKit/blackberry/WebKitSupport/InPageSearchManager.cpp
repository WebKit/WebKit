/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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

#include "config.h"
#include "InPageSearchManager.h"

#include "DOMSupport.h"
#include "Document.h"
#include "DocumentMarkerController.h"
#include "Editor.h"
#include "Frame.h"
#include "Page.h"
#include "WebPage_p.h"

static const int TextMatchMarkerLimit = 100;

using namespace WebCore;

namespace BlackBerry {
namespace WebKit {

InPageSearchManager::InPageSearchManager(WebPagePrivate* page)
    : m_webPage(page)
    , m_activeMatch(0)
{
}

InPageSearchManager::~InPageSearchManager()
{
}

bool InPageSearchManager::findNextString(const String& text, bool forward)
{
    if (!text.length()) {
        clearTextMatches();
        m_activeSearchString = String();
        return false;
    }

    if (!shouldSearchForText(text)) {
        m_activeSearchString = text;
        return false;
    }

    // Validate the range in case any node has been removed since last search.
    if (m_activeMatch && !m_activeMatch->boundaryPointsValid())
        m_activeMatch = 0;

    RefPtr<Range> searchStartingPoint(m_activeMatch);
    if (m_activeSearchString != text) { // Start a new search.
        m_activeSearchString = text;
        m_webPage->m_page->unmarkAllTextMatches();
        m_activeMatchCount = m_webPage->m_page->markAllMatchesForText(text, CaseInsensitive, true /* shouldHighlight */, TextMatchMarkerLimit);
        if (!m_activeMatchCount) {
            clearTextMatches();
            return false;
        }
    } else { // Search same string for next occurrence.
        setMarkerActive(m_activeMatch.get(), false /* active */);
        // Searching for same string should start from the end of last match.
        if (m_activeMatch) {
            if (forward)
                searchStartingPoint->setStart(searchStartingPoint->endPosition());
            else
                searchStartingPoint->setEnd(searchStartingPoint->startPosition());
        }
    }

    // If there is any active selection, new search should start from the beginning of it.
    VisibleSelection selection = m_webPage->focusedOrMainFrame()->selection()->selection();
    if (!selection.isNone()) {
        searchStartingPoint = selection.firstRange().get();
        m_webPage->focusedOrMainFrame()->selection()->clear();
    }

    Frame* currentActiveMatchFrame = selection.isNone() && m_activeMatch ? m_activeMatch->ownerDocument()->frame() : m_webPage->focusedOrMainFrame();

    const FindOptions findOptions = (forward ? 0 : Backwards)
        | CaseInsensitive
        | StartInSelection;

    if (findAndMarkText(text, searchStartingPoint.get(), currentActiveMatchFrame, findOptions))
        return true;

    Frame* startFrame = currentActiveMatchFrame;
    do {
        currentActiveMatchFrame = DOMSupport::incrementFrame(currentActiveMatchFrame, forward, true /* wrapFlag */);
        if (findAndMarkText(text, 0, currentActiveMatchFrame, findOptions))
            return true;
    } while (currentActiveMatchFrame && startFrame != currentActiveMatchFrame);

    clearTextMatches();

    ASSERT_NOT_REACHED();
    return false;
}

bool InPageSearchManager::shouldSearchForText(const String& text)
{
    if (text == m_activeSearchString)
        return m_activeMatchCount;

    // If the previous search string is prefix of new search string,
    // don't search if the previous one has zero result.
    if (!m_activeMatchCount
        && m_activeSearchString.length()
        && text.length() > m_activeSearchString.length()
        && m_activeSearchString == text.substring(0, m_activeSearchString.length()))
        return false;
    return true;
}

bool InPageSearchManager::findAndMarkText(const String& text, Range* range, Frame* frame, const FindOptions& options)
{
    m_activeMatch = frame->editor()->findStringAndScrollToVisible(text, range, options);
    if (m_activeMatch) {
        setMarkerActive(m_activeMatch.get(), true /* active */);
        return true;
    }
    return false;
}

void InPageSearchManager::clearTextMatches()
{
    m_webPage->m_page->unmarkAllTextMatches();
    m_activeMatch = 0;
}

void InPageSearchManager::setMarkerActive(Range* range, bool active)
{
    if (!range)
        return;
    Document* doc = m_activeMatch->ownerDocument();
    if (!doc)
        return;
    doc->markers()->setMarkersActive(range, active);
}

void InPageSearchManager::frameUnloaded(const Frame* frame)
{
    if (!m_activeMatch) {
        if (m_webPage->mainFrame() == frame && m_activeSearchString.length())
            m_activeSearchString = String();
        return;
    }

    Frame* currentActiveMatchFrame = m_activeMatch->ownerDocument()->frame();
    if (currentActiveMatchFrame == frame) {
        m_activeMatch = 0;
        m_activeSearchString = String();
        m_activeMatchCount = 0;
        // FIXME: We need to notify client here.
        if (frame == m_webPage->mainFrame()) // Don't need to unmark because the page will be destroyed.
            return;
        m_webPage->m_page->unmarkAllTextMatches();
    }
}

}
}
