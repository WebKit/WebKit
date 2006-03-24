/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef AccessibilityObjectCache_h
#define AccessibilityObjectCache_h

#include <kxmlcore/HashMap.h>
#include <kxmlcore/HashSet.h>

#ifdef __OBJC__
@class WebCoreAXObject;
@class WebCoreTextMarker;
#else
class WebCoreAXObject;
class WebCoreTextMarker;
#endif

namespace WebCore {

    class RenderObject;
    class String;
    class VisiblePosition;

    typedef unsigned AXID;

    struct AXIDHashTraits : KXMLCore::GenericHashTraits<unsigned> {
        static TraitType deletedValue() { return UINT_MAX; }
    };

    class AccessibilityObjectCache {
    public:
        ~AccessibilityObjectCache();

        WebCoreAXObject* get(RenderObject*);
        void remove(RenderObject*);

        void removeAXID(WebCoreAXObject*);

        WebCoreTextMarker* textMarkerForVisiblePosition(const VisiblePosition&);
        VisiblePosition visiblePositionForTextMarker(WebCoreTextMarker*);
        
        void childrenChanged(RenderObject*);
        void postNotification(RenderObject*, const String& message);
        void postNotificationToTopWebArea(RenderObject*, const String& message);
        void handleFocusedUIElementChanged();
        
        static void enableAccessibility() { gAccessibilityEnabled = true; }
        static bool accessibilityEnabled() { return gAccessibilityEnabled; }

    private:
        static bool gAccessibilityEnabled;

        AXID getAXID(WebCoreAXObject*);

        HashMap<RenderObject*, WebCoreAXObject*> m_objects;
        HashSet<AXID, PtrHash<AXID>, AXIDHashTraits> m_idsInUse;
    };

#ifndef __APPLE__
    inline AccessibilityObjectCache::~AccessibilityObjectCache() { }
    inline WebCoreAXObject* AccessibilityObjectCache::get(RenderObject*) { return 0; }
    inline void AccessibilityObjectCache::remove(RenderObject*) { }
    inline void AccessibilityObjectCache::removeAXID(WebCoreAXObject*) { }
    inline void AccessibilityObjectCache::childrenChanged(RenderObject*) { }
    inline void AccessibilityObjectCache::postNotification(RenderObject*, const String&) { }
    inline void AccessibilityObjectCache::postNotificationToTopWebArea(RenderObject*, const String&) { }
    inline void AccessibilityObjectCache::handleFocusedUIElementChanged() { }
#endif

}

#endif
