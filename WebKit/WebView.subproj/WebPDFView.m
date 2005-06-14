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

#ifndef OMIT_TIGER_FEATURES

#import <WebKit/WebAssertions.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDocumentInternal.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebLocalizableStrings.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebPDFView.h>
#import <WebKit/WebView.h>

#import <Quartz/Quartz.h>

// QuartzPrivate.h doesn't include the PDFKit private headers, so we can't get at PDFViewPriv.h. (3957971)
// Even if that was fixed, we'd have to tweak compile options to include QuartzPrivate.h. (3957839)

@interface PDFDocument (PDFKitSecretsIKnow)
- (NSPrintOperation *)getPrintOperationForPrintInfo:(NSPrintInfo *)printInfo autoRotate:(BOOL)doRotate;
@end

NSString *_NSPathForSystemFramework(NSString *framework);

@implementation WebPDFView

+ (NSBundle *)PDFKitBundle
{
    static NSBundle *PDFKitBundle = nil;
    if (PDFKitBundle == nil) {
        NSString *PDFKitPath = [_NSPathForSystemFramework(@"Quartz.framework") stringByAppendingString:@"/Frameworks/PDFKit.framework"];
        if (PDFKitPath == nil) {
            ERROR("Couldn't find PDFKit.framework");
            return nil;
        }
        PDFKitBundle = [NSBundle bundleWithPath:PDFKitPath];
        if (![PDFKitBundle load]) {
            ERROR("Couldn't load PDFKit.framework");
        }
    }
    return PDFKitBundle;
}

+ (Class)PDFViewClass
{
    static Class PDFViewClass = nil;
    if (PDFViewClass == nil) {
        PDFViewClass = [[WebPDFView PDFKitBundle] classNamed:@"PDFView"];
        if (PDFViewClass == nil) {
            ERROR("Couldn't find PDFView class in PDFKit.framework");
        }
    }
    return PDFViewClass;
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        [self setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
        PDFSubview = [[[[self class] PDFViewClass] alloc] initWithFrame:frame];
        [PDFSubview setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
        [self addSubview:PDFSubview];
        written = NO;
    }
    return self;
}

- (void)dealloc
{
    [PDFSubview release];
    [path release];
    [super dealloc];
}

- (PDFView *)PDFSubview
{
    return PDFSubview;
}

#define TEMP_PREFIX "/tmp/XXXXXX-"
#define OBJC_TEMP_PREFIX @"/tmp/XXXXXX-"

static void applicationInfoForMIMEType(NSString *type, NSString **name, NSImage **image)
{
    NSURL *appURL = nil;
    
    OSStatus error = LSCopyApplicationForMIMEType((CFStringRef)type, kLSRolesAll, (CFURLRef *)&appURL);
    if(error != noErr){
        return;
    }

    NSString *appPath = [appURL path];
    CFRelease (appURL);
    
    *image = [[NSWorkspace sharedWorkspace] iconForFile:appPath];  
    [*image setSize:NSMakeSize(16.f,16.f)];  
    
    NSString *appName = [[NSFileManager defaultManager] displayNameAtPath:appPath];
    *name = appName;
}


- (NSString *)path
{
    // Generate path once.
    if (path)
        return path;
        
    NSString *filename = [[dataSource response] suggestedFilename];
    NSFileManager *manager = [NSFileManager defaultManager];    
    
    path = [@"/tmp/" stringByAppendingPathComponent: filename];
    if ([manager fileExistsAtPath:path]) {
        path = [OBJC_TEMP_PREFIX stringByAppendingString:filename];
        char *cpath = (char *)[path fileSystemRepresentation];
        
        int fd = mkstemps (cpath, strlen(cpath) - strlen(TEMP_PREFIX) + 1);
        if (fd < 0) {
            // Couldn't create a temporary file!  Should never happen.  Do
            // we need an alert here?
            path = nil;
        }
        else {
            close (fd);
            path = [manager stringWithFileSystemRepresentation:cpath length:strlen(cpath)];
        }
    }
    
    [path retain];
    
    return path;
}

- (NSView *)hitTest:(NSPoint)point
{
    // Override hitTest so we can override menuForEvent.
    NSEvent *event = [NSApp currentEvent];
    NSEventType type = [event type];
    if (type == NSRightMouseDown || (type == NSLeftMouseDown && ([event modifierFlags] & NSControlKeyMask))) {
        return self;
    }
    return [super hitTest:point];
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
    NSMenu *menu = [PDFSubview menuForEvent:theEvent];
    
    NSString *appName = nil;
    NSImage *appIcon = nil;
    
    applicationInfoForMIMEType([[dataSource response] MIMEType], &appName, &appIcon);
    if (!appName)
        appName = UI_STRING("Finder", "Default application name for Open With context menu");
    
    NSString *title = [NSString stringWithFormat:UI_STRING("Open with %@", "Open document using the Finder"), appName];
    
    NSMenuItem *item = [menu insertItemWithTitle:title action:@selector(openWithFinder:) keyEquivalent:@"" atIndex:0];
    if (appIcon)
        [item setImage:appIcon];
        
    [menu insertItem:[NSMenuItem separatorItem] atIndex:1];
    
    return menu;
}

- (void)_updateScalingToReflectTextSize
{
    WebView *view = [[dataSource webFrame] webView];
    
    // The scale factor and text size multiplier conveniently use the same units, so we can just
    // treat the values as interchangeable.
    if (view != nil) {
        [PDFSubview setScaleFactor:[view textSizeMultiplier]];		
    }	
}

- (void)setDataSource:(WebDataSource *)ds
{
    dataSource = ds;
    [self setFrame:[[self superview] frame]];
    [self _updateScalingToReflectTextSize];
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

- (void)setNeedsLayout:(BOOL)flag
{
}

- (void)layout
{
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
}

- (void)viewDidMoveToHostWindow
{
}

- (void)openWithFinder:(id)sender
{
    NSString *opath = [self path];
    
    if (opath) {
        if (!written) {
            [[dataSource data] writeToFile:opath atomically:YES];
            written = YES;
        }
    
        if (![[NSWorkspace sharedWorkspace] openFile:opath]) {
            // NSWorkspace couldn't open file.  Do we need an alert
            // here?  We ignore the error elsewhere.
        }
    }
}

- (void)_web_textSizeMultiplierChanged
{
    [self _updateScalingToReflectTextSize];
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag;
{
    int options = 0;
    if (!forward) {
        options |= NSBackwardsSearch;
    }
    if (!caseFlag) {
        options |= NSCaseInsensitiveSearch;
    }
    PDFDocument *document = [PDFSubview document];
    PDFSelection *selection = [document findString:string fromSelection:[PDFSubview currentSelection] withOptions:options];
    if (selection == nil && wrapFlag) {
        selection = [document findString:string fromSelection:nil withOptions:options];
    }
    if (selection != nil) {
        [PDFSubview setCurrentSelection:selection];
        [PDFSubview scrollSelectionToVisible:nil];
        return YES;
    }
    return NO;
}

- (void)takeFindStringFromSelection:(id)sender
{
    [NSPasteboard _web_setFindPasteboardString:[[PDFSubview currentSelection] string] withOwner:self];
}

- (void)jumpToSelection:(id)sender
{
    [PDFSubview scrollSelectionToVisible:nil];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item 
{
    SEL action = [item action];    
    if (action == @selector(takeFindStringFromSelection:) || action == @selector(jumpToSelection:)) {
        return [PDFSubview currentSelection] != nil;
    }
    return YES;
}

- (BOOL)canPrintHeadersAndFooters
{
    return NO;
}

- (NSPrintOperation *)printOperationWithPrintInfo:(NSPrintInfo *)printInfo
{
    return [[PDFSubview document] getPrintOperationForPrintInfo:printInfo autoRotate:YES];
}

@end

#endif // OMIT_TIGER_FEATURES
