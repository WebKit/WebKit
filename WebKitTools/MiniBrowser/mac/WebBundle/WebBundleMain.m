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
#include <WebKit2/WKBundleFrame.h>
#include <WebKit2/WKBundleInitialize.h>
#include <WebKit2/WKBundlePage.h>
#include <WebKit2/WKString.h>
#include <WebKit2/WKStringCF.h>
#include <WebKit2/WKURLCF.h>
#include <stdio.h>

static WKBundleRef globalBundle;

// WKBundlePageClient

void _didStartProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo)
{
}

void _didReceiveServerRedirectForProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo)
{
}

void _didFailProvisionalLoadWithErrorForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo)
{
}

void _didCommitLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo)
{
}

void _didFinishLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo)
{
}

void _didFailLoadWithErrorForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void *clientInfo)
{
}

void _didReceiveTitleForFrame(WKBundlePageRef page, WKStringRef title, WKBundleFrameRef frame, const void *clientInfo)
{
}

void _didClearWindowForFrame(WKBundlePageRef page, WKBundleFrameRef frame, JSContextRef ctx, JSObjectRef window, const void *clientInfo)
{
    CFURLRef cfURL = WKURLCopyCFURL(0, WKBundleFrameGetURL(WKBundlePageGetMainFrame(page)));
    LOG(@"WKBundlePageClient - _didClearWindowForFrame %@", [(NSURL *)cfURL absoluteString]);
    CFRelease(cfURL);

    WKStringRef message = WKStringCreateWithCFString(CFSTR("Window was cleared"));
    WKBundlePostMessage(globalBundle, message);
    WKStringRelease(message);
}


// WKBundleClient

void _didCreatePage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    LOG(@"WKBundleClient - didCreatePage\n");

    WKBundlePageLoaderClient client = {
        0,
        0,
        _didStartProvisionalLoadForFrame,
        _didReceiveServerRedirectForProvisionalLoadForFrame,
        _didFailProvisionalLoadWithErrorForFrame,
        _didCommitLoadForFrame,
        _didFinishLoadForFrame,
        _didFailLoadWithErrorForFrame,
        _didReceiveTitleForFrame,
        _didClearWindowForFrame
    };
    WKBundlePageSetLoaderClient(page, &client);
}

void _willDestroyPage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    LOG(@"WKBundleClient - willDestroyPage\n");
}

void _didRecieveMessage(WKBundleRef bundle, WKStringRef message, const void *clientInfo)
{
    CFStringRef cfMessage = WKStringCopyCFString(0, message);
    LOG(@"WKBundleClient - didRecieveMessage %@\n", cfMessage);
    CFRelease(cfMessage);
}

void WKBundleInitialize(WKBundleRef bundle)
{
    globalBundle = bundle;

    WKBundleClient client = {
        0,
        0,
        _didCreatePage,
        _willDestroyPage,
        _didRecieveMessage
    };
    WKBundleSetClient(bundle, &client);
}
