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
#import "DOMDOMTokenListInternal.h"
#import "DOMHTMLCollectionInternal.h"
#import "DOMNodeInternal.h"
#import "DOMNodeListInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/Attr.h>
#import <WebCore/CSSStyleDeclaration.h>
#import <WebCore/DOMTokenList.h>
#import <WebCore/Element.h>
#import <WebCore/HTMLCollection.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/JSMainThreadExecState.h>
#import <WebCore/NameNodeList.h>
#import <WebCore/NodeList.h>
#import <WebCore/StyleProperties.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>

using namespace WebCore;

static inline Element& unwrap(DOMElement& wrapper)
{
    ASSERT(wrapper._internal);
    return downcast<Element>(reinterpret_cast<Node&>(*wrapper._internal));
}

Element* core(DOMElement *wrapper)
{
    return wrapper ? &unwrap(*wrapper) : nullptr;
}

DOMElement *kit(Element* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMElement*>(kit(static_cast<Node*>(value)));
}

@implementation DOMElement

- (NSString *)tagName
{
    JSMainThreadNullState state;
    return unwrap(*self).tagName();
}

- (DOMCSSStyleDeclaration *)style
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).cssomStyle());
}

- (int)offsetLeft
{
    JSMainThreadNullState state;
    return unwrap(*self).offsetLeft();
}

- (int)offsetTop
{
    JSMainThreadNullState state;
    return unwrap(*self).offsetTop();
}

- (int)offsetWidth
{
    JSMainThreadNullState state;
    return unwrap(*self).offsetWidth();
}

- (int)offsetHeight
{
    JSMainThreadNullState state;
    return unwrap(*self).offsetHeight();
}

- (int)clientLeft
{
    JSMainThreadNullState state;
    return unwrap(*self).clientLeft();
}

- (int)clientTop
{
    JSMainThreadNullState state;
    return unwrap(*self).clientTop();
}

- (int)clientWidth
{
    JSMainThreadNullState state;
    return unwrap(*self).clientWidth();
}

- (int)clientHeight
{
    JSMainThreadNullState state;
    return unwrap(*self).clientHeight();
}

- (int)scrollLeft
{
    JSMainThreadNullState state;
    return unwrap(*self).scrollLeft();
}

- (void)setScrollLeft:(int)newScrollLeft
{
    JSMainThreadNullState state;
    unwrap(*self).setScrollLeft(newScrollLeft);
}

- (int)scrollTop
{
    JSMainThreadNullState state;
    return unwrap(*self).scrollTop();
}

- (void)setScrollTop:(int)newScrollTop
{
    JSMainThreadNullState state;
    unwrap(*self).setScrollTop(newScrollTop);
}

- (int)scrollWidth
{
    JSMainThreadNullState state;
    return unwrap(*self).scrollWidth();
}

- (int)scrollHeight
{
    JSMainThreadNullState state;
    return unwrap(*self).scrollHeight();
}

- (DOMElement *)offsetParent
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).bindingsOffsetParent());
}

- (NSString *)innerHTML
{
    JSMainThreadNullState state;
    return unwrap(*self).innerHTML();
}

- (void)setInnerHTML:(NSString *)newInnerHTML
{
    JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setInnerHTML(newInnerHTML));
}

- (NSString *)outerHTML
{
    JSMainThreadNullState state;
    return unwrap(*self).outerHTML();
}

- (void)setOuterHTML:(NSString *)newOuterHTML
{
    JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setOuterHTML(newOuterHTML));
}

- (NSString *)className
{
    JSMainThreadNullState state;
    return unwrap(*self).getAttribute(HTMLNames::classAttr);
}

- (void)setClassName:(NSString *)newClassName
{
    JSMainThreadNullState state;
    unwrap(*self).setAttributeWithoutSynchronization(HTMLNames::classAttr, newClassName);
}

- (DOMDOMTokenList *)classList
{
    JSMainThreadNullState state;
    return kit(&unwrap(*self).classList());
}

- (NSString *)innerText
{
    JSMainThreadNullState state;
    return unwrap(*self).innerText();
}

- (NSString *)uiactions
{
    JSMainThreadNullState state;
    return unwrap(*self).getAttribute(HTMLNames::uiactionsAttr);
}

- (void)setUiactions:(NSString *)newUiactions
{
    JSMainThreadNullState state;
    unwrap(*self).setAttributeWithoutSynchronization(HTMLNames::uiactionsAttr, newUiactions);
}

#if ENABLE(CSS_REGIONS)
- (NSString *)webkitRegionOverset
{
    JSMainThreadNullState state;
    return unwrap(*self).webkitRegionOverset();
}
#endif

- (DOMElement *)previousElementSibling
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).previousElementSibling());
}

- (DOMElement *)nextElementSibling
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).nextElementSibling());
}

- (DOMHTMLCollection *)children
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).children().ptr());
}

- (DOMElement *)firstElementChild
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).firstElementChild());
}

- (DOMElement *)lastElementChild
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).lastElementChild());
}

- (unsigned)childElementCount
{
    JSMainThreadNullState state;
    return unwrap(*self).childElementCount();
}

- (NSString *)getAttribute:(NSString *)name
{
    JSMainThreadNullState state;
    return unwrap(*self).getAttribute(name);
}

- (void)setAttribute:(NSString *)name value:(NSString *)value
{
    JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setAttribute(name, value));
}

- (void)removeAttribute:(NSString *)name
{
    JSMainThreadNullState state;
    unwrap(*self).removeAttribute(name);
}

- (DOMAttr *)getAttributeNode:(NSString *)name
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).getAttributeNode(name).get());
}

- (DOMAttr *)setAttributeNode:(DOMAttr *)newAttr
{
    JSMainThreadNullState state;
    if (!newAttr)
        raiseTypeErrorException();
    return kit(raiseOnDOMError(unwrap(*self).setAttributeNode(*core(newAttr))).get());
}

- (DOMAttr *)removeAttributeNode:(DOMAttr *)oldAttr
{
    JSMainThreadNullState state;
    if (!oldAttr)
        raiseTypeErrorException();
    return kit(raiseOnDOMError(unwrap(*self).removeAttributeNode(*core(oldAttr))).ptr());
}

- (DOMNodeList *)getElementsByTagName:(NSString *)name
{
    if (!name)
        return nullptr;

    JSMainThreadNullState state;
    Ref<NodeList> result = unwrap(*self).getElementsByTagName(name);
    return kit(result.ptr());
}

- (NSString *)getAttributeNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    JSMainThreadNullState state;
    return unwrap(*self).getAttributeNS(namespaceURI, localName);
}

- (void)setAttributeNS:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName value:(NSString *)value
{
    JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setAttributeNS(namespaceURI, qualifiedName, value));
}

- (void)removeAttributeNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    JSMainThreadNullState state;
    unwrap(*self).removeAttributeNS(namespaceURI, localName);
}

- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    if (!localName)
        return nullptr;

    JSMainThreadNullState state;
    Ref<NodeList> result = unwrap(*self).getElementsByTagNameNS(namespaceURI, localName);
    return kit(result.ptr());
}

- (DOMAttr *)getAttributeNodeNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).getAttributeNodeNS(namespaceURI, localName).get());
}

- (DOMAttr *)setAttributeNodeNS:(DOMAttr *)newAttr
{
    JSMainThreadNullState state;
    if (!newAttr)
        raiseTypeErrorException();
    return kit(raiseOnDOMError(unwrap(*self).setAttributeNodeNS(*core(newAttr))).get());
}

- (BOOL)hasAttribute:(NSString *)name
{
    JSMainThreadNullState state;
    return unwrap(*self).hasAttribute(name);
}

- (BOOL)hasAttributeNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    JSMainThreadNullState state;
    return unwrap(*self).hasAttributeNS(namespaceURI, localName);
}

- (void)focus
{
    JSMainThreadNullState state;
    unwrap(*self).focus();
}

- (void)blur
{
    JSMainThreadNullState state;
    unwrap(*self).blur();
}

- (void)scrollIntoView:(BOOL)alignWithTop
{
    JSMainThreadNullState state;
    unwrap(*self).scrollIntoView(alignWithTop);
}

- (void)scrollIntoViewIfNeeded:(BOOL)centerIfNeeded
{
    JSMainThreadNullState state;
    unwrap(*self).scrollIntoViewIfNeeded(centerIfNeeded);
}

- (void)scrollByLines:(int)lines
{
    JSMainThreadNullState state;
    unwrap(*self).scrollByLines(lines);
}

- (void)scrollByPages:(int)pages
{
    JSMainThreadNullState state;
    unwrap(*self).scrollByPages(pages);
}

- (DOMNodeList *)getElementsByClassName:(NSString *)name
{
    JSMainThreadNullState state;
    Ref<NodeList> result = unwrap(*self).getElementsByClassName(name);
    return kit(result.ptr());
}

- (BOOL)matches:(NSString *)selectors
{
    JSMainThreadNullState state;
    return raiseOnDOMError(unwrap(*self).matches(selectors));
}

- (DOMElement *)closest:(NSString *)selectors
{
    JSMainThreadNullState state;
    return kit(raiseOnDOMError(unwrap(*self).closest(selectors)));
}

- (BOOL)webkitMatchesSelector:(NSString *)selectors
{
    JSMainThreadNullState state;
    return raiseOnDOMError(unwrap(*self).matches(selectors));
}

#if ENABLE(FULLSCREEN_API)

- (void)webkitRequestFullScreen:(unsigned short)flags
{
    JSMainThreadNullState state;
    unwrap(*self).webkitRequestFullScreen(flags);
}

- (void)webkitRequestFullscreen
{
    JSMainThreadNullState state;
    unwrap(*self).webkitRequestFullscreen();
}

#endif

- (void)remove
{
    JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).remove());
}

- (DOMElement *)querySelector:(NSString *)selectors
{
    JSMainThreadNullState state;
    return kit(raiseOnDOMError(unwrap(*self).querySelector(selectors)));
}

- (DOMNodeList *)querySelectorAll:(NSString *)selectors
{
    JSMainThreadNullState state;
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
