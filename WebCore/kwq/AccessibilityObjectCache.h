/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#if __APPLE__

#include <CoreFoundation/CoreFoundation.h>

#include "VisiblePosition.h"

#ifdef __OBJC__
@class WebCoreAXObject;
@class WebCoreTextMarker;
#else
class WebCoreAXObject;
class WebCoreTextMarker;
#endif

namespace WebCore {
    class RenderObject;
    class VisiblePosition;
}

typedef unsigned int        WebCoreAXID;

class AccessibilityObjectCache
{
public:
    AccessibilityObjectCache();
    ~AccessibilityObjectCache();
    
    WebCoreAXObject* accObject(WebCore::RenderObject* renderer);
    void setAccObject(WebCore::RenderObject* renderer, WebCoreAXObject* obj);
    void removeAccObject(WebCore::RenderObject* renderer);

    WebCoreAXID getAccObjectID(WebCoreAXObject* accObject);
    void removeAXObjectID(WebCoreAXObject* accObject);

    WebCoreTextMarker *textMarkerForVisiblePosition(const WebCore::VisiblePosition &);
    WebCore::VisiblePosition visiblePositionForTextMarker(WebCoreTextMarker *textMarker);

    void detach(WebCore::RenderObject* renderer);
    
    void childrenChanged(WebCore::RenderObject* renderer);

    void postNotification(WebCore::RenderObject* renderer, const WebCore::String& msg);
    void postNotificationToTopWebArea(WebCore::RenderObject* renderer, const WebCore::String& msg);
    void handleFocusedUIElementChanged(void);
    
    static void enableAccessibility() { gAccessibilityEnabled = true; }
    static bool accessibilityEnabled() { return gAccessibilityEnabled; }

private:
    static bool gAccessibilityEnabled;

private:
    CFMutableDictionaryRef accCache;
    CFMutableDictionaryRef accCacheByID;
    WebCoreAXID accObjectIDSource;
};
#else
class AccessibilityObjectCache;
#endif

