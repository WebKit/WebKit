/*
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WKWebProcessPlugInNodeHandleInternal.h"

#import "CocoaImage.h"
#import "WKSharedAPICast.h"
#import "WKWebProcessPlugInFrameInternal.h"
#import "WebImage.h"
#import <WebCore/HTMLTextFormControlElement.h>
#import <WebCore/IntRect.h>
#import <WebCore/NativeImage.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/AlignedStorage.h>

@implementation WKWebProcessPlugInNodeHandle {
    AlignedStorage<WebKit::InjectedBundleNodeHandle> _nodeHandle;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKWebProcessPlugInNodeHandle.class, self))
        return;
    SUPPRESS_UNCOUNTED_ARG _nodeHandle->~InjectedBundleNodeHandle();
    [super dealloc];
}

+ (WKWebProcessPlugInNodeHandle *)nodeHandleWithJSValue:(JSValue *)value inContext:(JSContext *)context
{
    JSContextRef contextRef = [context JSGlobalContextRef];
    JSObjectRef objectRef = JSValueToObject(contextRef, [value JSValueRef], nullptr);
    return WebKit::wrapper(WebKit::InjectedBundleNodeHandle::getOrCreate(contextRef, objectRef)).autorelease();
}

- (WKWebProcessPlugInFrame *)htmlIFrameElementContentFrame
{
    return WebKit::wrapper(protect(*_nodeHandle)->htmlIFrameElementContentFrame()).autorelease();
}

- (CocoaImage *)renderedImageWithOptions:(WKSnapshotOptions)options
{
    return [self renderedImageWithOptions:options width:nil];
}

- (CocoaImage *)renderedImageWithOptions:(WKSnapshotOptions)options width:(NSNumber *)width
{
    std::optional<float> optionalWidth;
    if (width)
        optionalWidth = width.floatValue;

    auto image = protect(*_nodeHandle)->renderedImage(WebKit::toSnapshotOptions(options), options & kWKSnapshotOptionsExcludeOverflow, optionalWidth);
    if (!image)
        return nil;

    auto nativeImage = image->copyNativeImage(WebCore::DontCopyBackingStore);
    if (!nativeImage)
        return nil;

#if USE(APPKIT)
    return adoptNS([[NSImage alloc] initWithCGImage:nativeImage->platformImage().get() size:NSZeroSize]).autorelease();
#else
    return adoptNS([[UIImage alloc] initWithCGImage:nativeImage->platformImage().get()]).autorelease();
#endif
}

- (CGRect)elementBounds
{
    return protect(*_nodeHandle)->elementBounds();
}

- (BOOL)HTMLInputElementIsAutoFilled
{
    return protect(*_nodeHandle)->isHTMLInputElementAutoFilled();
}

- (BOOL)HTMLInputElementIsAutoFilledAndViewable
{
    return protect(*_nodeHandle)->isHTMLInputElementAutoFilledAndViewable();
}

- (BOOL)HTMLInputElementIsAutoFilledAndObscured
{
    return protect(*_nodeHandle)->isHTMLInputElementAutoFilledAndObscured();
}

- (void)setHTMLInputElementIsAutoFilled:(BOOL)isAutoFilled
{
    protect(*_nodeHandle)->setHTMLInputElementAutoFilled(isAutoFilled);
}

- (void)setHTMLInputElementIsAutoFilledAndViewable:(BOOL)isAutoFilledAndViewable
{
    protect(*_nodeHandle)->setHTMLInputElementAutoFilledAndViewable(isAutoFilledAndViewable);
}

- (void)setHTMLInputElementIsAutoFilledAndObscured:(BOOL)isAutoFilledAndObscured
{
    protect(*_nodeHandle)->setHTMLInputElementAutoFilledAndObscured(isAutoFilledAndObscured);
}

- (BOOL)isHTMLInputElementAutoFillButtonEnabled
{
    return protect(*_nodeHandle)->isHTMLInputElementAutoFillButtonEnabled();
}

static WebCore::AutoFillButtonType NODELETE toAutoFillButtonType(_WKAutoFillButtonType autoFillButtonType)
{
    switch (autoFillButtonType) {
    case _WKAutoFillButtonTypeNone:
        return WebCore::AutoFillButtonType::None;
    case _WKAutoFillButtonTypeContacts:
        return WebCore::AutoFillButtonType::Contacts;
    case _WKAutoFillButtonTypeCredentials:
        return WebCore::AutoFillButtonType::Credentials;
    case _WKAutoFillButtonTypeStrongPassword:
        return WebCore::AutoFillButtonType::StrongPassword;
    case _WKAutoFillButtonTypeCreditCard:
        return WebCore::AutoFillButtonType::CreditCard;
    case _WKAutoFillButtonTypeLoading:
        return WebCore::AutoFillButtonType::Loading;
    }
    ASSERT_NOT_REACHED();
    return WebCore::AutoFillButtonType::None;
}

static _WKAutoFillButtonType NODELETE toWKAutoFillButtonType(WebCore::AutoFillButtonType autoFillButtonType)
{
    switch (autoFillButtonType) {
    case WebCore::AutoFillButtonType::None:
        return _WKAutoFillButtonTypeNone;
    case WebCore::AutoFillButtonType::Contacts:
        return _WKAutoFillButtonTypeContacts;
    case WebCore::AutoFillButtonType::Credentials:
        return _WKAutoFillButtonTypeCredentials;
    case WebCore::AutoFillButtonType::StrongPassword:
        return _WKAutoFillButtonTypeStrongPassword;
    case WebCore::AutoFillButtonType::CreditCard:
        return _WKAutoFillButtonTypeCreditCard;
    case WebCore::AutoFillButtonType::Loading:
        return _WKAutoFillButtonTypeLoading;
    }
    ASSERT_NOT_REACHED();
    return _WKAutoFillButtonTypeNone;

}

- (void)setHTMLInputElementAutoFillButtonEnabledWithButtonType:(_WKAutoFillButtonType)autoFillButtonType
{
    protect(*_nodeHandle)->setHTMLInputElementAutoFillButtonEnabled(toAutoFillButtonType(autoFillButtonType));
}

- (_WKAutoFillButtonType)htmlInputElementAutoFillButtonType
{
    return toWKAutoFillButtonType(protect(*_nodeHandle)->htmlInputElementAutoFillButtonType());
}

- (_WKAutoFillButtonType)htmlInputElementLastAutoFillButtonType
{
    return toWKAutoFillButtonType(protect(*_nodeHandle)->htmlInputElementLastAutoFillButtonType());
}

- (BOOL)HTMLInputElementIsUserEdited
{
    return protect(*_nodeHandle)->htmlInputElementLastChangeWasUserEdit();
}

- (BOOL)HTMLTextAreaElementIsUserEdited
{
    return protect(*_nodeHandle)->htmlTextAreaElementLastChangeWasUserEdit();
}

- (BOOL)isSelectElement
{
    return protect(*_nodeHandle)->isSelectElement();
}

- (BOOL)isSelectableTextNode
{
    return protect(*_nodeHandle)->isSelectableTextNode();
}

- (BOOL)isTextField
{
    return protect(*_nodeHandle)->isTextField();
}

- (WKWebProcessPlugInNodeHandle *)HTMLTableCellElementCellAbove
{
    return WebKit::wrapper(protect(*_nodeHandle)->htmlTableCellElementCellAbove()).autorelease();
}

- (WKWebProcessPlugInFrame *)frame
{
    return WebKit::wrapper(protect(*_nodeHandle)->document()->documentFrame()).autorelease();
}

- (WebKit::InjectedBundleNodeHandle&)_nodeHandle
{
    return *_nodeHandle;
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_nodeHandle;
}

@end
