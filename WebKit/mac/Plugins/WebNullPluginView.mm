/*
 * Copyright (C) 2005, 2007 Apple Inc. All rights reserved.
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

#import "WebNullPluginView.h"

#import "DOMElementInternal.h"
#import "WebDelegateImplementationCaching.h"
#import "WebFrameInternal.h"
#import "WebViewInternal.h"
#import <WebCore/Document.h>
#import <WebCore/Element.h>

@implementation WebNullPluginView

- initWithFrame:(NSRect)frame error:(NSError *)err DOMElement:(DOMElement *)elem
{    
    static NSImage *nullPlugInImage;
    if (!nullPlugInImage) {
        NSBundle *bundle = [NSBundle bundleForClass:[WebNullPluginView class]];
        NSString *imagePath = [bundle pathForResource:@"nullplugin" ofType:@"tiff"];
        nullPlugInImage = [[NSImage alloc] initWithContentsOfFile:imagePath];
    }
    
    self = [super initWithFrame:frame];
    if (!self)
        return nil;

    error = [err retain];
    if (err)
        element = [elem retain];

    [self setImage:nullPlugInImage];

    return self;
}

- (void)dealloc

{
    [error release];
    [element release];
    [super dealloc];
}

- (void)reportFailure
{
    NSError *localError = error;
    DOMElement *localElement = element;

    error = nil;
    element = nil;

    WebFrame *webFrame = kit(core(localElement)->document()->frame());
    if (webFrame) {
        WebView *webView = [webFrame webView];
        WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView);
        if (implementations->plugInFailedWithErrorFunc)
            CallResourceLoadDelegate(implementations->plugInFailedWithErrorFunc, webView,
                @selector(webView:plugInFailedWithError:dataSource:), localError, [webFrame _dataSource]);
    }

    [localError release];
    [localElement release];
}

- (void)viewDidMoveToWindow
{
    if (!error)
        return;

    if ([self window])
        [self reportFailure];
}

@end
