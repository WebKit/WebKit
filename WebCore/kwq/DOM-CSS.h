/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#import <Foundation/Foundation.h>

#import "DOM.h"

//=========================================================================
//=========================================================================
//=========================================================================

// Important Note:
// Though this file appears as an exported header from WebKit, the
// version you should edit is in WebCore. The WebKit version is copied
// to WebKit during the build process.

//=========================================================================
//=========================================================================
//=========================================================================

enum {
    //
    // CSSRule types
    //
    CSS_UNKNOWN_RULE               = 0,
    CSS_STYLE_RULE                 = 1,
    CSS_CHARSET_RULE               = 2,
    CSS_IMPORT_RULE                = 3,
    CSS_MEDIA_RULE                 = 4,
    CSS_FONT_FACE_RULE             = 5,
    CSS_PAGE_RULE                  = 6,
    //
    // CSSValue unit types
    //
    CSS_INHERIT                    = 0,
    CSS_PRIMITIVE_VALUE            = 1,
    CSS_VALUE_LIST                 = 2,
    CSS_CUSTOM                     = 3,
    //
    // CSSPrimitiveValue unit types
    //
    CSS_UNKNOWN                    = 0,
    CSS_NUMBER                     = 1,
    CSS_PERCENTAGE                 = 2,
    CSS_EMS                        = 3,
    CSS_EXS                        = 4,
    CSS_PX                         = 5,
    CSS_CM                         = 6,
    CSS_MM                         = 7,
    CSS_IN                         = 8,
    CSS_PT                         = 9,
    CSS_PC                         = 10,
    CSS_DEG                        = 11,
    CSS_RAD                        = 12,
    CSS_GRAD                       = 13,
    CSS_MS                         = 14,
    CSS_S                          = 15,
    CSS_HZ                         = 16,
    CSS_KHZ                        = 17,
    CSS_DIMENSION                  = 18,
    CSS_STRING                     = 19,
    CSS_URI                        = 20,
    CSS_IDENT                      = 21,
    CSS_ATTR                       = 22,
    CSS_COUNTER                    = 23,
    CSS_RECT                       = 24,
    CSS_RGBCOLOR                   = 25,
};

@class CSSCounter;
@class CSSMediaList;
@class CSSRect;
@class CSSRGBColor;
@class CSSRule;
@class CSSRuleList;
@class CSSStyleDeclaration;
@class CSSValue;

@interface DOMStyleSheet : DOMObject
- (NSString *)type;
- (BOOL)disabled;
- (void)setDisabled:(BOOL)disabled;
- (DOMNode *)ownerNode;
- (DOMStyleSheet *)parentStyleSheet;
- (NSString *)href;
- (NSString *)title;
- (CSSMediaList *)media;
@end

@interface DOMStyleSheetList : DOMObject
- (unsigned long)length;
- (DOMStyleSheet *)item:(unsigned long)index;
@end

@interface CSSStyleSheet : DOMStyleSheet
- (CSSRule *)ownerRule;
- (CSSRuleList *)cssRules;
- (unsigned long)insertRule:(NSString *)rule :(unsigned long)index;
- (void)deleteRule:(unsigned long)index;
@end

@interface CSSMediaList : DOMObject
- (NSString *)mediaText;
- (void)setMediaText:(NSString *)mediaText;
- (unsigned long)length;
- (NSString *)item:(unsigned long)index;
- (void)deleteMedium:(NSString *)oldMedium;
- (void)appendMedium:(NSString *)newMedium;
@end

@interface CSSRuleList : DOMObject
- (unsigned long)length;
- (CSSRule *)item:(unsigned long)index;
@end

@interface CSSRule : DOMObject
- (unsigned short)type;
- (NSString *)cssText;
- (void)setCSSText:(NSString *)cssText;
- (CSSStyleSheet *)parentStyleSheet;
- (CSSRule *)parentRule;
@end

@interface CSSStyleRule : CSSRule
- (NSString *)selectorText;
- (void)setSelectorText:(NSString *)selectorText;
- (CSSStyleDeclaration *)style;
@end

@interface CSSMediaRule : CSSRule
- (CSSMediaList *)media;
- (CSSRuleList *)cssRules;
- (unsigned long)insertRule:(NSString *)rule :(unsigned long)index;
- (void)deleteRule:(unsigned long)index;
@end

@interface CSSFontFaceRule : CSSRule
- (CSSStyleDeclaration *)style;
@end

@interface CSSPageRule : CSSRule
- (NSString *)selectorText;
- (void)setSelectorText:(NSString *)selectorText;
- (CSSStyleDeclaration *)style;
@end

@interface CSSImportRule : CSSRule
- (CSSMediaList *)media;
- (NSString *)href;
- (CSSStyleSheet *)styleSheet;
@end

@interface CSSCharsetRule : CSSRule
- (NSString *)encoding;
@end

@interface CSSStyleDeclaration : DOMObject
- (NSString *)cssText;
- (void)setCSSText:(NSString *)cssText;
- (NSString *)getPropertyValue:(NSString *)propertyName;
- (CSSValue *)getPropertyCSSValue:(NSString *)propertyName;
- (NSString *)removeProperty:(NSString *)propertyName;
- (NSString *)getPropertyPriority:(NSString *)propertyName;
- (void)setProperty:(NSString *)propertyName :(NSString *)value :(NSString *)priority;
- (unsigned long)length;
- (NSString *)item:(unsigned long)index;
- (CSSRule *)parentRule;
@end

@interface CSSValue : DOMObject
- (NSString *)cssText;
- (void)setCSSText:(NSString *)cssText;
- (unsigned short)cssValueType;
@end

@interface CSSPrimitiveValue : CSSValue
- (unsigned short)primitiveType;
- (void)setFloatValue:(unsigned short)unitType :(float)floatValue;
- (float)getFloatValue:(unsigned short)unitType;
- (void)setStringValue:(unsigned short)stringType :(NSString *)stringValue;
- (NSString *)getStringValue;
- (CSSCounter *)getCounterValue;
- (CSSRect *)getRectValue;
- (CSSRGBColor *)getRGBColorValue;
@end

@interface CSSValueList : CSSValue
- (unsigned long)length;
- (CSSValue *)item:(unsigned long)index;
@end

@interface CSSRGBColor : DOMObject
- (CSSPrimitiveValue *)red;
- (CSSPrimitiveValue *)green;
- (CSSPrimitiveValue *)blue;
@end

@interface CSSRect : DOMObject
- (CSSPrimitiveValue *)top;
- (CSSPrimitiveValue *)right;
- (CSSPrimitiveValue *)bottom;
- (CSSPrimitiveValue *)left;
@end

@interface CSSCounter : DOMObject
- (NSString *)identifier;
- (NSString *)listStyle;
- (NSString *)separator;
@end
