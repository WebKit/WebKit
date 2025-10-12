/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#import "_WKInspectorExtensionInternal.h"

#if ENABLE(INSPECTOR_EXTENSIONS)

#import "InspectorExtensionDelegate.h"
#import "InspectorExtensionTypes.h"
#import "JavaScriptEvaluationResult.h"
#import "WKError.h"
#import "WKWebViewInternal.h"
#import <WebCore/ExceptionDetails.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/BlockPtr.h>
#import <wtf/URL.h>

@implementation _WKInspectorExtension

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKInspectorExtension.class, self))
        return;

    _extension->API::InspectorExtension::~InspectorExtension();

    [super dealloc];
}

- (API::Object&)_apiObject
{
    return *_extension;
}

- (id <_WKInspectorExtensionDelegate>)delegate
{
    return _delegate->delegate().autorelease();
}

- (void)setDelegate:(id<_WKInspectorExtensionDelegate>)delegate
{
    if (!delegate && !_delegate)
        return;

    _delegate = makeUnique<WebKit::InspectorExtensionDelegate>(self, delegate);
}

// MARK: API

- (void)createTabWithName:(NSString *)tabName tabIconURL:(NSURL *)tabIconURL sourceURL:(NSURL *)sourceURL completionHandler:(void(^)(NSError *, NSString *))completionHandler
{
    _extension->createTab(tabName, { tabIconURL.baseURL, tabIconURL.relativeString }, { tabIconURL.baseURL, sourceURL.relativeString }, [protectedSelf = retainPtr(self), capturedBlock = makeBlockPtr(completionHandler)] (Expected<Inspector::ExtensionTabID, Inspector::ExtensionError> result) mutable {
        if (!result) {
            capturedBlock(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(result.error()).createNSString().get() }]).get(), nil);
            return;
        }

        capturedBlock(nil, result.value().createNSString().get());
    });
}

- (void)evaluateScript:(NSString *)scriptSource frameURL:(NSURL *)frameURL contextSecurityOrigin:(NSURL *)contextSecurityOrigin useContentScriptContext:(BOOL)useContentScriptContext completionHandler:(void(^)(NSError *, id))completionHandler
{
    std::optional<URL> optionalFrameURL = frameURL ? std::make_optional(URL(frameURL)) : std::nullopt;
    std::optional<URL> optionalContextSecurityOrigin = contextSecurityOrigin ? std::make_optional(URL(contextSecurityOrigin)) : std::nullopt;
    _extension->evaluateScript(scriptSource, optionalFrameURL, optionalContextSecurityOrigin, useContentScriptContext, [protectedSelf = retainPtr(self), capturedBlock = makeBlockPtr(WTFMove(completionHandler))] (Inspector::ExtensionEvaluationResult&& result) mutable {
        if (!result) {
            capturedBlock(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(result.error()).createNSString().get() }]).get(), nil);
            return;
        }
        
        auto& valueOrException = result.value();
        if (!valueOrException) {
            capturedBlock(nsErrorFromExceptionDetails(valueOrException.error()).get(), nil);
            return;
        }

        capturedBlock(nil, valueOrException->toID().get());
    });
}

- (void)evaluateScript:(NSString *)scriptSource inTabWithIdentifier:(NSString *)extensionTabIdentifier completionHandler:(void(^)(NSError *, id))completionHandler
{
    _extension->evaluateScriptInExtensionTab(extensionTabIdentifier, scriptSource, [protectedSelf = retainPtr(self), capturedBlock = makeBlockPtr(WTFMove(completionHandler))] (Inspector::ExtensionEvaluationResult&& result) mutable {
        if (!result) {
            capturedBlock(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(result.error()).createNSString().get() }]).get(), nil);
            return;
        }
        
        auto& valueOrException = result.value();
        if (!valueOrException) {
            capturedBlock(nsErrorFromExceptionDetails(valueOrException.error()).get(), nil);
            return;
        }

        capturedBlock(nil, valueOrException->toID().get());
    });
}

- (void)navigateToURL:(NSURL *)url inTabWithIdentifier:(NSString *)extensionTabIdentifier completionHandler:(void(^)(NSError *))completionHandler
{
    _extension->navigateTab(extensionTabIdentifier, url, [protectedSelf = retainPtr(self), capturedBlock = makeBlockPtr(WTFMove(completionHandler))] (const std::optional<Inspector::ExtensionError> result) mutable {
        if (result) {
            capturedBlock(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(result.value()).createNSString().get() }]).get());
            return;
        }

        capturedBlock(nil);
    });
}

- (void)reloadIgnoringCache:(BOOL)ignoreCache userAgent:(NSString *)userAgent injectedScript:(NSString *)injectedScript completionHandler:(void(^)(NSError *))completionHandler
{
    std::optional<String> optionalUserAgent = userAgent ? std::make_optional(String(userAgent)) : std::nullopt;
    std::optional<String> optionalInjectedScript = injectedScript ? std::make_optional(String(injectedScript)) : std::nullopt;
    _extension->reloadIgnoringCache(ignoreCache, optionalUserAgent, optionalInjectedScript, [protectedSelf = retainPtr(self), capturedBlock = makeBlockPtr(WTFMove(completionHandler))] (Inspector::ExtensionVoidResult&& result) mutable {
        if (!result) {
            capturedBlock(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(result.error()).createNSString().get() }]).get());
            return;
        }

        capturedBlock(nil);
    });
}

// MARK: Properties.

- (NSString *)extensionID
{
    return _extension->identifier().createNSString().autorelease();
}

@end

#endif // ENABLE(INSPECTOR_EXTENSIONS)
