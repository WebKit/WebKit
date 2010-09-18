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

#include "WKFrame.h"

#include "WKAPICast.h"
#include "WebFrameProxy.h"

using namespace WebKit;

WKTypeID WKFrameGetTypeID()
{
    return toRef(WebFrameProxy::APIType);
}

bool WKFrameIsMainFrame(WKFrameRef frameRef)
{
    return toWK(frameRef)->isMainFrame();
}

WKFrameLoadState WKFrameGetFrameLoadState(WKFrameRef frameRef)
{
    WebFrameProxy* frame = toWK(frameRef);
    switch (frame->loadState()) {
        case WebFrameProxy::LoadStateProvisional:
            return kWKFrameLoadStateProvisional;
        case WebFrameProxy::LoadStateCommitted:
            return kWKFrameLoadStateCommitted;
        case WebFrameProxy::LoadStateFinished:
            return kWKFrameLoadStateFinished;
    }
    
    ASSERT_NOT_REACHED();
    return kWKFrameLoadStateFinished;
}

WKURLRef WKFrameCopyProvisionalURL(WKFrameRef frameRef)
{
    return toCopiedURLRef(toWK(frameRef)->provisionalURL());
}

WKURLRef WKFrameCopyURL(WKFrameRef frameRef)
{
    return toCopiedURLRef(toWK(frameRef)->url());
}

WKPageRef WKFrameGetPage(WKFrameRef frameRef)
{
    return toRef(toWK(frameRef)->page());
}

WKCertificateInfoRef WKFrameGetCertificateInfo(WKFrameRef frameRef)
{
    return toRef(toWK(frameRef)->certificateInfo());
}
