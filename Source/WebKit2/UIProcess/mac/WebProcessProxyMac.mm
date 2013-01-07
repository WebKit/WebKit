/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import "WebProcessProxy.h"

#import "SecItemRequestData.h"
#import "SecItemResponseData.h"
#import "WebProcessMessages.h"
#import "WKFullKeyboardAccessWatcher.h"
#import <Security/SecItem.h>

namespace WebKit {

static void handleSecItemRequest(CoreIPC::Connection* connection, uint64_t requestID, const SecItemRequestData& request)
{
    SecItemResponseData response;

    switch (request.type()) {
        case SecItemRequestData::Invalid:
            ASSERT_NOT_REACHED();
            return;

        case SecItemRequestData::CopyMatching: {
            CFTypeRef resultObject = 0;
            OSStatus resultCode = SecItemCopyMatching(request.query(), &resultObject);
            response = SecItemResponseData(resultCode, adoptCF(resultObject).get());
            break;
        }

        case SecItemRequestData::Add: {
            CFTypeRef resultObject = 0;
            OSStatus resultCode = SecItemAdd(request.query(), &resultObject);
            response = SecItemResponseData(resultCode, adoptCF(resultObject).get());
            break;
        }

        case SecItemRequestData::Update: {
            OSStatus resultCode = SecItemUpdate(request.query(), request.attributesToMatch());
            response = SecItemResponseData(resultCode, 0);
            break;
        }

        case SecItemRequestData::Delete: {
            OSStatus resultCode = SecItemDelete(request.query());
            response = SecItemResponseData(resultCode, 0);
            break;
        }
    }

    connection->send(Messages::WebProcess::SecItemResponse(requestID, response), 0);
}

static void dispatchFunctionOnQueue(dispatch_queue_t queue, const Function<void ()>& function)
{
#if COMPILER(CLANG)
    dispatch_async(queue, function);
#else
    Function<void ()>* functionPtr = new Function<void ()>(function);
    dispatch_async(queue, ^{
        (*functionPtr)();
        delete functionPtr;
    });
#endif
}

void WebProcessProxy::secItemRequest(CoreIPC::Connection* connection, uint64_t requestID, const SecItemRequestData& request)
{
    // Since we don't want the connection work queue to be held up, we do all
    // keychain interaction work on a global dispatch queue.
    dispatch_queue_t keychainWorkQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatchFunctionOnQueue(keychainWorkQueue, bind(handleSecItemRequest, RefPtr<CoreIPC::Connection>(connection), requestID, request));
}

bool WebProcessProxy::fullKeyboardAccessEnabled()
{
    return [WKFullKeyboardAccessWatcher fullKeyboardAccessEnabled];
}

#if HAVE(XPC)
static bool shouldUseXPC()
{
    if (id value = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKit2UseXPCServiceForWebProcess"])
        return [value boolValue];

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    return true;
#else
    return false;
#endif
}
#endif

void WebProcessProxy::platformGetLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    // We want the web process to match the architecture of the UI process.
    launchOptions.architecture = ProcessLauncher::LaunchOptions::MatchCurrentArchitecture;
    launchOptions.executableHeap = false;

#if HAVE(XPC)
    launchOptions.useXPC = shouldUseXPC();
#endif
}

} // namespace WebKit
