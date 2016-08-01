/*
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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

#import "config.h"
#import "DOMHTMLOptionsCollectionInternal.h"

#import "DOMHTMLOptionElementInternal.h"
#import "DOMInternal.h"
#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import "HTMLOptionElement.h"
#import "HTMLOptionsCollection.h"
#import "JSMainThreadExecState.h"
#import "Node.h"
#import "ThreadCheck.h"
#import "URL.h"
#import "WebCoreObjCExtras.h"
#import "WebScriptObjectPrivate.h"
#import <wtf/GetPtr.h>

#define IMPL reinterpret_cast<WebCore::HTMLOptionsCollection*>(_internal)

@implementation DOMHTMLOptionsCollection

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([DOMHTMLOptionsCollection class], self))
        return;

    if (_internal)
        IMPL->deref();
    [super dealloc];
}

- (int)selectedIndex
{
    WebCore::JSMainThreadNullState state;
    return IMPL->selectedIndex();
}

- (void)setSelectedIndex:(int)newSelectedIndex
{
    WebCore::JSMainThreadNullState state;
    IMPL->setSelectedIndex(newSelectedIndex);
}

- (unsigned)length
{
    WebCore::JSMainThreadNullState state;
    return IMPL->length();
}

- (void)setLength:(unsigned)newLength
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setLength(newLength, ec);
    WebCore::raiseOnDOMError(ec);
}

- (DOMNode *)namedItem:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->namedItem(name)));
}

- (void)add:(DOMHTMLOptionElement *)option index:(unsigned)index
{
    WebCore::JSMainThreadNullState state;
    if (!option)
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    IMPL->add(*core(option), index, ec);
    WebCore::raiseOnDOMError(ec);
}

- (void)remove:(unsigned)index
{
    WebCore::JSMainThreadNullState state;
    IMPL->remove(index);
}

- (DOMNode *)item:(unsigned)index
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->item(index)));
}

@end

WebCore::HTMLOptionsCollection* core(DOMHTMLOptionsCollection *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::HTMLOptionsCollection*>(wrapper->_internal) : 0;
}

DOMHTMLOptionsCollection *kit(WebCore::HTMLOptionsCollection* value)
{
    WebCoreThreadViolationCheckRoundOne();
    if (!value)
        return nil;
    if (DOMHTMLOptionsCollection *wrapper = getDOMWrapper(value))
        return [[wrapper retain] autorelease];
    DOMHTMLOptionsCollection *wrapper = [[DOMHTMLOptionsCollection alloc] _init];
    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(value);
    value->ref();
    addDOMWrapper(wrapper, value);
    return [wrapper autorelease];
}
