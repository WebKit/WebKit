/*
 * Copyright (C) 2006, 2008, 2011, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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

#ifndef HistoryItem_h
#define HistoryItem_h

#include "FloatRect.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "SerializedScriptValue.h"
#include <memory>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS)
#include "ViewportArguments.h"
#endif

#if PLATFORM(COCOA)
#import <wtf/RetainPtr.h>
typedef struct objc_object* id;
#endif

namespace WebCore {

class CachedPage;
class Document;
class FormData;
class HistoryItem;
class Image;
class ResourceRequest;
class URL;

typedef Vector<RefPtr<HistoryItem>> HistoryItemVector;

extern void (*notifyHistoryItemChanged)(HistoryItem*);

class HistoryItem : public RefCounted<HistoryItem> {
    friend class PageCache;

public: 
    static PassRefPtr<HistoryItem> create() { return adoptRef(new HistoryItem); }
    static PassRefPtr<HistoryItem> create(const String& urlString, const String& title)
    {
        return adoptRef(new HistoryItem(urlString, title));
    }
    static PassRefPtr<HistoryItem> create(const String& urlString, const String& title, const String& alternateTitle)
    {
        return adoptRef(new HistoryItem(urlString, title, alternateTitle));
    }
    static PassRefPtr<HistoryItem> create(const URL& url, const String& target, const String& parent, const String& title)
    {
        return adoptRef(new HistoryItem(url, target, parent, title));
    }
    
    WEBCORE_EXPORT ~HistoryItem();

    WEBCORE_EXPORT PassRefPtr<HistoryItem> copy() const;

    // Resets the HistoryItem to its initial state, as returned by create().
    void reset();
    
    WEBCORE_EXPORT const String& originalURLString() const;
    WEBCORE_EXPORT const String& urlString() const;
    WEBCORE_EXPORT const String& title() const;
    
    bool isInPageCache() const { return m_cachedPage.get(); }
    WEBCORE_EXPORT bool hasCachedPageExpired() const;

    WEBCORE_EXPORT void setAlternateTitle(const String&);
    WEBCORE_EXPORT const String& alternateTitle() const;
    
    const String& parent() const;
    WEBCORE_EXPORT URL url() const;
    WEBCORE_EXPORT URL originalURL() const;
    WEBCORE_EXPORT const String& referrer() const;
    WEBCORE_EXPORT const String& target() const;
    WEBCORE_EXPORT bool isTargetItem() const;
    
    WEBCORE_EXPORT FormData* formData();
    WEBCORE_EXPORT String formContentType() const;
    
    bool lastVisitWasFailure() const { return m_lastVisitWasFailure; }

    WEBCORE_EXPORT const IntPoint& scrollPoint() const;
    WEBCORE_EXPORT void setScrollPoint(const IntPoint&);
    void clearScrollPoint();
    
    WEBCORE_EXPORT float pageScaleFactor() const;
    WEBCORE_EXPORT void setPageScaleFactor(float);
    
    WEBCORE_EXPORT const Vector<String>& documentState() const;
    WEBCORE_EXPORT void setDocumentState(const Vector<String>&);
    void clearDocumentState();

    void setURL(const URL&);
    WEBCORE_EXPORT void setURLString(const String&);
    WEBCORE_EXPORT void setOriginalURLString(const String&);
    WEBCORE_EXPORT void setReferrer(const String&);
    WEBCORE_EXPORT void setTarget(const String&);
    void setParent(const String&);
    WEBCORE_EXPORT void setTitle(const String&);
    WEBCORE_EXPORT void setIsTargetItem(bool);
    
    WEBCORE_EXPORT void setStateObject(PassRefPtr<SerializedScriptValue>);
    PassRefPtr<SerializedScriptValue> stateObject() const { return m_stateObject; }

    void setItemSequenceNumber(long long number) { m_itemSequenceNumber = number; }
    long long itemSequenceNumber() const { return m_itemSequenceNumber; }

    void setDocumentSequenceNumber(long long number) { m_documentSequenceNumber = number; }
    long long documentSequenceNumber() const { return m_documentSequenceNumber; }

    void setFormInfoFromRequest(const ResourceRequest&);
    WEBCORE_EXPORT void setFormData(PassRefPtr<FormData>);
    WEBCORE_EXPORT void setFormContentType(const String&);

    void setLastVisitWasFailure(bool wasFailure) { m_lastVisitWasFailure = wasFailure; }

    WEBCORE_EXPORT void addChildItem(PassRefPtr<HistoryItem>);
    void setChildItem(PassRefPtr<HistoryItem>);
    WEBCORE_EXPORT HistoryItem* childItemWithTarget(const String&) const;
    HistoryItem* childItemWithDocumentSequenceNumber(long long number) const;
    WEBCORE_EXPORT HistoryItem* targetItem();
    WEBCORE_EXPORT const HistoryItemVector& children() const;
    WEBCORE_EXPORT bool hasChildren() const;
    void clearChildren();
    bool isAncestorOf(const HistoryItem*) const;
    
    bool shouldDoSameDocumentNavigationTo(HistoryItem* otherItem) const;
    bool hasSameFrames(HistoryItem* otherItem) const;

    WEBCORE_EXPORT void addRedirectURL(const String&);
    WEBCORE_EXPORT Vector<String>* redirectURLs() const;
    WEBCORE_EXPORT void setRedirectURLs(std::unique_ptr<Vector<String>>);

    bool isCurrentDocument(Document*) const;
    
#if PLATFORM(COCOA)
    WEBCORE_EXPORT id viewState() const;
    WEBCORE_EXPORT void setViewState(id);
    
    // Transient properties may be of any ObjC type.  They are intended to be used to store state per back/forward list entry.
    // The properties will not be persisted; when the history item is removed, the properties will be lost.
    WEBCORE_EXPORT id getTransientProperty(const String&) const;
    WEBCORE_EXPORT void setTransientProperty(const String&, id);
#endif

#ifndef NDEBUG
    int showTree() const;
    int showTreeWithIndent(unsigned indentLevel) const;
#endif

#if PLATFORM(IOS)
    FloatRect exposedContentRect() const { return m_exposedContentRect; }
    void setExposedContentRect(FloatRect exposedContentRect) { m_exposedContentRect = exposedContentRect; }

    IntRect unobscuredContentRect() const { return m_unobscuredContentRect; }
    void setUnobscuredContentRect(IntRect unobscuredContentRect) { m_unobscuredContentRect = unobscuredContentRect; }

    FloatSize minimumLayoutSizeInScrollViewCoordinates() const { return m_minimumLayoutSizeInScrollViewCoordinates; }
    void setMinimumLayoutSizeInScrollViewCoordinates(FloatSize minimumLayoutSizeInScrollViewCoordinates) { m_minimumLayoutSizeInScrollViewCoordinates = minimumLayoutSizeInScrollViewCoordinates; }

    IntSize contentSize() const { return m_contentSize; }
    void setContentSize(IntSize contentSize) { m_contentSize = contentSize; }

    float scale() const { return m_scale; }
    bool scaleIsInitial() const { return m_scaleIsInitial; }
    void setScaleIsInitial(bool scaleIsInitial) { m_scaleIsInitial = scaleIsInitial; }
    void setScale(float newScale, bool isInitial)
    {
        m_scale = newScale;
        m_scaleIsInitial = isInitial;
    }

    const ViewportArguments& viewportArguments() const { return m_viewportArguments; }
    void setViewportArguments(const ViewportArguments& viewportArguments) { m_viewportArguments = viewportArguments; }

    uint32_t bookmarkID() const { return m_bookmarkID; }
    void setBookmarkID(uint32_t bookmarkID) { m_bookmarkID = bookmarkID; }
    String sharedLinkUniqueIdentifier() const { return m_sharedLinkUniqueIdentifier; }
    void setSharedLinkUniqueIdentifier(const String& sharedLinkUniqueidentifier) { m_sharedLinkUniqueIdentifier = sharedLinkUniqueidentifier; }
#endif

private:
    WEBCORE_EXPORT HistoryItem();
    WEBCORE_EXPORT HistoryItem(const String& urlString, const String& title);
    WEBCORE_EXPORT HistoryItem(const String& urlString, const String& title, const String& alternateTitle);
    WEBCORE_EXPORT HistoryItem(const URL&, const String& frameName, const String& parent, const String& title);

    explicit HistoryItem(const HistoryItem&);

    bool hasSameDocumentTree(HistoryItem* otherItem) const;

    HistoryItem* findTargetItem();

    String m_urlString;
    String m_originalURLString;
    String m_referrer;
    String m_target;
    String m_parent;
    String m_title;
    String m_displayTitle;
    
    IntPoint m_scrollPoint;
    float m_pageScaleFactor;
    Vector<String> m_documentState;
    
    HistoryItemVector m_children;
    
    bool m_lastVisitWasFailure;
    bool m_isTargetItem;

    std::unique_ptr<Vector<String>> m_redirectURLs;

    // If two HistoryItems have the same item sequence number, then they are
    // clones of one another.  Traversing history from one such HistoryItem to
    // another is a no-op.  HistoryItem clones are created for parent and
    // sibling frames when only a subframe navigates.
    int64_t m_itemSequenceNumber;

    // If two HistoryItems have the same document sequence number, then they
    // refer to the same instance of a document.  Traversing history from one
    // such HistoryItem to another preserves the document.
    int64_t m_documentSequenceNumber;

    // Support for HTML5 History
    RefPtr<SerializedScriptValue> m_stateObject;
    
    // info used to repost form data
    RefPtr<FormData> m_formData;
    String m_formContentType;

    // PageCache controls these fields.
    HistoryItem* m_next;
    HistoryItem* m_prev;
    std::unique_ptr<CachedPage> m_cachedPage;

#if PLATFORM(IOS)
    FloatRect m_exposedContentRect;
    IntRect m_unobscuredContentRect;
    FloatSize m_minimumLayoutSizeInScrollViewCoordinates;
    IntSize m_contentSize;
    float m_scale;
    bool m_scaleIsInitial;
    ViewportArguments m_viewportArguments;

    uint32_t m_bookmarkID;
    String m_sharedLinkUniqueIdentifier;
#endif

#if PLATFORM(COCOA)
    RetainPtr<id> m_viewState;
    std::unique_ptr<HashMap<String, RetainPtr<id>>> m_transientProperties;
#endif
}; //class HistoryItem

} //namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
extern "C" int showTree(const WebCore::HistoryItem*);
#endif

#endif // HISTORYITEM_H
