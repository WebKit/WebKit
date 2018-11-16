/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WKContentObservation.h"
#include "WKContentObservationInternal.h"

#if PLATFORM(IOS_FAMILY)

#include "JSDOMBinding.h"
#include "WebCoreThread.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

WKContentChange _WKContentChange                    = WKContentNoChange;
bool            _WKObservingContentChanges          = false;
bool            _WKObservingIndeterminateChanges    = false;


bool WKObservingContentChanges(void)
{
    return _WKObservingContentChanges;
}

void WKStopObservingContentChanges(void)
{
    _WKObservingContentChanges = false;
    _WKObservingIndeterminateChanges = false;
}

void WKBeginObservingContentChanges(bool allowsIntedeterminateChanges)
{
    _WKContentChange = WKContentNoChange;
    _WKObservingContentChanges = true;
    
    _WKObservingIndeterminateChanges = allowsIntedeterminateChanges;
    if (_WKObservingIndeterminateChanges)
        WebThreadClearObservedContentModifiers();
}

WKContentChange WKObservedContentChange(void)
{
    return _WKContentChange;
}

void WKSetObservedContentChange(WKContentChange aChange)
{
    if (aChange > _WKContentChange && (_WKObservingIndeterminateChanges || aChange != WKContentIndeterminateChange)) {
        _WKContentChange = aChange;
        if (_WKContentChange == WKContentVisibilityChange)
            WebThreadClearObservedContentModifiers();
    }
}

static HashMap<void *, void *> * WebThreadGetObservedContentModifiers()
{
    ASSERT(WebThreadIsLockedOrDisabled());
    typedef HashMap<void *, void *> VoidVoidMap;
    static NeverDestroyed<VoidVoidMap> observedContentModifiers;
    return &observedContentModifiers.get();
}

int WebThreadCountOfObservedContentModifiers(void)
{
    return WebThreadGetObservedContentModifiers()->size();
}

void WebThreadClearObservedContentModifiers()
{
    WebThreadGetObservedContentModifiers()->clear();
}

bool WebThreadContainsObservedContentModifier(void * aContentModifier)
{
    return WebThreadGetObservedContentModifiers()->contains(aContentModifier);
}

void WebThreadAddObservedContentModifier(void * aContentModifier)
{
    if (_WKContentChange != WKContentVisibilityChange && _WKObservingIndeterminateChanges)
        WebThreadGetObservedContentModifiers()->set(aContentModifier, aContentModifier);
}

void WebThreadRemoveObservedContentModifier(void * aContentModifier)
{
    WebThreadGetObservedContentModifiers()->remove(aContentModifier);
    // Force reset the content change flag when the last observed content modifier is removed. We should not be in indeterminate state anymore.
    if (WebThreadCountOfObservedContentModifiers())
        return;
    if (WKObservedContentChange() != WKContentIndeterminateChange)
        return;
    _WKContentChange = WKContentNoChange;
}

#endif // PLATFORM(IOS_FAMILY)
