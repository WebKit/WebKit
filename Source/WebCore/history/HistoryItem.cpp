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
#include "KeyedCoding.h"
#include "PageCache.h"
#include "ResourceRequest.h"
#include "SerializedScriptValue.h"
#include "SharedBuffer.h"
#include <stdio.h>
#include <wtf/DateMath.h>
#include <wtf/DebugUtilities.h>
#include <wtf/WallTime.h>
#include <wtf/text/CString.h>

namespace WebCore {

int64_t HistoryItem::generateSequenceNumber()
{
    // Initialize to the current time to reduce the likelihood of generating
    // identifiers that overlap with those from past/future browser sessions.
    static long long next = static_cast<long long>(WallTime::now().secondsSinceEpoch().microseconds());
    return ++next;
}

static void defaultNotifyHistoryItemChanged(HistoryItem&)
{
}

void (*notifyHistoryItemChanged)(HistoryItem&) = defaultNotifyHistoryItemChanged;

HistoryItem::HistoryItem()
    : HistoryItem({ }, { })
{
}

HistoryItem::HistoryItem(const String& urlString, const String& title)
    : HistoryItem(urlString, title, { })
{
}

HistoryItem::HistoryItem(const String& urlString, const String& title, const String& alternateTitle)
    : HistoryItem(urlString, title, alternateTitle, { Process::identifier(), generateObjectIdentifier<BackForwardItemIdentifier::ItemIdentifierType>() })
{
}

HistoryItem::HistoryItem(const String& urlString, const String& title, const String& alternateTitle, BackForwardItemIdentifier BackForwardItemIdentifier)
    : m_urlString(urlString)
    , m_originalURLString(urlString)
    , m_title(title)
    , m_displayTitle(alternateTitle)
    , m_pruningReason(PruningReason::None)
    , m_identifier(BackForwardItemIdentifier)
{
}

HistoryItem::~HistoryItem()
{
    ASSERT(!m_cachedPage);
}

inline HistoryItem::HistoryItem(const HistoryItem& item)
    : RefCounted<HistoryItem>()
    , m_urlString(item.m_urlString)
    , m_originalURLString(item.m_originalURLString)
    , m_referrer(item.m_referrer)
    , m_target(item.m_target)
    , m_title(item.m_title)
    , m_displayTitle(item.m_displayTitle)
    , m_scrollPosition(item.m_scrollPosition)
    , m_pageScaleFactor(item.m_pageScaleFactor)
    , m_lastVisitWasFailure(item.m_lastVisitWasFailure)
    , m_isTargetItem(item.m_isTargetItem)
    , m_itemSequenceNumber(item.m_itemSequenceNumber)
    , m_documentSequenceNumber(item.m_documentSequenceNumber)
    , m_formContentType(item.m_formContentType)
    , m_pruningReason(PruningReason::None)
#if PLATFORM(IOS_FAMILY)
    , m_obscuredInsets(item.m_obscuredInsets)
    , m_scale(item.m_scale)
    , m_scaleIsInitial(item.m_scaleIsInitial)
#endif
    , m_identifier(item.m_identifier)
{
    if (item.m_formData)
        m_formData = item.m_formData->copy();
        
    unsigned size = item.m_children.size();
    m_children.reserveInitialCapacity(size);
    for (unsigned i = 0; i < size; ++i)
        m_children.uncheckedAppend(item.m_children[i]->copy());
}

Ref<HistoryItem> HistoryItem::copy() const
{
    return adoptRef(*new HistoryItem(*this));
}

void HistoryItem::reset()
{
    m_urlString = String();
    m_originalURLString = String();
    m_referrer = String();
    m_target = String();
    m_title = String();
    m_displayTitle = String();

    m_lastVisitWasFailure = false;
    m_isTargetItem = false;

    m_itemSequenceNumber = generateSequenceNumber();

    m_stateObject = nullptr;
    m_documentSequenceNumber = generateSequenceNumber();

    m_formData = nullptr;
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
    return URL({ }, m_urlString);
}

URL HistoryItem::originalURL() const
{
    return URL({ }, m_originalURLString);
}

const String& HistoryItem::referrer() const
{
    return m_referrer;
}

const String& HistoryItem::target() const
{
    return m_target;
}

void HistoryItem::setAlternateTitle(const String& alternateTitle)
{
    m_displayTitle = alternateTitle;
    notifyChanged();
}

void HistoryItem::setURLString(const String& urlString)
{
    m_urlString = urlString;
    notifyChanged();
}

void HistoryItem::setURL(const URL& url)
{
    PageCache::singleton().remove(*this);
    setURLString(url.string());
    clearDocumentState();
}

void HistoryItem::setOriginalURLString(const String& urlString)
{
    m_originalURLString = urlString;
    notifyChanged();
}

void HistoryItem::setReferrer(const String& referrer)
{
    m_referrer = referrer;
    notifyChanged();
}

void HistoryItem::setTitle(const String& title)
{
    m_title = title;
    notifyChanged();
}

void HistoryItem::setTarget(const String& target)
{
    m_target = target;
    notifyChanged();
}

const IntPoint& HistoryItem::scrollPosition() const
{
    return m_scrollPosition;
}

void HistoryItem::setScrollPosition(const IntPoint& position)
{
    m_scrollPosition = position;
}

void HistoryItem::clearScrollPosition()
{
    m_scrollPosition = IntPoint();
}

bool HistoryItem::shouldRestoreScrollPosition() const
{
    return m_shouldRestoreScrollPosition;
}

void HistoryItem::setShouldRestoreScrollPosition(bool shouldRestore)
{
    m_shouldRestoreScrollPosition = shouldRestore;
    notifyChanged();
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

void HistoryItem::setShouldOpenExternalURLsPolicy(ShouldOpenExternalURLsPolicy policy)
{
    m_shouldOpenExternalURLsPolicy = policy;
}

ShouldOpenExternalURLsPolicy HistoryItem::shouldOpenExternalURLsPolicy() const
{
    return m_shouldOpenExternalURLsPolicy;
}

bool HistoryItem::isTargetItem() const
{
    return m_isTargetItem;
}

void HistoryItem::setIsTargetItem(bool flag)
{
    m_isTargetItem = flag;
}

void HistoryItem::setStateObject(RefPtr<SerializedScriptValue>&& object)
{
    m_stateObject = WTFMove(object);
    notifyChanged();
}

void HistoryItem::addChildItem(Ref<HistoryItem>&& child)
{
    ASSERT(!childItemWithTarget(child->target()));
    m_children.append(WTFMove(child));
}

void HistoryItem::setChildItem(Ref<HistoryItem>&& child)
{
    ASSERT(!child->isTargetItem());
    unsigned size = m_children.size();
    for (unsigned i = 0; i < size; ++i)  {
        if (m_children[i]->target() == child->target()) {
            child->setIsTargetItem(m_children[i]->isTargetItem());
            m_children[i] = WTFMove(child);
            return;
        }
    }
    m_children.append(WTFMove(child));
}

HistoryItem* HistoryItem::childItemWithTarget(const String& target)
{
    unsigned size = m_children.size();
    for (unsigned i = 0; i < size; ++i) {
        if (m_children[i]->target() == target)
            return m_children[i].ptr();
    }
    return nullptr;
}

HistoryItem* HistoryItem::childItemWithDocumentSequenceNumber(long long number)
{
    unsigned size = m_children.size();
    for (unsigned i = 0; i < size; ++i) {
        if (m_children[i]->documentSequenceNumber() == number)
            return m_children[i].ptr();
    }
    return nullptr;
}

const Vector<Ref<HistoryItem>>& HistoryItem::children() const
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

// We do same-document navigation if going to a different item and if either of the following is true:
// - The other item corresponds to the same document (for history entries created via pushState or fragment changes).
// - The other item corresponds to the same set of documents, including frames (for history entries created via regular navigation)
bool HistoryItem::shouldDoSameDocumentNavigationTo(HistoryItem& otherItem) const
{
    if (this == &otherItem)
        return false;

    if (stateObject() || otherItem.stateObject())
        return documentSequenceNumber() == otherItem.documentSequenceNumber();
    
    if ((url().hasFragmentIdentifier() || otherItem.url().hasFragmentIdentifier()) && equalIgnoringFragmentIdentifier(url(), otherItem.url()))
        return documentSequenceNumber() == otherItem.documentSequenceNumber();
    
    return hasSameDocumentTree(otherItem);
}

// Does a recursive check that this item and its descendants have the same
// document sequence numbers as the other item.
bool HistoryItem::hasSameDocumentTree(HistoryItem& otherItem) const
{
    if (documentSequenceNumber() != otherItem.documentSequenceNumber())
        return false;
        
    if (children().size() != otherItem.children().size())
        return false;

    for (size_t i = 0; i < children().size(); i++) {
        auto& child = children()[i].get();
        auto* otherChild = otherItem.childItemWithDocumentSequenceNumber(child.documentSequenceNumber());
        if (!otherChild || !child.hasSameDocumentTree(*otherChild))
            return false;
    }

    return true;
}

// Does a non-recursive check that this item and its immediate children have the
// same frames as the other item.
bool HistoryItem::hasSameFrames(HistoryItem& otherItem) const
{
    if (target() != otherItem.target())
        return false;
        
    if (children().size() != otherItem.children().size())
        return false;

    for (size_t i = 0; i < children().size(); i++) {
        if (!otherItem.childItemWithTarget(children()[i]->target()))
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
    
    if (equalLettersIgnoringASCIICase(request.httpMethod(), "post")) {
        // FIXME: Eventually we have to make this smart enough to handle the case where
        // we have a stream for the body to handle the "data interspersed with files" feature.
        m_formData = request.httpBody();
        m_formContentType = request.httpContentType();
    } else {
        m_formData = nullptr;
        m_formContentType = String();
    }
}

void HistoryItem::setFormData(RefPtr<FormData>&& formData)
{
    m_formData = WTFMove(formData);
}

void HistoryItem::setFormContentType(const String& formContentType)
{
    m_formContentType = formContentType;
}

FormData* HistoryItem::formData()
{
    return m_formData.get();
}

bool HistoryItem::isCurrentDocument(Document& document) const
{
    // FIXME: We should find a better way to check if this is the current document.
    return equalIgnoringFragmentIdentifier(url(), document.url());
}

void HistoryItem::notifyChanged()
{
    notifyHistoryItemChanged(*this);
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

#if !LOG_DISABLED
const char* HistoryItem::logString() const
{
    return debugString("HistoryItem current URL ", urlString(), ", identifier ", m_identifier.logString());
}
#endif

} // namespace WebCore

#ifndef NDEBUG

int showTree(const WebCore::HistoryItem* item)
{
    return item->showTree();
}

#endif
