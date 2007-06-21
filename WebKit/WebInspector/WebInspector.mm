/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebInspector.h"
#import "WebFrameInternal.h"
#import "WebView.h"

#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/InspectorController.h>
#include <WebCore/Page.h>

using namespace WebCore;

@implementation WebInspector

// This method name is used by the Safari 3 beta
+ (WebInspector *)sharedWebInspector
{
    return [self webInspector];
}

+ (WebInspector *)webInspector
{
    return [[[WebInspector alloc] init] autorelease];
}

- (void)dealloc
{
    [_webView release];
    [super dealloc];
}

- (void)setWebFrame:(WebFrame *)frame
{
    [_webView release];
    _webView = [[frame webView] retain];
}

- (void)showWindow:(id)sender
{
    if (Page* page = core(_webView))
        if (InspectorController* inspectorController = page->inspectorController())
            inspectorController->inspect(page->mainFrame()->document());
}
@end
