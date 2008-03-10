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

#import "WebFrameBridge.h"

#import "WebBackForwardList.h"
#import "WebBaseNetscapePluginView.h"
#import "WebBasePluginPackage.h"
#import "WebDataSourceInternal.h"
#import "WebDefaultUIDelegate.h"
#import "WebEditingDelegate.h"
#import "WebFormDelegate.h"
#import "WebFrameInternal.h"
#import "WebFrameLoadDelegate.h"
#import "WebFrameLoaderClient.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLRepresentationPrivate.h"
#import "WebHTMLViewInternal.h"
#import "WebHistoryItemInternal.h"
#import "WebHistoryItemPrivate.h"
#import "WebJavaPlugIn.h"
#import "WebJavaScriptTextInputPanel.h"
#import "WebKitErrorsPrivate.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitPluginContainerView.h"
#import "WebKitStatisticsPrivate.h"
#import "WebKitSystemBits.h"
#import "WebLocalizableStrings.h"
#import "WebNSObjectExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebNSViewExtras.h"
#import "WebNetscapePluginEmbeddedView.h"
#import "WebNetscapePluginPackage.h"
#import "WebNullPluginView.h"
#import "WebPlugin.h"
#import "WebPluginController.h"
#import "WebPluginDatabase.h"
#import "WebPluginPackage.h"
#import "WebPluginViewFactoryPrivate.h"
#import "WebPreferencesPrivate.h"
#import "WebResourcePrivate.h"
#import "WebScriptDebugServerPrivate.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLResponse.h>
#import <JavaScriptCore/Assertions.h>
#import <JavaScriptCore/JSLock.h>
#import <JavaScriptCore/object.h>
#import <JavaVM/jni.h>
#import <WebCore/Cache.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/DragController.h>
#import <WebCore/Element.h>
#import <WebCore/FoundationExtras.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameLoaderClient.h>
#import <WebCore/FrameTree.h>
#import <WebCore/HTMLFrameOwnerElement.h>
#import <WebCore/Page.h>
#import <WebCore/ResourceLoader.h>
#import <WebCore/SubresourceLoader.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebKitSystemInterface.h>
#import <wtf/RefPtr.h>
#import <WebCore/MIMETypeRegistry.h>

// For compatibility with old SPI. 
@interface NSView (OldWebPlugin)
- (void)setIsSelected:(BOOL)f;
@end

using namespace WebCore;

#define KeyboardUIModeDidChangeNotification @"com.apple.KeyboardUIModeDidChange"
#define AppleKeyboardUIMode CFSTR("AppleKeyboardUIMode")
#define UniversalAccessDomain CFSTR("com.apple.universalaccess")

@implementation WebFrameBridge

#ifndef BUILDING_ON_TIGER
+ (void)initialize
{
    WebCoreObjCFinalizeOnMainThread(self);
}
#endif

- (WebView *)webView
{
    if (!m_frame)
        return nil;
    
    return kit(m_frame->page());
}

- (void)finishInitializingWithPage:(Page*)page frameName:(const WebCore::String&)name frameView:(WebFrameView *)frameView ownerElement:(HTMLFrameOwnerElement*)ownerElement
{
    ++WebBridgeCount;

    WebView *webView = kit(page);

    _frame = [[WebFrame alloc] _initWithWebFrameView:frameView webView:webView bridge:self];

    m_frame = new Frame(page, ownerElement, new WebFrameLoaderClient(_frame));
    m_frame->setBridge(self);
    m_frame->tree()->setName(name);
    m_frame->init();
    
    [self setTextSizeMultiplier:[webView textSizeMultiplier]];
}

- (id)initMainFrameWithPage:(Page*)page frameName:(const WebCore::String&)name frameView:(WebFrameView *)frameView
{
    self = [super init];
    [self finishInitializingWithPage:page frameName:name frameView:frameView ownerElement:0];
    return self;
}

- (id)initSubframeWithOwnerElement:(HTMLFrameOwnerElement*)ownerElement frameName:(const WebCore::String&)name frameView:(WebFrameView *)frameView
{
    self = [super init];
    [self finishInitializingWithPage:ownerElement->document()->frame()->page() frameName:name frameView:frameView ownerElement:ownerElement];
    return self;
}

- (void)fini
{
    if (_keyboardUIModeAccessed) {
        [[NSDistributedNotificationCenter defaultCenter] 
            removeObserver:self name:KeyboardUIModeDidChangeNotification object:nil];
        [[NSNotificationCenter defaultCenter] 
            removeObserver:self name:WebPreferencesChangedNotification object:nil];
    }

    ASSERT(_frame == nil);
    --WebBridgeCount;
}

- (void)dealloc
{
    [_frame release];
    
    [self fini];
    [super dealloc];
}

- (void)finalize
{
    ASSERT_MAIN_THREAD();
    [self fini];
    [super finalize];
}

- (WebPreferences *)_preferences
{
    return [[self webView] preferences];
}

- (void)_retrieveKeyboardUIModeFromPreferences:(NSNotification *)notification
{
    CFPreferencesAppSynchronize(UniversalAccessDomain);

    Boolean keyExistsAndHasValidFormat;
    int mode = CFPreferencesGetAppIntegerValue(AppleKeyboardUIMode, UniversalAccessDomain, &keyExistsAndHasValidFormat);
    
    // The keyboard access mode is reported by two bits:
    // Bit 0 is set if feature is on
    // Bit 1 is set if full keyboard access works for any control, not just text boxes and lists
    // We require both bits to be on.
    // I do not know that we would ever get one bit on and the other off since
    // checking the checkbox in system preferences which is marked as "Turn on full keyboard access"
    // turns on both bits.
    _keyboardUIMode = (mode & 0x2) ? KeyboardAccessFull : KeyboardAccessDefault;
    
    // check for tabbing to links
    if ([[self _preferences] tabsToLinks])
        _keyboardUIMode = (KeyboardUIMode)(_keyboardUIMode | KeyboardAccessTabsToLinks);
}

- (KeyboardUIMode)keyboardUIMode
{
    if (!_keyboardUIModeAccessed) {
        _keyboardUIModeAccessed = YES;
        [self _retrieveKeyboardUIModeFromPreferences:nil];
        
        [[NSDistributedNotificationCenter defaultCenter] 
            addObserver:self selector:@selector(_retrieveKeyboardUIModeFromPreferences:) 
            name:KeyboardUIModeDidChangeNotification object:nil];

        [[NSNotificationCenter defaultCenter] 
            addObserver:self selector:@selector(_retrieveKeyboardUIModeFromPreferences:) 
                   name:WebPreferencesChangedNotification object:nil];
    }
    return _keyboardUIMode;
}

- (WebFrame *)webFrame
{
    return _frame;
}

- (WebCoreFrameBridge *)mainFrame
{
    ASSERT(_frame != nil);
    return [[[self webView] mainFrame] _bridge];
}

- (NSResponder *)firstResponder
{
    ASSERT(_frame != nil);
    WebView *webView = [self webView];
    return [[webView _UIDelegateForwarder] webViewFirstResponder:webView];
}

- (void)makeFirstResponder:(NSResponder *)view
{
    ASSERT(_frame != nil);
    WebView *webView = [self webView];
    ASSERT([view isKindOfClass:[NSView class]]);
    ASSERT([(NSView *)view window]);
    ASSERT([(NSView *)view window] == [webView window]);
    [webView _pushPerformingProgrammaticFocus];
    [[webView _UIDelegateForwarder] webView:webView makeFirstResponder:view];
    [webView _popPerformingProgrammaticFocus];
}

- (WebDataSource *)dataSource
{
    ASSERT(_frame != nil);
    WebDataSource *dataSource = [_frame _dataSource];

    ASSERT(dataSource != nil);

    return dataSource;
}

- (void)close
{
    [super close];
    [_frame release];
    _frame = nil;
}

- (void)setIsSelected:(BOOL)isSelected forView:(NSView *)view
{
    if ([view respondsToSelector:@selector(webPlugInSetIsSelected:)])
        [view webPlugInSetIsSelected:isSelected];
    else if ([view respondsToSelector:@selector(setIsSelected:)])
        [view setIsSelected:isSelected];
}

- (void)willPopupMenu:(NSMenu *)menu
{
    CallUIDelegate([self webView], @selector(webView:willPopupMenu:), menu);
}

@end
