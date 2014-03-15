/*
 * Copyright (C) 2006, 2008, 2011 Apple Inc. All rights reserved.
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

#include "IntPoint.h"
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
class KeyedDecoder;
class KeyedEncoder;
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
    
    ~HistoryItem();

    PassRefPtr<HistoryItem> copy() const;

    // Resets the HistoryItem to its initial state, as returned by create().
    void reset();
    
    void encodeBackForwardTree(Encoder&) const;
    void encodeBackForwardTree(KeyedEncoder&) const;
    static PassRefPtr<HistoryItem> decodeBackForwardTree(const String& urlString, const String& title, const String& originalURLString, Decoder&);
    static PassRefPtr<HistoryItem> decodeBackForwardTree(const String& urlString, const String& title, const String& originalURLString, KeyedDecoder&);

    const String& originalURLString() const;
    const String& urlString() const;
    const String& title() const;
    
    bool isInPageCache() const { return m_cachedPage.get(); }
    bool hasCachedPageExpired() const;

    void setAlternateTitle(const String& alternateTitle);
    const String& alternateTitle() const;
    
    const String& parent() const;
    URL url() const;
    URL originalURL() const;
    const String& referrer() const;
    const String& target() const;
    bool isTargetItem() const;
    
    FormData* formData();
    String formContentType() const;
    
    bool lastVisitWasFailure() const { return m_lastVisitWasFailure; }

    const IntPoint& scrollPoint() const;
    void setScrollPoint(const IntPoint&);
    void clearScrollPoint();
    
    float pageScaleFactor() const;
    void setPageScaleFactor(float);
    
    const Vector<String>& documentState() const;
    void setDocumentState(const Vector<String>&);
    void clearDocumentState();

    void setURL(const URL&);
    void setURLString(const String&);
    void setOriginalURLString(const String&);
    void setReferrer(const String&);
    void setTarget(const String&);
    void setParent(const String&);
    void setTitle(const String&);
    void setIsTargetItem(bool);
    
    void setStateObject(PassRefPtr<SerializedScriptValue> object);
    PassRefPtr<SerializedScriptValue> stateObject() const { return m_stateObject; }

    void setItemSequenceNumber(long long number) { m_itemSequenceNumber = number; }
    long long itemSequenceNumber() const { return m_itemSequenceNumber; }

    void setDocumentSequenceNumber(long long number) { m_documentSequenceNumber = number; }
    long long documentSequenceNumber() const { return m_documentSequenceNumber; }

    void setFormInfoFromRequest(const ResourceRequest&);
    void setFormData(PassRefPtr<FormData>);
    void setFormContentType(const String&);

    void setLastVisitWasFailure(bool wasFailure) { m_lastVisitWasFailure = wasFailure; }

    void addChildItem(PassRefPtr<HistoryItem>);
    void setChildItem(PassRefPtr<HistoryItem>);
    HistoryItem* childItemWithTarget(const String&) const;
    HistoryItem* childItemWithDocumentSequenceNumber(long long number) const;
    HistoryItem* targetItem();
    const HistoryItemVector& children() const;
    bool hasChildren() const;
    void clearChildren();
    bool isAncestorOf(const HistoryItem*) const;
    
    bool shouldDoSameDocumentNavigationTo(HistoryItem* otherItem) const;
    bool hasSameFrames(HistoryItem* otherItem) const;

    void addRedirectURL(const String&);
    Vector<String>* redirectURLs() const;
    void setRedirectURLs(std::unique_ptr<Vector<String>>);

    bool isCurrentDocument(Document*) const;
    
#if PLATFORM(COCOA)
    id viewState() const;
    void setViewState(id);
    
    // Transient properties may be of any ObjC type.  They are intended to be used to store state per back/forward list entry.
    // The properties will not be persisted; when the history item is removed, the properties will be lost.
    id getTransientProperty(const String&) const;
    void setTransientProperty(const String&, id);
#endif

#ifndef NDEBUG
    int showTree() const;
    int showTreeWithIndent(unsigned indentLevel) const;
#endif

#if PLATFORM(IOS)
    float scale() const { return m_scale; }
    bool scaleIsInitial() const { return m_scaleIsInitial; }
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
    HistoryItem();
    HistoryItem(const String& urlString, const String& title);
    HistoryItem(const String& urlString, const String& title, const String& alternateTitle);
    HistoryItem(const URL& url, const String& frameName, const String& parent, const String& title);

    explicit HistoryItem(const HistoryItem&);

    bool hasSameDocumentTree(HistoryItem* otherItem) const;

    HistoryItem* findTargetItem();

    void encodeBackForwardTreeNode(Encoder&) const;
    void encodeBackForwardTreeNode(KeyedEncoder&) const;

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
