/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionAPIRuntime.h"

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

void WebExtensionAPIRuntimeBase::reportErrorForCallbackHandler(WebExtensionCallbackHandler& callback, NSString *errorMessage, JSGlobalContextRef contextRef)
{
    ASSERT(errorMessage.length);
    ASSERT(contextRef);

    JSContext *context = [JSContext contextWithJSGlobalContextRef:contextRef];

    m_lastErrorAccessed = false;
    m_lastError = [JSValue valueWithNewErrorFromMessage:errorMessage inContext:context];

    callback.call();

    if (!m_lastErrorAccessed) {
        // Log the error to the console if it wasn't checked in the callback.
        JSValue *consoleErrorFunction = context.globalObject[@"console"][@"error"];
        [consoleErrorFunction callWithArguments:@[ [JSValue valueWithNewErrorFromMessage:[NSString stringWithFormat:@"Unchecked runtime.lastError: %@", errorMessage] inContext:context] ]];
    }

    m_lastErrorAccessed = false;
    m_lastError = nil;
}

NSURL *WebExtensionAPIRuntime::getURL(NSString *resourcePath, NSString **errorString)
{
    URL baseURL = extensionContext().baseURL();
    return resourcePath.length ? URL { baseURL, resourcePath } : baseURL;
}

NSDictionary *WebExtensionAPIRuntime::getManifest()
{
    return extensionContext().manifest();
}

NSString *WebExtensionAPIRuntime::runtimeIdentifier()
{
    return extensionContext().uniqueIdentifier();
}

void WebExtensionAPIRuntime::getPlatformInfo(Ref<WebExtensionCallbackHandler>&& callback)
{
#if PLATFORM(MAC)
    static NSString * const osValue = @"mac";
#elif PLATFORM(IOS_FAMILY)
    static NSString * const osValue = @"ios";
#else
    static NSString * const osValue = @"unknown";
#endif

#if CPU(X86_64)
    static NSString * const archValue = @"x86-64";
#elif CPU(ARM) || CPU(ARM64)
    static NSString * const archValue = @"arm";
#else
    static NSString * const archValue = @"unknown";
#endif

    static NSDictionary *platformInfo = @{
        @"os": osValue,
        @"arch": archValue
    };

    callback->call(platformInfo);
}

JSValueRef WebExtensionAPIRuntime::lastError()
{
    m_lastErrorAccessed = true;

    return m_lastError.get().JSValueRef;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
