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
#include "Node.h"
#include "Page.h"
#include "TextIterator.h"
#include "Timer.h"
#include "WebPage_p.h"

static const double MaxScopingDuration = 0.1;

using namespace WebCore;

namespace BlackBerry {
namespace WebKit {

class InPageSearchManager::DeferredScopeStringMatches {
public:
    DeferredScopeStringMatches(InPageSearchManager* ipsm, Frame* scopingFrame, const String& text, bool reset)
    : m_searchManager(ipsm)
    , m_scopingFrame(scopingFrame)
    , m_timer(this, &DeferredScopeStringMatches::doTimeout)
    , m_searchText(text)
    , m_reset(reset)
    {
        m_timer.startOneShot(0.0);
    }

private:
    void doTimeout(Timer<DeferredScopeStringMatches>*)
    {
        m_searchManager->callScopeStringMatches(this, m_scopingFrame, m_searchText, m_reset);
    }
    InPageSearchManager* m_searchManager;
    Frame* m_scopingFrame;
    Timer<DeferredScopeStringMatches> m_timer;
    String m_searchText;
    bool m_reset;
};

InPageSearchManager::InPageSearchManager(WebPagePrivate* page)
    : m_webPage(page)
    , m_activeMatch(0)
    , m_resumeScopingFromRange(0)
    , m_scopingComplete(false)
    , m_locatingActiveMatch(false)
{
}

InPageSearchManager::~InPageSearchManager()
{
    cancelPendingScopingEffort();
}

bool InPageSearchManager::findNextString(const String& text, bool forward)
{
    if (!text.length()) {
        clearTextMatches();
        cancelPendingScopingEffort();
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
    bool newSearch = m_activeSearchString != text;
    if (newSearch) { // Start a new search.
        m_activeSearchString = text;
        cancelPendingScopingEffort();
        m_webPage->m_page->unmarkAllTextMatches();
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

    if (findAndMarkText(text, searchStartingPoint.get(), currentActiveMatchFrame, findOptions, newSearch))
        return true;

    Frame* startFrame = currentActiveMatchFrame;
    do {
        currentActiveMatchFrame = DOMSupport::incrementFrame(currentActiveMatchFrame, forward, true /* wrapFlag */);
        if (findAndMarkText(text, 0, currentActiveMatchFrame, findOptions, newSearch))
            return true;
    } while (currentActiveMatchFrame && startFrame != currentActiveMatchFrame);

    clearTextMatches();

    // FIXME: We need to notify client here.
    return false;
}

bool InPageSearchManager::shouldSearchForText(const String& text)
{
    if (text == m_activeSearchString)
        return m_activeMatchCount;

    // If the previous search string is prefix of new search string,
    // don't search if the previous one has zero result.
    if (m_scopingComplete
        && !m_activeMatchCount
        && m_activeSearchString.length()
        && text.length() > m_activeSearchString.length()
        && m_activeSearchString == text.substring(0, m_activeSearchString.length()))
        return false;
    return true;
}

bool InPageSearchManager::findAndMarkText(const String& text, Range* range, Frame* frame, const FindOptions& options, bool isNewSearch)
{
    m_activeMatch = frame->editor()->findStringAndScrollToVisible(text, range, options);
    if (m_activeMatch) {
        setMarkerActive(m_activeMatch.get(), true /* active */);
        if (isNewSearch) {
            scopeStringMatches(text, true /* reset */);
            // FIXME: If it is a not new search, we need to calculate activeMatchIndex and notify client.
        }
        return true;
    }
    return false;
}

void InPageSearchManager::clearTextMatches()
{
    m_webPage->m_page->unmarkAllTextMatches();
    m_activeMatch = 0;
    m_activeMatchCount = 0;
    m_activeMatchIndex = 0;
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
        // FIXME: We need to re-scope this frame instead of cancelling all effort?
        cancelPendingScopingEffort();
        m_activeMatch = 0;
        m_activeSearchString = String();
        m_activeMatchCount = 0;
        // FIXME: We need to notify client here.
        if (frame == m_webPage->mainFrame()) // Don't need to unmark because the page will be destroyed.
            return;
        m_webPage->m_page->unmarkAllTextMatches();
    }
}

void InPageSearchManager::scopeStringMatches(const String& text, bool reset, Frame* scopingFrame)
{
    if (reset) {
        m_activeMatchCount = 0;
        m_resumeScopingFromRange = 0;
        m_scopingComplete = false;
        m_locatingActiveMatch = true;
        // New search should always start from mainFrame.
        scopeStringMatchesSoon(m_webPage->mainFrame(), text, false /* reset */);
        return;
    }

    if (m_resumeScopingFromRange && scopingFrame != m_resumeScopingFromRange->ownerDocument()->frame())
        m_resumeScopingFromRange = 0;

    RefPtr<Range> searchRange(rangeOfContents(scopingFrame->document()));
    Node* originalEndContainer = searchRange->endContainer();
    int originalEndOffset = searchRange->endOffset();
    ExceptionCode ec = 0, ec2 = 0;
    if (m_resumeScopingFromRange) {
        searchRange->setStart(m_resumeScopingFromRange->startContainer(), m_resumeScopingFromRange->startOffset(ec2) + 1, ec);
        if (ec || ec2) {
            m_scopingComplete = true; // We should stop scoping because of some stale data.
            return;
        }
    }

    int matchCount = 0;
    bool timeout = false;
    double startTime = currentTime();
    do {
        RefPtr<Range> resultRange(findPlainText(searchRange.get(), text, CaseInsensitive));
        if (resultRange->collapsed(ec)) {
            if (!resultRange->startContainer()->isInShadowTree())
                break;
            searchRange->setStartAfter(resultRange->startContainer()->shadowAncestorNode(), ec);
            searchRange->setEnd(originalEndContainer, originalEndOffset, ec);
            continue;
        }

        if (scopingFrame->editor()->insideVisibleArea(resultRange.get())) {
            ++matchCount;
            bool foundActiveMatch = false;
            if (m_locatingActiveMatch && areRangesEqual(resultRange.get(), m_activeMatch.get())) {
                foundActiveMatch = true;
                m_locatingActiveMatch = false;
                m_activeMatchIndex = m_activeMatchCount + matchCount;
                // FIXME: We need to notify client with m_activeMatchIndex.
            }
            resultRange->ownerDocument()->markers()->addTextMatchMarker(resultRange.get(), foundActiveMatch);
        }
        searchRange->setStart(resultRange->endContainer(ec), resultRange->endOffset(ec), ec);
        Node* shadowTreeRoot = searchRange->shadowTreeRootNode();
        if (searchRange->collapsed(ec) && shadowTreeRoot)
            searchRange->setEnd(shadowTreeRoot, shadowTreeRoot->childNodeCount(), ec);
        m_resumeScopingFromRange = resultRange;
        timeout = (currentTime() - startTime) >= MaxScopingDuration;
    } while (!timeout);

    if (matchCount > 0) {
        scopingFrame->editor()->setMarkedTextMatchesAreHighlighted(true /* highlight */);
        m_activeMatchCount += matchCount;
    }

    if (timeout)
        scopeStringMatchesSoon(scopingFrame, text, false /* reset */);
    else {
        // Scoping is done for this frame.
        Frame* nextFrame = DOMSupport::incrementFrame(scopingFrame, true /* forward */, false /* wrapFlag */);
        if (!nextFrame) {
            m_scopingComplete = true;
            return; // Scoping is done for all frames;
        }
        scopeStringMatchesSoon(nextFrame, text, false /* reset */);
    }
}

void InPageSearchManager::scopeStringMatchesSoon(Frame* scopingFrame, const String& text, bool reset)
{
    m_deferredScopingWork.append(new DeferredScopeStringMatches(this, scopingFrame, text, reset));
}

void InPageSearchManager::callScopeStringMatches(DeferredScopeStringMatches* caller, Frame* scopingFrame, const String& text, bool reset)
{
    m_deferredScopingWork.remove(m_deferredScopingWork.find(caller));
    scopeStringMatches(text, reset, scopingFrame);
    delete caller;
}

void InPageSearchManager::cancelPendingScopingEffort()
{
    deleteAllValues(m_deferredScopingWork);
    m_deferredScopingWork.clear();
}

}
}
