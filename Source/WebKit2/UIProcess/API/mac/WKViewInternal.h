/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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

#import "PluginComplexTextInputState.h"
#import "WKView.h"
#import "WebFindOptions.h"
#import <wtf/Forward.h>
#import <wtf/Vector.h>

namespace CoreIPC {
    class DataReference;
}

namespace WebCore {
    struct KeypressCommand;
    class Image;
    class SharedBuffer;
}

namespace WebKit {
    class DrawingAreaProxy;
    class FindIndicator;
    class LayerTreeContext;
    struct ColorSpaceData;
    struct EditorState;
}

@class WKFullScreenWindowController;

@interface WKView (Internal)

- (PassOwnPtr<WebKit::DrawingAreaProxy>)_wk_createDrawingAreaProxy;
- (BOOL)_wk_isFocused;
- (void)_wk_processDidCrash;
- (void)_wk_pageClosed;
- (void)_wk_didRelaunchProcess;
- (void)_wk_toolTipChangedFrom:(NSString *)oldToolTip to:(NSString *)newToolTip;
- (void)_wk_setCursor:(NSCursor *)cursor;
- (void)_wk_setUserInterfaceItemState:(NSString *)commandName enabled:(BOOL)isEnabled state:(int)newState;
- (BOOL)_wk_interpretKeyEvent:(NSEvent *)theEvent savingCommandsTo:(Vector<WebCore::KeypressCommand>&)commands;
- (void)_wk_doneWithKeyEvent:(NSEvent *)event eventWasHandled:(BOOL)eventWasHandled;
- (bool)_wk_executeSavedCommandBySelector:(SEL)selector;
- (NSRect)_wk_convertToDeviceSpace:(NSRect)rect;
- (NSRect)_wk_convertToUserSpace:(NSRect)rect;
- (void)_wk_setFindIndicator:(PassRefPtr<WebKit::FindIndicator>)findIndicator fadeOut:(BOOL)fadeOut animate:(BOOL)animate;

- (void)_wk_enterAcceleratedCompositingMode:(const WebKit::LayerTreeContext&)layerTreeContext;
- (void)_wk_exitAcceleratedCompositingMode;
- (void)_wk_updateAcceleratedCompositingMode:(const WebKit::LayerTreeContext&)layerTreeContext;

- (void)_wk_setAccessibilityWebProcessToken:(NSData *)data;

- (void)_wk_pluginFocusOrWindowFocusChanged:(BOOL)pluginHasFocusAndWindowHasFocus pluginComplexTextInputIdentifier:(uint64_t)pluginComplexTextInputIdentifier;
- (void)_wk_setPluginComplexTextInputState:(WebKit::PluginComplexTextInputState)pluginComplexTextInputState pluginComplexTextInputIdentifier:(uint64_t)pluginComplexTextInputIdentifier;

- (void)_wk_setPageHasCustomRepresentation:(BOOL)pageHasCustomRepresentation;
- (void)_wk_didFinishLoadingDataForCustomRepresentationWithSuggestedFilename:(const String&)suggestedFilename dataReference:(const CoreIPC::DataReference&)dataReference;
- (double)_wk_customRepresentationZoomFactor;
- (void)_wk_setCustomRepresentationZoomFactor:(double)zoomFactor;
- (void)_wk_findStringInCustomRepresentation:(NSString *)string withFindOptions:(WebKit::FindOptions)options maxMatchCount:(NSUInteger)count;
- (void)_wk_countStringMatchesInCustomRepresentation:(NSString *)string withFindOptions:(WebKit::FindOptions)options maxMatchCount:(NSUInteger)count;
- (void)_wk_setDragImage:(NSImage *)image at:(NSPoint)clientPoint linkDrag:(BOOL)linkDrag;
- (void)_wk_setPromisedData:(WebCore::Image *)image withFileName:(NSString *)filename withExtension:(NSString *)extension withTitle:(NSString *)title withURL:(NSString *)url withVisibleURL:(NSString *)visibleUrl withArchive:(WebCore::SharedBuffer*) archiveBuffer forPasteboard:(NSString *)pasteboardName;
- (void)_wk_updateSecureInputState;
- (void)_wk_updateTextInputStateIncludingSecureInputState:(BOOL)updateSecureInputState;
- (void)_wk_resetTextInputState;

- (void)_wk_didChangeScrollbarsForMainFrame;

- (WebKit::ColorSpaceData)_wk_colorSpace;

#if ENABLE(FULLSCREEN_API)
- (BOOL)_wk_hasFullScreenWindowController;
- (WKFullScreenWindowController*)_wk_fullScreenWindowController;
- (void)_wk_closeFullScreenWindowController;
#endif

- (void)_wk_cacheWindowBottomCornerRect;

- (NSInteger)_wk_spellCheckerDocumentTag;
- (void)_wk_handleAcceptedAlternativeText:(NSString*)text;

- (void)_wk_setSuppressVisibilityUpdates:(BOOL)suppressVisibilityUpdates;
- (BOOL)_wk_suppressVisibilityUpdates;

@end
