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

#import "PageClientImpl.h"

#import "WKAPICast.h"
#import "WKStringCF.h"
#import "WKViewInternal.h"
#import <WebCore/Cursor.h>
#import <WebCore/FoundationExtras.h>
#import <wtf/PassOwnPtr.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

NSString* nsStringFromWebCoreString(const WTF::String& string)
{
    return string.impl() ? HardAutorelease(WKStringCopyCFString(0, toRef(string.impl()))) : @"";
}

PassOwnPtr<PageClientImpl> PageClientImpl::create(WKView* wkView)
{
    return adoptPtr(new PageClientImpl(wkView));
}

PageClientImpl::PageClientImpl(WKView* wkView)
    : m_wkView(wkView)
{
}

PageClientImpl::~PageClientImpl()
{
}

void PageClientImpl::processDidExit()
{
    [m_wkView _processDidExit];
}

void PageClientImpl::processDidRevive()
{
    [m_wkView _processDidRevive];
}

void PageClientImpl::takeFocus(bool direction)
{
    [m_wkView _takeFocus:direction];
}

void PageClientImpl::toolTipChanged(const String& oldToolTip, const String& newToolTip)
{
    [m_wkView _toolTipChangedFrom:nsStringFromWebCoreString(oldToolTip) to:nsStringFromWebCoreString(newToolTip)];
}

void PageClientImpl::setCursor(const WebCore::Cursor& cursor)
{
    [m_wkView _setCursor:cursor.platformCursor()];
}

#if USE(ACCELERATED_COMPOSITING)
void PageClientImpl::pageDidEnterAcceleratedCompositing()
{
    [m_wkView _pageDidEnterAcceleratedCompositing];
}

void PageClientImpl::pageDidLeaveAcceleratedCompositing()
{
    [m_wkView _pageDidLeaveAcceleratedCompositing];
}
#endif // USE(ACCELERATED_COMPOSITING)

} // namespace WebKit
