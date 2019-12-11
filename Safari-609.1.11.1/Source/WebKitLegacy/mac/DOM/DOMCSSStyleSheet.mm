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

#import "DOMCSSStyleSheetInternal.h"

#import <WebCore/CSSImportRule.h>
#import <WebCore/CSSRule.h>
#import <WebCore/CSSRuleList.h>
#import <WebCore/CSSStyleSheet.h>
#import "DOMCSSRuleInternal.h"
#import "DOMCSSRuleListInternal.h"
#import "DOMNodeInternal.h"
#import "DOMStyleSheetInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/JSExecState.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::CSSStyleSheet*>(reinterpret_cast<WebCore::StyleSheet*>(_internal))

@implementation DOMCSSStyleSheet

- (DOMCSSRule *)ownerRule
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->ownerRule()));
}

- (DOMCSSRuleList *)cssRules
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->cssRules()));
}

- (DOMCSSRuleList *)rules
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->rules()));
}

- (unsigned)insertRule:(NSString *)rule index:(unsigned)index
{
    WebCore::JSMainThreadNullState state;
    return raiseOnDOMError(IMPL->insertRule(rule, index));
}

- (void)deleteRule:(unsigned)index
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->deleteRule(index));
}

- (int)addRule:(NSString *)selector style:(NSString *)style index:(unsigned)index
{
    WebCore::JSMainThreadNullState state;
    return raiseOnDOMError(IMPL->addRule(selector, style, index));
}

- (void)removeRule:(unsigned)index
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->removeRule(index));
}

@end

@implementation DOMCSSStyleSheet (DOMCSSStyleSheetDeprecated)

- (unsigned)insertRule:(NSString *)rule :(unsigned)index
{
    return [self insertRule:rule index:index];
}

@end

DOMCSSStyleSheet *kit(WebCore::CSSStyleSheet* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMCSSStyleSheet*>(kit(static_cast<WebCore::StyleSheet*>(value)));
}

#undef IMPL
