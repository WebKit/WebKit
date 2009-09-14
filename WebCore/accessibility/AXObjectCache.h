/*
 * Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef AXObjectCache_h
#define AXObjectCache_h

#include "AccessibilityObject.h"
#include "EventHandler.h"
#include "Timer.h"
#include <limits.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>

#ifdef __OBJC__
@class WebCoreTextMarker;
#else
class WebCoreTextMarker;
#endif

namespace WebCore {

    class Node;
    class Page;
    class RenderObject;
    class String;
    class VisiblePosition;

    struct TextMarkerData  {
        AXID axID;
        Node* node;
        int offset;
        EAffinity affinity;
    };

    class AXObjectCache {
    public:
        AXObjectCache();
        ~AXObjectCache();

        static AccessibilityObject* focusedUIElementForPage(const Page*);

        // to be used with render objects
        AccessibilityObject* getOrCreate(RenderObject*);
        
        // used for objects without backing elements
        AccessibilityObject* getOrCreate(AccessibilityRole);
        
        // will only return the AccessibilityObject if it already exists
        AccessibilityObject* get(RenderObject*);
        
        void remove(RenderObject*);
        void remove(AXID);

        void detachWrapper(AccessibilityObject*);
        void attachWrapper(AccessibilityObject*);
        void childrenChanged(RenderObject*);
        void selectedChildrenChanged(RenderObject*);
        void handleActiveDescendantChanged(RenderObject*);
        void handleAriaRoleChanged(RenderObject*);
        void handleFocusedUIElementChanged(RenderObject* oldFocusedRenderer, RenderObject* newFocusedRenderer);
        void handleScrolledToAnchor(const Node* anchorNode);

        static void enableAccessibility() { gAccessibilityEnabled = true; }
        static void enableEnhancedUserInterfaceAccessibility() { gAccessibilityEnhancedUserInterfaceEnabled = true; }
        
        static bool accessibilityEnabled() { return gAccessibilityEnabled; }
        static bool accessibilityEnhancedUserInterfaceEnabled() { return gAccessibilityEnhancedUserInterfaceEnabled; }

        void removeAXID(AccessibilityObject*);
        bool isIDinUse(AXID id) const { return m_idsInUse.contains(id); }
        AXID platformGenerateAXID() const;
        AccessibilityObject* objectFromAXID(AXID id) const { return m_objects.get(id).get(); }

        // Text marker utilities.
        static void textMarkerDataForVisiblePosition(TextMarkerData&, const VisiblePosition&);
        static VisiblePosition visiblePositionForTextMarkerData(TextMarkerData&);

        enum AXNotification {
            AXCheckedStateChanged,
            AXFocusedUIElementChanged,
            AXLayoutComplete,
            AXLoadComplete,
            AXSelectedChildrenChanged,
            AXSelectedTextChanged,
            AXValueChanged,
            AXScrolledToAnchor,
        };
        
        void postNotification(RenderObject*, AXNotification, bool postToElement);

    protected:
        void postPlatformNotification(AccessibilityObject*, AXNotification);

    private:
        HashMap<AXID, RefPtr<AccessibilityObject> > m_objects;
        HashMap<RenderObject*, AXID> m_renderObjectMapping;
        static bool gAccessibilityEnabled;
        static bool gAccessibilityEnhancedUserInterfaceEnabled;
        
        HashSet<AXID> m_idsInUse;
        
        Timer<AXObjectCache> m_notificationPostTimer;
        Vector<pair<RefPtr<AccessibilityObject>, AXNotification> > m_notificationsToPost;
        void notificationPostTimerFired(Timer<AXObjectCache>*);
        
        AXID getAXID(AccessibilityObject*);
        bool nodeIsAriaType(Node* node, String role);
    };

#if !HAVE(ACCESSIBILITY)
    inline void AXObjectCache::handleActiveDescendantChanged(RenderObject*) { }
    inline void AXObjectCache::handleAriaRoleChanged(RenderObject*) { }
    inline void AXObjectCache::detachWrapper(AccessibilityObject*) { }
    inline void AXObjectCache::attachWrapper(AccessibilityObject*) { }
    inline void AXObjectCache::selectedChildrenChanged(RenderObject*) { }
    inline void AXObjectCache::postNotification(RenderObject*, AXNotification, bool postToElement) { }
    inline void AXObjectCache::postPlatformNotification(AccessibilityObject*, AXNotification) { }
    inline void AXObjectCache::handleFocusedUIElementChanged(RenderObject*, RenderObject*) { }
    inline void AXObjectCache::handleScrolledToAnchor(const Node*) { }
#endif

}

#endif
