/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#import "config.h"
#import "AXObjectCache.h"

#if HAVE(ACCESSIBILITY)

#import "AccessibilityObject.h"
#import "AccessibilityObjectWrapper.h"
#import "RenderObject.h"
#import "WebCoreViewFactory.h"

#import <wtf/PassRefPtr.h>

// The simple Cocoa calls in this file don't throw exceptions.

namespace WebCore {

void AXObjectCache::detachWrapper(AccessibilityObject* obj)
{
    [obj->wrapper() detach];
    [obj->wrapper() release];
    obj->setWrapper(0);
}

void AXObjectCache::attachWrapper(AccessibilityObject* obj)
{
    obj->setWrapper([[AccessibilityObjectWrapper alloc] initWithAccessibilityObject:obj]);
}

void AXObjectCache::postPlatformNotification(AccessibilityObject* obj, const String& message)
{
    if (!obj)
        return;
    
    NSAccessibilityPostNotification(obj->wrapper(), message);
}

void AXObjectCache::handleFocusedUIElementChanged()
{
    [[WebCoreViewFactory sharedFactory] accessibilityHandleFocusChanged];
}

}

#endif // HAVE(ACCESSIBILITY)
