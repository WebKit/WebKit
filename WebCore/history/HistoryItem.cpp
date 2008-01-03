/*
 * Copyright (C) 2005, 2006 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "HistoryItem.h"

#include "Document.h"
#include "FrameLoader.h"
#include "IconDatabase.h"
#include "IntSize.h"
#include "KURL.h"
#include "Logging.h"
#include "PageCache.h"
#include "ResourceRequest.h"

namespace WebCore {

static void defaultNotifyHistoryItemChanged() {}
void (*notifyHistoryItemChanged)() = defaultNotifyHistoryItemChanged;

HistoryItem::HistoryItem()
    : m_lastVisitedTime(0)
    , m_isInPageCache(false)
    , m_isTargetItem(false)
    , m_visitCount(0)
{
}

HistoryItem::HistoryItem(const String& urlString, const String& title, double time)
    : m_urlString(urlString)
    , m_originalURLString(urlString)
    , m_title(title)
    , m_lastVisitedTime(time)
    , m_isInPageCache(false)
    , m_isTargetItem(false)
    , m_visitCount(0)
{    
    iconDatabase()->retainIconForPageURL(m_urlString);
}

HistoryItem::HistoryItem(const String& urlString, const String& title, const String& alternateTitle, double time)
    : m_urlString(urlString)
    , m_originalURLString(urlString)
    , m_title(title)
    , m_displayTitle(alternateTitle)
    , m_lastVisitedTime(time)
    , m_isInPageCache(false)
    , m_isTargetItem(false)
    , m_visitCount(0)
{    
    iconDatabase()->retainIconForPageURL(m_urlString);
}

HistoryItem::HistoryItem(const KURL& url, const String& title)
    : m_urlString(url.string())
    , m_originalURLString(url.string())
    , m_title(title)
    , m_lastVisitedTime(0)
    , m_isInPageCache(false)
    , m_isTargetItem(false)
    , m_visitCount(0)
{    
    iconDatabase()->retainIconForPageURL(m_urlString);
}

HistoryItem::HistoryItem(const KURL& url, const String& target, const String& parent, const String& title)
    : m_urlString(url.string())
    , m_originalURLString(url.string())
    , m_target(target)
    , m_parent(parent)
    , m_title(title)
    , m_lastVisitedTime(0)
    , m_isInPageCache(false)
    , m_isTargetItem(false)
    , m_visitCount(0)
{    
    iconDatabase()->retainIconForPageURL(m_urlString);
}

HistoryItem::~HistoryItem()
{
    ASSERT(!m_isInPageCache);
    iconDatabase()->releaseIconForPageURL(m_urlString);
}

HistoryItem::HistoryItem(const HistoryItem& item)
    : RefCounted<HistoryItem>()
    , m_urlString(item.m_urlString)
    , m_originalURLString(item.m_originalURLString)
    , m_target(item.m_target)
    , m_parent(item.m_parent)
    , m_title(item.m_title)
    , m_displayTitle(item.m_displayTitle)
    , m_lastVisitedTime(item.m_lastVisitedTime)
    , m_scrollPoint(item.m_scrollPoint)
    , m_isInPageCache(item.m_isInPageCache)
    , m_isTargetItem(item.m_isTargetItem)
    , m_visitCount(item.m_visitCount)
    , m_formContentType(item.m_formContentType)
    , m_formReferrer(item.m_formReferrer)
    , m_rssFeedReferrer(item.m_rssFeedReferrer)
{
    if (item.m_formData)
        m_formData = item.m_formData->copy();
        
    unsigned size = item.m_subItems.size();
    m_subItems.reserveCapacity(size);
    for (unsigned i = 0; i < size; ++i)
        m_subItems.append(item.m_subItems[i]->copy());
}

PassRefPtr<HistoryItem> HistoryItem::copy() const
{
    return new HistoryItem(*this);
}

const String& HistoryItem::urlString() const
{
    return m_urlString;
}

// The first URL we loaded to get to where this history item points.  Includes both client
// and server redirects.
const String& HistoryItem::originalURLString() const
{
    return m_originalURLString;
}

const String& HistoryItem::title() const
{
    return m_title;
}

const String& HistoryItem::alternateTitle() const
{
    return m_displayTitle;
}

Image* HistoryItem::icon() const
{
    Image* result = iconDatabase()->iconForPageURL(m_urlString, IntSize(16,16));
    return result ? result : iconDatabase()->defaultIcon(IntSize(16,16));
}

double HistoryItem::lastVisitedTime() const
{
    return m_lastVisitedTime;
}

KURL HistoryItem::url() const
{
    return KURL(m_urlString.deprecatedString());
}

KURL HistoryItem::originalURL() const
{
    return KURL(m_originalURLString.deprecatedString());
}

const String& HistoryItem::target() const
{
    return m_target;
}

const String& HistoryItem::parent() const
{
    return m_parent;
}

void HistoryItem::setAlternateTitle(const String& alternateTitle)
{
    m_displayTitle = alternateTitle;
    notifyHistoryItemChanged();
}

void HistoryItem::setURLString(const String& urlString)
{
    if (m_urlString != urlString) {
        iconDatabase()->releaseIconForPageURL(m_urlString);
        m_urlString = urlString;
        iconDatabase()->retainIconForPageURL(m_urlString);
    }
    
    notifyHistoryItemChanged();
}

void HistoryItem::setURL(const KURL& url)
{
    pageCache()->remove(this);
    setURLString(url.string());
    clearDocumentState();
}

void HistoryItem::setOriginalURLString(const String& urlString)
{
    m_originalURLString = urlString;
    notifyHistoryItemChanged();
}

void HistoryItem::setTitle(const String& title)
{
    m_title = title;
    notifyHistoryItemChanged();
}

void HistoryItem::setTarget(const String& target)
{
    m_target = target;
    notifyHistoryItemChanged();
}

void HistoryItem::setParent(const String& parent)
{
    m_parent = parent;
}

void HistoryItem::setLastVisitedTime(double time)
{
    if (m_lastVisitedTime != time) {
        m_lastVisitedTime = time;
        m_visitCount++;
    }
}

int HistoryItem::visitCount() const
{
    return m_visitCount;
}

void HistoryItem::setVisitCount(int count)
{
    m_visitCount = count;
}

const IntPoint& HistoryItem::scrollPoint() const
{
    return m_scrollPoint;
}

void HistoryItem::setScrollPoint(const IntPoint& point)
{
    m_scrollPoint = point;
}

void HistoryItem::clearScrollPoint()
{
    m_scrollPoint.setX(0);
    m_scrollPoint.setY(0);
}

void HistoryItem::setDocumentState(const Vector<String>& state)
{
    m_documentState = state;
}

const Vector<String>& HistoryItem::documentState() const
{
    return m_documentState;
}

void HistoryItem::clearDocumentState()
{
    m_documentState.clear();
}

bool HistoryItem::isTargetItem() const
{
    return m_isTargetItem;
}

void HistoryItem::setIsTargetItem(bool flag)
{
    m_isTargetItem = flag;
}

void HistoryItem::addChildItem(PassRefPtr<HistoryItem> child)
{
    m_subItems.append(child);
}

HistoryItem* HistoryItem::childItemWithName(const String& name) const
{
    unsigned size = m_subItems.size();
    for (unsigned i = 0; i < size; ++i) 
        if (m_subItems[i]->target() == name)
            return m_subItems[i].get();
    return 0;
}

// <rdar://problem/4895849> HistoryItem::recurseToFindTargetItem() should be replace with a non-recursive method
HistoryItem* HistoryItem::recurseToFindTargetItem()
{
    if (m_isTargetItem)
        return this;
    if (!m_subItems.size())
        return 0;
    
    HistoryItem* match;
    unsigned size = m_subItems.size();
    for (unsigned i = 0; i < size; ++i) {
        match = m_subItems[i]->recurseToFindTargetItem();
        if (match)
            return match;
    }
    
    return 0;
}

HistoryItem* HistoryItem::targetItem()
{
    if (!m_subItems.size())
        return this;
    return recurseToFindTargetItem();
}

const HistoryItemVector& HistoryItem::children() const
{
    return m_subItems;
}

bool HistoryItem::hasChildren() const
{
    return m_subItems.size();
}

String HistoryItem::formContentType() const
{
    return m_formContentType;
}

String HistoryItem::formReferrer() const
{
    return m_formReferrer;
}

String HistoryItem::rssFeedReferrer() const
{
    return m_rssFeedReferrer;
}

void HistoryItem::setRSSFeedReferrer(const String& referrer)
{
    m_rssFeedReferrer = referrer;
}

void HistoryItem::setFormInfoFromRequest(const ResourceRequest& request)
{
    if (equalIgnoringCase(request.httpMethod(), "POST")) {
        // FIXME: Eventually we have to make this smart enough to handle the case where
        // we have a stream for the body to handle the "data interspersed with files" feature.
        m_formData = request.httpBody();
        m_formContentType = request.httpContentType();
        m_formReferrer = request.httpReferrer();
    } else {
        m_formData = 0;
        m_formContentType = String();
        m_formReferrer = String();
    }
}

FormData* HistoryItem::formData()
{
    return m_formData.get();
}

bool HistoryItem::isCurrentDocument(Document* doc) const
{
    // FIXME: We should find a better way to check if this is the current document.
    return urlString() == doc->url();
}

void HistoryItem::mergeAutoCompleteHints(HistoryItem* otherItem)
{
    ASSERT(otherItem);
    if (otherItem != this)
        m_visitCount += otherItem->m_visitCount;
}

#ifndef NDEBUG
int HistoryItem::showTree() const
{
    return showTreeWithIndent(0);
}

int HistoryItem::showTreeWithIndent(unsigned indentLevel) const
{
    String prefix("");
    int totalSubItems = 0;
    unsigned i;
    for (i = 0; i < indentLevel; ++i)
        prefix.append("  ");

    fprintf(stderr, "%s+-%s (%p)\n", prefix.ascii().data(), m_urlString.ascii().data(), this);
    
    for (unsigned int i = 0; i < m_subItems.size(); ++i) {
        totalSubItems += m_subItems[i]->showTreeWithIndent(indentLevel + 1);
    }
    return totalSubItems + 1;
}
#endif
                
}; //namespace WebCore

#ifndef NDEBUG
int showTree(const WebCore::HistoryItem* item)
{
    return item->showTree();
}
#endif
