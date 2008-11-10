/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#if ENABLE(NETSCAPE_PLUGIN_API)

#import "WebBaseNetscapePluginView.h"

#import "WebKitSystemInterface.h"
#import "WebFrameInternal.h"
#import "WebKitLogging.h"
#import "WebView.h"

#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/Document.h>
#import <WebCore/Element.h>
#import <wtf/Assertions.h>

@implementation WebBaseNetscapePluginView

+ (void)initialize
{
#ifndef BUILDING_ON_TIGER
    WebCoreObjCFinalizeOnMainThread(self);
#endif
    WKSendUserChangeNotifications();
}

- (id)initWithFrame:(NSRect)frame
      pluginPackage:(WebNetscapePluginPackage *)pluginPackage
                URL:(NSURL *)URL
            baseURL:(NSURL *)baseURL
           MIMEType:(NSString *)MIME
      attributeKeys:(NSArray *)keys
    attributeValues:(NSArray *)values
       loadManually:(BOOL)loadManually
         DOMElement:(DOMElement *)anElement
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;
    
    _pluginPackage = pluginPackage;
    _element = anElement;
    _sourceURL.adoptNS([URL copy]);
    _baseURL.adoptNS([baseURL copy]);
    
    [self setAttributeKeys:keys andValues:values];
    if (loadManually)
        _mode = NP_FULL;
    else
        _mode = NP_EMBED;
    
    _loadManually = loadManually;
    
    return self;
}

- (void)dealloc
{
    ASSERT(!_isStarted);

    [super dealloc];
}

- (void)finalize
{
    ASSERT_MAIN_THREAD();
    ASSERT(!_isStarted);

    [super finalize];
}

// Methods that subclasses must override
- (void)setAttributeKeys:(NSArray *)keys andValues:(NSArray *)values
{
    ASSERT_NOT_REACHED();
}

- (void)handleMouseMoved:(NSEvent *)event
{
    ASSERT_NOT_REACHED();
}

- (void)removeTrackingRect
{
    if (_trackingTag) {
        [self removeTrackingRect:_trackingTag];
        _trackingTag = 0;
        
        // Do the following after setting trackingTag to 0 so we don't re-enter.
        
        // Balance the retain in resetTrackingRect. Use autorelease in case we hold 
        // the last reference to the window during tear-down, to avoid crashing AppKit. 
        [[self window] autorelease];
    }
}

- (void)resetTrackingRect
{
    [self removeTrackingRect];
    if (_isStarted) {
        // Retain the window so that removeTrackingRect can work after the window is closed.
        [[self window] retain];
        _trackingTag = [self addTrackingRect:[self bounds] owner:self userData:nil assumeInside:NO];
    }
}

- (WebDataSource *)dataSource
{
    WebFrame *webFrame = kit(core(_element.get())->document()->frame());
    return [webFrame _dataSource];
}

- (WebFrame *)webFrame
{
    return [[self dataSource] webFrame];
}

- (WebView *)webView
{
    return [[self webFrame] webView];
}

- (NSWindow *)currentWindow
{
    return [self window] ? [self window] : [[self webView] hostWindow];
}

// We want to treat these as regular keyboard events.

- (void)cut:(id)sender
{
    [self keyDown:[NSApp currentEvent]];
}

- (void)copy:(id)sender
{
    [self keyDown:[NSApp currentEvent]];
}

- (void)paste:(id)sender
{
    [self keyDown:[NSApp currentEvent]];
}

- (void)selectAll:(id)sender
{
    [self keyDown:[NSApp currentEvent]];
}

@end

#endif //  ENABLE(NETSCAPE_PLUGIN_API)

