/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2008 Google Inc.
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
#include "AXObjectCache.h"
#include "AccessibilityObject.h"
#include "Chrome.h"
#include "ChromeClientChromium.h"
#include "FrameView.h"

namespace WebCore {

static ChromeClientChromium* toChromeClientChromium(FrameView* view)
{
    Page* page = view->frame() ? view->frame()->page() : 0;
    if (!page)
        return 0;

    return static_cast<ChromeClientChromium*>(page->chrome()->client());
}

void AXObjectCache::detachWrapper(AccessibilityObject* obj)
{
    // In Chromium, AccessibilityObjects are wrapped lazily.
    if (AccessibilityObjectWrapper* wrapper = obj->wrapper())
        wrapper->detach();
}

void AXObjectCache::attachWrapper(AccessibilityObject*)
{
    // In Chromium, AccessibilityObjects are wrapped lazily.
}

void AXObjectCache::postPlatformNotification(AccessibilityObject* obj, AXNotification notification)
{
    if (notification != AXCheckedStateChanged)
        return;

    if (!obj || !obj->document() || !obj->documentFrameView())
        return;

    ChromeClientChromium* client = toChromeClientChromium(obj->documentFrameView());
    if (client)
        client->didChangeAccessibilityObjectState(obj);
}

void AXObjectCache::handleFocusedUIElementChanged(RenderObject*, RenderObject*)
{
}

void AXObjectCache::handleScrolledToAnchor(const Node*)
{
}

} // namespace WebCore
