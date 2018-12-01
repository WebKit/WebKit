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

#import "DOMHTMLInputElementInternal.h"

#import "DOMFileListInternal.h"
#import "DOMHTMLElementInternal.h"
#import "DOMHTMLFormElementInternal.h"
#import "DOMNodeInternal.h"
#import "DOMNodeListInternal.h"
#import "DOMInternal.h"
#import "DOMPrivate.h"
#import "ExceptionHandlers.h"

#if TARGET_OS_IPHONE
#if __has_include(<UIKit/UITextAutofillSuggestion.h>)

#import <UIKit/UITextAutofillSuggestion.h>

#else

@interface UITextSuggestion : NSObject
@end

@interface UITextAutofillSuggestion : UITextSuggestion
@property (nonatomic, assign) NSString *username;
@property (nonatomic, assign) NSString *password;
@end

#endif // __has_include(<UIKit/UITextAutofillSuggestion.h>)
#endif // TARGET_OS_IPHONE

#import <WebCore/AutofillElements.h>
#import <WebCore/FileList.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLInputElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/JSExecState.h>
#import <WebCore/NameNodeList.h>
#import <WebCore/NodeList.h>
#import <WebCore/RenderElement.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::HTMLInputElement*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMHTMLInputElement

- (NSString *)accept
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::acceptAttr);
}

- (void)setAccept:(NSString *)newAccept
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::acceptAttr, newAccept);
}

- (NSString *)alt
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::altAttr);
}

- (void)setAlt:(NSString *)newAlt
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::altAttr, newAlt);
}

- (NSString *)autocomplete
{
    WebCore::JSMainThreadNullState state;
    return IMPL->autocomplete();
}

- (void)setAutocomplete:(NSString *)newAutocomplete
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAutocomplete(newAutocomplete);
}

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

- (BOOL)defaultChecked
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::checkedAttr);
}

- (void)setDefaultChecked:(BOOL)newDefaultChecked
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::checkedAttr, newDefaultChecked);
}

- (BOOL)checked
{
    WebCore::JSMainThreadNullState state;
    return IMPL->checked();
}

- (void)setChecked:(BOOL)newChecked
{
    WebCore::JSMainThreadNullState state;
    IMPL->setChecked(newChecked);
}

- (NSString *)dirName
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::dirnameAttr);
}

- (void)setDirName:(NSString *)newDirName
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::dirnameAttr, newDirName);
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

- (DOMFileList *)files
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->files()));
}

- (void)setFiles:(DOMFileList *)newFiles
{
    WebCore::JSMainThreadNullState state;
    ASSERT(newFiles);

    IMPL->setFiles(core(newFiles));
}

- (NSString *)formAction
{
    WebCore::JSMainThreadNullState state;
    return IMPL->formAction();
}

- (void)setFormAction:(NSString *)newFormAction
{
    WebCore::JSMainThreadNullState state;
    IMPL->setFormAction(newFormAction);
}

- (NSString *)formEnctype
{
    WebCore::JSMainThreadNullState state;
    return IMPL->formEnctype();
}

- (void)setFormEnctype:(NSString *)newFormEnctype
{
    WebCore::JSMainThreadNullState state;
    IMPL->setFormEnctype(newFormEnctype);
}

- (NSString *)formMethod
{
    WebCore::JSMainThreadNullState state;
    return IMPL->formMethod();
}

- (void)setFormMethod:(NSString *)newFormMethod
{
    WebCore::JSMainThreadNullState state;
    IMPL->setFormMethod(newFormMethod);
}

- (BOOL)formNoValidate
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::formnovalidateAttr);
}

- (void)setFormNoValidate:(BOOL)newFormNoValidate
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::formnovalidateAttr, newFormNoValidate);
}

- (NSString *)formTarget
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::formtargetAttr);
}

- (void)setFormTarget:(NSString *)newFormTarget
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::formtargetAttr, newFormTarget);
}

- (unsigned)height
{
    WebCore::JSMainThreadNullState state;
    return IMPL->height();
}

- (void)setHeight:(unsigned)newHeight
{
    WebCore::JSMainThreadNullState state;
    IMPL->setHeight(newHeight);
}

- (BOOL)indeterminate
{
    WebCore::JSMainThreadNullState state;
    return IMPL->indeterminate();
}

- (void)setIndeterminate:(BOOL)newIndeterminate
{
    WebCore::JSMainThreadNullState state;
    IMPL->setIndeterminate(newIndeterminate);
}

#if ENABLE(DATALIST_ELEMENT)
- (DOMHTMLElement *)list
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->list()));
}
#endif

- (NSString *)max
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::maxAttr);
}

- (void)setMax:(NSString *)newMax
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::maxAttr, newMax);
}

- (int)maxLength
{
    WebCore::JSMainThreadNullState state;
    return IMPL->maxLength();
}

- (void)setMaxLength:(int)newMaxLength
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setMaxLength(newMaxLength));
}

- (NSString *)min
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::minAttr);
}

- (void)setMin:(NSString *)newMin
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::minAttr, newMin);
}

- (BOOL)multiple
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::multipleAttr);
}

- (void)setMultiple:(BOOL)newMultiple
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::multipleAttr, newMultiple);
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

- (NSString *)pattern
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::patternAttr);
}

- (void)setPattern:(NSString *)newPattern
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::patternAttr, newPattern);
}

- (NSString *)placeholder
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::placeholderAttr);
}

- (void)setPlaceholder:(NSString *)newPlaceholder
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::placeholderAttr, newPlaceholder);
}

- (BOOL)readOnly
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::readonlyAttr);
}

- (void)setReadOnly:(BOOL)newReadOnly
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::readonlyAttr, newReadOnly);
}

- (BOOL)required
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::requiredAttr);
}

- (void)setRequired:(BOOL)newRequired
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::requiredAttr, newRequired);
}

- (NSString *)size
{
    WebCore::JSMainThreadNullState state;
    return WTF::String::number(IMPL->size());
}

- (void)setSize:(NSString *)newSize
{
    WebCore::JSMainThreadNullState state;
    IMPL->setSize(WTF::String(newSize).toInt());
}

- (NSString *)src
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getURLAttribute(WebCore::HTMLNames::srcAttr);
}

- (void)setSrc:(NSString *)newSrc
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::srcAttr, newSrc);
}

- (NSString *)step
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::stepAttr);
}

- (void)setStep:(NSString *)newStep
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::stepAttr, newStep);
}

- (NSString *)type
{
    WebCore::JSMainThreadNullState state;
    return IMPL->type();
}

- (void)setType:(NSString *)newType
{
    WebCore::JSMainThreadNullState state;
    IMPL->setType(newType);
}

- (NSString *)defaultValue
{
    WebCore::JSMainThreadNullState state;
    return IMPL->defaultValue();
}

- (void)setDefaultValue:(NSString *)newDefaultValue
{
    WebCore::JSMainThreadNullState state;
    IMPL->setDefaultValue(newDefaultValue);
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

- (NSTimeInterval)valueAsDate
{
    WebCore::JSMainThreadNullState state;
    return kit(IMPL->valueAsDate());
}

- (void)setValueAsDate:(NSTimeInterval)newValueAsDate
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setValueAsDate(core(newValueAsDate)));
}

- (double)valueAsNumber
{
    WebCore::JSMainThreadNullState state;
    return IMPL->valueAsNumber();
}

- (void)setValueAsNumber:(double)newValueAsNumber
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setValueAsNumber(newValueAsNumber));
}

- (unsigned)width
{
    WebCore::JSMainThreadNullState state;
    return IMPL->width();
}

- (void)setWidth:(unsigned)newWidth
{
    WebCore::JSMainThreadNullState state;
    IMPL->setWidth(newWidth);
}

- (BOOL)willValidate
{
    WebCore::JSMainThreadNullState state;
    return IMPL->willValidate();
}

- (NSString *)validationMessage
{
    WebCore::JSMainThreadNullState state;
    return IMPL->validationMessage();
}

- (DOMNodeList *)labels
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->labels()));
}

- (int)selectionStart
{
    WebCore::JSMainThreadNullState state;
    return IMPL->selectionStart();
}

- (void)setSelectionStart:(int)newSelectionStart
{
    WebCore::JSMainThreadNullState state;
    IMPL->setSelectionStart(newSelectionStart);
}

- (int)selectionEnd
{
    WebCore::JSMainThreadNullState state;
    return IMPL->selectionEnd();
}

- (void)setSelectionEnd:(int)newSelectionEnd
{
    WebCore::JSMainThreadNullState state;
    IMPL->setSelectionEnd(newSelectionEnd);
}

- (NSString *)selectionDirection
{
    WebCore::JSMainThreadNullState state;
    return IMPL->selectionDirection();
}

- (void)setSelectionDirection:(NSString *)newSelectionDirection
{
    WebCore::JSMainThreadNullState state;
    IMPL->setSelectionDirection(newSelectionDirection);
}

- (NSString *)align
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::alignAttr);
}

- (void)setAlign:(NSString *)newAlign
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::alignAttr, newAlign);
}

- (NSString *)useMap
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::usemapAttr);
}

- (void)setUseMap:(NSString *)newUseMap
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::usemapAttr, newUseMap);
}

- (BOOL)incremental
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::incrementalAttr);
}

- (void)setIncremental:(BOOL)newIncremental
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::incrementalAttr, newIncremental);
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

- (NSString *)altDisplayString
{
    WebCore::JSMainThreadNullState state;
    return WebCore::displayString(IMPL->alt(), core(self));
}

- (NSURL *)absoluteImageURL
{
    WebCore::JSMainThreadNullState state;
    if (!IMPL->renderer() || !IMPL->renderer()->isImage())
        return nil;
    return [self _getURLAttribute:@"src"];
}

#if ENABLE(MEDIA_CAPTURE)
- (BOOL)capture
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::captureAttr);
}

- (void)setCapture:(BOOL)newCapture
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::captureAttr, newCapture);
}
#endif

- (void)stepUp:(int)n
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->stepUp(n));
}

- (void)stepDown:(int)n
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->stepDown(n));
}

- (BOOL)checkValidity
{
    WebCore::JSMainThreadNullState state;
    return IMPL->checkValidity();
}

- (void)setCustomValidity:(NSString *)error
{
    WebCore::JSMainThreadNullState state;
    IMPL->setCustomValidity(error);
}

- (void)select
{
    WebCore::JSMainThreadNullState state;
    IMPL->select();
}

- (void)setRangeText:(NSString *)replacement
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setRangeText(replacement));
}

- (void)setRangeText:(NSString *)replacement start:(unsigned)start end:(unsigned)end selectionMode:(NSString *)selectionMode
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setRangeText(replacement, start, end, selectionMode));
}

- (void)setSelectionRange:(int)start end:(int)end
{
    WebCore::JSMainThreadNullState state;
    IMPL->setSelectionRange(start, end);
}

- (void)click
{
    WebCore::JSMainThreadNullState state;
    IMPL->click();
}

- (void)setValueForUser:(NSString *)inValue
{
    WebCore::JSMainThreadNullState state;
    IMPL->setValueForUser(inValue);
}

- (NSDictionary *)_autofillContext
{
    WebCore::JSMainThreadNullState state;
    if (!WebCore::AutofillElements::computeAutofillElements(*IMPL))
        return nil;

    NSURL *documentURL = [NSURL URLWithString:self.ownerDocument.URL];
    if (!documentURL)
        return nil;

    return @{ @"_WebViewURL" : documentURL };
}

#if TARGET_OS_IPHONE
- (void)insertTextSuggestion:(UITextAutofillSuggestion *)credentialSuggestion
{
    WebCore::JSMainThreadNullState state;
    if (is<WebCore::HTMLInputElement>(IMPL)) {
        if (auto autofillElements = WebCore::AutofillElements::computeAutofillElements(*IMPL))
            autofillElements->autofill(credentialSuggestion.username, credentialSuggestion.password);
    }
}
#endif // TARGET_OS_IPHONE

@end

WebCore::HTMLInputElement* core(DOMHTMLInputElement *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::HTMLInputElement*>(wrapper->_internal) : 0;
}

DOMHTMLInputElement *kit(WebCore::HTMLInputElement* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMHTMLInputElement*>(kit(static_cast<WebCore::Node*>(value)));
}
