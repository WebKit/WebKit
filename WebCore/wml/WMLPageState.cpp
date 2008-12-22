/*
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * Copyright (C) 2004-2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#if ENABLE(WML)
#include "WMLPageState.h"

#include "HistoryItem.h"
#include "KURL.h"
#include "Page.h"

namespace WebCore {

WMLPageState::WMLPageState(Page* page)
    : m_page(page)
    , m_historyLength(0)
    , m_activeCard(0)
    , m_hasDeckAccess(false)
{
}

WMLPageState::~WMLPageState()
{
    m_variables.clear();
}

void WMLPageState::reset()
{
    // remove all the variables in the current browser context
    m_variables.clear();

    // clear the navigation history state 
    if (m_page)
        m_page->backForwardList()->clearWmlPageHistory();

    // reset implementation-specfic state if UA has
    m_historyLength = 1;
}

bool WMLPageState::setNeedCheckDeckAccess(bool need)
{
    if (m_hasDeckAccess && need)
        return false;

    m_hasDeckAccess = need;
    m_accessPath = String();
    m_accessDomain = String();
    return true;
}

// FIXME: We may want another name, it does far more than just checking wheter the deck is accessable
bool WMLPageState::isDeckAccessible()
{
    if (!m_hasDeckAccess || !m_page || !m_page->backForwardList() || !m_page->backForwardList()->backItem())
        return true;

    HistoryItem* histItem = m_page->backForwardList()->backItem();
    KURL url(histItem->urlString());

    String prevHost = url.host();
    String prevPath = url.path();

    // for 'file' URI, the host part may be empty, so we should complete it.
    if (prevHost.isEmpty())
        prevHost = "localhost";

    histItem = m_page->backForwardList()->currentItem();
    KURL curUrl(histItem->urlString());
    String curPath = curUrl.path();

    if (equalIgnoringRef(url, curUrl))
       return true;

    // "/" is the default value if not specified
    if (m_accessPath.isEmpty())  
        m_accessPath = "/";
    else if (m_accessPath.endsWith("/") && (m_accessPath != "/"))
        m_accessPath = m_accessPath.left((m_accessPath.length()-1));


    // current deck domain is the default value if not specified
    if (m_accessDomain.isEmpty())
        m_accessDomain = prevHost;

    // convert relative URL to absolute URL before performing path matching
    if (prevHost == m_accessDomain) {
        if (!m_accessPath.startsWith("/")) {
           int index = curPath.reverseFind('/');
           if (index != -1) {
               curPath = curPath.left(index + 1);
               curPath += m_accessPath;
               curUrl.setPath(curPath);
               m_accessPath = curUrl.path();
           }
        }
    }

    // path prefix matching
    if (prevPath.startsWith(m_accessPath) && 
       (prevPath[m_accessPath.length()] == '/' || prevPath.length() == m_accessPath.length())) {
        // domain suffix matching
        unsigned domainLength = m_accessDomain.length();
        unsigned hostLength = prevHost.length();
        return (prevHost.endsWith(m_accessDomain) && prevHost[hostLength - domainLength - 1] == '.') || hostLength == domainLength;
    }

    return false;
}

}

#endif
