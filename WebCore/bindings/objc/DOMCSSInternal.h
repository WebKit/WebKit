/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 James G. Speth (speth@end.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source ec must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "DOMCSS.h"
#import "Color.h"

#import "DOMCSSPrimitiveValue.h"
#import "DOMCSSRule.h"
#import "DOMCSSRuleList.h"
#import "DOMCSSStyleDeclaration.h"
#import "DOMCSSStyleSheet.h"
#import "DOMCSSValue.h"
#import "DOMCounter.h"
#import "DOMRGBColor.h"
#import "DOMRect.h"

namespace WebCore {
    class CSSRule;
    class CSSRuleList;
    class CSSStyleDeclaration;
    class CSSStyleSheet;
    class CSSValue;
    class Counter;
    class RectImpl;
}

@interface DOMCSSRuleList (WebCoreInternal)
+ (DOMCSSRuleList *)_ruleListWith:(WebCore::CSSRuleList *)impl;
@end

@interface DOMCSSRule (WebCoreInternal)
+ (DOMCSSRule *)_ruleWith:(WebCore::CSSRule *)impl;
@end

@interface DOMCSSValue (WebCoreInternal)
+ (DOMCSSValue *)_valueWith:(WebCore::CSSValue *)impl;
@end

@interface DOMCSSPrimitiveValue (WebCoreInternal)
+ (DOMCSSPrimitiveValue *)_valueWith:(WebCore::CSSValue *)impl;
@end

@interface DOMRGBColor (WebCoreInternal)
+ (DOMRGBColor *)_RGBColorWithRGB:(WebCore::RGBA32)value;
@end

@interface DOMRect (WebCoreInternal)
+ (DOMRect *)_rectWith:(WebCore::RectImpl *)impl;
@end

@interface DOMCounter (WebCoreInternal)
+ (DOMCounter *)_counterWith:(WebCore::Counter *)impl;
@end

@interface DOMCSSStyleDeclaration (WebCoreInternal)
+ (DOMCSSStyleDeclaration *)_styleDeclarationWith:(WebCore::CSSStyleDeclaration *)impl;
- (WebCore::CSSStyleDeclaration *)_styleDeclaration;
@end

@interface DOMCSSStyleSheet (WebCoreInternal)
+ (DOMCSSStyleSheet *)_CSSStyleSheetWith:(WebCore::CSSStyleSheet *)impl;
@end
