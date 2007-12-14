/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import <WebCore/WebCoreFrameBridge.h>

namespace WebCore {
    class Page;
}

@class WebFrame;
@class WebFrameView;

@protocol WebOpenPanelResultListener;

@interface WebFrameBridge : WebCoreFrameBridge <WebCoreFrameBridge>
{
@public
    WebFrame *_frame;

@private
    WebCore::KeyboardUIMode _keyboardUIMode;
    BOOL _keyboardUIModeAccessed;
    BOOL _doingClientRedirect;
    BOOL _haveUndoRedoOperations;
    
    NSDictionary *lastDashboardRegions;
}

- (id)initMainFrameWithPage:(WebCore::Page*)page frameName:(NSString *)name frameView:(WebFrameView *)frameView;
- (void)close;

- (WebFrame *)webFrame;

// The following methods can all move off the bridge; they're used on the WebKit side only.

- (NSView *)viewForPluginWithFrame:(NSRect)frame
                               URL:(NSURL *)URL
                    attributeNames:(NSArray *)attributeNames
                   attributeValues:(NSArray *)attributeValues
                          MIMEType:(NSString *)MIMEType
                        DOMElement:(DOMElement *)element
                      loadManually:(BOOL)loadManually;
- (NSView *)viewForJavaAppletWithFrame:(NSRect)frame
                        attributeNames:(NSArray *)attributeNames
                       attributeValues:(NSArray *)attributeValues
                               baseURL:(NSURL *)baseURL
                            DOMElement:(DOMElement *)element;

- (WebCore::Frame*)createChildFrameNamed:(NSString *)frameName withURL:(NSURL *)URL referrer:(const WebCore::String&)referrer
    ownerElement:(WebCore::HTMLFrameOwnerElement *)ownerElement allowsScrolling:(BOOL)allowsScrolling marginWidth:(int)width marginHeight:(int)height;

- (void)redirectDataToPlugin:(NSView *)pluginView;

- (WebCore::ObjectContentType)determineObjectFromMIMEType:(NSString*)MIMEType URL:(NSURL*)URL;

- (void)windowObjectCleared;

@end
