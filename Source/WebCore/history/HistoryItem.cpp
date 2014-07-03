/*
 * Copyright (C) 2005, 2006, 2008, 2011, 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include "CachedPage.h"
#include "Document.h"
#include "IconDatabase.h"
#include "KeyedCoding.h"
#include "PageCache.h"
#include "ResourceRequest.h"
#include "SerializedScriptValue.h"
#include "SharedBuffer.h"
#include <stdio.h>
#include <wtf/CurrentTime.h>
#include <wtf/DateMath.h>
#include <wtf/text/CString.h>

namespace WebCore {

static long long generateSequenceNumber()
{
    // Initialize to the current time to reduce the likelihood of generating
    // identifiers that overlap with those from past/future browser sessions.
    static long long next = static_cast<long long>(currentTime() * 1000000.0);
    return ++next;
}

static void defaultNotifyHistoryItemChanged(HistoryItem*)
{
}

void (*notifyHistoryItemChanged)(HistoryItem*) = defaultNotifyHistoryItemChanged;

HistoryItem::HistoryItem()
    : m_pageScaleFactor(0)
    , m_lastVisitWasFailure(false)
    , m_isTargetItem(false)
    , m_itemSequenceNumber(generateSequenceNumber())
    , m_documentSequenceNumber(generateSequenceNumber())
    , m_next(0)
    , m_prev(0)
#if PLATFORM(IOS)
    , m_scale(0)
    , m_scaleIsInitial(false)
    , m_bookmarkID(0)
#endif
{
}

HistoryItem::HistoryItem(const String& urlString, const String& title)
    : m_urlString(urlString)
    , m_originalURLString(urlString)
    , m_title(title)
    , m_pageScaleFactor(0)
    , m_lastVisitWasFailure(false)
    , m_isTargetItem(false)
    , m_itemSequenceNumber(generateSequenceNumber())
    , m_documentSequenceNumber(generateSequenceNumber())
    , m_next(0)
    , m_prev(0)
#if PLATFORM(IOS)
    , m_scale(0)
    , m_scaleIsInitial(false)
    , m_bookmarkID(0)
#endif
{    
    iconDatabase().retainIconForPageURL(m_urlString);
}

HistoryItem::HistoryItem(const String& urlString, const String& title, const String& alternateTitle)
    : m_urlString(urlString)
    , m_originalURLString(urlString)
    , m_title(title)
    , m_displayTitle(alternateTitle)
    , m_pageScaleFactor(0)
    , m_lastVisitWasFailure(false)
    , m_isTargetItem(false)
    , m_itemSequenceNumber(generateSequenceNumber())
    , m_documentSequenceNumber(generateSequenceNumber())
    , m_next(0)
    , m_prev(0)
#if PLATFORM(IOS)
    , m_scale(0)
    , m_scaleIsInitial(false)
    , m_bookmarkID(0)
#endif
{
    iconDatabase().retainIconForPageURL(m_urlString);
}

HistoryItem::HistoryItem(const URL& url, const String& target, const String& parent, const String& title)
    : m_urlString(url.string())
    , m_originalURLString(url.string())
    , m_target(target)
    , m_parent(parent)
    , m_title(title)
    , m_pageScaleFactor(0)
    , m_lastVisitWasFailure(false)
    , m_isTargetItem(false)
    , m_itemSequenceNumber(generateSequenceNumber())
    , m_documentSequenceNumber(generateSequenceNumber())
    , m_next(0)
    , m_prev(0)
#if PLATFORM(IOS)
    , m_scale(0)
    , m_scaleIsInitial(false)
    , m_bookmarkID(0)
#endif
{    
    iconDatabase().retainIconForPageURL(m_urlString);
}

HistoryItem::~HistoryItem()
{
    ASSERT(!m_cachedPage);
    iconDatabase().releaseIconForPageURL(m_urlString);
}

inline HistoryItem::HistoryItem(const HistoryItem& item)
    : RefCounted<HistoryItem>()
    , m_urlString(item.m_urlString)
    , m_originalURLString(item.m_originalURLString)
    , m_referrer(item.m_referrer)
    , m_target(item.m_target)
    , m_parent(item.m_parent)
    , m_title(item.m_title)
    , m_displayTitle(item.m_displayTitle)
    , m_scrollPoint(item.m_scrollPoint)
    , m_pageScaleFactor(item.m_pageScaleFactor)
    , m_lastVisitWasFailure(item.m_lastVisitWasFailure)
    , m_isTargetItem(item.m_isTargetItem)
    , m_itemSequenceNumber(item.m_itemSequenceNumber)
    , m_documentSequenceNumber(item.m_documentSequenceNumber)
    , m_formContentType(item.m_formContentType)
#if PLATFORM(IOS)
    , m_scale(item.m_scale)
    , m_scaleIsInitial(item.m_scaleIsInitial)
    , m_bookmarkID(item.m_bookmarkID)
    , m_sharedLinkUniqueIdentifier(item.m_sharedLinkUniqueIdentifier)
#endif
{
    if (item.m_formData)
        m_formData = item.m_formData->copy();
        
    unsigned size = item.m_children.size();
    m_children.reserveInitialCapacity(size);
    for (unsigned i = 0; i < size; ++i)
        m_children.uncheckedAppend(item.m_children[i]->copy());

    if (item.m_redirectURLs)
        m_redirectURLs = std::make_unique<Vector<String>>(*item.m_redirectURLs);
}

PassRefPtr<HistoryItem> HistoryItem::copy() const
{
    return adoptRef(new HistoryItem(*this));
}

void HistoryItem::reset()
{
    iconDatabase().releaseIconForPageURL(m_urlString);

    m_urlString = String();
    m_originalURLString = String();
    m_referrer = String();
    m_target = String();
    m_parent = String();
    m_title = String();
    m_displayTitle = String();

    m_lastVisitWasFailure = false;
    m_isTargetItem = false;

    m_redirectURLs = nullptr;

    m_itemSequenceNumber = generateSequenceNumber();

    m_stateObject = 0;
    m_documentSequenceNumber = generateSequenceNumber();

    m_formData = 0;
    m_formContentType = String();

    clearChildren();
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

bool HistoryItem::hasCachedPageExpired() const
{
    return m_cachedPage ? m_cachedPage->hasExpired() : false;
}

URL HistoryItem::url() const
{
    return URL(ParsedURLString, m_urlString);
}

URL HistoryItem::originalURL() const
{
    return URL(ParsedURLString, m_originalURLString);
}

const String& HistoryItem::referrer() const
{
    return m_referrer;
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
    notifyHistoryItemChanged(this);
}

void HistoryItem::setURLString(const String& urlString)
{
    if (m_urlString != urlString) {
        iconDatabase().releaseIconForPageURL(m_urlString);
        m_urlString = urlString;
        iconDatabase().retainIconForPageURL(m_urlString);
    }
    
    notifyHistoryItemChanged(this);
}

void HistoryItem::setURL(const URL& url)
{
    pageCache()->remove(this);
    setURLString(url.string());
    clearDocumentState();
}

void HistoryItem::setOriginalURLString(const String& urlString)
{
    m_originalURLString = urlString;
    notifyHistoryItemChanged(this);
}

void HistoryItem::setReferrer(const String& referrer)
{
    m_referrer = referrer;
    notifyHistoryItemChanged(this);
}

void HistoryItem::setTitle(const String& title)
{
    m_title = title;
    notifyHistoryItemChanged(this);
}

void HistoryItem::setTarget(const String& target)
{
    m_target = target;
    notifyHistoryItemChanged(this);
}

void HistoryItem::setParent(const String& parent)
{
    m_parent = parent;
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

float HistoryItem::pageScaleFactor() const
{
    return m_pageScaleFactor;
}

void HistoryItem::setPageScaleFactor(float scaleFactor)
{
    m_pageScaleFactor = scaleFactor;
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

void HistoryItem::setStateObject(PassRefPtr<SerializedScriptValue> object)
{
    m_stateObject = object;
}

void HistoryItem::addChildItem(PassRefPtr<HistoryItem> child)
{
    ASSERT(!childItemWithTarget(child->target()));
    m_children.append(child);
}

void HistoryItem::setChildItem(PassRefPtr<HistoryItem> child)
{
    ASSERT(!child->isTargetItem());
    unsigned size = m_children.size();
    for (unsigned i = 0; i < size; ++i)  {
        if (m_children[i]->target() == child->target()) {
            child->setIsTargetItem(m_children[i]->isTargetItem());
            m_children[i] = child;
            return;
        }
    }
    m_children.append(child);
}

HistoryItem* HistoryItem::childItemWithTarget(const String& target) const
{
    unsigned size = m_children.size();
    for (unsigned i = 0; i < size; ++i) {
        if (m_children[i]->target() == target)
            return m_children[i].get();
    }
    return 0;
}

HistoryItem* HistoryItem::childItemWithDocumentSequenceNumber(long long number) const
{
    unsigned size = m_children.size();
    for (unsigned i = 0; i < size; ++i) {
        if (m_children[i]->documentSequenceNumber() == number)
            return m_children[i].get();
    }
    return 0;
}

// <rdar://problem/4895849> HistoryItem::findTargetItem() should be replaced with a non-recursive method.
HistoryItem* HistoryItem::findTargetItem()
{
    if (m_isTargetItem)
        return this;
    unsigned size = m_children.size();
    for (unsigned i = 0; i < size; ++i) {
        if (HistoryItem* match = m_children[i]->targetItem())
            return match;
    }
    return 0;
}

HistoryItem* HistoryItem::targetItem()
{
    HistoryItem* foundItem = findTargetItem();
    return foundItem ? foundItem : this;
}

const HistoryItemVector& HistoryItem::children() const
{
    return m_children;
}

bool HistoryItem::hasChildren() const
{
    return !m_children.isEmpty();
}

void HistoryItem::clearChildren()
{
    m_children.clear();
}

bool HistoryItem::isAncestorOf(const HistoryItem* item) const
{
    for (size_t i = 0; i < m_children.size(); ++i) {
        HistoryItem* child = m_children[i].get();
        if (child == item)
            return true;
        if (child->isAncestorOf(item))
            return true;
    }
    return false;
}

// We do same-document navigation if going to a different item and if either of the following is true:
// - The other item corresponds to the same document (for history entries created via pushState or fragment changes).
// - The other item corresponds to the same set of documents, including frames (for history entries created via regular navigation)
bool HistoryItem::shouldDoSameDocumentNavigationTo(HistoryItem* otherItem) const
{
    if (this == otherItem)
        return false;

    if (stateObject() || otherItem->stateObject())
        return documentSequenceNumber() == otherItem->documentSequenceNumber();
    
    if ((url().hasFragmentIdentifier() || otherItem->url().hasFragmentIdentifier()) && equalIgnoringFragmentIdentifier(url(), otherItem->url()))
        return documentSequenceNumber() == otherItem->documentSequenceNumber();        
    
    return hasSameDocumentTree(otherItem);
}

// Does a recursive check that this item and its descendants have the same
// document sequence numbers as the other item.
bool HistoryItem::hasSameDocumentTree(HistoryItem* otherItem) const
{
    if (documentSequenceNumber() != otherItem->documentSequenceNumber())
        return false;
        
    if (children().size() != otherItem->children().size())
        return false;

    for (size_t i = 0; i < children().size(); i++) {
        HistoryItem* child = children()[i].get();
        HistoryItem* otherChild = otherItem->childItemWithDocumentSequenceNumber(child->documentSequenceNumber());
        if (!otherChild || !child->hasSameDocumentTree(otherChild))
            return false;
    }

    return true;
}

// Does a non-recursive check that this item and its immediate children have the
// same frames as the other item.
bool HistoryItem::hasSameFrames(HistoryItem* otherItem) const
{
    if (target() != otherItem->target())
        return false;
        
    if (children().size() != otherItem->children().size())
        return false;

    for (size_t i = 0; i < children().size(); i++) {
        if (!otherItem->childItemWithTarget(children()[i]->target()))
            return false;
    }

    return true;
}

String HistoryItem::formContentType() const
{
    return m_formContentType;
}

void HistoryItem::setFormInfoFromRequest(const ResourceRequest& request)
{
    m_referrer = request.httpReferrer();
    
    if (equalIgnoringCase(request.httpMethod(), "POST")) {
        // FIXME: Eventually we have to make this smart enough to handle the case where
        // we have a stream for the body to handle the "data interspersed with files" feature.
        m_formData = request.httpBody();
        m_formContentType = request.httpContentType();
    } else {
        m_formData = 0;
        m_formContentType = String();
    }
}

void HistoryItem::setFormData(PassRefPtr<FormData> formData)
{
    m_formData = formData;
}

void HistoryItem::setFormContentType(const String& formContentType)
{
    m_formContentType = formContentType;
}

FormData* HistoryItem::formData()
{
    return m_formData.get();
}

bool HistoryItem::isCurrentDocument(Document* doc) const
{
    // FIXME: We should find a better way to check if this is the current document.
    return equalIgnoringFragmentIdentifier(url(), doc->url());
}

void HistoryItem::addRedirectURL(const String& url)
{
    if (!m_redirectURLs)
        m_redirectURLs = std::make_unique<Vector<String>>();

    // Our API allows us to store all the URLs in the redirect chain, but for
    // now we only have a use for the final URL.
    (*m_redirectURLs).resize(1);
    (*m_redirectURLs)[0] = url;
}

Vector<String>* HistoryItem::redirectURLs() const
{
    return m_redirectURLs.get();
}

void HistoryItem::setRedirectURLs(std::unique_ptr<Vector<String>> redirectURLs)
{
    m_redirectURLs = WTF::move(redirectURLs);
}

#ifndef NDEBUG

int HistoryItem::showTree() const
{
    return showTreeWithIndent(0);
}

int HistoryItem::showTreeWithIndent(unsigned indentLevel) const
{
    Vector<char> prefix;
    for (unsigned i = 0; i < indentLevel; ++i)
        prefix.append("  ", 2);
    prefix.append("\0", 1);

    fprintf(stderr, "%s+-%s (%p)\n", prefix.data(), m_urlString.utf8().data(), this);
    
    int totalSubItems = 0;
    for (unsigned i = 0; i < m_children.size(); ++i)
        totalSubItems += m_children[i]->showTreeWithIndent(indentLevel + 1);
    return totalSubItems + 1;
}

#endif
                
} // namespace WebCore

#ifndef NDEBUG

int showTree(const WebCore::HistoryItem* item)
{
    return item->showTree();
}

#endif
