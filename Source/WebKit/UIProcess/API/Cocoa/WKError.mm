/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "WKErrorInternal.h"

#import <WebCore/LocalizedStrings.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

NSString * const WKErrorDomain = @"WKErrorDomain";
NSString * const _WKLegacyErrorDomain = @"WebKitErrorDomain";

NSString * const _WKJavaScriptExceptionMessageErrorKey = @"WKJavaScriptExceptionMessage";
NSString * const _WKJavaScriptExceptionLineNumberErrorKey = @"WKJavaScriptExceptionLineNumber";
NSString * const _WKJavaScriptExceptionColumnNumberErrorKey = @"WKJavaScriptExceptionColumnNumber";
NSString * const _WKJavaScriptExceptionSourceURLErrorKey = @"WKJavaScriptExceptionSourceURL";

NSString *localizedDescriptionForErrorCode(WKErrorCode errorCode)
{
    switch (errorCode) {
    case WKErrorUnknown:
        return WEB_UI_STRING("An unknown error occurred", "WKErrorUnknown description");

    case WKErrorWebContentProcessTerminated:
        return WEB_UI_STRING("The Web Content process was terminated", "WKErrorWebContentProcessTerminated description");

    case WKErrorWebViewInvalidated:
        return WEB_UI_STRING("The WKWebView was invalidated", "WKErrorWebViewInvalidated description");

    case WKErrorJavaScriptExceptionOccurred:
        return WEB_UI_STRING("A JavaScript exception occurred", "WKErrorJavaScriptExceptionOccurred description");

    case WKErrorJavaScriptResultTypeIsUnsupported:
        return WEB_UI_STRING("JavaScript execution returned a result of an unsupported type", "WKErrorJavaScriptResultTypeIsUnsupported description");

    case WKErrorContentRuleListStoreLookUpFailed:
        return WEB_UI_STRING("Looking up a WKContentRuleList failed", "WKErrorContentRuleListStoreLookupFailed description");

    case WKErrorContentRuleListStoreVersionMismatch:
        return WEB_UI_STRING("Looking up a WKContentRuleList found a binary that is incompatible", "WKErrorContentRuleListStoreVersionMismatch description");

    case WKErrorContentRuleListStoreCompileFailed:
        return WEB_UI_STRING("Compiling a WKContentRuleList failed", "WKErrorContentRuleListStoreCompileFailed description");

    case WKErrorContentRuleListStoreRemoveFailed:
        return WEB_UI_STRING("Removing a WKContentRuleList failed", "WKErrorContentRuleListStoreRemoveFailed description");

    case WKErrorAttributedStringContentFailedToLoad:
        return WEB_UI_STRING("Attributed string content failed to load", "WKErrorAttributedStringContentFailedToLoad description");

    case WKErrorAttributedStringContentLoadTimedOut:
        return WEB_UI_STRING("Timed out while loading attributed string content", "WKErrorAttributedStringContentLoadTimedOut description");
    }
}

RetainPtr<NSError> createNSError(WKErrorCode errorCode, NSError* underlyingError)
{
    NSDictionary *userInfo = nil;
    if (underlyingError)
        userInfo = @{ NSLocalizedDescriptionKey: localizedDescriptionForErrorCode(errorCode), NSUnderlyingErrorKey: underlyingError };
    else
        userInfo = @{ NSLocalizedDescriptionKey: localizedDescriptionForErrorCode(errorCode) };

    return adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:errorCode userInfo:userInfo]);
}
