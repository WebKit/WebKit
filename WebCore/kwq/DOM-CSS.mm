/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source exceptionCode must retain the above copyright
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

#include <objc/objc-class.h>

#import <css/css_base.h>
#import <css/css_ruleimpl.h>
#import <css/css_stylesheetimpl.h>
#import <css/css_valueimpl.h>
#import <dom/css_value.h>
#import <dom/dom_string.h>
#import <qcolor.h>
#import <shared.h>
#import <xml/dom_stringimpl.h>

#import "DOMInternal.h"
#import "KWQAssertions.h"

using DOM::CounterImpl;
using DOM::CSSCharsetRuleImpl;
using DOM::CSSFontFaceRuleImpl;
using DOM::CSSImportRuleImpl;
using DOM::CSSMediaRuleImpl;
using DOM::CSSPageRuleImpl;
using DOM::CSSPrimitiveValueImpl;
using DOM::CSSRuleImpl;
using DOM::CSSRuleListImpl;
using DOM::CSSStyleDeclarationImpl;
using DOM::CSSStyleRuleImpl;
using DOM::CSSStyleSheetImpl;
using DOM::CSSValueImpl;
using DOM::CSSValueListImpl;
using DOM::DOMString;
using DOM::MediaListImpl;
using DOM::RectImpl;
using DOM::StyleSheetImpl;
using DOM::StyleSheetListImpl;

@interface DOMStyleSheet (WebCoreInternal)
+ (DOMStyleSheet *)_DOMStyleSheetWithImpl:(StyleSheetImpl *)impl;
@end

@interface DOMMediaList (WebCoreInternal)
+ (DOMMediaList *)_mediaListWithImpl:(MediaListImpl *)impl;
@end

@interface DOMCSSRuleList (WebCoreInternal)
+ (DOMCSSRuleList *)_ruleListWithImpl:(CSSRuleListImpl *)impl;
@end

@interface DOMCSSRule (WebCoreInternal)
+ (DOMCSSRule *)_ruleWithImpl:(CSSRuleImpl *)impl;
@end

@interface DOMCSSValue (WebCoreInternal)
+ (DOMCSSValue *)_valueWithImpl:(CSSValueImpl *)impl;
@end

@interface DOMRGBColor (WebCoreInternal)
+ (DOMRGBColor *)_RGBColorWithRGB:(QRgb)value;
@end

@interface DOMRect (WebCoreInternal)
+ (DOMRect *)_rectWithImpl:(RectImpl *)impl;
@end

@interface DOMCounter (WebCoreInternal)
+ (DOMCounter *)_counterWithImpl:(CounterImpl *)impl;
@end

static inline int getPropertyID(NSString *string)
{
    const char *s = [string UTF8String];
    return DOM::getPropertyID(s, strlen(s));
}

//------------------------------------------------------------------------------------------
// DOMStyleSheet

@implementation DOMStyleSheet

- (void)dealloc
{
    if (_internal) {
        DOM_cast<StyleSheetImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (StyleSheetImpl *)_DOMStyleSheetImpl
{
    return DOM_cast<StyleSheetImpl *>(_internal);
}

- (NSString *)type
{
    return [self _DOMStyleSheetImpl]->type();
}

- (BOOL)disabled
{
    return [self _DOMStyleSheetImpl]->disabled();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _DOMStyleSheetImpl]->setDisabled(disabled);
}

- (DOMNode *)ownerNode
{
    return [DOMNode _nodeWithImpl:[self _DOMStyleSheetImpl]->ownerNode()];
}

- (DOMStyleSheet *)parentStyleSheet
{
    return [DOMStyleSheet _DOMStyleSheetWithImpl:[self _DOMStyleSheetImpl]->parentStyleSheet()];
}

- (NSString *)href
{
    return [self _DOMStyleSheetImpl]->href();
}

- (NSString *)title
{
    return [self _DOMStyleSheetImpl]->title();
}

- (DOMMediaList *)media
{
    return nil;
}

@end

@implementation DOMStyleSheet (WebCoreInternal)

- (id)_initWithDOMStyleSheetImpl:(StyleSheetImpl *)impl
{
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMStyleSheet *)_DOMStyleSheetWithImpl:(StyleSheetImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithDOMStyleSheetImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMStyleSheetList

@implementation DOMStyleSheetList

- (void)dealloc
{
    if (_internal) {
        DOM_cast<StyleSheetListImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (StyleSheetListImpl *)_styleSheetListImpl
{
    return DOM_cast<StyleSheetListImpl *>(_internal);
}

- (unsigned long)length
{
    return [self _styleSheetListImpl]->length();
}

- (DOMStyleSheet *)item:(unsigned long)index
{
    return [DOMStyleSheet _DOMStyleSheetWithImpl:[self _styleSheetListImpl]->item(index)];
}

@end

@implementation DOMStyleSheetList (WebCoreInternal)

- (id)_initWithStyleSheetListImpl:(StyleSheetListImpl *)impl
{
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMStyleSheetList *)_styleSheetListWithImpl:(StyleSheetListImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithStyleSheetListImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSStyleSheet

@implementation DOMCSSStyleSheet

- (void)dealloc
{
    if (_internal) {
        DOM_cast<CSSStyleSheetImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (CSSStyleSheetImpl *)_CSSStyleSheetImpl
{
    return DOM_cast<CSSStyleSheetImpl *>(_internal);
}

- (DOMCSSRule *)ownerRule
{
    return [DOMCSSRule _ruleWithImpl:[self _CSSStyleSheetImpl]->ownerRule()];
}

- (DOMCSSRuleList *)cssRules
{
    return [DOMCSSRuleList _ruleListWithImpl:[self _CSSStyleSheetImpl]->cssRules().handle()];
}

- (unsigned long)insertRule:(NSString *)rule :(unsigned long)index
{
    int exceptionCode;
    unsigned long result = [self _CSSStyleSheetImpl]->insertRule(rule, index, exceptionCode);
    raiseOnDOMError(exceptionCode);
    return result;
}

- (void)deleteRule:(unsigned long)index
{
    int exceptionCode;
    [self _CSSStyleSheetImpl]->deleteRule(index, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

@end

@implementation DOMCSSStyleSheet (WebCoreInternal)

- (id)_initWithCSSStyleSheetImpl:(CSSStyleSheetImpl *)impl
{
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMCSSStyleSheet *)_CSSStyleSheetWithImpl:(CSSStyleSheetImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithCSSStyleSheetImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMMediaList

@implementation DOMMediaList

- (void)dealloc
{
    if (_internal) {
        DOM_cast<MediaListImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (MediaListImpl *)_mediaListImpl
{
    return DOM_cast<MediaListImpl *>(_internal);
}

- (NSString *)mediaText
{
    return [self _mediaListImpl]->mediaText();
}

- (void)setMediaText:(NSString *)mediaText
{
    [self _mediaListImpl]->setMediaText(mediaText);
}

- (unsigned long)length
{
    return [self _mediaListImpl]->length();
}

- (NSString *)item:(unsigned long)index
{
    return [self _mediaListImpl]->item(index);
}

- (void)deleteMedium:(NSString *)oldMedium
{
    [self _mediaListImpl]->deleteMedium(oldMedium);
}

- (void)appendMedium:(NSString *)newMedium
{
    [self _mediaListImpl]->appendMedium(newMedium);
}

@end

@implementation DOMMediaList (WebCoreInternal)

- (id)_initWithMediaListImpl:(MediaListImpl *)impl
{
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMMediaList *)_mediaListWithImpl:(MediaListImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithMediaListImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSRuleList

@implementation DOMCSSRuleList

- (void)dealloc
{
    if (_internal) {
        DOM_cast<CSSRuleListImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (CSSRuleListImpl *)_ruleListImpl
{
    return DOM_cast<CSSRuleListImpl *>(_internal);
}

- (unsigned long)length
{
    return [self _ruleListImpl]->length();
}

- (DOMCSSRule *)item:(unsigned long)index
{
    return [DOMCSSRule _ruleWithImpl:[self _ruleListImpl]->item(index)];
}

@end

@implementation DOMCSSRuleList (WebCoreInternal)

- (id)_initWithRuleListImpl:(CSSRuleListImpl *)impl
{
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMCSSRuleList *)_ruleListWithImpl:(CSSRuleListImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithRuleListImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSRule

@implementation DOMCSSRule

- (void)dealloc
{
    if (_internal) {
        DOM_cast<CSSRuleImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (CSSRuleImpl *)_ruleImpl
{
    return DOM_cast<CSSRuleImpl *>(_internal);
}

- (unsigned short)type
{
    return [self _ruleImpl]->type();
}

- (NSString *)cssText
{
    return [self _ruleImpl]->cssText();
}

- (void)setCssText:(NSString *)cssText
{
    [self _ruleImpl]->setCssText(cssText);
}

- (DOMCSSStyleSheet *)parentStyleSheet
{
    return [DOMCSSStyleSheet _CSSStyleSheetWithImpl:[self _ruleImpl]->parentStyleSheet()];
}

- (DOMCSSRule *)parentRule
{
    return [DOMCSSRule _ruleWithImpl:[self _ruleImpl]->parentRule()];
}

@end

@implementation DOMCSSRule (WebCoreInternal)

- (id)_initWithRuleImpl:(CSSRuleImpl *)impl
{
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMCSSRule *)_ruleWithImpl:(CSSRuleImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];

    Class wrapperClass = nil;
    switch (impl->type()) {
        case DOM_UNKNOWN_RULE:
            wrapperClass = [DOMCSSRule class];
            break;
        case DOM_STYLE_RULE:
            wrapperClass = [DOMCSSStyleRule class];
            break;
        case DOM_CHARSET_RULE:
            wrapperClass = [DOMCSSCharsetRule class];
            break;
        case DOM_IMPORT_RULE:
            wrapperClass = [DOMCSSImportRule class];
            break;
        case DOM_MEDIA_RULE:
            wrapperClass = [DOMCSSMediaRule class];
            break;
        case DOM_FONT_FACE_RULE:
            wrapperClass = [DOMCSSFontFaceRule class];
            break;
        case DOM_PAGE_RULE:
            wrapperClass = [DOMCSSPageRule class];
            break;
    }
    return [[[wrapperClass alloc] _initWithRuleImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSStyleRule

@implementation DOMCSSStyleRule

- (CSSStyleRuleImpl *)_styleRuleImpl
{
    return static_cast<CSSStyleRuleImpl *>(DOM_cast<CSSRuleImpl *>(_internal));
}

- (NSString *)selectorText
{
    return [self _styleRuleImpl]->selectorText();
}

- (void)setSelectorText:(NSString *)selectorText
{
    [self _styleRuleImpl]->setSelectorText(selectorText);
}

- (DOMCSSStyleDeclaration *)style
{
    return [DOMCSSStyleDeclaration _styleDeclarationWithImpl:[self _styleRuleImpl]->style()];
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSMediaRule

@implementation DOMCSSMediaRule

- (CSSMediaRuleImpl *)_mediaRuleImpl
{
    return static_cast<CSSMediaRuleImpl *>(DOM_cast<CSSRuleImpl *>(_internal));
}

- (DOMMediaList *)media
{
    return [DOMMediaList _mediaListWithImpl:[self _mediaRuleImpl]->media()];
}

- (DOMCSSRuleList *)cssRules
{
    return [DOMCSSRuleList _ruleListWithImpl:[self _mediaRuleImpl]->cssRules()];
}

- (unsigned long)insertRule:(NSString *)rule :(unsigned long)index
{
    return [self _mediaRuleImpl]->insertRule(rule, index);
}

- (void)deleteRule:(unsigned long)index
{
    [self _mediaRuleImpl]->deleteRule(index);
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSFontFaceRule

@implementation DOMCSSFontFaceRule

- (CSSFontFaceRuleImpl *)_fontFaceRuleImpl
{
    return static_cast<CSSFontFaceRuleImpl *>(DOM_cast<CSSRuleImpl *>(_internal));
}

- (DOMCSSStyleDeclaration *)style
{
    return [DOMCSSStyleDeclaration _styleDeclarationWithImpl:[self _fontFaceRuleImpl]->style()];
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSPageRule

@implementation DOMCSSPageRule

- (CSSPageRuleImpl *)_pageRuleImpl
{
    return static_cast<CSSPageRuleImpl *>(DOM_cast<CSSRuleImpl *>(_internal));
}

- (NSString *)selectorText
{
    return [self _pageRuleImpl]->selectorText();
}

- (void)setSelectorText:(NSString *)selectorText
{
    [self _pageRuleImpl]->setSelectorText(selectorText);
}

- (DOMCSSStyleDeclaration *)style
{
    return [DOMCSSStyleDeclaration _styleDeclarationWithImpl:[self _pageRuleImpl]->style()];
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSImportRule

@implementation DOMCSSImportRule

- (CSSImportRuleImpl *)_importRuleImpl
{
    return static_cast<CSSImportRuleImpl *>(DOM_cast<CSSRuleImpl *>(_internal));
}

- (DOMMediaList *)media
{
    return [DOMMediaList _mediaListWithImpl:[self _importRuleImpl]->media()];
}

- (NSString *)href
{
    return [self _importRuleImpl]->href();
}

- (DOMCSSStyleSheet *)styleSheet
{
    return [DOMCSSStyleSheet _CSSStyleSheetWithImpl:[self _importRuleImpl]->styleSheet()];
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSCharsetRule

@implementation DOMCSSCharsetRule

- (CSSCharsetRuleImpl *)_importRuleImpl
{
    return static_cast<CSSCharsetRuleImpl *>(DOM_cast<CSSRuleImpl *>(_internal));
}

- (NSString *)encoding
{
    return [self _importRuleImpl]->encoding();
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSUnknownRule

@implementation DOMCSSUnknownRule

@end

//------------------------------------------------------------------------------------------
// DOMCSSStyleDeclaration

@implementation DOMCSSStyleDeclaration

- (void)dealloc
{
    if (_internal) {
        DOM_cast<CSSStyleDeclarationImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (NSString *)cssText
{
    return [self _styleDeclarationImpl]->cssText();
}

- (void)setCssText:(NSString *)cssText
{
    [self _styleDeclarationImpl]->setCssText(cssText);
}

- (NSString *)getPropertyValue:(NSString *)propertyName
{
    int propid = getPropertyID(propertyName);
    if (!propid) 
        return nil;
    return [self _styleDeclarationImpl]->getPropertyValue(propid);
}

- (DOMCSSValue *)getPropertyCSSValue:(NSString *)propertyName
{
    int propid = getPropertyID(propertyName);
    if (!propid) 
        return nil;
    return [DOMCSSValue _valueWithImpl:[self _styleDeclarationImpl]->getPropertyCSSValue(propid)];
}

- (NSString *)removeProperty:(NSString *)propertyName
{
    int propid = getPropertyID(propertyName);
    if (!propid) 
        return nil;
    return [self _styleDeclarationImpl]->removeProperty(propid);
}

- (NSString *)getPropertyPriority:(NSString *)propertyName
{
    int propid = getPropertyID(propertyName);
    if (!propid) 
        return nil;
    if ([self _styleDeclarationImpl]->getPropertyPriority(propid))
        return @"important";
    else
        return @"";
}

- (void)setProperty:(NSString *)propertyName :(NSString *)value :(NSString *)priority
{
    int propid = getPropertyID(propertyName);
    if (!propid) 
        return;
    [self _styleDeclarationImpl]->setProperty(propid, value, priority);
}

- (unsigned long)length
{
    return [self _styleDeclarationImpl]->length();
}

- (NSString *)item:(unsigned long)index
{
    return [self _styleDeclarationImpl]->item(index);
}

- (DOMCSSRule *)parentRule
{
    return [DOMCSSRule _ruleWithImpl:[self _styleDeclarationImpl]->parentRule()];
}

@end

@implementation DOMCSSStyleDeclaration (WebCoreInternal)

- (id)_initWithStyleDeclarationImpl:(CSSStyleDeclarationImpl *)impl
{
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMCSSStyleDeclaration *)_styleDeclarationWithImpl:(CSSStyleDeclarationImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithStyleDeclarationImpl:impl] autorelease];
}

- (CSSStyleDeclarationImpl *)_styleDeclarationImpl
{
    return DOM_cast<CSSStyleDeclarationImpl *>(_internal);
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSValue

@implementation DOMCSSValue

- (void)dealloc
{
    if (_internal) {
        DOM_cast<CSSValueImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (CSSValueImpl *)_valueImpl
{
    return DOM_cast<CSSValueImpl *>(_internal);
}

- (NSString *)cssText
{
    return [self _valueImpl]->cssText();
}

- (void)setCssText:(NSString *)cssText
{
    ERROR("unimplemented");
}

- (unsigned short)cssValueType
{
    return [self _valueImpl]->cssValueType();
}

@end

@implementation DOMCSSValue (WebCoreInternal)

- (id)_initWithValueImpl:(CSSValueImpl *)impl
{
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMCSSValue *)_valueWithImpl:(CSSValueImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    Class wrapperClass = nil;
    switch (impl->cssValueType()) {
        case DOM_CSS_INHERIT:
            wrapperClass = [DOMCSSValue class];
            break;
        case DOM_CSS_PRIMITIVE_VALUE:
            wrapperClass = [DOMCSSPrimitiveValue class];
            break;
        case DOM_CSS_VALUE_LIST:
            wrapperClass = [DOMCSSValueList class];
            break;
        case DOM_CSS_CUSTOM:
            wrapperClass = [DOMCSSValue class];
            break;
    }
    return [[[wrapperClass alloc] _initWithValueImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSPrimitiveValue

@implementation DOMCSSPrimitiveValue

- (CSSPrimitiveValueImpl *)_primitiveValueImpl
{
    return static_cast<CSSPrimitiveValueImpl *>(DOM_cast<CSSValueImpl *>(_internal));
}

- (unsigned short)primitiveType
{
    return [self _primitiveValueImpl]->primitiveType();
}

- (void)setFloatValue:(unsigned short)unitType :(float)floatValue
{
    int exceptionCode;
    [self _primitiveValueImpl]->setFloatValue(unitType, floatValue, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (float)getFloatValue:(unsigned short)unitType
{
    return [self _primitiveValueImpl]->getFloatValue(unitType);
}

- (void)setStringValue:(unsigned short)stringType :(NSString *)stringValue
{
    int exceptionCode;
    DOMString string(stringValue);
    [self _primitiveValueImpl]->setStringValue(stringType, string, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (NSString *)getStringValue
{
    return DOMString([self _primitiveValueImpl]->getStringValue());
}

- (DOMCounter *)getCounterValue
{
    return [DOMCounter _counterWithImpl:[self _primitiveValueImpl]->getCounterValue()];
}

- (DOMRect *)getRectValue
{
    return [DOMRect _rectWithImpl:[self _primitiveValueImpl]->getRectValue()];
}

- (DOMRGBColor *)getRGBColorValue
{
    return [DOMRGBColor _RGBColorWithRGB:[self _primitiveValueImpl]->getRGBColorValue()];
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSValueList

@implementation DOMCSSValueList

- (CSSValueListImpl *)_valueListImpl
{
    return static_cast<CSSValueListImpl *>(DOM_cast<CSSValueImpl *>(_internal));
}

- (unsigned long)length
{
    return [self _valueListImpl]->length();
}

- (DOMCSSValue *)item:(unsigned long)index
{
    return [DOMCSSValue _valueWithImpl:[self _valueListImpl]->item(index)];
}

@end

//------------------------------------------------------------------------------------------
// DOMRGBColor

static CFMutableDictionaryRef wrapperCache = NULL;

id getWrapperForRGB(QRgb value)
{
    if (!wrapperCache)
        return nil;
    return (id)CFDictionaryGetValue(wrapperCache, reinterpret_cast<const void *>(value));
}

void setWrapperForRGB(id wrapper, QRgb value)
{
    if (!wrapperCache) {
        // No need to retain/free either impl key, or id value.  Items will be removed
        // from the cache in dealloc methods.
        wrapperCache = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    }
    CFDictionarySetValue(wrapperCache, reinterpret_cast<const void *>(value), wrapper);
}

void removeWrapperForRGB(QRgb value)
{
    if (!wrapperCache)
        return;
    CFDictionaryRemoveValue(wrapperCache, reinterpret_cast<const void *>(value));
}

@implementation DOMRGBColor

- (void)dealloc
{
    removeWrapperForRGB(reinterpret_cast<QRgb>(_internal));
    [super dealloc];
}

- (DOMCSSPrimitiveValue *)red
{
    QRgb rgb = reinterpret_cast<QRgb>(_internal);
    int value = (rgb >> 16) & 0xFF;
    return [DOMCSSPrimitiveValue _valueWithImpl:new CSSPrimitiveValueImpl(value, DOM::CSSPrimitiveValue::CSS_NUMBER)];
}

- (DOMCSSPrimitiveValue *)green
{
    QRgb rgb = reinterpret_cast<QRgb>(_internal);
    int value = (rgb >> 8) & 0xFF;
    return [DOMCSSPrimitiveValue _valueWithImpl:new CSSPrimitiveValueImpl(value, DOM::CSSPrimitiveValue::CSS_NUMBER)];
}

- (DOMCSSPrimitiveValue *)blue
{
    QRgb rgb = reinterpret_cast<QRgb>(_internal);
    int value = rgb & 0xFF;
    return [DOMCSSPrimitiveValue _valueWithImpl:new CSSPrimitiveValueImpl(value, DOM::CSSPrimitiveValue::CSS_NUMBER)];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

@end

@implementation DOMRGBColor (WebCoreInternal)

- (id)_initWithRGB:(QRgb)value
{
    [super _init];
    _internal = reinterpret_cast<DOMObjectInternal *>(value);
    setWrapperForRGB(self, value);
    return self;
}

+ (DOMRGBColor *)_RGBColorWithRGB:(QRgb)value
{
    id cachedInstance;
    cachedInstance = getWrapperForRGB(value);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithRGB:value] autorelease];
}

@end

@implementation DOMRGBColor (DOMRGBColorExtensions)

- (DOMCSSPrimitiveValue *)alpha
{
    QRgb rgb = reinterpret_cast<QRgb>(_internal);
    float value = (float)qAlpha(rgb) / 0xFF;
    return [DOMCSSPrimitiveValue _valueWithImpl:new CSSPrimitiveValueImpl(value, DOM::CSSPrimitiveValue::CSS_NUMBER)];
    
}

@end

//------------------------------------------------------------------------------------------
// DOMRect

@implementation DOMRect

- (void)dealloc
{
    if (_internal) {
        DOM_cast<RectImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (RectImpl *)_rectImpl
{
    return DOM_cast<RectImpl *>(_internal);
}

- (DOMCSSPrimitiveValue *)top
{
    return [DOMCSSPrimitiveValue _valueWithImpl:[self _rectImpl]->top()];
}

- (DOMCSSPrimitiveValue *)right
{
    return [DOMCSSPrimitiveValue _valueWithImpl:[self _rectImpl]->right()];
}

- (DOMCSSPrimitiveValue *)bottom
{
    return [DOMCSSPrimitiveValue _valueWithImpl:[self _rectImpl]->bottom()];
}

- (DOMCSSPrimitiveValue *)left
{
    return [DOMCSSPrimitiveValue _valueWithImpl:[self _rectImpl]->left()];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

@end

@implementation DOMRect (WebCoreInternal)

- (id)_initWithRectImpl:(RectImpl *)impl
{
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMRect *)_rectWithImpl:(RectImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithRectImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMCounter

@implementation DOMCounter

- (void)dealloc
{
    if (_internal) {
        DOM_cast<CounterImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (CounterImpl *)_counterImpl
{
    return DOM_cast<CounterImpl *>(_internal);
}

- (NSString *)identifier
{
    return [self _counterImpl]->identifier();
}

- (NSString *)listStyle
{
    return [self _counterImpl]->listStyle();
}

- (NSString *)separator
{
    return [self _counterImpl]->separator();
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

@end

@implementation DOMCounter (WebCoreInternal)

- (id)_initWithCounterImpl:(CounterImpl *)impl
{
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMCounter *)_counterWithImpl:(CounterImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithCounterImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------

@implementation DOMCSSStyleDeclaration (DOMCSS2Properties)

- (NSString *)azimuth
{
    return [self getPropertyValue:@"azimuth"];
}

- (void)setAzimuth:(NSString *)azimuth
{
    [self setProperty:@"azimuth" :azimuth :@""];
}

- (NSString *)background
{
    return [self getPropertyValue:@"background"];
}

- (void)setBackground:(NSString *)background
{
    [self setProperty:@"background" :background :@""];
}

- (NSString *)backgroundAttachment
{
    return [self getPropertyValue:@"backgroundAttachment"];
}

- (void)setBackgroundAttachment:(NSString *)backgroundAttachment
{
    [self setProperty:@"backgroundAttachment" :backgroundAttachment :@""];
}

- (NSString *)backgroundColor
{
    return [self getPropertyValue:@"backgroundColor"];
}

- (void)setBackgroundColor:(NSString *)backgroundColor
{
    [self setProperty:@"backgroundColor" :backgroundColor :@""];
}

- (NSString *)backgroundImage
{
    return [self getPropertyValue:@"backgroundImage"];
}

- (void)setBackgroundImage:(NSString *)backgroundImage
{
    [self setProperty:@"backgroundImage" :backgroundImage :@""];
}

- (NSString *)backgroundPosition
{
    return [self getPropertyValue:@"backgroundPosition"];
}

- (void)setBackgroundPosition:(NSString *)backgroundPosition
{
    [self setProperty:@"backgroundPosition" :backgroundPosition :@""];
}

- (NSString *)backgroundRepeat
{
    return [self getPropertyValue:@"backgroundRepeat"];
}

- (void)setBackgroundRepeat:(NSString *)backgroundRepeat
{
    [self setProperty:@"backgroundRepeat" :backgroundRepeat :@""];
}

- (NSString *)border
{
    return [self getPropertyValue:@"border"];
}

- (void)setBorder:(NSString *)border
{
    [self setProperty:@"border" :border :@""];
}

- (NSString *)borderCollapse
{
    return [self getPropertyValue:@"borderCollapse"];
}

- (void)setBorderCollapse:(NSString *)borderCollapse
{
    [self setProperty:@"borderCollapse" :borderCollapse :@""];
}

- (NSString *)borderColor
{
    return [self getPropertyValue:@"borderColor"];
}

- (void)setBorderColor:(NSString *)borderColor
{
    [self setProperty:@"borderColor" :borderColor :@""];
}

- (NSString *)borderSpacing
{
    return [self getPropertyValue:@"borderSpacing"];
}

- (void)setBorderSpacing:(NSString *)borderSpacing
{
    [self setProperty:@"borderSpacing" :borderSpacing :@""];
}

- (NSString *)borderStyle
{
    return [self getPropertyValue:@"borderStyle"];
}

- (void)setBorderStyle:(NSString *)borderStyle
{
    [self setProperty:@"borderStyle" :borderStyle :@""];
}

- (NSString *)borderTop
{
    return [self getPropertyValue:@"borderTop"];
}

- (void)setBorderTop:(NSString *)borderTop
{
    [self setProperty:@"borderTop" :borderTop :@""];
}

- (NSString *)borderRight
{
    return [self getPropertyValue:@"borderRight"];
}

- (void)setBorderRight:(NSString *)borderRight
{
    [self setProperty:@"borderRight" :borderRight :@""];
}

- (NSString *)borderBottom
{
    return [self getPropertyValue:@"borderBottom"];
}

- (void)setBorderBottom:(NSString *)borderBottom
{
    [self setProperty:@"borderBottom" :borderBottom :@""];
}

- (NSString *)borderLeft
{
    return [self getPropertyValue:@"borderLeft"];
}

- (void)setBorderLeft:(NSString *)borderLeft
{
    [self setProperty:@"borderLeft" :borderLeft :@""];
}

- (NSString *)borderTopColor
{
    return [self getPropertyValue:@"borderTopColor"];
}

- (void)setBorderTopColor:(NSString *)borderTopColor
{
    [self setProperty:@"borderTopColor" :borderTopColor :@""];
}

- (NSString *)borderRightColor
{
    return [self getPropertyValue:@"borderRightColor"];
}

- (void)setBorderRightColor:(NSString *)borderRightColor
{
    [self setProperty:@"borderRightColor" :borderRightColor :@""];
}

- (NSString *)borderBottomColor
{
    return [self getPropertyValue:@"borderBottomColor"];
}

- (void)setBorderBottomColor:(NSString *)borderBottomColor
{
    [self setProperty:@"borderBottomColor" :borderBottomColor :@""];
}

- (NSString *)borderLeftColor
{
    return [self getPropertyValue:@"borderLeftColor"];
}

- (void)setBorderLeftColor:(NSString *)borderLeftColor
{
    [self setProperty:@"borderLeftColor" :borderLeftColor :@""];
}

- (NSString *)borderTopStyle
{
    return [self getPropertyValue:@"borderTopStyle"];
}

- (void)setBorderTopStyle:(NSString *)borderTopStyle
{
    [self setProperty:@"borderTopStyle" :borderTopStyle :@""];
}

- (NSString *)borderRightStyle
{
    return [self getPropertyValue:@"borderRightStyle"];
}

- (void)setBorderRightStyle:(NSString *)borderRightStyle
{
    [self setProperty:@"borderRightStyle" :borderRightStyle :@""];
}

- (NSString *)borderBottomStyle
{
    return [self getPropertyValue:@"borderBottomStyle"];
}

- (void)setBorderBottomStyle:(NSString *)borderBottomStyle
{
    [self setProperty:@"borderBottomStyle" :borderBottomStyle :@""];
}

- (NSString *)borderLeftStyle
{
    return [self getPropertyValue:@"borderLeftStyle"];
}

- (void)setBorderLeftStyle:(NSString *)borderLeftStyle
{
    [self setProperty:@"borderLeftStyle" :borderLeftStyle :@""];
}

- (NSString *)borderTopWidth
{
    return [self getPropertyValue:@"borderTopWidth"];
}

- (void)setBorderTopWidth:(NSString *)borderTopWidth
{
    [self setProperty:@"borderTopWidth" :borderTopWidth :@""];
}

- (NSString *)borderRightWidth
{
    return [self getPropertyValue:@"borderRightWidth"];
}

- (void)setBorderRightWidth:(NSString *)borderRightWidth
{
    [self setProperty:@"borderRightWidth" :borderRightWidth :@""];
}

- (NSString *)borderBottomWidth
{
    return [self getPropertyValue:@"borderBottomWidth"];
}

- (void)setBorderBottomWidth:(NSString *)borderBottomWidth
{
    [self setProperty:@"borderBottomWidth" :borderBottomWidth :@""];
}

- (NSString *)borderLeftWidth
{
    return [self getPropertyValue:@"borderLeftWidth"];
}

- (void)setBorderLeftWidth:(NSString *)borderLeftWidth
{
    [self setProperty:@"borderLeftWidth" :borderLeftWidth :@""];
}

- (NSString *)borderWidth
{
    return [self getPropertyValue:@"borderWidth"];
}

- (void)setBorderWidth:(NSString *)borderWidth
{
    [self setProperty:@"borderWidth" :borderWidth :@""];
}

- (NSString *)bottom
{
    return [self getPropertyValue:@"bottom"];
}

- (void)setBottom:(NSString *)bottom
{
    [self setProperty:@"bottom" :bottom :@""];
}

- (NSString *)captionSide
{
    return [self getPropertyValue:@"captionSide"];
}

- (void)setCaptionSide:(NSString *)captionSide
{
    [self setProperty:@"captionSide" :captionSide :@""];
}

- (NSString *)clear
{
    return [self getPropertyValue:@"clear"];
}

- (void)setClear:(NSString *)clear
{
    [self setProperty:@"clear" :clear :@""];
}

- (NSString *)clip
{
    return [self getPropertyValue:@"clip"];
}

- (void)setClip:(NSString *)clip
{
    [self setProperty:@"clip" :clip :@""];
}

- (NSString *)color
{
    return [self getPropertyValue:@"color"];
}

- (void)setColor:(NSString *)color
{
    [self setProperty:@"color" :color :@""];
}

- (NSString *)content
{
    return [self getPropertyValue:@"content"];
}

- (void)setContent:(NSString *)content
{
    [self setProperty:@"content" :content :@""];
}

- (NSString *)counterIncrement
{
    return [self getPropertyValue:@"counterIncrement"];
}

- (void)setCounterIncrement:(NSString *)counterIncrement
{
    [self setProperty:@"counterIncrement" :counterIncrement :@""];
}

- (NSString *)counterReset
{
    return [self getPropertyValue:@"counterReset"];
}

- (void)setCounterReset:(NSString *)counterReset
{
    [self setProperty:@"counterReset" :counterReset :@""];
}

- (NSString *)cue
{
    return [self getPropertyValue:@"cue"];
}

- (void)setCue:(NSString *)cue
{
    [self setProperty:@"cue" :cue :@""];
}

- (NSString *)cueAfter
{
    return [self getPropertyValue:@"cueAfter"];
}

- (void)setCueAfter:(NSString *)cueAfter
{
    [self setProperty:@"cueAfter" :cueAfter :@""];
}

- (NSString *)cueBefore
{
    return [self getPropertyValue:@"cueBefore"];
}

- (void)setCueBefore:(NSString *)cueBefore
{
    [self setProperty:@"cueBefore" :cueBefore :@""];
}

- (NSString *)cursor
{
    return [self getPropertyValue:@"cursor"];
}

- (void)setCursor:(NSString *)cursor
{
    [self setProperty:@"cursor" :cursor :@""];
}

- (NSString *)direction
{
    return [self getPropertyValue:@"direction"];
}

- (void)setDirection:(NSString *)direction
{
    [self setProperty:@"direction" :direction :@""];
}

- (NSString *)display
{
    return [self getPropertyValue:@"display"];
}

- (void)setDisplay:(NSString *)display
{
    [self setProperty:@"display" :display :@""];
}

- (NSString *)elevation
{
    return [self getPropertyValue:@"elevation"];
}

- (void)setElevation:(NSString *)elevation
{
    [self setProperty:@"elevation" :elevation :@""];
}

- (NSString *)emptyCells
{
    return [self getPropertyValue:@"emptyCells"];
}

- (void)setEmptyCells:(NSString *)emptyCells
{
    [self setProperty:@"emptyCells" :emptyCells :@""];
}

- (NSString *)cssFloat
{
    return [self getPropertyValue:@"cssFloat"];
}

- (void)setCssFloat:(NSString *)cssFloat
{
    [self setProperty:@"cssFloat" :cssFloat :@""];
}

- (NSString *)font
{
    return [self getPropertyValue:@"font"];
}

- (void)setFont:(NSString *)font
{
    [self setProperty:@"font" :font :@""];
}

- (NSString *)fontFamily
{
    return [self getPropertyValue:@"fontFamily"];
}

- (void)setFontFamily:(NSString *)fontFamily
{
    [self setProperty:@"fontFamily" :fontFamily :@""];
}

- (NSString *)fontSize
{
    return [self getPropertyValue:@"fontSize"];
}

- (void)setFontSize:(NSString *)fontSize
{
    [self setProperty:@"fontSize" :fontSize :@""];
}

- (NSString *)fontSizeAdjust
{
    return [self getPropertyValue:@"fontSizeAdjust"];
}

- (void)setFontSizeAdjust:(NSString *)fontSizeAdjust
{
    [self setProperty:@"fontSizeAdjust" :fontSizeAdjust :@""];
}

- (NSString *)fontStretch
{
    return [self getPropertyValue:@"fontStretch"];
}

- (void)setFontStretch:(NSString *)fontStretch
{
    [self setProperty:@"fontStretch" :fontStretch :@""];
}

- (NSString *)fontStyle
{
    return [self getPropertyValue:@"fontStyle"];
}

- (void)setFontStyle:(NSString *)fontStyle
{
    [self setProperty:@"fontStyle" :fontStyle :@""];
}

- (NSString *)fontVariant
{
    return [self getPropertyValue:@"fontVariant"];
}

- (void)setFontVariant:(NSString *)fontVariant
{
    [self setProperty:@"fontVariant" :fontVariant :@""];
}

- (NSString *)fontWeight
{
    return [self getPropertyValue:@"fontWeight"];
}

- (void)setFontWeight:(NSString *)fontWeight
{
    [self setProperty:@"fontWeight" :fontWeight :@""];
}

- (NSString *)height
{
    return [self getPropertyValue:@"height"];
}

- (void)setHeight:(NSString *)height
{
    [self setProperty:@"height" :height :@""];
}

- (NSString *)left
{
    return [self getPropertyValue:@"left"];
}

- (void)setLeft:(NSString *)left
{
    [self setProperty:@"left" :left :@""];
}

- (NSString *)letterSpacing
{
    return [self getPropertyValue:@"letterSpacing"];
}

- (void)setLetterSpacing:(NSString *)letterSpacing
{
    [self setProperty:@"letterSpacing" :letterSpacing :@""];
}

- (NSString *)lineHeight
{
    return [self getPropertyValue:@"lineHeight"];
}

- (void)setLineHeight:(NSString *)lineHeight
{
    [self setProperty:@"lineHeight" :lineHeight :@""];
}

- (NSString *)listStyle
{
    return [self getPropertyValue:@"listStyle"];
}

- (void)setListStyle:(NSString *)listStyle
{
    [self setProperty:@"listStyle" :listStyle :@""];
}

- (NSString *)listStyleImage
{
    return [self getPropertyValue:@"listStyleImage"];
}

- (void)setListStyleImage:(NSString *)listStyleImage
{
    [self setProperty:@"listStyleImage" :listStyleImage :@""];
}

- (NSString *)listStylePosition
{
    return [self getPropertyValue:@"listStylePosition"];
}

- (void)setListStylePosition:(NSString *)listStylePosition
{
    [self setProperty:@"listStylePosition" :listStylePosition :@""];
}

- (NSString *)listStyleType
{
    return [self getPropertyValue:@"listStyleType"];
}

- (void)setListStyleType:(NSString *)listStyleType
{
    [self setProperty:@"listStyleType" :listStyleType :@""];
}

- (NSString *)margin
{
    return [self getPropertyValue:@"margin"];
}

- (void)setMargin:(NSString *)margin
{
    [self setProperty:@"margin" :margin :@""];
}

- (NSString *)marginTop
{
    return [self getPropertyValue:@"marginTop"];
}

- (void)setMarginTop:(NSString *)marginTop
{
    [self setProperty:@"marginTop" :marginTop :@""];
}

- (NSString *)marginRight
{
    return [self getPropertyValue:@"marginRight"];
}

- (void)setMarginRight:(NSString *)marginRight
{
    [self setProperty:@"marginRight" :marginRight :@""];
}

- (NSString *)marginBottom
{
    return [self getPropertyValue:@"marginBottom"];
}

- (void)setMarginBottom:(NSString *)marginBottom
{
    [self setProperty:@"marginBottom" :marginBottom :@""];
}

- (NSString *)marginLeft
{
    return [self getPropertyValue:@"marginLeft"];
}

- (void)setMarginLeft:(NSString *)marginLeft
{
    [self setProperty:@"marginLeft" :marginLeft :@""];
}

- (NSString *)markerOffset
{
    return [self getPropertyValue:@"markerOffset"];
}

- (void)setMarkerOffset:(NSString *)markerOffset
{
    [self setProperty:@"markerOffset" :markerOffset :@""];
}

- (NSString *)marks
{
    return [self getPropertyValue:@"marks"];
}

- (void)setMarks:(NSString *)marks
{
    [self setProperty:@"marks" :marks :@""];
}

- (NSString *)maxHeight
{
    return [self getPropertyValue:@"maxHeight"];
}

- (void)setMaxHeight:(NSString *)maxHeight
{
    [self setProperty:@"maxHeight" :maxHeight :@""];
}

- (NSString *)maxWidth
{
    return [self getPropertyValue:@"maxWidth"];
}

- (void)setMaxWidth:(NSString *)maxWidth
{
    [self setProperty:@"maxWidth" :maxWidth :@""];
}

- (NSString *)minHeight
{
    return [self getPropertyValue:@"minHeight"];
}

- (void)setMinHeight:(NSString *)minHeight
{
    [self setProperty:@"minHeight" :minHeight :@""];
}

- (NSString *)minWidth
{
    return [self getPropertyValue:@"minWidth"];
}

- (void)setMinWidth:(NSString *)minWidth
{
    [self setProperty:@"minWidth" :minWidth :@""];
}

- (NSString *)orphans
{
    return [self getPropertyValue:@"orphans"];
}

- (void)setOrphans:(NSString *)orphans
{
    [self setProperty:@"orphans" :orphans :@""];
}

- (NSString *)outline
{
    return [self getPropertyValue:@"outline"];
}

- (void)setOutline:(NSString *)outline
{
    [self setProperty:@"outline" :outline :@""];
}

- (NSString *)outlineColor
{
    return [self getPropertyValue:@"outlineColor"];
}

- (void)setOutlineColor:(NSString *)outlineColor
{
    [self setProperty:@"outlineColor" :outlineColor :@""];
}

- (NSString *)outlineStyle
{
    return [self getPropertyValue:@"outlineStyle"];
}

- (void)setOutlineStyle:(NSString *)outlineStyle
{
    [self setProperty:@"outlineStyle" :outlineStyle :@""];
}

- (NSString *)outlineWidth
{
    return [self getPropertyValue:@"outlineWidth"];
}

- (void)setOutlineWidth:(NSString *)outlineWidth
{
    [self setProperty:@"outlineWidth" :outlineWidth :@""];
}

- (NSString *)overflow
{
    return [self getPropertyValue:@"overflow"];
}

- (void)setOverflow:(NSString *)overflow
{
    [self setProperty:@"overflow" :overflow :@""];
}

- (NSString *)padding
{
    return [self getPropertyValue:@"padding"];
}

- (void)setPadding:(NSString *)padding
{
    [self setProperty:@"padding" :padding :@""];
}

- (NSString *)paddingTop
{
    return [self getPropertyValue:@"paddingTop"];
}

- (void)setPaddingTop:(NSString *)paddingTop
{
    [self setProperty:@"paddingTop" :paddingTop :@""];
}

- (NSString *)paddingRight
{
    return [self getPropertyValue:@"paddingRight"];
}

- (void)setPaddingRight:(NSString *)paddingRight
{
    [self setProperty:@"paddingRight" :paddingRight :@""];
}

- (NSString *)paddingBottom
{
    return [self getPropertyValue:@"paddingBottom"];
}

- (void)setPaddingBottom:(NSString *)paddingBottom
{
    [self setProperty:@"paddingBottom" :paddingBottom :@""];
}

- (NSString *)paddingLeft
{
    return [self getPropertyValue:@"paddingLeft"];
}

- (void)setPaddingLeft:(NSString *)paddingLeft
{
    [self setProperty:@"paddingLeft" :paddingLeft :@""];
}

- (NSString *)page
{
    return [self getPropertyValue:@"page"];
}

- (void)setPage:(NSString *)page
{
    [self setProperty:@"page" :page :@""];
}

- (NSString *)pageBreakAfter
{
    return [self getPropertyValue:@"pageBreakAfter"];
}

- (void)setPageBreakAfter:(NSString *)pageBreakAfter
{
    [self setProperty:@"pageBreakAfter" :pageBreakAfter :@""];
}

- (NSString *)pageBreakBefore
{
    return [self getPropertyValue:@"pageBreakBefore"];
}

- (void)setPageBreakBefore:(NSString *)pageBreakBefore
{
    [self setProperty:@"pageBreakBefore" :pageBreakBefore :@""];
}

- (NSString *)pageBreakInside
{
    return [self getPropertyValue:@"pageBreakInside"];
}

- (void)setPageBreakInside:(NSString *)pageBreakInside
{
    [self setProperty:@"pageBreakInside" :pageBreakInside :@""];
}

- (NSString *)pause
{
    return [self getPropertyValue:@"pause"];
}

- (void)setPause:(NSString *)pause
{
    [self setProperty:@"pause" :pause :@""];
}

- (NSString *)pauseAfter
{
    return [self getPropertyValue:@"pauseAfter"];
}

- (void)setPauseAfter:(NSString *)pauseAfter
{
    [self setProperty:@"pauseAfter" :pauseAfter :@""];
}

- (NSString *)pauseBefore
{
    return [self getPropertyValue:@"pauseBefore"];
}

- (void)setPauseBefore:(NSString *)pauseBefore
{
    [self setProperty:@"pauseBefore" :pauseBefore :@""];
}

- (NSString *)pitch
{
    return [self getPropertyValue:@"pitch"];
}

- (void)setPitch:(NSString *)pitch
{
    [self setProperty:@"pitch" :pitch :@""];
}

- (NSString *)pitchRange
{
    return [self getPropertyValue:@"pitchRange"];
}

- (void)setPitchRange:(NSString *)pitchRange
{
    [self setProperty:@"pitchRange" :pitchRange :@""];
}

- (NSString *)playDuring
{
    return [self getPropertyValue:@"playDuring"];
}

- (void)setPlayDuring:(NSString *)playDuring
{
    [self setProperty:@"playDuring" :playDuring :@""];
}

- (NSString *)position
{
    return [self getPropertyValue:@"position"];
}

- (void)setPosition:(NSString *)position
{
    [self setProperty:@"position" :position :@""];
}

- (NSString *)quotes
{
    return [self getPropertyValue:@"quotes"];
}

- (void)setQuotes:(NSString *)quotes
{
    [self setProperty:@"quotes" :quotes :@""];
}

- (NSString *)richness
{
    return [self getPropertyValue:@"richness"];
}

- (void)setRichness:(NSString *)richness
{
    [self setProperty:@"richness" :richness :@""];
}

- (NSString *)right
{
    return [self getPropertyValue:@"right"];
}

- (void)setRight:(NSString *)right
{
    [self setProperty:@"right" :right :@""];
}

- (NSString *)size
{
    return [self getPropertyValue:@"size"];
}

- (void)setSize:(NSString *)size
{
    [self setProperty:@"size" :size :@""];
}

- (NSString *)speak
{
    return [self getPropertyValue:@"speak"];
}

- (void)setSpeak:(NSString *)speak
{
    [self setProperty:@"speak" :speak :@""];
}

- (NSString *)speakHeader
{
    return [self getPropertyValue:@"speakHeader"];
}

- (void)setSpeakHeader:(NSString *)speakHeader
{
    [self setProperty:@"speakHeader" :speakHeader :@""];
}

- (NSString *)speakNumeral
{
    return [self getPropertyValue:@"speakNumeral"];
}

- (void)setSpeakNumeral:(NSString *)speakNumeral
{
    [self setProperty:@"speakNumeral" :speakNumeral :@""];
}

- (NSString *)speakPunctuation
{
    return [self getPropertyValue:@"speakPunctuation"];
}

- (void)setSpeakPunctuation:(NSString *)speakPunctuation
{
    [self setProperty:@"speakPunctuation" :speakPunctuation :@""];
}

- (NSString *)speechRate
{
    return [self getPropertyValue:@"speechRate"];
}

- (void)setSpeechRate:(NSString *)speechRate
{
    [self setProperty:@"speechRate" :speechRate :@""];
}

- (NSString *)stress
{
    return [self getPropertyValue:@"stress"];
}

- (void)setStress:(NSString *)stress
{
    [self setProperty:@"stress" :stress :@""];
}

- (NSString *)tableLayout
{
    return [self getPropertyValue:@"tableLayout"];
}

- (void)setTableLayout:(NSString *)tableLayout
{
    [self setProperty:@"tableLayout" :tableLayout :@""];
}

- (NSString *)textAlign
{
    return [self getPropertyValue:@"textAlign"];
}

- (void)setTextAlign:(NSString *)textAlign
{
    [self setProperty:@"textAlign" :textAlign :@""];
}

- (NSString *)textDecoration
{
    return [self getPropertyValue:@"textDecoration"];
}

- (void)setTextDecoration:(NSString *)textDecoration
{
    [self setProperty:@"textDecoration" :textDecoration :@""];
}

- (NSString *)textIndent
{
    return [self getPropertyValue:@"textIndent"];
}

- (void)setTextIndent:(NSString *)textIndent
{
    [self setProperty:@"textIndent" :textIndent :@""];
}

- (NSString *)textShadow
{
    return [self getPropertyValue:@"textShadow"];
}

- (void)setTextShadow:(NSString *)textShadow
{
    [self setProperty:@"textShadow" :textShadow :@""];
}

- (NSString *)textTransform
{
    return [self getPropertyValue:@"textTransform"];
}

- (void)setTextTransform:(NSString *)textTransform
{
    [self setProperty:@"textTransform" :textTransform :@""];
}

- (NSString *)top
{
    return [self getPropertyValue:@"top"];
}

- (void)setTop:(NSString *)top
{
    [self setProperty:@"top" :top :@""];
}

- (NSString *)unicodeBidi
{
    return [self getPropertyValue:@"unicodeBidi"];
}

- (void)setUnicodeBidi:(NSString *)unicodeBidi
{
    [self setProperty:@"unicodeBidi" :unicodeBidi :@""];
}

- (NSString *)verticalAlign
{
    return [self getPropertyValue:@"verticalAlign"];
}

- (void)setVerticalAlign:(NSString *)verticalAlign
{
    [self setProperty:@"verticalAlign" :verticalAlign :@""];
}

- (NSString *)visibility
{
    return [self getPropertyValue:@"visibility"];
}

- (void)setVisibility:(NSString *)visibility
{
    [self setProperty:@"visibility" :visibility :@""];
}

- (NSString *)voiceFamily
{
    return [self getPropertyValue:@"voiceFamily"];
}

- (void)setVoiceFamily:(NSString *)voiceFamily
{
    [self setProperty:@"voiceFamily" :voiceFamily :@""];
}

- (NSString *)volume
{
    return [self getPropertyValue:@"volume"];
}

- (void)setVolume:(NSString *)volume
{
    [self setProperty:@"volume" :volume :@""];
}

- (NSString *)whiteSpace
{
    return [self getPropertyValue:@"whiteSpace"];
}

- (void)setWhiteSpace:(NSString *)whiteSpace
{
    [self setProperty:@"whiteSpace" :whiteSpace :@""];
}

- (NSString *)widows
{
    return [self getPropertyValue:@"widows"];
}

- (void)setWidows:(NSString *)widows
{
    [self setProperty:@"widows" :widows :@""];
}

- (NSString *)width
{
    return [self getPropertyValue:@"width"];
}

- (void)setWidth:(NSString *)width
{
    [self setProperty:@"width" :width :@""];
}

- (NSString *)wordSpacing
{
    return [self getPropertyValue:@"wordSpacing"];
}

- (void)setWordSpacing:(NSString *)wordSpacing
{
    [self setProperty:@"wordSpacing" :wordSpacing :@""];
}

- (NSString *)zIndex
{
    return [self getPropertyValue:@"zIndex"];
}

- (void)setZIndex:(NSString *)zIndex
{
    [self setProperty:@"zIndex" :zIndex :@""];
}

@end

//------------------------------------------------------------------------------------------


@implementation DOMObject (DOMLinkStyle)

- (DOMStyleSheet *)sheet
{
    ERROR("unimplemented");
    return nil;
}

@end

@implementation DOMDocument (DOMViewCSS)

- (DOMCSSStyleDeclaration *)getComputedStyle:(DOMElement *)elt :(NSString *)pseudoElt
{
    ERROR("unimplemented");
    return nil;
}

@end
