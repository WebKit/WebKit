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

#import "DOMHTMLTextAreaElementInternal.h"

#import "DOMHTMLFormElementInternal.h"
#import "DOMNodeInternal.h"
#import "DOMNodeListInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/HTMLTextAreaElement.h>
#import <WebCore/JSExecState.h>
#import <WebCore/NodeList.h>
#import <WebCore/ThreadCheck.h>

using namespace WebCore;

static inline HTMLTextAreaElement& unwrap(DOMHTMLTextAreaElement& wrapper)
{
    ASSERT(wrapper._internal);
    return downcast<HTMLTextAreaElement>(reinterpret_cast<Node&>(*wrapper._internal));
}

HTMLTextAreaElement* core(DOMHTMLTextAreaElement *wrapper)
{
    return wrapper ? &unwrap(*wrapper) : nullptr;
}

DOMHTMLTextAreaElement *kit(HTMLTextAreaElement* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMHTMLTextAreaElement*>(kit(static_cast<Node*>(value)));
}

@implementation DOMHTMLTextAreaElement

- (BOOL)autofocus
{
    JSMainThreadNullState state;
    return unwrap(*self).hasAttributeWithoutSynchronization(HTMLNames::autofocusAttr);
}

- (void)setAutofocus:(BOOL)newAutofocus
{
    JSMainThreadNullState state;
    unwrap(*self).setBooleanAttribute(HTMLNames::autofocusAttr, newAutofocus);
}

- (NSString *)dirName
{
    JSMainThreadNullState state;
    return unwrap(*self).getAttribute(HTMLNames::dirnameAttr);
}

- (void)setDirName:(NSString *)newDirName
{
    JSMainThreadNullState state;
    unwrap(*self).setAttributeWithoutSynchronization(HTMLNames::dirnameAttr, newDirName);
}

- (BOOL)disabled
{
    JSMainThreadNullState state;
    return unwrap(*self).hasAttributeWithoutSynchronization(HTMLNames::disabledAttr);
}

- (void)setDisabled:(BOOL)newDisabled
{
    JSMainThreadNullState state;
    unwrap(*self).setBooleanAttribute(HTMLNames::disabledAttr, newDisabled);
}

- (DOMHTMLFormElement *)form
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).form());
}

- (int)maxLength
{
    JSMainThreadNullState state;
    return unwrap(*self).maxLength();
}

- (void)setMaxLength:(int)newMaxLength
{
    JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setMaxLength(newMaxLength));
}

- (NSString *)name
{
    JSMainThreadNullState state;
    return unwrap(*self).getNameAttribute();
}

- (void)setName:(NSString *)newName
{
    JSMainThreadNullState state;
    unwrap(*self).setAttributeWithoutSynchronization(HTMLNames::nameAttr, newName);
}

- (NSString *)placeholder
{
    JSMainThreadNullState state;
    return unwrap(*self).getAttribute(HTMLNames::placeholderAttr);
}

- (void)setPlaceholder:(NSString *)newPlaceholder
{
    JSMainThreadNullState state;
    unwrap(*self).setAttributeWithoutSynchronization(HTMLNames::placeholderAttr, newPlaceholder);
}

- (BOOL)readOnly
{
    JSMainThreadNullState state;
    return unwrap(*self).hasAttributeWithoutSynchronization(HTMLNames::readonlyAttr);
}

- (void)setReadOnly:(BOOL)newReadOnly
{
    JSMainThreadNullState state;
    unwrap(*self).setBooleanAttribute(HTMLNames::readonlyAttr, newReadOnly);
}

- (BOOL)required
{
    JSMainThreadNullState state;
    return unwrap(*self).hasAttributeWithoutSynchronization(HTMLNames::requiredAttr);
}

- (void)setRequired:(BOOL)newRequired
{
    JSMainThreadNullState state;
    unwrap(*self).setBooleanAttribute(HTMLNames::requiredAttr, newRequired);
}

- (int)rows
{
    JSMainThreadNullState state;
    return unwrap(*self).rows();
}

- (void)setRows:(int)newRows
{
    JSMainThreadNullState state;
    unwrap(*self).setRows(newRows);
}

- (int)cols
{
    JSMainThreadNullState state;
    return unwrap(*self).cols();
}

- (void)setCols:(int)newCols
{
    JSMainThreadNullState state;
    unwrap(*self).setCols(newCols);
}

- (NSString *)wrap
{
    JSMainThreadNullState state;
    return unwrap(*self).getAttribute(HTMLNames::wrapAttr);
}

- (void)setWrap:(NSString *)newWrap
{
    JSMainThreadNullState state;
    unwrap(*self).setAttributeWithoutSynchronization(HTMLNames::wrapAttr, newWrap);
}

- (NSString *)type
{
    JSMainThreadNullState state;
    return unwrap(*self).type();
}

- (NSString *)defaultValue
{
    JSMainThreadNullState state;
    return unwrap(*self).defaultValue();
}

- (void)setDefaultValue:(NSString *)newDefaultValue
{
    JSMainThreadNullState state;
    unwrap(*self).setDefaultValue(newDefaultValue);
}

- (NSString *)value
{
    JSMainThreadNullState state;
    return unwrap(*self).value();
}

- (void)setValue:(NSString *)newValue
{
    JSMainThreadNullState state;
    unwrap(*self).setValue(newValue);
}

- (unsigned)textLength
{
    JSMainThreadNullState state;
    return unwrap(*self).textLength();
}

- (BOOL)willValidate
{
    JSMainThreadNullState state;
    return unwrap(*self).willValidate();
}

- (DOMNodeList *)labels
{
    JSMainThreadNullState state;
    return kit(unwrap(*self).labels().get());
}

- (int)selectionStart
{
    JSMainThreadNullState state;
    return unwrap(*self).selectionStart();
}

- (void)setSelectionStart:(int)newSelectionStart
{
    JSMainThreadNullState state;
    unwrap(*self).setSelectionStart(newSelectionStart);
}

- (int)selectionEnd
{
    JSMainThreadNullState state;
    return unwrap(*self).selectionEnd();
}

- (void)setSelectionEnd:(int)newSelectionEnd
{
    JSMainThreadNullState state;
    unwrap(*self).setSelectionEnd(newSelectionEnd);
}

- (NSString *)selectionDirection
{
    JSMainThreadNullState state;
    return unwrap(*self).selectionDirection();
}

- (void)setSelectionDirection:(NSString *)newSelectionDirection
{
    JSMainThreadNullState state;
    unwrap(*self).setSelectionDirection(newSelectionDirection);
}

- (NSString *)accessKey
{
    JSMainThreadNullState state;
    return unwrap(*self).getAttribute(HTMLNames::accesskeyAttr);
}

- (void)setAccessKey:(NSString *)newAccessKey
{
    JSMainThreadNullState state;
    unwrap(*self).setAttributeWithoutSynchronization(HTMLNames::accesskeyAttr, newAccessKey);
}

- (NSString *)autocomplete
{
    JSMainThreadNullState state;
    return unwrap(*self).autocomplete();
}

- (void)setAutocomplete:(NSString *)newAutocomplete
{
    JSMainThreadNullState state;
    unwrap(*self).setAutocomplete(newAutocomplete);
}

- (void)select
{
    JSMainThreadNullState state;
    unwrap(*self).select();
}

- (void)setRangeText:(NSString *)replacement
{
    JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setRangeText(replacement));
}

- (void)setRangeText:(NSString *)replacement start:(unsigned)start end:(unsigned)end selectionMode:(NSString *)selectionMode
{
    JSMainThreadNullState state;
    raiseOnDOMError(unwrap(*self).setRangeText(replacement, start, end, selectionMode));
}

- (void)setSelectionRange:(int)start end:(int)end
{
    JSMainThreadNullState state;
    unwrap(*self).setSelectionRange(start, end);
}

@end
