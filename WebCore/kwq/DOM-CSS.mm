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

#import "css_base.h"
#import "css_ruleimpl.h"
#import "css_stylesheetimpl.h"
#import "css_valueimpl.h"
#import "css_value.h"
#import "dom_string.h"
#import "dom_xmlimpl.h"
#import "KWQColor.h"
#import "shared.h"
#import "dom_stringimpl.h"
#import "dom2_viewsimpl.h"
#import "html_headimpl.h"

#import "DOMInternal.h"
#import "KWQAssertions.h"
#import "KWQFoundationExtras.h"

using DOM::AbstractViewImpl;
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
using DOM::HTMLLinkElementImpl;
using DOM::HTMLStyleElementImpl;
using DOM::MediaListImpl;
using DOM::ProcessingInstructionImpl;
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

@interface DOMCSSPrimitiveValue (WebCoreInternal)
+ (DOMCSSPrimitiveValue *)_valueWithImpl:(CSSValueImpl *)impl;
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

- (void)finalize
{
    if (_internal) {
        DOM_cast<StyleSheetImpl *>(_internal)->deref();
    }
    [super finalize];
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

- (id)_initWithStyleSheetImpl:(StyleSheetImpl *)impl
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
    
    Class wrapperClass;
    if (impl->isCSSStyleSheet())
        wrapperClass = [DOMCSSStyleSheet class];
    else
        wrapperClass = [DOMStyleSheet class];
    return [[[wrapperClass alloc] _initWithStyleSheetImpl:impl] autorelease];
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

- (void)finalize
{
    if (_internal) {
        DOM_cast<StyleSheetListImpl *>(_internal)->deref();
    }
    [super finalize];
}

- (StyleSheetListImpl *)_styleSheetListImpl
{
    return DOM_cast<StyleSheetListImpl *>(_internal);
}

- (unsigned)length
{
    return [self _styleSheetListImpl]->length();
}

- (DOMStyleSheet *)item:(unsigned)index
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
    return [DOMCSSRuleList _ruleListWithImpl:[self _CSSStyleSheetImpl]->cssRules()];
}

- (unsigned)insertRule:(NSString *)rule :(unsigned)index
{
    int exceptionCode;
    unsigned result = [self _CSSStyleSheetImpl]->insertRule(rule, index, exceptionCode);
    raiseOnDOMError(exceptionCode);
    return result;
}

- (void)deleteRule:(unsigned)index
{
    int exceptionCode;
    [self _CSSStyleSheetImpl]->deleteRule(index, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

@end

@implementation DOMCSSStyleSheet (WebCoreInternal)

+ (DOMCSSStyleSheet *)_CSSStyleSheetWithImpl:(CSSStyleSheetImpl *)impl
{
    return (DOMCSSStyleSheet *)[DOMStyleSheet _DOMStyleSheetWithImpl:impl];
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

- (void)finalize
{
    if (_internal) {
        DOM_cast<MediaListImpl *>(_internal)->deref();
    }
    [super finalize];
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

- (unsigned)length
{
    return [self _mediaListImpl]->length();
}

- (NSString *)item:(unsigned)index
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

- (void)finalize
{
    if (_internal) {
        DOM_cast<CSSRuleListImpl *>(_internal)->deref();
    }
    [super finalize];
}

- (CSSRuleListImpl *)_ruleListImpl
{
    return DOM_cast<CSSRuleListImpl *>(_internal);
}

- (unsigned)length
{
    return [self _ruleListImpl]->length();
}

- (DOMCSSRule *)item:(unsigned)index
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

- (void)finalize
{
    if (_internal) {
        DOM_cast<CSSRuleImpl *>(_internal)->deref();
    }
    [super finalize];
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

- (unsigned)insertRule:(NSString *)rule :(unsigned)index
{
    return [self _mediaRuleImpl]->insertRule(rule, index);
}

- (void)deleteRule:(unsigned)index
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

- (void)finalize
{
    if (_internal) {
        DOM_cast<CSSStyleDeclarationImpl *>(_internal)->deref();
    }
    [super finalize];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"DOMCSSStyleDeclaration: %@", [self cssText]];
}

- (NSString *)cssText
{
    return [self _styleDeclarationImpl]->cssText();
}

- (void)setCssText:(NSString *)cssText
{
    int exceptionCode;
    [self _styleDeclarationImpl]->setCssText(cssText, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (NSString *)getPropertyValue:(NSString *)propertyName
{
    return [self _styleDeclarationImpl]->getPropertyValue(propertyName);
}

- (DOMCSSValue *)getPropertyCSSValue:(NSString *)propertyName
{
    return [DOMCSSValue _valueWithImpl:[self _styleDeclarationImpl]->getPropertyCSSValue(propertyName)];
}

- (NSString *)removeProperty:(NSString *)propertyName
{
    int exceptionCode = 0;
    DOMString result = [self _styleDeclarationImpl]->removeProperty(propertyName, exceptionCode);
    raiseOnDOMError(exceptionCode);
    return result;
}

- (NSString *)getPropertyPriority:(NSString *)propertyName
{
    return [self _styleDeclarationImpl]->getPropertyPriority(propertyName);
}

- (void)setProperty:(NSString *)propertyName :(NSString *)value :(NSString *)priority
{
    int exceptionCode;
    [self _styleDeclarationImpl]->setProperty(propertyName, value, priority, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (unsigned)length
{
    return [self _styleDeclarationImpl]->length();
}

- (NSString *)item:(unsigned)index
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

- (void)finalize
{
    if (_internal) {
        DOM_cast<CSSValueImpl *>(_internal)->deref();
    }
    [super finalize];
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
    [self _valueImpl]->setCssText(cssText);
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

+ (DOMCSSPrimitiveValue *)_valueWithImpl:(CSSValueImpl *)impl
{
    return (DOMCSSPrimitiveValue *)([DOMCSSValue _valueWithImpl: impl]);
}

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

- (unsigned)length
{
    return [self _valueListImpl]->length();
}

- (DOMCSSValue *)item:(unsigned)index
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

- (void)finalize
{
    removeWrapperForRGB(reinterpret_cast<QRgb>(_internal));
    [super finalize];
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

@implementation DOMRGBColor (WebPrivate)

- (NSColor *)_color
{
    QRgb rgb = reinterpret_cast<QRgb>(_internal);
    return nsColor(QColor(rgb));
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

- (void)finalize
{
    if (_internal) {
        DOM_cast<RectImpl *>(_internal)->deref();
    }
    [super finalize];
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

- (void)finalize
{
    if (_internal) {
        DOM_cast<CounterImpl *>(_internal)->deref();
    }
    [super finalize];
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
    return [self getPropertyValue:@"background-attachment"];
}

- (void)setBackgroundAttachment:(NSString *)backgroundAttachment
{
    [self setProperty:@"background-attachment" :backgroundAttachment :@""];
}

- (NSString *)backgroundColor
{
    return [self getPropertyValue:@"background-color"];
}

- (void)setBackgroundColor:(NSString *)backgroundColor
{
    [self setProperty:@"background-color" :backgroundColor :@""];
}

- (NSString *)backgroundImage
{
    return [self getPropertyValue:@"background-image"];
}

- (void)setBackgroundImage:(NSString *)backgroundImage
{
    [self setProperty:@"background-image" :backgroundImage :@""];
}

- (NSString *)backgroundPosition
{
    return [self getPropertyValue:@"background-position"];
}

- (void)setBackgroundPosition:(NSString *)backgroundPosition
{
    [self setProperty:@"background-position" :backgroundPosition :@""];
}

- (NSString *)backgroundRepeat
{
    return [self getPropertyValue:@"background-repeat"];
}

- (void)setBackgroundRepeat:(NSString *)backgroundRepeat
{
    [self setProperty:@"background-repeat" :backgroundRepeat :@""];
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
    return [self getPropertyValue:@"border-collapse"];
}

- (void)setBorderCollapse:(NSString *)borderCollapse
{
    [self setProperty:@"border-collapse" :borderCollapse :@""];
}

- (NSString *)borderColor
{
    return [self getPropertyValue:@"border-color"];
}

- (void)setBorderColor:(NSString *)borderColor
{
    [self setProperty:@"border-color" :borderColor :@""];
}

- (NSString *)borderSpacing
{
    return [self getPropertyValue:@"border-spacing"];
}

- (void)setBorderSpacing:(NSString *)borderSpacing
{
    [self setProperty:@"border-spacing" :borderSpacing :@""];
}

- (NSString *)borderStyle
{
    return [self getPropertyValue:@"border-style"];
}

- (void)setBorderStyle:(NSString *)borderStyle
{
    [self setProperty:@"border-style" :borderStyle :@""];
}

- (NSString *)borderTop
{
    return [self getPropertyValue:@"border-top"];
}

- (void)setBorderTop:(NSString *)borderTop
{
    [self setProperty:@"border-top" :borderTop :@""];
}

- (NSString *)borderRight
{
    return [self getPropertyValue:@"border-right"];
}

- (void)setBorderRight:(NSString *)borderRight
{
    [self setProperty:@"border-right" :borderRight :@""];
}

- (NSString *)borderBottom
{
    return [self getPropertyValue:@"border-bottom"];
}

- (void)setBorderBottom:(NSString *)borderBottom
{
    [self setProperty:@"border-bottom" :borderBottom :@""];
}

- (NSString *)borderLeft
{
    return [self getPropertyValue:@"border-left"];
}

- (void)setBorderLeft:(NSString *)borderLeft
{
    [self setProperty:@"border-left" :borderLeft :@""];
}

- (NSString *)borderTopColor
{
    return [self getPropertyValue:@"border-top-color"];
}

- (void)setBorderTopColor:(NSString *)borderTopColor
{
    [self setProperty:@"border-top-color" :borderTopColor :@""];
}

- (NSString *)borderRightColor
{
    return [self getPropertyValue:@"border-right-color"];
}

- (void)setBorderRightColor:(NSString *)borderRightColor
{
    [self setProperty:@"border-right-color" :borderRightColor :@""];
}

- (NSString *)borderBottomColor
{
    return [self getPropertyValue:@"border-bottom-color"];
}

- (void)setBorderBottomColor:(NSString *)borderBottomColor
{
    [self setProperty:@"border-bottom-color" :borderBottomColor :@""];
}

- (NSString *)borderLeftColor
{
    return [self getPropertyValue:@"border-left-color"];
}

- (void)setBorderLeftColor:(NSString *)borderLeftColor
{
    [self setProperty:@"border-left-color" :borderLeftColor :@""];
}

- (NSString *)borderTopStyle
{
    return [self getPropertyValue:@"border-top-style"];
}

- (void)setBorderTopStyle:(NSString *)borderTopStyle
{
    [self setProperty:@"border-top-style" :borderTopStyle :@""];
}

- (NSString *)borderRightStyle
{
    return [self getPropertyValue:@"border-right-style"];
}

- (void)setBorderRightStyle:(NSString *)borderRightStyle
{
    [self setProperty:@"border-right-style" :borderRightStyle :@""];
}

- (NSString *)borderBottomStyle
{
    return [self getPropertyValue:@"border-bottom-style"];
}

- (void)setBorderBottomStyle:(NSString *)borderBottomStyle
{
    [self setProperty:@"border-bottom-style" :borderBottomStyle :@""];
}

- (NSString *)borderLeftStyle
{
    return [self getPropertyValue:@"border-left-style"];
}

- (void)setBorderLeftStyle:(NSString *)borderLeftStyle
{
    [self setProperty:@"border-left-style" :borderLeftStyle :@""];
}

- (NSString *)borderTopWidth
{
    return [self getPropertyValue:@"border-top-width"];
}

- (void)setBorderTopWidth:(NSString *)borderTopWidth
{
    [self setProperty:@"border-top-width" :borderTopWidth :@""];
}

- (NSString *)borderRightWidth
{
    return [self getPropertyValue:@"border-right-width"];
}

- (void)setBorderRightWidth:(NSString *)borderRightWidth
{
    [self setProperty:@"border-right-width" :borderRightWidth :@""];
}

- (NSString *)borderBottomWidth
{
    return [self getPropertyValue:@"border-bottom-width"];
}

- (void)setBorderBottomWidth:(NSString *)borderBottomWidth
{
    [self setProperty:@"border-bottom-width" :borderBottomWidth :@""];
}

- (NSString *)borderLeftWidth
{
    return [self getPropertyValue:@"border-left-width"];
}

- (void)setBorderLeftWidth:(NSString *)borderLeftWidth
{
    [self setProperty:@"border-left-width" :borderLeftWidth :@""];
}

- (NSString *)borderWidth
{
    return [self getPropertyValue:@"border-width"];
}

- (void)setBorderWidth:(NSString *)borderWidth
{
    [self setProperty:@"border-width" :borderWidth :@""];
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
    return [self getPropertyValue:@"caption-side"];
}

- (void)setCaptionSide:(NSString *)captionSide
{
    [self setProperty:@"caption-side" :captionSide :@""];
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
    return [self getPropertyValue:@"counter-increment"];
}

- (void)setCounterIncrement:(NSString *)counterIncrement
{
    [self setProperty:@"counter-increment" :counterIncrement :@""];
}

- (NSString *)counterReset
{
    return [self getPropertyValue:@"counter-reset"];
}

- (void)setCounterReset:(NSString *)counterReset
{
    [self setProperty:@"counter-reset" :counterReset :@""];
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
    return [self getPropertyValue:@"cue-after"];
}

- (void)setCueAfter:(NSString *)cueAfter
{
    [self setProperty:@"cue-after" :cueAfter :@""];
}

- (NSString *)cueBefore
{
    return [self getPropertyValue:@"cue-before"];
}

- (void)setCueBefore:(NSString *)cueBefore
{
    [self setProperty:@"cue-before" :cueBefore :@""];
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
    return [self getPropertyValue:@"empty-cells"];
}

- (void)setEmptyCells:(NSString *)emptyCells
{
    [self setProperty:@"empty-cells" :emptyCells :@""];
}

- (NSString *)cssFloat
{
    return [self getPropertyValue:@"css-float"];
}

- (void)setCssFloat:(NSString *)cssFloat
{
    [self setProperty:@"css-float" :cssFloat :@""];
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
    return [self getPropertyValue:@"font-family"];
}

- (void)setFontFamily:(NSString *)fontFamily
{
    [self setProperty:@"font-family" :fontFamily :@""];
}

- (NSString *)fontSize
{
    return [self getPropertyValue:@"font-size"];
}

- (void)setFontSize:(NSString *)fontSize
{
    [self setProperty:@"font-size" :fontSize :@""];
}

- (NSString *)fontSizeAdjust
{
    return [self getPropertyValue:@"font-size-adjust"];
}

- (void)setFontSizeAdjust:(NSString *)fontSizeAdjust
{
    [self setProperty:@"font-size-adjust" :fontSizeAdjust :@""];
}

- (NSString *)_fontSizeDelta
{
    return [self getPropertyValue:@"-khtml-font-size-delta"];
}

- (void)_setFontSizeDelta:(NSString *)fontSizeDelta
{
    [self setProperty:@"-khtml-font-size-delta" :fontSizeDelta :@""];
}

- (NSString *)fontStretch
{
    return [self getPropertyValue:@"font-stretch"];
}

- (void)setFontStretch:(NSString *)fontStretch
{
    [self setProperty:@"font-stretch" :fontStretch :@""];
}

- (NSString *)fontStyle
{
    return [self getPropertyValue:@"font-style"];
}

- (void)setFontStyle:(NSString *)fontStyle
{
    [self setProperty:@"font-style" :fontStyle :@""];
}

- (NSString *)fontVariant
{
    return [self getPropertyValue:@"font-variant"];
}

- (void)setFontVariant:(NSString *)fontVariant
{
    [self setProperty:@"font-variant" :fontVariant :@""];
}

- (NSString *)fontWeight
{
    return [self getPropertyValue:@"font-weight"];
}

- (void)setFontWeight:(NSString *)fontWeight
{
    [self setProperty:@"font-weight" :fontWeight :@""];
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
    return [self getPropertyValue:@"letter-spacing"];
}

- (void)setLetterSpacing:(NSString *)letterSpacing
{
    [self setProperty:@"letter-spacing" :letterSpacing :@""];
}

- (NSString *)lineHeight
{
    return [self getPropertyValue:@"line-height"];
}

- (void)setLineHeight:(NSString *)lineHeight
{
    [self setProperty:@"line-height" :lineHeight :@""];
}

- (NSString *)listStyle
{
    return [self getPropertyValue:@"list-style"];
}

- (void)setListStyle:(NSString *)listStyle
{
    [self setProperty:@"list-style" :listStyle :@""];
}

- (NSString *)listStyleImage
{
    return [self getPropertyValue:@"list-style-image"];
}

- (void)setListStyleImage:(NSString *)listStyleImage
{
    [self setProperty:@"list-style-image" :listStyleImage :@""];
}

- (NSString *)listStylePosition
{
    return [self getPropertyValue:@"list-style-position"];
}

- (void)setListStylePosition:(NSString *)listStylePosition
{
    [self setProperty:@"list-style-position" :listStylePosition :@""];
}

- (NSString *)listStyleType
{
    return [self getPropertyValue:@"list-style-type"];
}

- (void)setListStyleType:(NSString *)listStyleType
{
    [self setProperty:@"list-style-type" :listStyleType :@""];
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
    return [self getPropertyValue:@"margin-top"];
}

- (void)setMarginTop:(NSString *)marginTop
{
    [self setProperty:@"margin-top" :marginTop :@""];
}

- (NSString *)marginRight
{
    return [self getPropertyValue:@"margin-right"];
}

- (void)setMarginRight:(NSString *)marginRight
{
    [self setProperty:@"margin-right" :marginRight :@""];
}

- (NSString *)marginBottom
{
    return [self getPropertyValue:@"margin-bottom"];
}

- (void)setMarginBottom:(NSString *)marginBottom
{
    [self setProperty:@"margin-bottom" :marginBottom :@""];
}

- (NSString *)marginLeft
{
    return [self getPropertyValue:@"margin-left"];
}

- (void)setMarginLeft:(NSString *)marginLeft
{
    [self setProperty:@"margin-left" :marginLeft :@""];
}

- (NSString *)markerOffset
{
    return [self getPropertyValue:@"marker-offset"];
}

- (void)setMarkerOffset:(NSString *)markerOffset
{
    [self setProperty:@"marker-offset" :markerOffset :@""];
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
    return [self getPropertyValue:@"max-height"];
}

- (void)setMaxHeight:(NSString *)maxHeight
{
    [self setProperty:@"max-height" :maxHeight :@""];
}

- (NSString *)maxWidth
{
    return [self getPropertyValue:@"max-width"];
}

- (void)setMaxWidth:(NSString *)maxWidth
{
    [self setProperty:@"max-width" :maxWidth :@""];
}

- (NSString *)minHeight
{
    return [self getPropertyValue:@"min-height"];
}

- (void)setMinHeight:(NSString *)minHeight
{
    [self setProperty:@"min-height" :minHeight :@""];
}

- (NSString *)minWidth
{
    return [self getPropertyValue:@"min-width"];
}

- (void)setMinWidth:(NSString *)minWidth
{
    [self setProperty:@"min-width" :minWidth :@""];
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
    return [self getPropertyValue:@"outline-color"];
}

- (void)setOutlineColor:(NSString *)outlineColor
{
    [self setProperty:@"outline-color" :outlineColor :@""];
}

- (NSString *)outlineStyle
{
    return [self getPropertyValue:@"outline-style"];
}

- (void)setOutlineStyle:(NSString *)outlineStyle
{
    [self setProperty:@"outline-style" :outlineStyle :@""];
}

- (NSString *)outlineWidth
{
    return [self getPropertyValue:@"outline-width"];
}

- (void)setOutlineWidth:(NSString *)outlineWidth
{
    [self setProperty:@"outline-width" :outlineWidth :@""];
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
    return [self getPropertyValue:@"padding-top"];
}

- (void)setPaddingTop:(NSString *)paddingTop
{
    [self setProperty:@"padding-top" :paddingTop :@""];
}

- (NSString *)paddingRight
{
    return [self getPropertyValue:@"padding-right"];
}

- (void)setPaddingRight:(NSString *)paddingRight
{
    [self setProperty:@"padding-right" :paddingRight :@""];
}

- (NSString *)paddingBottom
{
    return [self getPropertyValue:@"padding-bottom"];
}

- (void)setPaddingBottom:(NSString *)paddingBottom
{
    [self setProperty:@"padding-bottom" :paddingBottom :@""];
}

- (NSString *)paddingLeft
{
    return [self getPropertyValue:@"padding-left"];
}

- (void)setPaddingLeft:(NSString *)paddingLeft
{
    [self setProperty:@"padding-left" :paddingLeft :@""];
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
    return [self getPropertyValue:@"page-break-after"];
}

- (void)setPageBreakAfter:(NSString *)pageBreakAfter
{
    [self setProperty:@"page-break-after" :pageBreakAfter :@""];
}

- (NSString *)pageBreakBefore
{
    return [self getPropertyValue:@"page-break-before"];
}

- (void)setPageBreakBefore:(NSString *)pageBreakBefore
{
    [self setProperty:@"page-break-before" :pageBreakBefore :@""];
}

- (NSString *)pageBreakInside
{
    return [self getPropertyValue:@"page-break-inside"];
}

- (void)setPageBreakInside:(NSString *)pageBreakInside
{
    [self setProperty:@"page-break-inside" :pageBreakInside :@""];
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
    return [self getPropertyValue:@"pause-after"];
}

- (void)setPauseAfter:(NSString *)pauseAfter
{
    [self setProperty:@"pause-after" :pauseAfter :@""];
}

- (NSString *)pauseBefore
{
    return [self getPropertyValue:@"pause-before"];
}

- (void)setPauseBefore:(NSString *)pauseBefore
{
    [self setProperty:@"pause-before" :pauseBefore :@""];
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
    return [self getPropertyValue:@"pitch-range"];
}

- (void)setPitchRange:(NSString *)pitchRange
{
    [self setProperty:@"pitch-range" :pitchRange :@""];
}

- (NSString *)playDuring
{
    return [self getPropertyValue:@"play-during"];
}

- (void)setPlayDuring:(NSString *)playDuring
{
    [self setProperty:@"play-during" :playDuring :@""];
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
    return [self getPropertyValue:@"speak-header"];
}

- (void)setSpeakHeader:(NSString *)speakHeader
{
    [self setProperty:@"speak-header" :speakHeader :@""];
}

- (NSString *)speakNumeral
{
    return [self getPropertyValue:@"speak-numeral"];
}

- (void)setSpeakNumeral:(NSString *)speakNumeral
{
    [self setProperty:@"speak-numeral" :speakNumeral :@""];
}

- (NSString *)speakPunctuation
{
    return [self getPropertyValue:@"speak-punctuation"];
}

- (void)setSpeakPunctuation:(NSString *)speakPunctuation
{
    [self setProperty:@"speak-punctuation" :speakPunctuation :@""];
}

- (NSString *)speechRate
{
    return [self getPropertyValue:@"speech-rate"];
}

- (void)setSpeechRate:(NSString *)speechRate
{
    [self setProperty:@"speech-rate" :speechRate :@""];
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
    return [self getPropertyValue:@"table-layout"];
}

- (void)setTableLayout:(NSString *)tableLayout
{
    [self setProperty:@"table-layout" :tableLayout :@""];
}

- (NSString *)textAlign
{
    return [self getPropertyValue:@"text-align"];
}

- (void)setTextAlign:(NSString *)textAlign
{
    [self setProperty:@"text-align" :textAlign :@""];
}

- (NSString *)textDecoration
{
    return [self getPropertyValue:@"text-decoration"];
}

- (void)setTextDecoration:(NSString *)textDecoration
{
    [self setProperty:@"text-decoration" :textDecoration :@""];
}

- (NSString *)textIndent
{
    return [self getPropertyValue:@"text-indent"];
}

- (void)setTextIndent:(NSString *)textIndent
{
    [self setProperty:@"text-indent" :textIndent :@""];
}

- (NSString *)textShadow
{
    return [self getPropertyValue:@"text-shadow"];
}

- (void)setTextShadow:(NSString *)textShadow
{
    [self setProperty:@"text-shadow" :textShadow :@""];
}

- (NSString *)textTransform
{
    return [self getPropertyValue:@"text-transform"];
}

- (void)setTextTransform:(NSString *)textTransform
{
    [self setProperty:@"text-transform" :textTransform :@""];
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
    return [self getPropertyValue:@"unicode-bidi"];
}

- (void)setUnicodeBidi:(NSString *)unicodeBidi
{
    [self setProperty:@"unicode-bidi" :unicodeBidi :@""];
}

- (NSString *)verticalAlign
{
    return [self getPropertyValue:@"vertical-align"];
}

- (void)setVerticalAlign:(NSString *)verticalAlign
{
    [self setProperty:@"vertical-align" :verticalAlign :@""];
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
    return [self getPropertyValue:@"voice-family"];
}

- (void)setVoiceFamily:(NSString *)voiceFamily
{
    [self setProperty:@"voice-family" :voiceFamily :@""];
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
    return [self getPropertyValue:@"white-space"];
}

- (void)setWhiteSpace:(NSString *)whiteSpace
{
    [self setProperty:@"white-space" :whiteSpace :@""];
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
    return [self getPropertyValue:@"word-spacing"];
}

- (void)setWordSpacing:(NSString *)wordSpacing
{
    [self setProperty:@"word-spacing" :wordSpacing :@""];
}

- (NSString *)zIndex
{
    return [self getPropertyValue:@"z-index"];
}

- (void)setZIndex:(NSString *)zIndex
{
    [self setProperty:@"z-index" :zIndex :@""];
}

@end

//------------------------------------------------------------------------------------------

@implementation DOMObject (DOMLinkStyle)

- (DOMStyleSheet *)sheet
{
    StyleSheetImpl *sheet;

    if ([self isKindOfClass:[DOMProcessingInstruction class]])
        sheet = static_cast<ProcessingInstructionImpl *>([(DOMProcessingInstruction *)self _nodeImpl])->sheet();
    else if ([self isKindOfClass:[DOMHTMLLinkElement class]])
        sheet = static_cast<HTMLLinkElementImpl *>([(DOMHTMLLinkElement *)self _nodeImpl])->sheet();
    else if ([self isKindOfClass:[DOMHTMLStyleElement class]])
        sheet = static_cast<HTMLStyleElementImpl *>([(DOMHTMLStyleElement *)self _nodeImpl])->sheet();
    else
        return nil;

    return [DOMStyleSheet _DOMStyleSheetWithImpl:sheet];
}

@end

@implementation DOMDocument (DOMViewCSS)

- (DOMCSSStyleDeclaration *)getComputedStyle:(DOMElement *)elt :(NSString *)pseudoElt
{
    return [DOMCSSStyleDeclaration _styleDeclarationWithImpl:
        AbstractViewImpl([self _documentImpl]).getComputedStyle([elt _elementImpl], DOMString(pseudoElt).impl())];
}

@end
