/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WKBundleNodeHandle.h"
#include "WKBundleNodeHandlePrivate.h"

#include "InjectedBundleNodeHandle.h"
#include "InjectedBundleRangeHandle.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebFrame.h"
#include "WebImage.h"
#include <WebCore/HTMLTextFormControlElement.h>

static WebCore::AutoFillButtonType toAutoFillButtonType(WKAutoFillButtonType wkAutoFillButtonType)
{
    switch (wkAutoFillButtonType) {
    case kWKAutoFillButtonTypeNone:
        return WebCore::AutoFillButtonType::None;
    case kWKAutoFillButtonTypeContacts:
        return WebCore::AutoFillButtonType::Contacts;
    case kWKAutoFillButtonTypeCredentials:
        return WebCore::AutoFillButtonType::Credentials;
    case kWKAutoFillButtonTypeStrongPassword:
        return WebCore::AutoFillButtonType::StrongPassword;
    case kWKAutoFillButtonTypeCreditCard:
        return WebCore::AutoFillButtonType::CreditCard;
    }
    ASSERT_NOT_REACHED();
    return WebCore::AutoFillButtonType::None;
}

static WKAutoFillButtonType toWKAutoFillButtonType(WebCore::AutoFillButtonType autoFillButtonType)
{
    switch (autoFillButtonType) {
    case WebCore::AutoFillButtonType::None:
        return kWKAutoFillButtonTypeNone;
    case WebCore::AutoFillButtonType::Contacts:
        return kWKAutoFillButtonTypeContacts;
    case WebCore::AutoFillButtonType::Credentials:
        return kWKAutoFillButtonTypeCredentials;
    case WebCore::AutoFillButtonType::StrongPassword:
        return kWKAutoFillButtonTypeStrongPassword;
    case WebCore::AutoFillButtonType::CreditCard:
        return kWKAutoFillButtonTypeCreditCard;
    }
    ASSERT_NOT_REACHED();
    return kWKAutoFillButtonTypeNone;
}

WKTypeID WKBundleNodeHandleGetTypeID()
{
    return WebKit::toAPI(WebKit::InjectedBundleNodeHandle::APIType);
}

WKBundleNodeHandleRef WKBundleNodeHandleCreate(JSContextRef contextRef, JSObjectRef objectRef)
{
    RefPtr<WebKit::InjectedBundleNodeHandle> nodeHandle = WebKit::InjectedBundleNodeHandle::getOrCreate(contextRef, objectRef);
    return toAPI(nodeHandle.leakRef());
}

WKBundleNodeHandleRef WKBundleNodeHandleCopyDocument(WKBundleNodeHandleRef nodeHandleRef)
{
    RefPtr<WebKit::InjectedBundleNodeHandle> nodeHandle = WebKit::toImpl(nodeHandleRef)->document();
    return toAPI(nodeHandle.leakRef());
}

WKRect WKBundleNodeHandleGetRenderRect(WKBundleNodeHandleRef nodeHandleRef, bool* isReplaced)
{
    return WebKit::toAPI(WebKit::toImpl(nodeHandleRef)->renderRect(isReplaced));
}

WKImageRef WKBundleNodeHandleCopySnapshotWithOptions(WKBundleNodeHandleRef nodeHandleRef, WKSnapshotOptions options)
{
    RefPtr<WebKit::WebImage> image = WebKit::toImpl(nodeHandleRef)->renderedImage(WebKit::toSnapshotOptions(options), options & kWKSnapshotOptionsExcludeOverflow);
    return toAPI(image.leakRef());
}

WKBundleRangeHandleRef WKBundleNodeHandleCopyVisibleRange(WKBundleNodeHandleRef nodeHandleRef)
{
    RefPtr<WebKit::InjectedBundleRangeHandle> rangeHandle = WebKit::toImpl(nodeHandleRef)->visibleRange();
    return toAPI(rangeHandle.leakRef());
}

WKRect WKBundleNodeHandleGetElementBounds(WKBundleNodeHandleRef elementHandleRef)
{
    return WebKit::toAPI(WebKit::toImpl(elementHandleRef)->elementBounds());
}

void WKBundleNodeHandleSetHTMLInputElementValueForUser(WKBundleNodeHandleRef htmlInputElementHandleRef, WKStringRef valueRef)
{
    WebKit::toImpl(htmlInputElementHandleRef)->setHTMLInputElementValueForUser(WebKit::toWTFString(valueRef));
}

void WKBundleNodeHandleSetHTMLInputElementSpellcheckEnabled(WKBundleNodeHandleRef htmlInputElementHandleRef, bool enabled)
{
    WebKit::toImpl(htmlInputElementHandleRef)->setHTMLInputElementSpellcheckEnabled(enabled);
}

bool WKBundleNodeHandleGetHTMLInputElementAutoFilled(WKBundleNodeHandleRef htmlInputElementHandleRef)
{
    return WebKit::toImpl(htmlInputElementHandleRef)->isHTMLInputElementAutoFilled();
}

void WKBundleNodeHandleSetHTMLInputElementAutoFilled(WKBundleNodeHandleRef htmlInputElementHandleRef, bool filled)
{
    WebKit::toImpl(htmlInputElementHandleRef)->setHTMLInputElementAutoFilled(filled);
}

void WKBundleNodeHandleSetHTMLInputElementAutoFilledAndViewable(WKBundleNodeHandleRef htmlInputElementHandleRef, bool autoFilledAndViewable)
{
    WebKit::toImpl(htmlInputElementHandleRef)->setHTMLInputElementAutoFilledAndViewable(autoFilledAndViewable);
}

bool WKBundleNodeHandleGetHTMLInputElementAutoFillButtonEnabled(WKBundleNodeHandleRef htmlInputElementHandleRef)
{
    return WebKit::toImpl(htmlInputElementHandleRef)->isHTMLInputElementAutoFillButtonEnabled();
}

void WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabledWithButtonType(WKBundleNodeHandleRef htmlInputElementHandleRef, WKAutoFillButtonType autoFillButtonType)
{
    WebKit::toImpl(htmlInputElementHandleRef)->setHTMLInputElementAutoFillButtonEnabled(toAutoFillButtonType(autoFillButtonType));
}

WKAutoFillButtonType WKBundleNodeHandleGetHTMLInputElementAutoFillButtonType(WKBundleNodeHandleRef htmlInputElementHandleRef)
{
    return toWKAutoFillButtonType(WebKit::toImpl(htmlInputElementHandleRef)->htmlInputElementAutoFillButtonType());
}

WKAutoFillButtonType WKBundleNodeHandleGetHTMLInputElementLastAutoFillButtonType(WKBundleNodeHandleRef htmlInputElementHandleRef)
{
    return toWKAutoFillButtonType(WebKit::toImpl(htmlInputElementHandleRef)->htmlInputElementLastAutoFillButtonType());
}

bool WKBundleNodeHandleGetHTMLInputElementAutoFillAvailable(WKBundleNodeHandleRef htmlInputElementHandleRef)
{
    return WebKit::toImpl(htmlInputElementHandleRef)->isAutoFillAvailable();
}

void WKBundleNodeHandleSetHTMLInputElementAutoFillAvailable(WKBundleNodeHandleRef htmlInputElementHandleRef, bool autoFillAvailable)
{
    WebKit::toImpl(htmlInputElementHandleRef)->setAutoFillAvailable(autoFillAvailable);
}

WKRect WKBundleNodeHandleGetHTMLInputElementAutoFillButtonBounds(WKBundleNodeHandleRef htmlInputElementHandleRef)
{
    return WebKit::toAPI(WebKit::toImpl(htmlInputElementHandleRef)->htmlInputElementAutoFillButtonBounds());
}

bool WKBundleNodeHandleGetHTMLInputElementLastChangeWasUserEdit(WKBundleNodeHandleRef htmlInputElementHandleRef)
{
    return WebKit::toImpl(htmlInputElementHandleRef)->htmlInputElementLastChangeWasUserEdit();
}

bool WKBundleNodeHandleGetHTMLTextAreaElementLastChangeWasUserEdit(WKBundleNodeHandleRef htmlTextAreaElementHandleRef)
{
    return WebKit::toImpl(htmlTextAreaElementHandleRef)->htmlTextAreaElementLastChangeWasUserEdit();
}

WKBundleNodeHandleRef WKBundleNodeHandleCopyHTMLTableCellElementCellAbove(WKBundleNodeHandleRef htmlTableCellElementHandleRef)
{
    RefPtr<WebKit::InjectedBundleNodeHandle> nodeHandle = WebKit::toImpl(htmlTableCellElementHandleRef)->htmlTableCellElementCellAbove();
    return toAPI(nodeHandle.leakRef());
}

WKBundleFrameRef WKBundleNodeHandleCopyDocumentFrame(WKBundleNodeHandleRef documentHandleRef)
{
    RefPtr<WebKit::WebFrame> frame = WebKit::toImpl(documentHandleRef)->documentFrame();
    return toAPI(frame.leakRef());
}

WKBundleFrameRef WKBundleNodeHandleCopyHTMLFrameElementContentFrame(WKBundleNodeHandleRef htmlFrameElementHandleRef)
{
    RefPtr<WebKit::WebFrame> frame = WebKit::toImpl(htmlFrameElementHandleRef)->htmlFrameElementContentFrame();
    return toAPI(frame.leakRef());
}

WKBundleFrameRef WKBundleNodeHandleCopyHTMLIFrameElementContentFrame(WKBundleNodeHandleRef htmlIFrameElementHandleRef)
{
    RefPtr<WebKit::WebFrame> frame = WebKit::toImpl(htmlIFrameElementHandleRef)->htmlIFrameElementContentFrame();
    return toAPI(frame.leakRef());
}

// Deprecated - use WKBundleNodeHandleGetHTMLInputElementAutoFilled(WKBundleNodeHandleRef).
bool WKBundleNodeHandleGetHTMLInputElementAutofilled(WKBundleNodeHandleRef htmlInputElementHandleRef)
{
    return WebKit::toImpl(htmlInputElementHandleRef)->isHTMLInputElementAutoFilled();
}

// Deprecated - use WKBundleNodeHandleSetHTMLInputElementAutoFilled(WKBundleNodeHandleRef, bool).
void WKBundleNodeHandleSetHTMLInputElementAutofilled(WKBundleNodeHandleRef htmlInputElementHandleRef, bool filled)
{
    WebKit::toImpl(htmlInputElementHandleRef)->setHTMLInputElementAutoFilled(filled);
}

// Deprecated - use WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabledWithButtonType(WKBundleNodeHandleRef, WKAutoFillButtonType).
void WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabled(WKBundleNodeHandleRef htmlInputElementHandleRef, bool enabled)
{
    WebCore::AutoFillButtonType autoFillButtonType = enabled ? WebCore::AutoFillButtonType::Credentials : WebCore::AutoFillButtonType::None;

    WebKit::toImpl(htmlInputElementHandleRef)->setHTMLInputElementAutoFillButtonEnabled(autoFillButtonType);
}
