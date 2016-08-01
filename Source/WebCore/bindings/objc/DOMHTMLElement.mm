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
#import "DOMHTMLElementInternal.h"

#import "DOMElementInternal.h"
#import "DOMHTMLCollectionInternal.h"
#import "DOMNodeInternal.h"
#import "Element.h"
#import "ExceptionHandlers.h"
#import "HTMLCollection.h"
#import "HTMLElement.h"
#import "HTMLNames.h"
#import "HitTestResult.h"
#import "JSMainThreadExecState.h"
#import "ThreadCheck.h"
#import "URL.h"
#import "WebScriptObjectPrivate.h"
#import <wtf/GetPtr.h>

#define IMPL static_cast<WebCore::HTMLElement*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMHTMLElement

- (NSString *)title
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::titleAttr);
}

- (void)setTitle:(NSString *)newTitle
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::titleAttr, newTitle);
}

- (NSString *)lang
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::langAttr);
}

- (void)setLang:(NSString *)newLang
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::langAttr, newLang);
}

- (BOOL)translate
{
    WebCore::JSMainThreadNullState state;
    return IMPL->translate();
}

- (void)setTranslate:(BOOL)newTranslate
{
    WebCore::JSMainThreadNullState state;
    IMPL->setTranslate(newTranslate);
}

- (NSString *)dir
{
    WebCore::JSMainThreadNullState state;
    return IMPL->dir();
}

- (void)setDir:(NSString *)newDir
{
    WebCore::JSMainThreadNullState state;
    IMPL->setDir(newDir);
}

- (int)tabIndex
{
    WebCore::JSMainThreadNullState state;
    return IMPL->tabIndex();
}

- (void)setTabIndex:(int)newTabIndex
{
    WebCore::JSMainThreadNullState state;
    IMPL->setTabIndex(newTabIndex);
}

- (BOOL)draggable
{
    WebCore::JSMainThreadNullState state;
    return IMPL->draggable();
}

- (void)setDraggable:(BOOL)newDraggable
{
    WebCore::JSMainThreadNullState state;
    IMPL->setDraggable(newDraggable);
}

- (NSString *)webkitdropzone
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::webkitdropzoneAttr);
}

- (void)setWebkitdropzone:(NSString *)newWebkitdropzone
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::webkitdropzoneAttr, newWebkitdropzone);
}

- (BOOL)hidden
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::hiddenAttr);
}

- (void)setHidden:(BOOL)newHidden
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::hiddenAttr, newHidden);
}

- (NSString *)accessKey
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::accesskeyAttr);
}

- (void)setAccessKey:(NSString *)newAccessKey
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::accesskeyAttr, newAccessKey);
}

- (NSString *)innerText
{
    WebCore::JSMainThreadNullState state;
    return IMPL->innerText();
}

- (void)setInnerText:(NSString *)newInnerText
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setInnerText(newInnerText, ec);
    WebCore::raiseOnDOMError(ec);
}

- (NSString *)outerText
{
    WebCore::JSMainThreadNullState state;
    return IMPL->outerText();
}

- (void)setOuterText:(NSString *)newOuterText
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setOuterText(newOuterText, ec);
    WebCore::raiseOnDOMError(ec);
}

- (NSString *)contentEditable
{
    WebCore::JSMainThreadNullState state;
    return IMPL->contentEditable();
}

- (void)setContentEditable:(NSString *)newContentEditable
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setContentEditable(newContentEditable, ec);
    WebCore::raiseOnDOMError(ec);
}

- (BOOL)isContentEditable
{
    WebCore::JSMainThreadNullState state;
    return IMPL->isContentEditable();
}

- (BOOL)spellcheck
{
    WebCore::JSMainThreadNullState state;
    return IMPL->spellcheck();
}

- (void)setSpellcheck:(BOOL)newSpellcheck
{
    WebCore::JSMainThreadNullState state;
    IMPL->setSpellcheck(newSpellcheck);
}

- (NSString *)idName
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getIdAttribute();
}

- (void)setIdName:(NSString *)newIdName
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::idAttr, newIdName);
}

- (DOMHTMLCollection *)children
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->children()));
}

- (NSString *)titleDisplayString
{
    WebCore::JSMainThreadNullState state;
    return WebCore::displayString(IMPL->title(), core(self));
}

- (DOMElement *)insertAdjacentElement:(NSString *)where element:(DOMElement *)element
{
    WebCore::JSMainThreadNullState state;
    if (!element)
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    DOMElement *result = kit(WTF::getPtr(IMPL->insertAdjacentElement(where, *core(element), ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (void)insertAdjacentHTML:(NSString *)where html:(NSString *)html
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->insertAdjacentHTML(where, html, ec);
    WebCore::raiseOnDOMError(ec);
}

- (void)insertAdjacentText:(NSString *)where text:(NSString *)text
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->insertAdjacentText(where, text, ec);
    WebCore::raiseOnDOMError(ec);
}

- (void)click
{
    WebCore::JSMainThreadNullState state;
    IMPL->click();
}

@end

WebCore::HTMLElement* core(DOMHTMLElement *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::HTMLElement*>(wrapper->_internal) : 0;
}

DOMHTMLElement *kit(WebCore::HTMLElement* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMHTMLElement*>(kit(static_cast<WebCore::Node*>(value)));
}
