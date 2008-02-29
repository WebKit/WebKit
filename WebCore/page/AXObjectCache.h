/*
 * Copyright (C) 2003, 2006, 2007 Apple Inc. All rights reserved.
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

#include <limits.h>

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

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

    struct AXIDHashTraits : WTF::GenericHashTraits<unsigned> {
        static TraitType deletedValue() { return UINT_MAX; }
    };

    class AXObjectCache {
    public:
        ~AXObjectCache();

        WebCoreAXObject* get(RenderObject*);
        void remove(RenderObject*);

        void removeAXID(WebCoreAXObject*);

        WebCoreTextMarker* textMarkerForVisiblePosition(const VisiblePosition&);
        VisiblePosition visiblePositionForTextMarker(WebCoreTextMarker*);
        
        void childrenChanged(RenderObject*);
        void postNotification(RenderObject*, const String& message);
        void postNotificationToElement(RenderObject*, const String& message);
        void handleFocusedUIElementChanged();
        
#if PLATFORM(MAC)
        static void enableAccessibility() { gAccessibilityEnabled = true; }
        static bool accessibilityEnabled() { return gAccessibilityEnabled; }
#else
        static bool accessibilityEnabled() { return false; }
#endif

    private:
#if PLATFORM(MAC)
        static bool gAccessibilityEnabled;
#endif

        AXID getAXID(WebCoreAXObject*);

        HashMap<RenderObject*, WebCoreAXObject*> m_objects;
        HashSet<AXID, IntHash<AXID>, AXIDHashTraits> m_idsInUse;
    };

#if !PLATFORM(MAC)
    inline AXObjectCache::~AXObjectCache() { }
    inline WebCoreAXObject* AXObjectCache::get(RenderObject*) { return 0; }
    inline void AXObjectCache::remove(RenderObject*) { }
    inline void AXObjectCache::removeAXID(WebCoreAXObject*) { }
    inline void AXObjectCache::childrenChanged(RenderObject*) { }
    inline void AXObjectCache::postNotification(RenderObject*, const String&) { }
    inline void AXObjectCache::postNotificationToElement(RenderObject*, const String&) { }
    inline void AXObjectCache::handleFocusedUIElementChanged() { }
#endif

}

#endif
