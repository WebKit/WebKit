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

@interface BrowserWindowController ()

@end

@implementation BrowserWindowController

- (id)initWithWindow:(NSWindow *)window
{
    self = [super initWithWindow:window];
    return self;
}

- (void)windowDidLoad
{
    self.window.styleMask |= NSWindowStyleMaskFullSizeContentView;

    [super windowDidLoad];
}

- (IBAction)openLocation:(id)sender
{
    [[self window] makeFirstResponder:urlText];
}

- (void)loadURLString:(NSString *)urlString
{
}

- (void)applicationTerminating
{
}

- (NSString *)addProtocolIfNecessary:(NSString *)address
{
    if ([address rangeOfString:@"://"].length > 0)
        return address;

    if ([address hasPrefix:@"data:"])
        return address;

    return [@"http://" stringByAppendingString:address];
}

- (IBAction)fetch:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)reload:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)forceRepaint:(id)sender
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
    [self doesNotRecognizeSelector:_cmd];
}

- (IBAction)removeReinsertWebView:(id)sender
{
    [self doesNotRecognizeSelector:_cmd];
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

- (IBAction)setScale:(id)sender
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

- (IBAction)find:(id)sender
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

@end
