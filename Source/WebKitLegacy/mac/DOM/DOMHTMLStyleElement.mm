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

#import "DOMHTMLStyleElementInternal.h"

#import "DOMNodeInternal.h"
#import "DOMStyleSheetInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLStyleElement.h>
#import <WebCore/JSExecState.h>
#import <WebCore/StyleSheet.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::HTMLStyleElement*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMHTMLStyleElement

- (BOOL)disabled
{
    WebCore::JSMainThreadNullState state;
    return IMPL->disabled();
}

- (void)setDisabled:(BOOL)newDisabled
{
    WebCore::JSMainThreadNullState state;
    IMPL->setDisabled(newDisabled);
}

- (NSString *)media
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::mediaAttr);
}

- (void)setMedia:(NSString *)newMedia
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::mediaAttr, newMedia);
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

- (DOMStyleSheet *)sheet
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->sheet()));
}

- (NSString *)nonce
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::nonceAttr);
}

- (void)setNonce:(NSString *)newNonce
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::nonceAttr, newNonce);
}

@end

WebCore::HTMLStyleElement* core(DOMHTMLStyleElement *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::HTMLStyleElement*>(wrapper->_internal) : 0;
}
