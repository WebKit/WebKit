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
#import "DOMCSSPrimitiveValueInternal.h"

#import "CSSPrimitiveValue.h"
#import "Counter.h"
#import "DOMCSSValueInternal.h"
#import "DOMCounterInternal.h"
#import "DOMNodeInternal.h"
#import "DOMRGBColorInternal.h"
#import "DOMRectInternal.h"
#import "ExceptionHandlers.h"
#import "JSMainThreadExecState.h"
#import "RGBColor.h"
#import "Rect.h"
#import "ThreadCheck.h"
#import "URL.h"
#import "WebScriptObjectPrivate.h"
#import <wtf/GetPtr.h>

#define IMPL static_cast<WebCore::CSSPrimitiveValue*>(reinterpret_cast<WebCore::CSSValue*>(_internal))

@implementation DOMCSSPrimitiveValue

- (unsigned short)primitiveType
{
    WebCore::JSMainThreadNullState state;
    return IMPL->primitiveType();
}

- (void)setFloatValue:(unsigned short)unitType floatValue:(float)floatValue
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setFloatValue(unitType, floatValue, ec);
    WebCore::raiseOnDOMError(ec);
}

- (void)setFloatValue:(unsigned short)unitType :(float)floatValue
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setFloatValue(unitType, floatValue, ec);
    WebCore::raiseOnDOMError(ec);
}

- (float)getFloatValue:(unsigned short)unitType
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    float result = IMPL->getFloatValue(unitType, ec);
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (void)setStringValue:(unsigned short)stringType stringValue:(NSString *)stringValue
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setStringValue(stringType, stringValue, ec);
    WebCore::raiseOnDOMError(ec);
}

- (void)setStringValue:(unsigned short)stringType :(NSString *)stringValue
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setStringValue(stringType, stringValue, ec);
    WebCore::raiseOnDOMError(ec);
}

- (NSString *)getStringValue
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    NSString *result = IMPL->getStringValue(ec);
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMCounter *)getCounterValue
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMCounter *result = kit(WTF::getPtr(IMPL->getCounterValue(ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMRect *)getRectValue
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMRect *result = kit(WTF::getPtr(IMPL->getRectValue(ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMRGBColor *)getRGBColorValue
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMRGBColor *result = kit(WTF::getPtr(IMPL->getRGBColorValue(ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

@end

WebCore::CSSPrimitiveValue* core(DOMCSSPrimitiveValue *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::CSSPrimitiveValue*>(wrapper->_internal) : 0;
}

DOMCSSPrimitiveValue *kit(WebCore::CSSPrimitiveValue* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMCSSPrimitiveValue*>(kit(static_cast<WebCore::CSSValue*>(value)));
}
