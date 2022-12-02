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

#import "BrowserWindowController.h"

#import "AppDelegate.h"
#import "SettingsController.h"

@interface BrowserWindowController () <NSSharingServicePickerDelegate, NSSharingServiceDelegate>
@end

@implementation BrowserWindowController

@synthesize editable = _editable;

- (id)initWithWindow:(NSWindow *)window
{
    self = [super initWithWindow:window];
    return self;
}

- (void)windowDidLoad
{
    // FIXME: We should probably adopt the default unified style, but we'd need
    // somewhere to put the window/page title.
    self.window.toolbarStyle = NSWindowToolbarStyleExpanded;

    [share sendActionOn:NSEventMaskLeftMouseDown];
    [super windowDidLoad];
}

- (IBAction)openLocation:(id)sender
{
    [[self window] makeFirstResponder:urlText];
}

- (void)loadURLString:(NSString *)urlString
{
}

- (void)loadHTMLString:(NSString *)HTMLString
{
}

- (NSString *)addProtocolIfNecessary:(NSString *)address
{
    if ([address rangeOfString:@"://"].length > 0)
        return address;

    if ([address hasPrefix:@"data:"])
        return address;

    if ([address hasPrefix:@"about:"])
        return address;

    return [@"http://" stringByAppendingString:address];
}

- (IBAction)share:(id)sender
{
    NSSharingServicePicker *picker = [[NSSharingServicePicker alloc] initWithItems:@[ self.currentURL ]];
    picker.delegate = self;
    [picker showRelativeToRect:NSZeroRect ofView:sender preferredEdge:NSRectEdgeMinY];
}

- (IBAction)fetch:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)reload:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)showCertificate:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)forceRepaint:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)saveAsPDF:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)saveAsWebArchive:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)goBack:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)goForward:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)showHideWebView:(id)sender
{
    self.mainContentView.hidden = !self.mainContentView.isHidden;
}

- (IBAction)removeReinsertWebView:(id)sender
{
    if (self.mainContentView.window)
        [self.mainContentView removeFromSuperview];
    else
        [containerView addSubview:self.mainContentView];
}

- (IBAction)toggleFullWindowWebView:(id)sender
{
    BOOL newFillWindow = ![self webViewFillsWindow];
    [self setWebViewFillsWindow:newFillWindow];

    SettingsController *settings = [[NSApplication sharedApplication] browserAppDelegate].settingsController;
    settings.webViewFillsWindow = newFillWindow;
}

- (BOOL)webViewFillsWindow
{
    return NSEqualRects(containerView.bounds, self.mainContentView.frame);
}

- (void)setWebViewFillsWindow:(BOOL)fillWindow
{
    if (fillWindow)
        [self.mainContentView setFrame:containerView.bounds];
    else {
        const CGFloat viewInset = 100.0f;
        NSRect viewRect = NSInsetRect(containerView.bounds, viewInset, viewInset);
        // Make it not vertically centered, to reveal y-flipping bugs.
        viewRect = NSOffsetRect(viewRect, 0, -25);
        [self.mainContentView setFrame:viewRect];
    }
}

- (IBAction)zoomIn:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)zoomOut:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)resetZoom:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (BOOL)canZoomIn
{
    [self doesNotRecognizeSelector:_cmd];
    return NO;
}

- (BOOL)canZoomOut
{
    [self doesNotRecognizeSelector:_cmd];
    return NO;
}

- (BOOL)canResetZoom
{
    [self doesNotRecognizeSelector:_cmd];
    return NO;
}

- (IBAction)toggleZoomMode:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (CGFloat)pageScaleForMenuItemTag:(NSInteger)tag
{
    if (tag == 1)
        return 1;
    if (tag == 2)
        return 1.25;
    if (tag == 3)
        return 1.5;
    if (tag == 4)
        return 2.0;

    return 1;
}

- (IBAction)setPageScale:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)setViewScale:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)toggleShrinkToFit:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)dumpSourceToConsole:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)showHideWebInspector:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (void)didChangeSettings
{
    [self doesNotRecognizeSelector:_cmd];
}

- (NSURL *)currentURL
{
    [self doesNotRecognizeSelector:_cmd];
    return nil;
}

- (NSView *)mainContentView
{
    [self doesNotRecognizeSelector:_cmd];
    return nil;
}

- (IBAction)toggleEditable:(id)sender
{
    self.editable = !self.isEditable;
}

#pragma mark -
#pragma mark NSSharingServicePickerDelegate

- (NSArray *)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker sharingServicesForItems:(NSArray *)items proposedSharingServices:(NSArray *)proposedServices
{
    return proposedServices;
}

- (id <NSSharingServiceDelegate>)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker delegateForSharingService:(NSSharingService *)sharingService
{
    return self;
}

- (void)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker didChooseSharingService:(NSSharingService *)service
{
}

#pragma mark -
#pragma mark NSSharingServiceDelegate

- (NSRect)sharingService:(NSSharingService *)sharingService sourceFrameOnScreenForShareItem:(id)item
{
    NSRect rect = [self.window convertRectToScreen:self.mainContentView.bounds];
    
    return rect;
}

static CGRect coreGraphicsScreenRectForAppKitScreenRect(NSRect rect)
{
    NSScreen *firstScreen = [NSScreen screens][0];
    return CGRectMake(NSMinX(rect), NSHeight(firstScreen.frame) - NSMinY(rect) - NSHeight(rect), NSWidth(rect), NSHeight(rect));
}

- (NSImage *)sharingService:(NSSharingService *)sharingService transitionImageForShareItem:(id)item contentRect:(NSRect *)contentRect
{
    NSRect contentFrame = [self.window convertRectToScreen:self.mainContentView.bounds];

    CGRect frame = coreGraphicsScreenRectForAppKitScreenRect(contentFrame);
    CGImageRef imageRef = CGWindowListCreateImage(frame, kCGWindowListOptionIncludingWindow, (CGWindowID)[self.window windowNumber], kCGWindowImageBoundsIgnoreFraming);
    
    if (!imageRef)
        return nil;
    
    NSImage *image = [[NSImage alloc] initWithCGImage:imageRef size:NSZeroSize];
    CGImageRelease(imageRef);

    return image;
}

- (NSWindow *)sharingService:(NSSharingService *)sharingService sourceWindowForShareItems:(NSArray *)items sharingContentScope:(NSSharingContentScope *)sharingContentScope
{
    *sharingContentScope = NSSharingContentScopeFull;
    return self.window;
}

@end
