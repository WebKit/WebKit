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

    if (m_activeSearchString != text) {
        clearTextMatches();
        m_activeSearchString = text;
        m_activeMatchCount = m_webPage->m_page->markAllMatchesForText(text, WebCore::CaseInsensitive, true, TextMatchMarkerLimit);
    } else
        setMarkerActive(m_activeMatch.get(), false);

    if (!m_activeMatchCount)
        return false;

    const FindOptions findOptions = (forward ? 0 : WebCore::Backwards)
        | WebCore::CaseInsensitive
        | WebCore::WrapAround; // TODO should wrap around in page, not in frame

    // TODO StartInSelection

    m_activeMatch = m_webPage->mainFrame()->editor()->findStringAndScrollToVisible(text, m_activeMatch.get(), findOptions);
    setMarkerActive(m_activeMatch.get(), true);

    return true;
}

void InPageSearchManager::clearTextMatches()
{
    m_webPage->m_page->unmarkAllTextMatches();
    m_activeMatch = 0;
}

void InPageSearchManager::setMarkerActive(WebCore::Range* range, bool active)
{
    if (!range)
        return;
    WebCore::Document* doc = m_webPage->mainFrame()->document();
    if (!doc)
        return;
    doc->markers()->setMarkersActive(range, active);
}

}
}
