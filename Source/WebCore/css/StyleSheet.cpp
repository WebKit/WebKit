/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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
 */
#include "config.h"
#include "StyleSheet.h"

#include "CSSRule.h"
#include "CSSStyleSheet.h"
#include "Document.h"
#include "MediaList.h"
#include "Node.h"

namespace WebCore {

StyleSheet::StyleSheet(Node* parentNode, const String& originalURL, const KURL& finalURL)
    : m_parentRule(0)
    , m_parentNode(parentNode)
    , m_originalURL(originalURL)
    , m_finalURL(finalURL)
    , m_disabled(false)
{
}

StyleSheet::StyleSheet(CSSRule* parentRule, const String& originalURL, const KURL& finalURL)
    : m_parentRule(parentRule)
    , m_parentNode(0)
    , m_originalURL(originalURL)
    , m_finalURL(finalURL)
    , m_disabled(false)
{
}

StyleSheet::~StyleSheet()
{
    if (m_media)
        m_media->setParentStyleSheet(0);
}

void StyleSheet::setMedia(PassRefPtr<MediaList> media)
{
    ASSERT(isCSSStyleSheet());
    ASSERT(!media->parentStyleSheet() || media->parentStyleSheet() == this);

    if (m_media)
        m_media->setParentStyleSheet(0);

    m_media = media;
    m_media->setParentStyleSheet(static_cast<CSSStyleSheet*>(this));
}

KURL StyleSheet::baseURL() const
{
    if (!m_finalURL.isNull())
        return m_finalURL;
    if (StyleSheet* parentSheet = parentStyleSheet())
        return parentSheet->baseURL();
    if (!m_parentNode)
        return KURL();
    return m_parentNode->document()->baseURL();
}

KURL StyleSheet::completeURL(const String& url) const
{
    // Always return a null URL when passed a null string.
    // FIXME: Should we change the KURL constructor to have this behavior?
    // See also Document::completeURL(const String&)
    if (url.isNull())
        return KURL();
    return KURL(baseURL(), url);
}

}
