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
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebFrame.h"
#include "WebImage.h"

WKTypeID WKBundleNodeHandleGetTypeID()
{
    return WebKit::toAPI(WebKit::InjectedBundleNodeHandle::APIType);
}

WKBundleNodeHandleRef WKBundleNodeHandleCreate(JSContextRef contextRef, JSObjectRef objectRef)
{
    RefPtr<WebKit::InjectedBundleNodeHandle> nodeHandle = WebKit::InjectedBundleNodeHandle::getOrCreate(contextRef, objectRef);
    return toAPILeakingRef(WTF::move(nodeHandle));
}

WKBundleNodeHandleRef WKBundleNodeHandleCopyDocument(WKBundleNodeHandleRef nodeHandleRef)
{
    RefPtr<WebKit::InjectedBundleNodeHandle> nodeHandle = protect(WebKit::toImpl(nodeHandleRef))->document();
    return toAPILeakingRef(WTF::move(nodeHandle));
}

WKRect WKBundleNodeHandleGetRenderRect(WKBundleNodeHandleRef, bool*)
{
    return { };
}

WKImageRef WKBundleNodeHandleCopySnapshotWithOptions(WKBundleNodeHandleRef nodeHandleRef, WKSnapshotOptions options)
{
    RefPtr<WebKit::WebImage> image = protect(WebKit::toImpl(nodeHandleRef))->renderedImage(WebKit::toSnapshotOptions(options), options & kWKSnapshotOptionsExcludeOverflow);
    return toAPILeakingRef(WTF::move(image));
}

WKBundleRangeHandleRef WKBundleNodeHandleCopyVisibleRange(WKBundleNodeHandleRef)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

WKRect WKBundleNodeHandleGetElementBounds(WKBundleNodeHandleRef elementHandleRef)
{
    return WebKit::toAPI(protect(WebKit::toImpl(elementHandleRef))->elementBounds());
}

void WKBundleNodeHandleSetHTMLInputElementValueForUser(WKBundleNodeHandleRef, WKStringRef)
{
}

void WKBundleNodeHandleSetHTMLInputElementSpellcheckEnabled(WKBundleNodeHandleRef, bool)
{
}

bool WKBundleNodeHandleGetHTMLInputElementAutoFilled(WKBundleNodeHandleRef)
{
    ASSERT_NOT_REACHED();
    return false;
}

void WKBundleNodeHandleSetHTMLInputElementAutoFilled(WKBundleNodeHandleRef, bool)
{
}

void WKBundleNodeHandleSetHTMLInputElementAutoFilledAndViewable(WKBundleNodeHandleRef, bool)
{
}

void WKBundleNodeHandleSetHTMLInputElementAutoFilledAndObscured(WKBundleNodeHandleRef, bool)
{
}

bool WKBundleNodeHandleGetHTMLInputElementAutoFillButtonEnabled(WKBundleNodeHandleRef)
{
    return false;
}

void WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabledWithButtonType(WKBundleNodeHandleRef, uint8_t)
{
}

uint8_t WKBundleNodeHandleGetHTMLInputElementAutoFillButtonType(WKBundleNodeHandleRef)
{
    return 0;
}

uint8_t WKBundleNodeHandleGetHTMLInputElementLastAutoFillButtonType(WKBundleNodeHandleRef)
{
    return 0;
}

bool WKBundleNodeHandleGetHTMLInputElementAutoFillAvailable(WKBundleNodeHandleRef)
{
    ASSERT_NOT_REACHED();
    return false;
}

void WKBundleNodeHandleSetHTMLInputElementAutoFillAvailable(WKBundleNodeHandleRef, bool)
{
}

WKRect WKBundleNodeHandleGetHTMLInputElementAutoFillButtonBounds(WKBundleNodeHandleRef)
{
    ASSERT_NOT_REACHED();
    return { };
}

bool WKBundleNodeHandleGetHTMLInputElementLastChangeWasUserEdit(WKBundleNodeHandleRef)
{
    return false;
}

bool WKBundleNodeHandleGetHTMLTextAreaElementLastChangeWasUserEdit(WKBundleNodeHandleRef)
{
    return false;
}

WKBundleNodeHandleRef WKBundleNodeHandleCopyHTMLTableCellElementCellAbove(WKBundleNodeHandleRef)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

WKBundleFrameRef WKBundleNodeHandleCopyDocumentFrame(WKBundleNodeHandleRef)
{
    return nullptr;
}

WKBundleFrameRef WKBundleNodeHandleCopyHTMLFrameElementContentFrame(WKBundleNodeHandleRef)
{
    return nullptr;
}

WKBundleFrameRef WKBundleNodeHandleCopyHTMLIFrameElementContentFrame(WKBundleNodeHandleRef htmlIFrameElementHandleRef)
{
    RefPtr<WebKit::WebFrame> frame = protect(WebKit::toImpl(htmlIFrameElementHandleRef))->htmlIFrameElementContentFrame();
    return toAPILeakingRef(WTF::move(frame));
}

bool WKBundleNodeHandleGetHTMLInputElementAutofilled(WKBundleNodeHandleRef htmlInputElementHandleRef)
{
    ASSERT_NOT_REACHED();
    return false;
}

void WKBundleNodeHandleSetHTMLInputElementAutofilled(WKBundleNodeHandleRef, bool)
{
}

void WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabled(WKBundleNodeHandleRef, bool)
{
    // FIXME: Would put ASSERT_NOT_REACHED() here but some compilers are warning the function is "noreturn".
}

WKBundleFrameRef WKBundleNodeHandleCopyOwningDocumentFrame(WKBundleNodeHandleRef documentHandleRef)
{
    if (RefPtr document = protect(WebKit::toImpl(documentHandleRef))->document())
        return toAPILeakingRef(document->documentFrame());
    return nullptr;
}
