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

#import "DOMCSSValueInternal.h"

#import <WebCore/DeprecatedCSSOMValue.h>
#import "DOMInternal.h"
#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/JSExecState.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL reinterpret_cast<WebCore::DeprecatedCSSOMValue*>(_internal)

@implementation DOMCSSValue

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([DOMCSSValue class], self))
        return;

    if (_internal)
        IMPL->deref();
    [super dealloc];
}

- (NSString *)cssText
{
    WebCore::JSMainThreadNullState state;
    return IMPL->cssText();
}

- (void)setCssText:(NSString *)newCssText
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setCssText(newCssText));
}

- (unsigned short)cssValueType
{
    WebCore::JSMainThreadNullState state;
    return IMPL->cssValueType();
}

@end

DOMCSSValue *kit(WebCore::DeprecatedCSSOMValue* value)
{
    WebCoreThreadViolationCheckRoundOne();
    if (!value)
        return nil;
    if (DOMCSSValue *wrapper = getDOMWrapper(value))
        return [[wrapper retain] autorelease];
    DOMCSSValue *wrapper = [[kitClass(value) alloc] _init];
    if (!wrapper)
        return nil;
    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(value);
    value->ref();
    addDOMWrapper(wrapper, value);
    return [wrapper autorelease];
}

#undef IMPL
