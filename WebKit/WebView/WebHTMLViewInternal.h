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

// Things internal to the WebKit framework; not SPI.

#import <WebKit/WebHTMLViewPrivate.h>

@class WebTextCompleteController;
@class DOMDocumentFragment;
@class DOMElement;

namespace WebCore {
    class KeyboardEvent;
    class CachedImage;
}

struct WebHTMLViewInterpretKeyEventsParameters;

@interface WebHTMLViewPrivate : NSObject
{
@public
    BOOL closed;
    BOOL needsLayout;
    BOOL needsToApplyStyles;
    BOOL ignoringMouseDraggedEvents;
    BOOL printing;
    BOOL avoidingPrintOrphan;
    
    id savedSubviews;
    BOOL subviewsSetAside;

    NSEvent *mouseDownEvent; // Kept after handling the event.
    BOOL handlingMouseDownEvent;
    NSEvent *keyDownEvent; // Kept after handling the event.
    
    NSSize lastLayoutSize;
    NSSize lastLayoutFrameSize;
    BOOL laidOutAtLeastOnce;
    
    NSPoint lastScrollPosition;

    WebPluginController *pluginController;
    
    NSString *toolTip;
    NSToolTipTag lastToolTipTag;
    id trackingRectOwner;
    void *trackingRectUserData;
    
    NSTimer *autoscrollTimer;
    NSEvent *autoscrollTriggerEvent;
    
    NSArray* pageRects;

    NSMutableDictionary* highlighters;

    BOOL resigningFirstResponder;
    BOOL nextResponderDisabledOnce;
    BOOL willBecomeFirstResponderForNodeFocus;
    
    WebTextCompleteController *compController;
    
    BOOL transparentBackground;

    WebHTMLViewInterpretKeyEventsParameters *interpretKeyEventsParameters;
    BOOL receivedNOOP;
    
    NSTextView *firstResponderTextViewAtMouseDownTime;
    
    WebDataSource *dataSource;
    WebCore::CachedImage *promisedDragTIFFDataSource;
    
    CFRunLoopTimerRef updateActiveStateTimer;
    CFRunLoopTimerRef updateMouseoverTimer;

    SEL selectorForDoCommandBySelector;

#ifndef NDEBUG
    BOOL enumeratingSubviews;
#endif
}
- (void)clear;
@end

@interface WebHTMLView (WebInternal)
- (void)_selectionChanged;
- (void)_updateFontPanel;
- (BOOL)_canSmartCopyOrDelete;
- (BOOL)_textViewWasFirstResponderAtMouseDownTime:(NSTextView *)textView;
#ifndef __LP64__
- (void)_pauseNullEventsForAllNetscapePlugins;
- (void)_resumeNullEventsForAllNetscapePlugins;
#endif
- (void)_willMakeFirstResponderForNodeFocus;
- (id<WebHTMLHighlighter>)_highlighterForType:(NSString*)type;
- (WebFrame *)_frame;
- (void)copy:(id)sender;
- (void)cut:(id)sender;
- (void)paste:(id)sender;
- (void)pasteAsPlainText:(id)sender;
- (void)closeIfNotCurrentView;
- (void)_lookUpInDictionaryFromMenu:(id)sender;
- (void)_hoverFeedbackSuspendedChanged;
- (BOOL)_interceptEditingKeyEvent:(WebCore::KeyboardEvent *)event shouldSaveCommand:(BOOL)shouldSave;
- (DOMDocumentFragment*)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard;
- (NSEvent *)_mouseDownEvent;
#ifndef BUILDING_ON_TIGER
- (BOOL)isGrammarCheckingEnabled;
- (void)setGrammarCheckingEnabled:(BOOL)flag;
- (void)toggleGrammarChecking:(id)sender;
#endif
- (WebCore::CachedImage*)promisedDragTIFFDataSource;
- (void)setPromisedDragTIFFDataSource:(WebCore::CachedImage*)source;
@end

