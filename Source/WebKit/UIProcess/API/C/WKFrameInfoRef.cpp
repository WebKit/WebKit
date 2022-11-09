/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "WKFrameInfoRef.h"

#include "APIFrameHandle.h"
#include "APIFrameInfo.h"
#include "APISecurityOrigin.h"
#include "WKAPICast.h"

WKTypeID WKFrameInfoGetTypeID()
{
    return WebKit::toAPI(API::FrameInfo::APIType);
}

WKFrameHandleRef WKFrameInfoCreateFrameHandleRef(WKFrameInfoRef frameInfo)
{
    return WebKit::toAPI(&WebKit::toImpl(frameInfo)->handle().leakRef());
}

WKSecurityOriginRef WKFrameInfoCopySecurityOrigin(WKFrameInfoRef frameInfo)
{
    auto origin = WebKit::toImpl(frameInfo)->securityOrigin();
    return WebKit::toAPI(&API::SecurityOrigin::create(origin.protocol, origin.host, origin.port).leakRef());
}

bool WKFrameInfoGetIsMainFrame(WKFrameInfoRef frameInfo)
{
    return WebKit::toImpl(frameInfo)->isMainFrame();
}
