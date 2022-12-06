/*
 * Copyright (C) 2004-2016, 2020 Apple Inc. All rights reserved.
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

#import "DOMHTMLTextAreaElementInternal.h"

#import "DOMHTMLFormElementInternal.h"
#import "DOMNodeInternal.h"
#import "DOMNodeListInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/HTMLTextAreaElement.h>
#import <WebCore/JSExecState.h>
#import <WebCore/NodeList.h>
#import <WebCore/ThreadCheck.h>

static inline WebCore::HTMLTextAreaElement& unwrap(DOMHTMLTextAreaElement& wrapper)
{
    ASSERT(wrapper._internal);
    return downcast<WebCore::HTMLTextAreaElement>(reinterpret_cast<WebCore::Node&>(*wrapper._internal));
}

WebCore::HTMLTextAreaElement* core(DOMHTMLTextAreaElement *wrapper)
{
    return wrapper ? &unwrap(*wrapper) : nullptr;
}

DOMHTMLTextAreaElement *kit(WebCore::HTMLTextAreaElement* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMHTMLTextAreaElement*>(kit(static_cast<WebCore::Node*>(value)));
}

@implementation DOMHTMLTextAreaElement

- (BOOL)autofocus
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).hasAttributeWithoutSynchronization(WebCore::HTMLNames::autofocusAttr);
}

- (void)setAutofocus:(BOOL)newAutofocus
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setBooleanAttribute(WebCore::HTMLNames::autofocusAttr, newAutofocus);
}

- (NSString *)dirName
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).getAttribute(WebCore::HTMLNames::dirnameAttr);
}

- (void)setDirName:(NSString *)newDirName
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setAttributeWithoutSynchronization(WebCore::HTMLNames::dirnameAttr, newDirName);
}

- (BOOL)disabled
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).hasAttributeWithoutSynchronization(WebCore::HTMLNames::disabledAttr);
}

- (void)setDisabled:(BOOL)newDisabled
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setBooleanAttribute(WebCore::HTMLNames::disabledAttr, newDisabled);
}

- (DOMHTMLFormElement *)form
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).form());
}

- (int)maxLength
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).maxLength();
}

- (void)setMaxLength:(int)newMaxLength
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setMaxLength(newMaxLength));
}

- (NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).getNameAttribute();
}

- (void)setName:(NSString *)newName
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, newName);
}

- (NSString *)placeholder
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).getAttribute(WebCore::HTMLNames::placeholderAttr);
}

- (void)setPlaceholder:(NSString *)newPlaceholder
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setAttributeWithoutSynchronization(WebCore::HTMLNames::placeholderAttr, newPlaceholder);
}

- (BOOL)readOnly
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).hasAttributeWithoutSynchronization(WebCore::HTMLNames::readonlyAttr);
}

- (void)setReadOnly:(BOOL)newReadOnly
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setBooleanAttribute(WebCore::HTMLNames::readonlyAttr, newReadOnly);
}

- (BOOL)required
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).hasAttributeWithoutSynchronization(WebCore::HTMLNames::requiredAttr);
}

- (void)setRequired:(BOOL)newRequired
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setBooleanAttribute(WebCore::HTMLNames::requiredAttr, newRequired);
}

- (int)rows
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).rows();
}

- (void)setRows:(int)newRows
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setRows(newRows);
}

- (int)cols
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).cols();
}

- (void)setCols:(int)newCols
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setCols(newCols);
}

- (NSString *)wrap
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).getAttribute(WebCore::HTMLNames::wrapAttr);
}

- (void)setWrap:(NSString *)newWrap
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setAttributeWithoutSynchronization(WebCore::HTMLNames::wrapAttr, newWrap);
}

- (NSString *)type
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).type();
}

- (NSString *)defaultValue
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).defaultValue();
}

- (void)setDefaultValue:(NSString *)newDefaultValue
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setDefaultValue(newDefaultValue);
}

- (NSString *)value
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).value();
}

- (void)setValue:(NSString *)newValue
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setValue(newValue);
}

- (unsigned)textLength
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).textLength();
}

- (BOOL)willValidate
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).willValidate();
}

- (DOMNodeList *)labels
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).labels().get());
}

- (int)selectionStart
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).selectionStart();
}

- (void)setSelectionStart:(int)newSelectionStart
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setSelectionStart(newSelectionStart);
}

- (int)selectionEnd
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).selectionEnd();
}

- (void)setSelectionEnd:(int)newSelectionEnd
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setSelectionEnd(newSelectionEnd);
}

- (NSString *)selectionDirection
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).selectionDirection();
}

- (void)setSelectionDirection:(NSString *)newSelectionDirection
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setSelectionDirection(newSelectionDirection);
}

- (NSString *)accessKey
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).getAttribute(WebCore::HTMLNames::accesskeyAttr);
}

- (void)setAccessKey:(NSString *)newAccessKey
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setAttributeWithoutSynchronization(WebCore::HTMLNames::accesskeyAttr, newAccessKey);
}

- (NSString *)autocomplete
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).autocomplete();
}

- (void)setAutocomplete:(NSString *)newAutocomplete
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setAutocomplete(newAutocomplete);
}

- (void)select
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).select();
}

- (void)setRangeText:(NSString *)replacement
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setRangeText(String { replacement }));
}

- (void)setRangeText:(NSString *)replacement start:(unsigned)start end:(unsigned)end selectionMode:(NSString *)selectionMode
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setRangeText(String { replacement }, start, end, selectionMode));
}

- (void)setSelectionRange:(int)start end:(int)end
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setSelectionRange(start, end);
}

- (BOOL)canShowPlaceholder
{
    WebCore::JSMainThreadNullState state;
    return unwrap(*self).canShowPlaceholder();
}

- (void)setCanShowPlaceholder:(BOOL)canShowPlaceholder
{
    WebCore::JSMainThreadNullState state;
    unwrap(*self).setCanShowPlaceholder(canShowPlaceholder);
}

@end
