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

#import "config.h"

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import <WebKit/WKBundlePage.h>
#import <WebKit/WKBundlePageUIClient.h>
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKStringCF.h>
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <wtf/StdLibExtras.h>

static NSArray<NSString *> *stringsArrayFromWKArrayRef(WKArrayRef wkArray)
{
    size_t itemCount = WKArrayGetSize(wkArray);
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:itemCount];
    for (size_t i = 0; i < itemCount; ++i)
        [array addObject:TestWebKitAPI::Util::toNS((WKStringRef)WKArrayGetItemAtIndex(wkArray, i))];

    return array;
}

static void willAddMessageWithDetailsToConsoleCallback(WKBundlePageRef page, WKStringRef message, WKArrayRef messageArguments, uint32_t lineNumber, uint32_t columnNumber, WKStringRef sourceID, const void *)
{
    WKRetainPtr<WKStringRef> messageName = adoptWK(WKStringCreateWithUTF8CString("ConsoleMessage"));

    NSString *nsMessage = message ? TestWebKitAPI::Util::toNS(message) : @"";
    NSArray<NSString *> *nsMessageArguments = stringsArrayFromWKArrayRef(messageArguments);
    if (nsMessageArguments.count) {
        nsMessage = [nsMessage stringByAppendingString:@", arguments: "];
        nsMessage = [nsMessage stringByAppendingString:[nsMessageArguments componentsJoinedByString:@", "]];
    }

    WKRetainPtr<WKStringRef> fullMessage = adoptWK(WKStringCreateWithCFString((__bridge CFStringRef)nsMessage));

    WKBundlePagePostMessage(page, messageName.get(), fullMessage.get());
}

@interface BundlePageConsoleMessageWithDetails : NSObject <WKWebProcessPlugIn>
@end

@implementation BundlePageConsoleMessageWithDetails

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    WKBundlePageUIClientV5 client;
    zeroBytes(client);
    client.base.version = 5;
    client.willAddMessageWithDetailsToConsole = willAddMessageWithDetailsToConsoleCallback;
    WKBundlePageSetUIClient([browserContextController _bundlePageRef], &client.base);
}

@end

#endif // PLATFORM(MAC)
