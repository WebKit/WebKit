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
#include "WKErrorRef.h"

#include "APIError.h"
#include "WKAPICast.h"

WKTypeID WKErrorGetTypeID()
{
    return WebKit::toAPI(API::Error::APIType);
}

WKStringRef WKErrorCopyWKErrorDomain()
{
    return WebKit::toCopiedAPI(API::Error::webKitErrorDomain());
}

WKStringRef WKErrorCopyDomain(WKErrorRef errorRef)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(errorRef)->domain());
}

int WKErrorGetErrorCode(WKErrorRef errorRef)
{
    auto errorCode = WebKit::toImpl(errorRef)->errorCode();
    switch (errorCode) {
    case API::Error::Policy::CannotShowMIMEType:
        return kWKErrorCodeCannotShowMIMEType;
    case API::Error::Policy::CannotShowURL:
        return kWKErrorCodeCannotShowURL;
    case API::Error::Policy::FrameLoadInterruptedByPolicyChange:
        return kWKErrorCodeFrameLoadInterruptedByPolicyChange;
    case API::Error::Policy::CannotUseRestrictedPort:
        return kWKErrorCodeCannotUseRestrictedPort;
    case API::Error::Policy::FrameLoadBlockedByContentBlocker:
        return kWKErrorCodeFrameLoadBlockedByContentBlocker;
    case API::Error::Policy::FrameLoadBlockedByContentFilter:
        return kWKErrorCodeFrameLoadBlockedByContentFilter;
    case API::Error::Plugin::CannotFindPlugIn:
        return kWKErrorCodeCannotFindPlugIn;
    case API::Error::Plugin::CannotLoadPlugIn:
        return kWKErrorCodeCannotLoadPlugIn;
    case API::Error::Plugin::JavaUnavailable:
        return kWKErrorCodeJavaUnavailable;
    case API::Error::Plugin::PlugInCancelledConnection:
        return kWKErrorCodePlugInCancelledConnection;
    case API::Error::Plugin::PlugInWillHandleLoad:
        return kWKErrorCodePlugInWillHandleLoad;
    case API::Error::Plugin::InsecurePlugInVersion:
        return kWKErrorCodeInsecurePlugInVersion;
    case API::Error::General::Internal:
        return kWKErrorInternal;
    }

    return errorCode;
}

WKURLRef WKErrorCopyFailingURL(WKErrorRef errorRef)
{
    return WebKit::toCopiedURLAPI(WebKit::toImpl(errorRef)->failingURL());
}

WKStringRef WKErrorCopyLocalizedDescription(WKErrorRef errorRef)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(errorRef)->localizedDescription());
}
