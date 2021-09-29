/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(INSPECTOR_EXTENSIONS)

#import "APISerializedScriptValue.h"
#import "InspectorExtensionTypes.h"
#import "WKError.h"
#import "WKWebViewInternal.h"
#import "_WKInspectorExtensionInternal.h"
#import "_WKInspectorExtensionPrivateForTesting.h"
#import <WebCore/ExceptionDetails.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/BlockPtr.h>

// This file exists to centralize all fragile code that is used by _WKInspectorExtension API tests.

@implementation _WKInspectorExtension (WKTesting)

- (void)_evaluateScript:(NSString *)scriptSource inExtensionTabWithIdentifier:(NSString *)extensionTabIdentifier completionHandler:(void(^)(NSError *, NSDictionary *))completionHandler
{
    _extension->evaluateScriptInExtensionTab(extensionTabIdentifier, scriptSource, [protectedSelf = retainPtr(self), capturedBlock = makeBlockPtr(WTFMove(completionHandler))] (Inspector::ExtensionEvaluationResult&& result) mutable {
        if (!result) {
            capturedBlock([NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(result.error()) }], nil);
            return;
        }
        
        auto valueOrException = result.value();
        if (!valueOrException) {
            capturedBlock(nsErrorFromExceptionDetails(valueOrException.error()).get(), nil);
            return;
        }

        id body = API::SerializedScriptValue::deserialize(valueOrException.value()->internalRepresentation(), 0);
        capturedBlock(nil, body);
    });
}

@end

#endif // ENABLE(INSPECTOR_EXTENSIONS)
