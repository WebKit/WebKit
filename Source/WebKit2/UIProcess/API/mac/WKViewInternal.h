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

#if !TARGET_OS_IPHONE

#import "WKViewPrivate.h"

#import <wtf/Forward.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

@class _WKRemoteObjectRegistry;

namespace API {
class PageConfiguration;
}

namespace IPC {
class DataReference;
}

namespace WebCore {
class Image;
class SharedBuffer;
}

namespace WebKit {
class DrawingAreaProxy;
class WebProcessPool;
struct ColorSpaceData;
}

@class WKWebView;
#if WK_API_ENABLED
@class _WKThumbnailView;
#endif

@interface WKView ()
#if WK_API_ENABLED
- (instancetype)initWithFrame:(NSRect)frame processPool:(WebKit::WebProcessPool&)processPool configuration:(Ref<API::PageConfiguration>&&)configuration webView:(WKWebView *)webView;
#endif

- (std::unique_ptr<WebKit::DrawingAreaProxy>)_createDrawingAreaProxy;
- (void)_processDidExit;
- (void)_pageClosed;
- (void)_didRelaunchProcess;
- (void)_setUserInterfaceItemState:(NSString *)commandName enabled:(BOOL)isEnabled state:(int)newState;
- (void)_doneWithKeyEvent:(NSEvent *)event eventWasHandled:(BOOL)eventWasHandled;
- (bool)_executeSavedCommandBySelector:(SEL)selector;
- (NSRect)_convertToDeviceSpace:(NSRect)rect;
- (NSRect)_convertToUserSpace:(NSRect)rect;

- (void)_dragImageForView:(NSView *)view withImage:(NSImage *)image at:(NSPoint)clientPoint linkDrag:(BOOL)linkDrag;
- (void)_setPromisedDataForImage:(WebCore::Image *)image withFileName:(NSString *)filename withExtension:(NSString *)extension withTitle:(NSString *)title withURL:(NSString *)url withVisibleURL:(NSString *)visibleUrl withArchive:(WebCore::SharedBuffer*) archiveBuffer forPasteboard:(NSString *)pasteboardName;
#if ENABLE(ATTACHMENT_ELEMENT)
- (void)_setPromisedDataForAttachment:(NSString *)filename withExtension:(NSString *)extension withTitle:(NSString *)title withURL:(NSString *)url withVisibleURL:(NSString *)visibleUrl forPasteboard:(NSString *)pasteboardName;
#endif

- (WebKit::ColorSpaceData)_colorSpace;

- (NSInteger)spellCheckerDocumentTag;
- (void)handleAcceptedAlternativeText:(NSString*)text;

#if WK_API_ENABLED
@property (nonatomic, setter=_setThumbnailView:) _WKThumbnailView *_thumbnailView;
#endif

- (void)_addFontPanelObserver;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
- (void)_startWindowDrag;
#endif

#if WK_API_ENABLED
@property (nonatomic, readonly) _WKRemoteObjectRegistry *_remoteObjectRegistry;
#endif

@end

#endif
