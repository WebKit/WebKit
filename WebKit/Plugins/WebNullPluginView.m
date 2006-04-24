/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebNullPluginView.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebViewInternal.h>

static NSImage *image = nil;

@implementation WebNullPluginView

- initWithFrame:(NSRect)frame error:(NSError *)pluginError;
{    
    self = [super initWithFrame:frame];
    if (self) {

        if (!image) {
            NSBundle *bundle = [NSBundle bundleForClass:[WebNullPluginView class]];
            NSString *imagePath = [bundle pathForResource:@"nullplugin" ofType:@"tiff"];
            image = [[NSImage alloc] initWithContentsOfFile:imagePath];
        }
        
        [self setImage:image];

        error = [pluginError retain];
    }
    return self;
}

- (void)dealloc
{
    [error release];
    [super dealloc];
}

- (void)setWebFrame:(WebFrame *)webFrame
{
    _webFrame = webFrame;
}

- (void)viewDidMoveToWindow
{
    if(!didSendError && _window && error){
        didSendError = YES;
        WebFrame *webFrame = _webFrame;
        WebView *webView = [webFrame webView];
        WebDataSource *dataSource = [webFrame dataSource];
        
        [[webView _resourceLoadDelegateForwarder] webView:webView plugInFailedWithError:error dataSource:dataSource];
    }
}

@end
