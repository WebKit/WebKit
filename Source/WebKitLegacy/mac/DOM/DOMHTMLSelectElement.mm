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

#import "DOMHTMLSelectElementInternal.h"

#import "DOMHTMLCollectionInternal.h"
#import "DOMHTMLElementInternal.h"
#import "DOMHTMLFormElementInternal.h"
#import "DOMHTMLOptionsCollectionInternal.h"
#import "DOMNodeInternal.h"
#import "DOMNodeListInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/HTMLCollection.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLOptGroupElement.h>
#import <WebCore/HTMLOptionsCollection.h>
#import <WebCore/HTMLSelectElement.h>
#import <WebCore/JSExecState.h>
#import <WebCore/NameNodeList.h>
#import <WebCore/Node.h>
#import <WebCore/NodeList.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::HTMLSelectElement*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMHTMLSelectElement

- (BOOL)autofocus
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::autofocusAttr);
}

- (void)setAutofocus:(BOOL)newAutofocus
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::autofocusAttr, newAutofocus);
}

- (BOOL)disabled
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::disabledAttr);
}

- (void)setDisabled:(BOOL)newDisabled
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::disabledAttr, newDisabled);
}

- (DOMHTMLFormElement *)form
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->form()));
}

- (BOOL)multiple
{
    WebCore::JSMainThreadNullState state;
    return IMPL->multiple();
}

- (void)setMultiple:(BOOL)newMultiple
{
    WebCore::JSMainThreadNullState state;
    IMPL->setMultiple(newMultiple);
}

- (NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getNameAttribute();
}

- (void)setName:(NSString *)newName
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, newName);
}

- (int)size
{
    WebCore::JSMainThreadNullState state;
    return IMPL->size();
}

- (void)setSize:(int)newSize
{
    WebCore::JSMainThreadNullState state;
    IMPL->setSize(newSize);
}

- (NSString *)type
{
    WebCore::JSMainThreadNullState state;
    return IMPL->type();
}

- (DOMHTMLOptionsCollection *)options
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->options()));
}

- (int)length
{
    WebCore::JSMainThreadNullState state;
    return IMPL->length();
}

- (int)selectedIndex
{
    WebCore::JSMainThreadNullState state;
    return IMPL->selectedIndex();
}

- (void)setSelectedIndex:(int)newSelectedIndex
{
    WebCore::JSMainThreadNullState state;
    IMPL->setSelectedIndex(newSelectedIndex);
}

- (NSString *)value
{
    WebCore::JSMainThreadNullState state;
    return IMPL->value();
}

- (void)setValue:(NSString *)newValue
{
    WebCore::JSMainThreadNullState state;
    IMPL->setValue(newValue);
}

- (BOOL)willValidate
{
    WebCore::JSMainThreadNullState state;
    return IMPL->willValidate();
}

- (DOMNode *)item:(unsigned)index
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->item(index)));
}

- (DOMNode *)namedItem:(NSString *)inName
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->namedItem(inName)));
}

- (void)add:(DOMHTMLElement *)element before:(DOMHTMLElement *)before
{
    WebCore::JSMainThreadNullState state;
    if (!element)
        raiseTypeErrorException();

    auto& coreElement = *core(element);
    Variant<RefPtr<WebCore::HTMLOptionElement>, RefPtr<WebCore::HTMLOptGroupElement>> variantElement;
    if (is<WebCore::HTMLOptionElement>(coreElement))
        variantElement = &downcast<WebCore::HTMLOptionElement>(coreElement);
    else if (is<WebCore::HTMLOptGroupElement>(coreElement))
        variantElement = &downcast<WebCore::HTMLOptGroupElement>(coreElement);
    else
        raiseTypeErrorException();
    raiseOnDOMError(IMPL->add(WTFMove(variantElement), WebCore::HTMLSelectElement::HTMLElementOrInt(core(before))));
}

- (void)remove:(int)index
{
    WebCore::JSMainThreadNullState state;
    IMPL->remove(index);
}

@end

@implementation DOMHTMLSelectElement (DOMHTMLSelectElementDeprecated)

- (void)add:(DOMHTMLElement *)element :(DOMHTMLElement *)before
{
    [self add:element before:before];
}

@end

WebCore::HTMLSelectElement* core(DOMHTMLSelectElement *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::HTMLSelectElement*>(wrapper->_internal) : 0;
}
