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
        reinterpret_cast<StyleSheetImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (StyleSheetImpl *)_DOMStyleSheetImpl
{
    return reinterpret_cast<StyleSheetImpl *>(_internal);
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
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setDOMWrapperForImpl(self, impl);
    return self;
}

+ (DOMStyleSheet *)_DOMStyleSheetWithImpl:(StyleSheetImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapperForImpl(impl);
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
        reinterpret_cast<StyleSheetListImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (StyleSheetListImpl *)_styleSheetListImpl
{
    return reinterpret_cast<StyleSheetListImpl *>(_internal);
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
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setDOMWrapperForImpl(self, impl);
    return self;
}

+ (DOMStyleSheetList *)_styleSheetListWithImpl:(StyleSheetListImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithStyleSheetListImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSStyleSheet

@implementation DOMCSSStyleSheet

- (CSSStyleSheetImpl *)_CSSStyleSheetImpl
{
    return reinterpret_cast<CSSStyleSheetImpl *>(_internal);
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
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setDOMWrapperForImpl(self, impl);
    return self;
}

+ (DOMCSSStyleSheet *)_CSSStyleSheetWithImpl:(CSSStyleSheetImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapperForImpl(impl);
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
        reinterpret_cast<MediaListImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (MediaListImpl *)_mediaListImpl
{
    return reinterpret_cast<MediaListImpl *>(_internal);
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
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setDOMWrapperForImpl(self, impl);
    return self;
}

+ (DOMMediaList *)_mediaListWithImpl:(MediaListImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapperForImpl(impl);
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
        reinterpret_cast<CSSRuleListImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (CSSRuleListImpl *)_ruleListImpl
{
    return reinterpret_cast<CSSRuleListImpl *>(_internal);
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
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setDOMWrapperForImpl(self, impl);
    return self;
}

+ (DOMCSSRuleList *)_ruleListWithImpl:(CSSRuleListImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapperForImpl(impl);
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
        reinterpret_cast<CSSRuleImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (CSSRuleImpl *)_ruleImpl
{
    return reinterpret_cast<CSSRuleImpl *>(_internal);
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
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setDOMWrapperForImpl(self, impl);
    return self;
}

+ (DOMCSSRule *)_ruleWithImpl:(CSSRuleImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapperForImpl(impl);
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
    return reinterpret_cast<CSSStyleRuleImpl *>(_internal);
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
    return reinterpret_cast<CSSMediaRuleImpl *>(_internal);
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
    return reinterpret_cast<CSSFontFaceRuleImpl *>(_internal);
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
    return reinterpret_cast<CSSPageRuleImpl *>(_internal);
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
    return reinterpret_cast<CSSImportRuleImpl *>(_internal);
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
    return reinterpret_cast<CSSCharsetRuleImpl *>(_internal);
}

- (NSString *)encoding
{
    return [self _importRuleImpl]->encoding();
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSStyleDeclaration

@implementation DOMCSSStyleDeclaration

- (void)dealloc
{
    if (_internal) {
        reinterpret_cast<CSSStyleDeclarationImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (CSSStyleDeclarationImpl *)_styleDeclarationImpl
{
    return reinterpret_cast<CSSStyleDeclarationImpl *>(_internal);
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
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setDOMWrapperForImpl(self, impl);
    return self;
}

+ (DOMCSSStyleDeclaration *)_styleDeclarationWithImpl:(CSSStyleDeclarationImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithStyleDeclarationImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMCSSValue

@implementation DOMCSSValue

- (void)dealloc
{
    if (_internal) {
        reinterpret_cast<CSSValueImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (CSSValueImpl *)_valueImpl
{
    return reinterpret_cast<CSSValueImpl *>(_internal);
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
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setDOMWrapperForImpl(self, impl);
    return self;
}

+ (DOMCSSValue *)_valueWithImpl:(CSSValueImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapperForImpl(impl);
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
    return static_cast<CSSPrimitiveValueImpl *>(reinterpret_cast<CSSValueImpl *>(_internal));
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
    return static_cast<CSSValueListImpl *>(reinterpret_cast<CSSValueImpl *>(_internal));
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

//------------------------------------------------------------------------------------------
// DOMRect

@implementation DOMRect

- (void)dealloc
{
    if (_internal) {
        reinterpret_cast<RectImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (RectImpl *)_rectImpl
{
    return reinterpret_cast<RectImpl *>(_internal);
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
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setDOMWrapperForImpl(self, impl);
    return self;
}

+ (DOMRect *)_rectWithImpl:(RectImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapperForImpl(impl);
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
        reinterpret_cast<CounterImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (CounterImpl *)_counterImpl
{
    return reinterpret_cast<CounterImpl *>(_internal);
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
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setDOMWrapperForImpl(self, impl);
    return self;
}

+ (DOMCounter *)_counterWithImpl:(CounterImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithCounterImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
