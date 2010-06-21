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

#include <Cocoa/Cocoa.h>
#include <WebKit2/WKBundle.h>
#include <WebKit2/WKBundleInitialize.h>
#include <WebKit2/WKBundlePage.h>
#include <WebKit2/WKString.h>
#include <WebKit2/WKStringCF.h>
#include <WebKit2/WKURLCF.h>
#include <stdio.h>

static WKBundleRef globalBundle;

void _didClearWindow(WKBundlePageRef page, WKBundleFrameRef frame, JSContextRef ctx, JSObjectRef window, const void *clientInfo)
{
    CFURLRef cfURL = WKURLCopyCFURL(0, WKBundlePageGetMainFrameURL(page));
    NSLog(@"WKBundlePageClient - _didClearWindowForFrame %@", [(NSURL *)cfURL absoluteString]);
    CFRelease(cfURL);

    WKStringRef message = WKStringCreateWithCFString(CFSTR("Window was cleared"));
    WKBundlePostMessage(globalBundle, message);
    WKStringRelease(message);
}

void _didCreatePage(WKBundlePageRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    NSLog(@"WKBundleClient - didCreatePage\n");

    WKBundlePageClient client = {
        0,
        0,
        _didClearWindow
    };
    WKBundlePageSetClient(page, &client);
}

void _didRecieveMessage(WKBundleRef bundle, WKStringRef message, const void *clientInfo)
{
    CFStringRef cfMessage = WKStringCopyCFString(0, message);
    NSLog(@"WKBundleClient - didRecieveMessage %@\n", cfMessage);
    CFRelease(cfMessage);
}

void WKBundleInitialize(WKBundleRef bundle)
{
    globalBundle = bundle;

    WKBundleClient client = {
        0,
        0,
        _didCreatePage,
        _didRecieveMessage
    };
    WKBundleSetClient(bundle, &client);
}
