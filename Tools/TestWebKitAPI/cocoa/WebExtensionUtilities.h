/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "TestCocoa.h"
#include "Utilities.h"
#include "WTFTestUtilities.h"
#include <WebKit/_WKWebExtensionContextPrivate.h>
#include <WebKit/_WKWebExtensionControllerDelegatePrivate.h>
#include <WebKit/_WKWebExtensionControllerPrivate.h>
#include <WebKit/_WKWebExtensionPrivate.h>

#ifdef __OBJC__

@interface TestWebExtensionManager : NSObject

- (instancetype)initWithExtension:(_WKWebExtension *)extension;

@property (nonatomic, strong) _WKWebExtension *extension;
@property (nonatomic, strong) _WKWebExtensionContext *context;
@property (nonatomic, strong) _WKWebExtensionController *controller;
@property (nonatomic, weak) id <_WKWebExtensionControllerDelegate> controllerDelegate;

@property (nonatomic, readonly, strong) NSString *yieldMessage;

- (void)load;
- (void)run;
- (void)loadAndRun;

@end

#else // not __OBJC__

OBJC_CLASS TestWebExtensionManager;

#endif // __OBJC__

namespace TestWebKitAPI::Util {

#ifdef __OBJC__

inline NSString *constructScript(NSArray *lines) { return [lines componentsJoinedByString:@";\n"]; }

#endif

RetainPtr<TestWebExtensionManager> loadAndRunExtension(_WKWebExtension *);
RetainPtr<TestWebExtensionManager> loadAndRunExtension(NSDictionary *manifest, NSDictionary *resources);
RetainPtr<TestWebExtensionManager> loadAndRunExtension(NSDictionary *resources);
RetainPtr<TestWebExtensionManager> loadAndRunExtension(NSURL *baseURL);

} // namespace TestWebKitAPI::Util
