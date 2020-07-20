/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import <Cocoa/Cocoa.h>

@interface BrowserWindowController : NSWindowController {
    IBOutlet NSProgressIndicator *progressIndicator;
    IBOutlet NSButton *reloadButton;
    IBOutlet NSButton *lockButton;
    IBOutlet NSButton *backButton;
    IBOutlet NSButton *forwardButton;
    IBOutlet NSButton *share;
    IBOutlet NSToolbar *toolbar;
    IBOutlet NSTextField *urlText;
    IBOutlet NSView *containerView;
    IBOutlet NSButton *toggleUseShrinkToFitButton;

    BOOL _zoomTextOnly;
    BOOL _editable;
}

- (void)loadURLString:(NSString *)urlString;
- (void)loadHTMLString:(NSString *)HTMLString;
- (NSString *)addProtocolIfNecessary:(NSString *)address;

- (IBAction)openLocation:(id)sender;

- (IBAction)saveAsPDF:(id)sender;
- (IBAction)saveAsWebArchive:(id)sender;

- (IBAction)fetch:(id)sender;
- (IBAction)share:(id)sender;
- (IBAction)reload:(id)sender;
- (IBAction)forceRepaint:(id)sender;
- (IBAction)goBack:(id)sender;
- (IBAction)goForward:(id)sender;

- (IBAction)showHideWebView:(id)sender;
- (IBAction)removeReinsertWebView:(id)sender;
- (IBAction)toggleFullWindowWebView:(id)sender;

- (IBAction)zoomIn:(id)sender;
- (IBAction)zoomOut:(id)sender;
- (IBAction)resetZoom:(id)sender;
- (BOOL)canZoomIn;
- (BOOL)canZoomOut;
- (BOOL)canResetZoom;

- (IBAction)toggleZoomMode:(id)sender;

- (IBAction)setPageScale:(id)sender;
- (IBAction)setViewScale:(id)sender;

- (IBAction)toggleShrinkToFit:(id)sender;

- (IBAction)dumpSourceToConsole:(id)sender;

- (IBAction)showHideWebInspector:(id)sender;

- (void)didChangeSettings;
- (BOOL)webViewFillsWindow;
- (void)setWebViewFillsWindow:(BOOL)fillWindow;

- (NSURL *)currentURL;
- (NSView *)mainContentView;

- (CGFloat)pageScaleForMenuItemTag:(NSInteger)tag;

@property (nonatomic, assign, getter=isEditable) BOOL editable;
- (IBAction)toggleEditable:(id)sender;

@end

