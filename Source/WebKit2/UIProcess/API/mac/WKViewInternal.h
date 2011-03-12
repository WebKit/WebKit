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

#import "WKView.h"
#import <WebCore/Editor.h>
#import <WebCore/KeyboardEvent.h>

namespace WebKit {
    class DrawingAreaProxy;
    class FindIndicator;
    class LayerTreeContext;
}

#if ENABLE(FULLSCREEN_API)
@class WKFullScreenWindowController;
#endif

@interface WKView (Internal)
- (PassOwnPtr<WebKit::DrawingAreaProxy>)_createDrawingAreaProxy;
- (BOOL)_isFocused;
- (void)_processDidCrash;
- (void)_pageClosed;
- (void)_didRelaunchProcess;
- (void)_takeFocus:(BOOL)direction;
- (void)_toolTipChangedFrom:(NSString *)oldToolTip to:(NSString *)newToolTip;
- (void)_setCursor:(NSCursor *)cursor;
- (void)_setUserInterfaceItemState:(NSString *)commandName enabled:(BOOL)isEnabled state:(int)newState;
- (Vector<WebCore::KeypressCommand>&)_interceptKeyEvent:(NSEvent *)theEvent;
- (void)_getTextInputState:(unsigned)start selectionEnd:(unsigned)end underlines:(Vector<WebCore::CompositionUnderline>&)lines;
- (void)_setEventBeingResent:(NSEvent *)event;
- (NSRect)_convertToDeviceSpace:(NSRect)rect;
- (NSRect)_convertToUserSpace:(NSRect)rect;
- (void)_setFindIndicator:(PassRefPtr<WebKit::FindIndicator>)findIndicator fadeOut:(BOOL)fadeOut;

- (void)_enterAcceleratedCompositingMode:(const WebKit::LayerTreeContext&)layerTreeContext;
- (void)_exitAcceleratedCompositingMode;

- (void)_setAccessibilityWebProcessToken:(NSData *)data;
- (void)_setComplexTextInputEnabled:(BOOL)complexTextInputEnabled pluginComplexTextInputIdentifier:(uint64_t)pluginComplexTextInputIdentifier;

- (void)_setPageHasCustomRepresentation:(BOOL)pageHasCustomRepresentation;
- (void)_didFinishLoadingDataForCustomRepresentationWithSuggestedFilename:(const String&)suggestedFilename dataReference:(const CoreIPC::DataReference&)dataReference;
- (double)_customRepresentationZoomFactor;
- (void)_setCustomRepresentationZoomFactor:(double)zoomFactor;
- (void)_setDragImage:(NSImage *)image at:(NSPoint)clientPoint linkDrag:(BOOL)linkDrag;

- (void)_setDrawingAreaSize:(NSSize)size;

- (void)_didChangeScrollbarsForMainFrame;

#if ENABLE(FULLSCREEN_API)
- (WKFullScreenWindowController*)fullScreenWindowController;
#endif
@end
