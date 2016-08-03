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
#import "DOMElementInternal.h"

#import "Attr.h"
#import "CSSStyleDeclaration.h"
#import "DOMAttrInternal.h"
#import "DOMCSSStyleDeclarationInternal.h"
#import "DOMDOMTokenListInternal.h"
#import "DOMHTMLCollectionInternal.h"
#import "DOMNodeInternal.h"
#import "DOMNodeListInternal.h"
#import "DOMTokenList.h"
#import "Element.h"
#import "ExceptionHandlers.h"
#import "HTMLCollection.h"
#import "HTMLNames.h"
#import "JSMainThreadExecState.h"
#import "NameNodeList.h"
#import "NodeList.h"
#import "StyleProperties.h"
#import "ThreadCheck.h"
#import "URL.h"
#import "WebScriptObjectPrivate.h"
#import <wtf/GetPtr.h>

#define IMPL static_cast<WebCore::Element*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMElement

- (NSString *)tagName
{
    WebCore::JSMainThreadNullState state;
    return IMPL->tagName();
}

- (DOMCSSStyleDeclaration *)style
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->cssomStyle()));
}

- (int)offsetLeft
{
    WebCore::JSMainThreadNullState state;
    return IMPL->offsetLeft();
}

- (int)offsetTop
{
    WebCore::JSMainThreadNullState state;
    return IMPL->offsetTop();
}

- (int)offsetWidth
{
    WebCore::JSMainThreadNullState state;
    return IMPL->offsetWidth();
}

- (int)offsetHeight
{
    WebCore::JSMainThreadNullState state;
    return IMPL->offsetHeight();
}

- (int)clientLeft
{
    WebCore::JSMainThreadNullState state;
    return IMPL->clientLeft();
}

- (int)clientTop
{
    WebCore::JSMainThreadNullState state;
    return IMPL->clientTop();
}

- (int)clientWidth
{
    WebCore::JSMainThreadNullState state;
    return IMPL->clientWidth();
}

- (int)clientHeight
{
    WebCore::JSMainThreadNullState state;
    return IMPL->clientHeight();
}

- (int)scrollLeft
{
    WebCore::JSMainThreadNullState state;
    return IMPL->scrollLeft();
}

- (void)setScrollLeft:(int)newScrollLeft
{
    WebCore::JSMainThreadNullState state;
    IMPL->setScrollLeft(newScrollLeft);
}

- (int)scrollTop
{
    WebCore::JSMainThreadNullState state;
    return IMPL->scrollTop();
}

- (void)setScrollTop:(int)newScrollTop
{
    WebCore::JSMainThreadNullState state;
    IMPL->setScrollTop(newScrollTop);
}

- (int)scrollWidth
{
    WebCore::JSMainThreadNullState state;
    return IMPL->scrollWidth();
}

- (int)scrollHeight
{
    WebCore::JSMainThreadNullState state;
    return IMPL->scrollHeight();
}

- (DOMElement *)offsetParent
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->bindingsOffsetParent()));
}

- (NSString *)innerHTML
{
    WebCore::JSMainThreadNullState state;
    return IMPL->innerHTML();
}

- (void)setInnerHTML:(NSString *)newInnerHTML
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setInnerHTML(newInnerHTML, ec);
    WebCore::raiseOnDOMError(ec);
}

- (NSString *)outerHTML
{
    WebCore::JSMainThreadNullState state;
    return IMPL->outerHTML();
}

- (void)setOuterHTML:(NSString *)newOuterHTML
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setOuterHTML(newOuterHTML, ec);
    WebCore::raiseOnDOMError(ec);
}

- (NSString *)className
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::classAttr);
}

- (void)setClassName:(NSString *)newClassName
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::classAttr, newClassName);
}

- (DOMDOMTokenList *)classList
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->classList()));
}

- (NSString *)innerText
{
    WebCore::JSMainThreadNullState state;
    return IMPL->innerText();
}

- (NSString *)uiactions
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::uiactionsAttr);
}

- (void)setUiactions:(NSString *)newUiactions
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::uiactionsAttr, newUiactions);
}

#if ENABLE(CSS_REGIONS)
- (NSString *)webkitRegionOverset
{
    WebCore::JSMainThreadNullState state;
    return IMPL->webkitRegionOverset();
}
#endif

- (DOMElement *)previousElementSibling
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->previousElementSibling()));
}

- (DOMElement *)nextElementSibling
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->nextElementSibling()));
}

- (DOMHTMLCollection *)children
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->children()));
}

- (DOMElement *)firstElementChild
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->firstElementChild()));
}

- (DOMElement *)lastElementChild
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->lastElementChild()));
}

- (unsigned)childElementCount
{
    WebCore::JSMainThreadNullState state;
    return IMPL->childElementCount();
}

- (NSString *)getAttribute:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(name);
}

- (void)setAttribute:(NSString *)name value:(NSString *)value
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setAttribute(name, value, ec);
    WebCore::raiseOnDOMError(ec);
}

- (void)setAttribute:(NSString *)name :(NSString *)value
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setAttribute(name, value, ec);
    WebCore::raiseOnDOMError(ec);
}

- (void)removeAttribute:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    IMPL->removeAttribute(name);
}

- (DOMAttr *)getAttributeNode:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->getAttributeNode(name)));
}

- (DOMAttr *)setAttributeNode:(DOMAttr *)newAttr
{
    WebCore::JSMainThreadNullState state;
    if (!newAttr)
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    DOMAttr *result = kit(WTF::getPtr(IMPL->setAttributeNode(*core(newAttr), ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMAttr *)removeAttributeNode:(DOMAttr *)oldAttr
{
    WebCore::JSMainThreadNullState state;
    if (!oldAttr)
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    DOMAttr *result = kit(WTF::getPtr(IMPL->removeAttributeNode(*core(oldAttr), ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMNodeList *)getElementsByTagName:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->getElementsByTagNameForObjC(name)));
}

- (NSString *)getAttributeNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttributeNS(namespaceURI, localName);
}

- (NSString *)getAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttributeNS(namespaceURI, localName);
}

- (void)setAttributeNS:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName value:(NSString *)value
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setAttributeNS(namespaceURI, qualifiedName, value, ec);
    WebCore::raiseOnDOMError(ec);
}

- (void)setAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName :(NSString *)value
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setAttributeNS(namespaceURI, qualifiedName, value, ec);
    WebCore::raiseOnDOMError(ec);
}

- (void)removeAttributeNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    IMPL->removeAttributeNS(namespaceURI, localName);
}

- (void)removeAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    IMPL->removeAttributeNS(namespaceURI, localName);
}

- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->getElementsByTagNameNSForObjC(namespaceURI, localName)));
}

- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->getElementsByTagNameNSForObjC(namespaceURI, localName)));
}

- (DOMAttr *)getAttributeNodeNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->getAttributeNodeNS(namespaceURI, localName)));
}

- (DOMAttr *)getAttributeNodeNS:(NSString *)namespaceURI :(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->getAttributeNodeNS(namespaceURI, localName)));
}

- (DOMAttr *)setAttributeNodeNS:(DOMAttr *)newAttr
{
    WebCore::JSMainThreadNullState state;
    if (!newAttr)
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    DOMAttr *result = kit(WTF::getPtr(IMPL->setAttributeNodeNS(*core(newAttr), ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (BOOL)hasAttribute:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttribute(name);
}

- (BOOL)hasAttributeNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeNS(namespaceURI, localName);
}

- (BOOL)hasAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeNS(namespaceURI, localName);
}

- (void)focus
{
    WebCore::JSMainThreadNullState state;
    IMPL->focus();
}

- (void)blur
{
    WebCore::JSMainThreadNullState state;
    IMPL->blur();
}

- (void)scrollIntoView:(BOOL)alignWithTop
{
    WebCore::JSMainThreadNullState state;
    IMPL->scrollIntoView(alignWithTop);
}

- (void)scrollIntoViewIfNeeded:(BOOL)centerIfNeeded
{
    WebCore::JSMainThreadNullState state;
    IMPL->scrollIntoViewIfNeeded(centerIfNeeded);
}

- (void)scrollByLines:(int)lines
{
    WebCore::JSMainThreadNullState state;
    IMPL->scrollByLines(lines);
}

- (void)scrollByPages:(int)pages
{
    WebCore::JSMainThreadNullState state;
    IMPL->scrollByPages(pages);
}

- (DOMNodeList *)getElementsByClassName:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->getElementsByClassNameForObjC(name)));
}

- (BOOL)matches:(NSString *)selectors
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    BOOL result = IMPL->matches(selectors, ec);
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMElement *)closest:(NSString *)selectors
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMElement *result = kit(WTF::getPtr(IMPL->closest(selectors, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (BOOL)webkitMatchesSelector:(NSString *)selectors
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    BOOL result = IMPL->matches(selectors, ec);
    WebCore::raiseOnDOMError(ec);
    return result;
}

#if ENABLE(FULLSCREEN_API)
- (void)webkitRequestFullScreen:(unsigned short)flags
{
    WebCore::JSMainThreadNullState state;
    IMPL->webkitRequestFullScreen(flags);
}

- (void)webkitRequestFullscreen
{
    WebCore::JSMainThreadNullState state;
    IMPL->webkitRequestFullscreen();
}
#endif

#if ENABLE(POINTER_LOCK)
- (void)requestPointerLock
{
    WebCore::JSMainThreadNullState state;
    IMPL->requestPointerLock();
}

#endif

- (void)remove
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->remove(ec);
    WebCore::raiseOnDOMError(ec);
}

- (DOMElement *)querySelector:(NSString *)selectors
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMElement *result = kit(WTF::getPtr(IMPL->querySelector(selectors, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMNodeList *)querySelectorAll:(NSString *)selectors
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMNodeList *result = kit(WTF::getPtr(IMPL->querySelectorAll(selectors, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

@end

WebCore::Element* core(DOMElement *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::Element*>(wrapper->_internal) : 0;
}

DOMElement *kit(WebCore::Element* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMElement*>(kit(static_cast<WebCore::Node*>(value)));
}
