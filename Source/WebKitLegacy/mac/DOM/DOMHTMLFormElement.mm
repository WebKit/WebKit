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

#import "DOMHTMLFormElementInternal.h"

#import "DOMHTMLCollectionInternal.h"
#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/HTMLCollection.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/JSExecState.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::HTMLFormElement*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMHTMLFormElement

- (NSString *)acceptCharset
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::accept_charsetAttr);
}

- (void)setAcceptCharset:(NSString *)newAcceptCharset
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::accept_charsetAttr, newAcceptCharset);
}

- (NSString *)action
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getURLAttribute(WebCore::HTMLNames::actionAttr);
}

- (void)setAction:(NSString *)newAction
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::actionAttr, newAction);
}

- (NSString *)autocomplete
{
    WebCore::JSMainThreadNullState state;
    return IMPL->autocomplete();
}

- (void)setAutocomplete:(NSString *)newAutocomplete
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAutocomplete(newAutocomplete);
}

- (NSString *)enctype
{
    WebCore::JSMainThreadNullState state;
    return IMPL->enctype();
}

- (void)setEnctype:(NSString *)newEnctype
{
    WebCore::JSMainThreadNullState state;
    IMPL->setEnctype(newEnctype);
}

- (NSString *)encoding
{
    WebCore::JSMainThreadNullState state;
    return IMPL->enctype();
}

- (void)setEncoding:(NSString *)newEncoding
{
    WebCore::JSMainThreadNullState state;
    IMPL->setEnctype(newEncoding);
}

- (NSString *)method
{
    WebCore::JSMainThreadNullState state;
    return IMPL->method();
}

- (void)setMethod:(NSString *)newMethod
{
    WebCore::JSMainThreadNullState state;
    IMPL->setMethod(newMethod);
}

- (NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getNameAttribute();
}

- (void)setName:(NSString *)newName
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, newName);
}

- (BOOL)noValidate
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::novalidateAttr);
}

- (void)setNoValidate:(BOOL)newNoValidate
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::novalidateAttr, newNoValidate);
}

- (NSString *)target
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::targetAttr);
}

- (void)setTarget:(NSString *)newTarget
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::targetAttr, newTarget);
}

- (DOMHTMLCollection *)elements
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->elementsForNativeBindings()));
}

- (int)length
{
    WebCore::JSMainThreadNullState state;
    return IMPL->length();
}

- (void)submit
{
    WebCore::JSMainThreadNullState state;
    IMPL->submit();
}

- (void)reset
{
    WebCore::JSMainThreadNullState state;
    IMPL->reset();
}

- (BOOL)checkValidity
{
    WebCore::JSMainThreadNullState state;
    return IMPL->checkValidity();
}

@end

DOMHTMLFormElement *kit(WebCore::HTMLFormElement* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMHTMLFormElement*>(kit(static_cast<WebCore::Node*>(value)));
}

#undef IMPL
