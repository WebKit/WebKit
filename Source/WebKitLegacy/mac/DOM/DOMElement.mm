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

#import "DOMElementInternal.h"

#import "DOMAttrInternal.h"
#import "DOMCSSStyleDeclarationInternal.h"
#import "DOMHTMLCollectionInternal.h"
#import "DOMNodeInternal.h"
#import "DOMNodeListInternal.h"
#import "DOMTokenListInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/Attr.h>
#import <WebCore/CSSStyleDeclaration.h>
#import <WebCore/Element.h>
#import <WebCore/HTMLCollection.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/JSExecState.h>
#import <WebCore/NameNodeList.h>
#import <WebCore/NodeList.h>
#import <WebCore/ScrollIntoViewOptions.h>
#import <WebCore/StyleProperties.h>
#import <WebCore/StyledElement.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>

static inline WebCore::Element& unwrap(DOMElement& wrapper)
{
    ASSERT(wrapper._internal);
    return downcast<WebCore::Element>(reinterpret_cast<WebCore::Node&>(*wrapper._internal));
}

WebCore::Element* core(DOMElement *wrapper)
{
    return wrapper ? &unwrap(*wrapper) : nullptr;
}

DOMElement *kit(WebCore::Element* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMElement*>(kit(static_cast<WebCore::Node*>(value)));
}

@implementation DOMElement

- (NSString *)tagName
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).tagName();
}

- (DOMCSSStyleDeclaration *)style
{
    WebCore::JSMainThreadNullState state;
    auto& element = unwrap(*self);
    return is<WebCore::StyledElement>(element) ? kit(&downcast<WebCore::StyledElement>(element).cssomStyle()) : nullptr;
}

- (int)offsetLeft
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).offsetLeft();
}

- (int)offsetTop
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).offsetTop();
}

- (int)offsetWidth
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).offsetWidth();
}

- (int)offsetHeight
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).offsetHeight();
}

- (int)clientLeft
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).clientLeft();
}

- (int)clientTop
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).clientTop();
}

- (int)clientWidth
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).clientWidth();
}

- (int)clientHeight
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).clientHeight();
}

- (int)scrollLeft
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).scrollLeft();
}

- (void)setScrollLeft:(int)newScrollLeft
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setScrollLeft(newScrollLeft);
}

- (int)scrollTop
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).scrollTop();
}

- (void)setScrollTop:(int)newScrollTop
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setScrollTop(newScrollTop);
}

- (int)scrollWidth
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).scrollWidth();
}

- (int)scrollHeight
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).scrollHeight();
}

- (DOMElement *)offsetParent
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).bindingsOffsetParent());
}

- (NSString *)innerHTML
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).innerHTML();
}

- (void)setInnerHTML:(NSString *)newInnerHTML
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setInnerHTML(newInnerHTML));
}

- (NSString *)outerHTML
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).outerHTML();
}

- (void)setOuterHTML:(NSString *)newOuterHTML
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setOuterHTML(newOuterHTML));
}

- (NSString *)className
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).getAttribute(WebCore::HTMLNames::classAttr);
}

- (void)setClassName:(NSString *)newClassName
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setAttributeWithoutSynchronization(WebCore::HTMLNames::classAttr, newClassName);
}

- (DOMTokenList *)classList
{
    WebCore::JSMainThreadNullState state;
    return kit(&unwrap(*self).classList());
}

- (NSString *)innerText
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).innerText();
}

- (NSString *)uiactions
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).getAttribute(WebCore::HTMLNames::uiactionsAttr);
}

- (void)setUiactions:(NSString *)newUiactions
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setAttributeWithoutSynchronization(WebCore::HTMLNames::uiactionsAttr, newUiactions);
}

- (DOMElement *)previousElementSibling
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).previousElementSibling());
}

- (DOMElement *)nextElementSibling
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).nextElementSibling());
}

- (DOMHTMLCollection *)children
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).children().ptr());
}

- (DOMElement *)firstElementChild
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).firstElementChild());
}

- (DOMElement *)lastElementChild
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).lastElementChild());
}

- (unsigned)childElementCount
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).childElementCount();
}

#if PLATFORM(IOS_FAMILY)
- (CGRect)boundsInRootViewSpace
{
    WebCore::JSMainThreadNullState state;
    auto bounds = unwrap(*self).boundsInRootViewSpace();
    return CGRectMake(bounds.x(), bounds.y(), bounds.width(), bounds.height());
}
#endif

- (NSString *)getAttribute:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).getAttribute(name);
}

- (void)setAttribute:(NSString *)name value:(NSString *)value
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setAttribute(name, value));
}

- (void)removeAttribute:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).removeAttribute(name);
}

- (DOMAttr *)getAttributeNode:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).getAttributeNode(name).get());
}

- (DOMAttr *)setAttributeNode:(DOMAttr *)newAttr
{
    WebCore::JSMainThreadNullState state;
    if (!newAttr)
        raiseTypeErrorException();
    return kit(raiseOnDOMError(unwrap(*self).setAttributeNode(*core(newAttr))).get());
}

- (DOMAttr *)removeAttributeNode:(DOMAttr *)oldAttr
{
    WebCore::JSMainThreadNullState state;
    if (!oldAttr)
        raiseTypeErrorException();
    return kit(raiseOnDOMError(unwrap(*self).removeAttributeNode(*core(oldAttr))).ptr());
}

- (DOMNodeList *)getElementsByTagName:(NSString *)name
{
    if (!name)
        return nullptr;

    WebCore::JSMainThreadNullState state;
    Ref<WebCore::NodeList> result = unwrap(*self).getElementsByTagName(name);
    return kit(result.ptr());
}

- (NSString *)getAttributeNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).getAttributeNS(namespaceURI, localName);
}

- (void)setAttributeNS:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName value:(NSString *)value
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setAttributeNS(namespaceURI, qualifiedName, value));
}

- (void)removeAttributeNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).removeAttributeNS(namespaceURI, localName);
}

- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    if (!localName)
        return nullptr;

    WebCore::JSMainThreadNullState state;
    Ref<WebCore::NodeList> result = unwrap(*self).getElementsByTagNameNS(namespaceURI, localName);
    return kit(result.ptr());
}

- (DOMAttr *)getAttributeNodeNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).getAttributeNodeNS(namespaceURI, localName).get());
}

- (DOMAttr *)setAttributeNodeNS:(DOMAttr *)newAttr
{
    WebCore::JSMainThreadNullState state;
    if (!newAttr)
        raiseTypeErrorException();
    return kit(raiseOnDOMError(unwrap(*self).setAttributeNodeNS(*core(newAttr))).get());
}

- (BOOL)hasAttribute:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).hasAttribute(name);
}

- (BOOL)hasAttributeNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).hasAttributeNS(namespaceURI, localName);
}

- (void)focus
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).focus();
}

- (void)blur
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).blur();
}

- (void)scrollIntoView:(BOOL)alignWithTop
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).scrollIntoView(alignWithTop);
}

- (void)scrollIntoViewIfNeeded:(BOOL)centerIfNeeded
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).scrollIntoViewIfNeeded(centerIfNeeded);
}

- (void)scrollByLines:(int)lines
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).scrollByLines(lines);
}

- (void)scrollByPages:(int)pages
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).scrollByPages(pages);
}

- (DOMNodeList *)getElementsByClassName:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    Ref<WebCore::NodeList> result = unwrap(*self).getElementsByClassName(name);
    return kit(result.ptr());
}

- (BOOL)matches:(NSString *)selectors
{
    WebCore::JSMainThreadNullState state;
    return raiseOnDOMError(unwrap(*self).matches(selectors));
}

- (DOMElement *)closest:(NSString *)selectors
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(unwrap(*self).closest(selectors)));
}

- (BOOL)webkitMatchesSelector:(NSString *)selectors
{
    WebCore::JSMainThreadNullState state;
    return raiseOnDOMError(unwrap(*self).matches(selectors));
}

#if ENABLE(FULLSCREEN_API)

- (void)webkitRequestFullScreen:(unsigned short)flags
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).webkitRequestFullscreen();
}

- (void)webkitRequestFullscreen
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).webkitRequestFullscreen();
}

#endif

- (void)remove
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).remove());
}

- (DOMElement *)querySelector:(NSString *)selectors
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(unwrap(*self).querySelector(selectors)));
}

- (DOMNodeList *)querySelectorAll:(NSString *)selectors
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(unwrap(*self).querySelectorAll(selectors)).ptr());
}

@end

@implementation DOMElement (DOMElementDeprecated)

- (void)setAttribute:(NSString *)name :(NSString *)value
{
    [self setAttribute:name value:value];
}

- (NSString *)getAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    return [self getAttributeNS:namespaceURI localName:localName];
}

- (void)setAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName :(NSString *)value
{
    [self setAttributeNS:namespaceURI qualifiedName:qualifiedName value:value];
}

- (void)removeAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    [self removeAttributeNS:namespaceURI localName:localName];
}

- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName
{
    return [self getElementsByTagNameNS:namespaceURI localName:localName];
}

- (DOMAttr *)getAttributeNodeNS:(NSString *)namespaceURI :(NSString *)localName
{
    return [self getAttributeNodeNS:namespaceURI localName:localName];
}

- (BOOL)hasAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    return [self hasAttributeNS:namespaceURI localName:localName];
}

@end
