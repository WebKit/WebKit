/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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
#import <WebCore/ContainerNodeInlines.h>
#import <WebCore/ElementInlines.h>

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
#import <WebCore/NodeList.h>
#import <WebCore/RenderElement.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL protect(downcast<WebCore::HTMLInputElement>(reinterpret_cast<WebCore::Node*>(_internal)))

@implementation DOMHTMLInputElement

- (NSString *)accept
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::acceptAttr)).autorelease();
}

- (void)setAccept:(NSString *)newAccept
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::acceptAttr, AtomStringImpl::add((__bridge CFStringRef)newAccept).get());
}

- (NSString *)alt
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::altAttr)).autorelease();
}

- (void)setAlt:(NSString *)newAlt
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::altAttr, AtomStringImpl::add((__bridge CFStringRef)newAlt).get());
}

- (NSString *)autocomplete
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->autocomplete()).autorelease();
}

- (void)setAutocomplete:(NSString *)newAutocomplete
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::autocompleteAttr, AtomStringImpl::add((__bridge CFStringRef)newAutocomplete).get());
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
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::dirnameAttr)).autorelease();
}

- (void)setDirName:(NSString *)newDirName
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::dirnameAttr, AtomStringImpl::add((__bridge CFStringRef)newDirName).get());
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
    return createNSString(IMPL->formAction()).autorelease();
}

- (void)setFormAction:(NSString *)newFormAction
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::formactionAttr, AtomStringImpl::add((__bridge CFStringRef)newFormAction).get());
}

- (NSString *)formEnctype
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->formEnctype()).autorelease();
}

- (void)setFormEnctype:(NSString *)newFormEnctype
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::formenctypeAttr, AtomStringImpl::add((__bridge CFStringRef)newFormEnctype).get());
}

- (NSString *)formMethod
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->formMethod()).autorelease();
}

- (void)setFormMethod:(NSString *)newFormMethod
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::formmethodAttr, AtomStringImpl::add((__bridge CFStringRef)newFormMethod).get());
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
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::formtargetAttr)).autorelease();
}

- (void)setFormTarget:(NSString *)newFormTarget
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::formtargetAttr, AtomStringImpl::add((__bridge CFStringRef)newFormTarget).get());
}

- (unsigned)height
{
    WebCore::JSMainThreadNullState state;
    return IMPL->height();
}

- (void)setHeight:(unsigned)newHeight
{
    WebCore::JSMainThreadNullState state;
    IMPL->setUnsignedIntegralAttribute(WebCore::HTMLNames::heightAttr, newHeight);
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

- (DOMHTMLElement *)list
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->list()));
}

- (NSString *)max
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->attributeWithoutSynchronization(WebCore::HTMLNames::maxAttr)).autorelease();
}

- (void)setMax:(NSString *)newMax
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::maxAttr, AtomStringImpl::add((__bridge CFStringRef)newMax).get());
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
    return createNSString(IMPL->attributeWithoutSynchronization(WebCore::HTMLNames::minAttr)).autorelease();
}

- (void)setMin:(NSString *)newMin
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::minAttr, AtomStringImpl::add((__bridge CFStringRef)newMin).get());
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
    return createNSString(IMPL->getNameAttribute()).autorelease();
}

- (void)setName:(NSString *)newName
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::nameAttr, AtomStringImpl::add((__bridge CFStringRef)newName).get());
}

- (NSString *)pattern
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->attributeWithoutSynchronization(WebCore::HTMLNames::patternAttr)).autorelease();
}

- (void)setPattern:(NSString *)newPattern
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::patternAttr, AtomStringImpl::add((__bridge CFStringRef)newPattern).get());
}

- (NSString *)placeholder
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->attributeWithoutSynchronization(WebCore::HTMLNames::placeholderAttr)).autorelease();
}

- (void)setPlaceholder:(NSString *)newPlaceholder
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::placeholderAttr, AtomStringImpl::add((__bridge CFStringRef)newPlaceholder).get());
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
    return createNSString(makeString(IMPL->size())).autorelease();
}

- (void)setSize:(NSString *)newSize
{
    WebCore::JSMainThreadNullState state;
    IMPL->setSize(newSize.intValue);
}

- (NSString *)src
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getURLAttribute(WebCore::HTMLNames::srcAttr).string()).autorelease();
}

- (void)setSrc:(NSString *)newSrc
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::srcAttr, AtomStringImpl::add((__bridge CFStringRef)newSrc).get());
}

- (NSString *)step
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->attributeWithoutSynchronization(WebCore::HTMLNames::stepAttr)).autorelease();
}

- (void)setStep:(NSString *)newStep
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::stepAttr, AtomStringImpl::add((__bridge CFStringRef)newStep).get());
}

- (NSString *)type
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->type()).autorelease();
}

- (void)setType:(NSString *)newType
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::typeAttr, AtomStringImpl::add((__bridge CFStringRef)newType).get());
}

- (NSString *)defaultValue
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->attributeWithoutSynchronization(WebCore::HTMLNames::valueAttr)).autorelease();
}

- (void)setDefaultValue:(NSString *)newDefaultValue
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::valueAttr, AtomStringImpl::add((__bridge CFStringRef)newDefaultValue).get());
}

- (NSString *)value
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->value().get()).autorelease();
}

- (void)setValue:(NSString *)newValue
{
    WebCore::JSMainThreadNullState state;
    IMPL->setValue(StringImpl::create(newValue).get());
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
    IMPL->setUnsignedIntegralAttribute(WebCore::HTMLNames::widthAttr, newWidth);
}

- (BOOL)willValidate
{
    WebCore::JSMainThreadNullState state;
    return IMPL->willValidate();
}

- (NSString *)validationMessage
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->validationMessage()).autorelease();
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
    return createNSString(IMPL->selectionDirection()).autorelease();
}

- (void)setSelectionDirection:(NSString *)newSelectionDirection
{
    WebCore::JSMainThreadNullState state;
    IMPL->setSelectionDirection(StringImpl::create(newSelectionDirection).get());
}

- (NSString *)align
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::alignAttr)).autorelease();
}

- (void)setAlign:(NSString *)newAlign
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::alignAttr, AtomStringImpl::add((__bridge CFStringRef)newAlign).get());
}

- (NSString *)useMap
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::usemapAttr)).autorelease();
}

- (void)setUseMap:(NSString *)newUseMap
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::usemapAttr, AtomStringImpl::add((__bridge CFStringRef)newUseMap).get());
}

- (BOOL)incremental
{
    return NO;
}

- (void)setIncremental:(BOOL)newIncremental
{
}

- (NSString *)accessKey
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->getAttribute(WebCore::HTMLNames::accesskeyAttr)).autorelease();
}

- (void)setAccessKey:(NSString *)newAccessKey
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::accesskeyAttr, AtomStringImpl::add((__bridge CFStringRef)newAccessKey).get());
}

- (NSString *)altDisplayString
{
    WebCore::JSMainThreadNullState state;
    return createNSString(WebCore::displayString(IMPL->attributeWithoutSynchronization(WebCore::HTMLNames::altAttr), protect(core(self)))).autorelease();
}

- (NSURL *)absoluteImageURL
{
    WebCore::JSMainThreadNullState state;
    CheckedPtr renderer = IMPL->renderer();
    if (!renderer || !renderer->isImage())
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
    IMPL->setCustomValidity(StringImpl::create(error).get());
}

- (void)select
{
    WebCore::JSMainThreadNullState state;
    IMPL->select();
}

- (void)setRangeText:(NSString *)replacement
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setRangeText(StringImpl::create(replacement).get()));
}

- (void)setRangeText:(NSString *)replacement start:(unsigned)start end:(unsigned)end selectionMode:(NSString *)selectionMode
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setRangeText(StringImpl::create(replacement).get(), start, end, StringImpl::create(selectionMode).get()));
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
    IMPL->setValueForUser(StringImpl::create(inValue).get());
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
    if (is<WebCore::HTMLInputElement>(IMPL.get())) {
        if (auto autofillElements = WebCore::AutofillElements::computeAutofillElements(*IMPL))
            autofillElements->autofill(StringImpl::create(credentialSuggestion.username), StringImpl::create(credentialSuggestion.password));
    }
}
#endif // TARGET_OS_IPHONE

- (BOOL)canShowPlaceholder
{
    WebCore::JSMainThreadNullState state;
    return IMPL->canShowPlaceholder();
}

- (void)setCanShowPlaceholder:(BOOL)canShowPlaceholder
{
    WebCore::JSMainThreadNullState state;
    IMPL->setCanShowPlaceholder(canShowPlaceholder);
}

@end

WebCore::HTMLInputElement* core(DOMHTMLInputElement *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::HTMLInputElement*>(wrapper->_internal) : 0;
}

SUPPRESS_NODELETE DOMHTMLInputElement *kit(WebCore::HTMLInputElement* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMHTMLInputElement*>(kit(static_cast<WebCore::Node*>(value)));
}

#undef IMPL
