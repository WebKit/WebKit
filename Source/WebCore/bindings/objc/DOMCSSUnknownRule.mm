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
#import "DOMCSSUnknownRuleInternal.h"

#import "CSSUnknownRule.h"
#import "DOMCSSRuleInternal.h"
#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import "JSMainThreadExecState.h"
#import "ThreadCheck.h"
#import "WebScriptObjectPrivate.h"
#import <wtf/GetPtr.h>

#define IMPL static_cast<WebCore::CSSUnknownRule*>(reinterpret_cast<WebCore::CSSRule*>(_internal))

@implementation DOMCSSUnknownRule

@end

WebCore::CSSUnknownRule* core(DOMCSSUnknownRule *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::CSSUnknownRule*>(wrapper->_internal) : 0;
}

DOMCSSUnknownRule *kit(WebCore::CSSUnknownRule* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMCSSUnknownRule*>(kit(static_cast<WebCore::CSSRule*>(value)));
}
