/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import "WebHTMLViewPrivate.h"

@class CALayer;
@class WebFrame;
@class WebPluginController;

namespace WebCore {
    class CachedImage;
    class KeyboardEvent;
}

#if PLATFORM(MAC)

@interface WebHTMLView () <NSDraggingSource>
@end

#endif

@interface WebHTMLView (WebInternal)
- (void)_selectionChanged;

#if PLATFORM(MAC)
- (void)_updateFontPanel;
- (void)_setSoftSpaceRange:(NSRange)range;
#endif

- (BOOL)_canSmartCopyOrDelete;

- (WebFrame *)_frame;
- (void)closeIfNotCurrentView;

#if PLATFORM(MAC)
- (void)_lookUpInDictionaryFromMenu:(id)sender;
- (BOOL)_interpretKeyEvent:(WebCore::KeyboardEvent *)event savingCommands:(BOOL)savingCommands;
- (DOMDocumentFragment *)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard;
- (NSEvent *)_mouseDownEvent;
- (BOOL)isGrammarCheckingEnabled;
- (void)setGrammarCheckingEnabled:(BOOL)flag;
- (void)toggleGrammarChecking:(id)sender;
- (void)setPromisedDragTIFFDataSource:(WebCore::CachedImage*)source;
#endif

#if PLATFORM(IOS_FAMILY)
- (BOOL)_handleEditingKeyEvent:(WebCore::KeyboardEvent *)event;
#endif

- (void)_web_updateLayoutAndStyleIfNeededRecursive;
- (void)_destroyAllWebPlugins;
- (BOOL)_needsLayout;

#if PLATFORM(MAC)
- (void)attachRootLayer:(CALayer *)layer;
- (void)detachRootLayer;

- (BOOL)_web_isDrawingIntoLayer;
- (BOOL)_web_isDrawingIntoAcceleratedLayer;
#endif

#if PLATFORM(IOS_FAMILY)
- (void)_layoutIfNeeded;
#endif

#if PLATFORM(MAC)
- (void)_changeSpellingToWord:(NSString *)newWord;
- (void)_startAutoscrollTimer:(NSEvent *)event;
#endif

- (void)_stopAutoscrollTimer;

- (WebPluginController *)_pluginController;

- (void)_executeSavedKeypressCommands;

@end

@interface WebHTMLView (RemovedAppKitSuperclassMethods)
#if PLATFORM(IOS_FAMILY)
- (void)delete:(id)sender;
- (void)transpose:(id)sender;
#endif
- (BOOL)hasMarkedText;
@end
