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
#import "DOMTextInternal.h"

#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import "JSMainThreadExecState.h"
#import "Text.h"
#import "ThreadCheck.h"
#import "URL.h"
#import "WebScriptObjectPrivate.h"
#import <wtf/GetPtr.h>

#define IMPL static_cast<WebCore::Text*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMText

- (NSString *)wholeText
{
    WebCore::JSMainThreadNullState state;
    return IMPL->wholeText();
}

- (DOMText *)splitText:(unsigned)offset
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMText *result = kit(WTF::getPtr(IMPL->splitText(offset, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMText *)replaceWholeText:(NSString *)content
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMText *result = kit(WTF::getPtr(IMPL->replaceWholeText(content, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

@end

WebCore::Text* core(DOMText *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::Text*>(wrapper->_internal) : 0;
}

DOMText *kit(WebCore::Text* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMText*>(kit(static_cast<WebCore::Node*>(value)));
}
