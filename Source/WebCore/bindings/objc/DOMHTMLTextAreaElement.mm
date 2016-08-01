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
#import "DOMHTMLTextAreaElementInternal.h"

#import "DOMHTMLFormElementInternal.h"
#import "DOMNodeInternal.h"
#import "DOMNodeListInternal.h"
#import "DOMValidityStateInternal.h"
#import "ExceptionHandlers.h"
#import "HTMLTextAreaElement.h"
#import "JSMainThreadExecState.h"
#import "NodeList.h"
#import "ThreadCheck.h"
#import "ValidityState.h"

using namespace WebCore;

static inline HTMLTextAreaElement& wrapped(void* internal)
{
    ASSERT(internal);
    return downcast<HTMLTextAreaElement>(*static_cast<Node*>(internal));
}

HTMLTextAreaElement* core(DOMHTMLTextAreaElement *wrapper)
{
    return wrapper ? &wrapped(wrapper->_internal) : nullptr;
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
    return wrapped(_internal).hasAttributeWithoutSynchronization(HTMLNames::autofocusAttr);
}

- (void)setAutofocus:(BOOL)newAutofocus
{
    JSMainThreadNullState state;
    wrapped(_internal).setBooleanAttribute(HTMLNames::autofocusAttr, newAutofocus);
}

- (NSString *)dirName
{
    JSMainThreadNullState state;
    return wrapped(_internal).getAttribute(HTMLNames::dirnameAttr);
}

- (void)setDirName:(NSString *)newDirName
{
    JSMainThreadNullState state;
    wrapped(_internal).setAttributeWithoutSynchronization(HTMLNames::dirnameAttr, newDirName);
}

- (BOOL)disabled
{
    JSMainThreadNullState state;
    return wrapped(_internal).hasAttributeWithoutSynchronization(HTMLNames::disabledAttr);
}

- (void)setDisabled:(BOOL)newDisabled
{
    JSMainThreadNullState state;
    wrapped(_internal).setBooleanAttribute(HTMLNames::disabledAttr, newDisabled);
}

- (DOMHTMLFormElement *)form
{
    JSMainThreadNullState state;
    return kit(wrapped(_internal).form());
}

- (int)maxLength
{
    JSMainThreadNullState state;
    return wrapped(_internal).maxLengthForBindings();
}

- (void)setMaxLength:(int)newMaxLength
{
    JSMainThreadNullState state;
    ExceptionCode ec = 0;
    wrapped(_internal).setMaxLengthForBindings(newMaxLength, ec);
    raiseOnDOMError(ec);
}

- (NSString *)name
{
    JSMainThreadNullState state;
    return wrapped(_internal).getNameAttribute();
}

- (void)setName:(NSString *)newName
{
    JSMainThreadNullState state;
    wrapped(_internal).setAttributeWithoutSynchronization(HTMLNames::nameAttr, newName);
}

- (NSString *)placeholder
{
    JSMainThreadNullState state;
    return wrapped(_internal).getAttribute(HTMLNames::placeholderAttr);
}

- (void)setPlaceholder:(NSString *)newPlaceholder
{
    JSMainThreadNullState state;
    wrapped(_internal).setAttributeWithoutSynchronization(HTMLNames::placeholderAttr, newPlaceholder);
}

- (BOOL)readOnly
{
    JSMainThreadNullState state;
    return wrapped(_internal).hasAttributeWithoutSynchronization(HTMLNames::readonlyAttr);
}

- (void)setReadOnly:(BOOL)newReadOnly
{
    JSMainThreadNullState state;
    wrapped(_internal).setBooleanAttribute(HTMLNames::readonlyAttr, newReadOnly);
}

- (BOOL)required
{
    JSMainThreadNullState state;
    return wrapped(_internal).hasAttributeWithoutSynchronization(HTMLNames::requiredAttr);
}

- (void)setRequired:(BOOL)newRequired
{
    JSMainThreadNullState state;
    wrapped(_internal).setBooleanAttribute(HTMLNames::requiredAttr, newRequired);
}

- (int)rows
{
    JSMainThreadNullState state;
    return wrapped(_internal).rows();
}

- (void)setRows:(int)newRows
{
    JSMainThreadNullState state;
    wrapped(_internal).setRows(newRows);
}

- (int)cols
{
    JSMainThreadNullState state;
    return wrapped(_internal).cols();
}

- (void)setCols:(int)newCols
{
    JSMainThreadNullState state;
    wrapped(_internal).setCols(newCols);
}

- (NSString *)wrap
{
    JSMainThreadNullState state;
    return wrapped(_internal).getAttribute(HTMLNames::wrapAttr);
}

- (void)setWrap:(NSString *)newWrap
{
    JSMainThreadNullState state;
    wrapped(_internal).setAttributeWithoutSynchronization(HTMLNames::wrapAttr, newWrap);
}

- (NSString *)type
{
    JSMainThreadNullState state;
    return wrapped(_internal).type();
}

- (NSString *)defaultValue
{
    JSMainThreadNullState state;
    return wrapped(_internal).defaultValue();
}

- (void)setDefaultValue:(NSString *)newDefaultValue
{
    JSMainThreadNullState state;
    wrapped(_internal).setDefaultValue(newDefaultValue);
}

- (NSString *)value
{
    JSMainThreadNullState state;
    return wrapped(_internal).value();
}

- (void)setValue:(NSString *)newValue
{
    JSMainThreadNullState state;
    wrapped(_internal).setValue(newValue);
}

- (unsigned)textLength
{
    JSMainThreadNullState state;
    return wrapped(_internal).textLength();
}

- (BOOL)willValidate
{
    JSMainThreadNullState state;
    return wrapped(_internal).willValidate();
}

- (DOMValidityState *)validity
{
    JSMainThreadNullState state;
    return kit(wrapped(_internal).validity());
}

- (NSString *)validationMessage
{
    JSMainThreadNullState state;
    return wrapped(_internal).validationMessage();
}

- (DOMNodeList *)labels
{
    JSMainThreadNullState state;
    return kit(wrapped(_internal).labels().get());
}

- (int)selectionStart
{
    JSMainThreadNullState state;
    return wrapped(_internal).selectionStart();
}

- (void)setSelectionStart:(int)newSelectionStart
{
    JSMainThreadNullState state;
    wrapped(_internal).setSelectionStart(newSelectionStart);
}

- (int)selectionEnd
{
    JSMainThreadNullState state;
    return wrapped(_internal).selectionEnd();
}

- (void)setSelectionEnd:(int)newSelectionEnd
{
    JSMainThreadNullState state;
    wrapped(_internal).setSelectionEnd(newSelectionEnd);
}

- (NSString *)selectionDirection
{
    JSMainThreadNullState state;
    return wrapped(_internal).selectionDirection();
}

- (void)setSelectionDirection:(NSString *)newSelectionDirection
{
    JSMainThreadNullState state;
    wrapped(_internal).setSelectionDirection(newSelectionDirection);
}

- (NSString *)accessKey
{
    JSMainThreadNullState state;
    return wrapped(_internal).getAttribute(HTMLNames::accesskeyAttr);
}

- (void)setAccessKey:(NSString *)newAccessKey
{
    JSMainThreadNullState state;
    wrapped(_internal).setAttributeWithoutSynchronization(HTMLNames::accesskeyAttr, newAccessKey);
}

- (NSString *)autocomplete
{
    JSMainThreadNullState state;
    return wrapped(_internal).autocomplete();
}

- (void)setAutocomplete:(NSString *)newAutocomplete
{
    JSMainThreadNullState state;
    wrapped(_internal).setAutocomplete(newAutocomplete);
}

- (BOOL)checkValidity
{
    JSMainThreadNullState state;
    return wrapped(_internal).checkValidity();
}

- (void)setCustomValidity:(NSString *)error
{
    JSMainThreadNullState state;
    wrapped(_internal).setCustomValidity(error);
}

- (void)select
{
    JSMainThreadNullState state;
    wrapped(_internal).select();
}

- (void)setRangeText:(NSString *)replacement
{
    JSMainThreadNullState state;
    ExceptionCode ec = 0;
    wrapped(_internal).setRangeText(replacement, ec);
    raiseOnDOMError(ec);
}

- (void)setRangeText:(NSString *)replacement start:(unsigned)start end:(unsigned)end selectionMode:(NSString *)selectionMode
{
    JSMainThreadNullState state;
    ExceptionCode ec = 0;
    wrapped(_internal).setRangeText(replacement, start, end, selectionMode, ec);
    raiseOnDOMError(ec);
}

- (void)setSelectionRange:(int)start end:(int)end
{
    JSMainThreadNullState state;
    wrapped(_internal).setSelectionRange(start, end);
}

#if ENABLE(IOS_AUTOCORRECT_AND_AUTOCAPITALIZE)

- (BOOL)autocorrect
{
    JSMainThreadNullState state;
    return wrapped(_internal).autocorrect();
}

- (void)setAutocorrect:(BOOL)newAutocorrect
{
    JSMainThreadNullState state;
    wrapped(_internal).setAutocorrect(newAutocorrect);
}

- (NSString *)autocapitalize
{
    JSMainThreadNullState state;
    return wrapped(_internal).autocapitalize();
}

- (void)setAutocapitalize:(NSString *)newAutocapitalize
{
    JSMainThreadNullState state;
    wrapped(_internal).setAutocapitalize(newAutocapitalize);
}

#endif

@end
