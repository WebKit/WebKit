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
#import "DOMHTMLOListElementInternal.h"

#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import "HTMLNames.h"
#import "HTMLOListElement.h"
#import "JSMainThreadExecState.h"
#import "ThreadCheck.h"
#import "URL.h"
#import "WebScriptObjectPrivate.h"
#import <wtf/GetPtr.h>

#define IMPL static_cast<WebCore::HTMLOListElement*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMHTMLOListElement

- (BOOL)compact
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::compactAttr);
}

- (void)setCompact:(BOOL)newCompact
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::compactAttr, newCompact);
}

- (int)start
{
    WebCore::JSMainThreadNullState state;
    return IMPL->start();
}

- (void)setStart:(int)newStart
{
    WebCore::JSMainThreadNullState state;
    IMPL->setStart(newStart);
}

- (BOOL)reversed
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::reversedAttr);
}

- (void)setReversed:(BOOL)newReversed
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::reversedAttr, newReversed);
}

- (NSString *)type
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::typeAttr);
}

- (void)setType:(NSString *)newType
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::typeAttr, newType);
}

@end

WebCore::HTMLOListElement* core(DOMHTMLOListElement *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::HTMLOListElement*>(wrapper->_internal) : 0;
}

DOMHTMLOListElement *kit(WebCore::HTMLOListElement* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMHTMLOListElement*>(kit(static_cast<WebCore::Node*>(value)));
}
