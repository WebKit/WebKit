/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "WKError.h"
#import <wtf/BlockPtr.h>
#import <wtf/URL.h>

#if ENABLE(INSPECTOR_EXTENSIONS)

@implementation _WKInspectorExtension

- (void)dealloc
{
    _extension->API::InspectorExtension::~InspectorExtension();

    [super dealloc];
}

- (API::Object&)_apiObject
{
    return *_extension;
}

// MARK: API

- (void)createTabWithName:(NSString *)tabName tabIconURL:(NSURL *)tabIconURL sourceURL:(NSURL *)sourceURL completionHandler:(void(^)(NSError *, NSString *))completionHandler
{
    _extension->createTab(tabName, tabIconURL, sourceURL, [protectedSelf = retainPtr(self), capturedBlock = makeBlockPtr(completionHandler)] (Expected<WebKit::InspectorExtensionTabID, WebKit::InspectorExtensionError> result) mutable {
        if (!result) {
            capturedBlock([NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: inspectorExtensionErrorToString(result.error())}], nil);
            return;
        }

        capturedBlock(nil, result.value());
    });
}

// MARK: Properties.

- (NSString *)extensionID
{
    return _extension->identifier();
}

@end

#endif // ENABLE(INSPECTOR_EXTENSIONS)
