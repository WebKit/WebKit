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
#import <WebKit/WebDocumentPrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameInternal.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebLocalizableStrings.h>
#import <WebKit/WebNSAttributedStringExtras.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPDFView.h>
#import <WebKit/WebUIDelegate.h>
#import <WebKit/WebView.h>
#import <WebKit/WebViewPrivate.h>

#import <WebKitSystemInterface.h>
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
        [PDFSubview setDelegate:self];
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

- (NSDictionary *)elementAtPoint:(NSPoint)point
{
    WebFrame *frame = [dataSource webFrame];
    ASSERT(frame);

    // FIXME 4158121: should determine whether the point is over a selection, and if so set WebElementIsSelectedKey
    // as in WebTextView.m. Would need to convert coordinates, and make sure that the code that checks
    // WebElementIsSelectedKey would work with PDF documents.
    return [NSDictionary dictionaryWithObjectsAndKeys:
        frame, WebElementFrameKey, nil];
}

- (NSMutableArray *)_menuItemsFromPDFKitForEvent:(NSEvent *)theEvent
{
    NSMutableArray *copiedItems = [NSMutableArray array];
    NSDictionary *actionsToTags = [[NSDictionary alloc] initWithObjectsAndKeys:
        [NSNumber numberWithInt:WebMenuItemPDFActualSize], NSStringFromSelector(@selector(_setActualSize:)),
        [NSNumber numberWithInt:WebMenuItemPDFZoomIn], NSStringFromSelector(@selector(zoomIn:)),
        [NSNumber numberWithInt:WebMenuItemPDFZoomOut], NSStringFromSelector(@selector(zoomOut:)),
        [NSNumber numberWithInt:WebMenuItemPDFAutoSize], NSStringFromSelector(@selector(_setAutoSize:)),
        [NSNumber numberWithInt:WebMenuItemPDFSinglePage], NSStringFromSelector(@selector(_setSinglePage:)),
        [NSNumber numberWithInt:WebMenuItemPDFFacingPages], NSStringFromSelector(@selector(_setDoublePage:)),
        [NSNumber numberWithInt:WebMenuItemPDFContinuous], NSStringFromSelector(@selector(_toggleContinuous:)),
        [NSNumber numberWithInt:WebMenuItemPDFNextPage], NSStringFromSelector(@selector(goToNextPage:)),
        [NSNumber numberWithInt:WebMenuItemPDFPreviousPage], NSStringFromSelector(@selector(goToPreviousPage:)),
        nil];
    
    NSEnumerator *e = [[[PDFSubview menuForEvent:theEvent] itemArray] objectEnumerator];
    NSMenuItem *item;
    while ((item = [e nextObject]) != nil) {
        // Copy items since a menu item can be in only one menu at a time, and we don't
        // want to modify the original menu supplied by PDFKit.
        NSMenuItem *itemCopy = [item copy];
        [copiedItems addObject:itemCopy];
        
        if ([itemCopy isSeparatorItem]) {
            continue;
        }
        NSString *actionString = NSStringFromSelector([itemCopy action]);
        NSNumber *tagNumber = [actionsToTags objectForKey:actionString];
        
        int tag;
        if (tagNumber != nil) {
            tag = [tagNumber intValue];
        } else {
            tag = WebMenuItemTagOther;
            ERROR("no WebKit menu item tag found for PDF context menu item action \"%@\", using WebMenuItemTagOther", actionString);
        }
        if ([itemCopy tag] == 0) {
            [itemCopy setTag:tag];
        } else {
            ERROR("PDF context menu item %@ came with tag %d, so no WebKit tag was applied. This could mean that the item doesn't appear in clients such as Safari.", [itemCopy title], [itemCopy tag]);
        }
        
        // Intercept some of these menu items for better WebKit integration.
        switch (tag) {
            // Convert the scale-factor-related items to use WebKit's text sizing API instead, so they match other
            // UI that uses the text sizing API (such as Make Text Larger/Smaller menu items in Safari).
            case WebMenuItemPDFActualSize:
                [itemCopy setTarget:[[dataSource webFrame] webView]];
                [itemCopy setAction:@selector(makeTextStandardSize:)];
                break;
            case WebMenuItemPDFZoomIn:
                [itemCopy setTarget:[[dataSource webFrame] webView]];
                [itemCopy setAction:@selector(makeTextLarger:)];
                break;
            case WebMenuItemPDFZoomOut:
                [itemCopy setTarget:[[dataSource webFrame] webView]];
                [itemCopy setAction:@selector(makeTextSmaller:)];
                break;
            default:
                break;
        }
    }
    
    [actionsToTags release];
    
    return copiedItems;
}

- (BOOL)_anyPDFTagsFoundInMenu:(NSMenu *)menu
{
    NSEnumerator *e = [[menu itemArray] objectEnumerator];
    NSMenuItem *item;
    while ((item = [e nextObject]) != nil) {
        switch ([item tag]) {
            case WebMenuItemTagOpenWithDefaultApplication:
            case WebMenuItemPDFActualSize:
            case WebMenuItemPDFZoomIn:
            case WebMenuItemPDFZoomOut:
            case WebMenuItemPDFAutoSize:
            case WebMenuItemPDFSinglePage:
            case WebMenuItemPDFFacingPages:
            case WebMenuItemPDFContinuous:
            case WebMenuItemPDFNextPage:
            case WebMenuItemPDFPreviousPage:
                return YES;
        }
    }
    return NO;
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
    // Start with the menu items supplied by PDFKit, with WebKit tags applied
    NSMutableArray *items = [self _menuItemsFromPDFKitForEvent:theEvent];

    // Add in an "Open with <default PDF viewer>" item
    NSString *appName = nil;
    NSImage *appIcon = nil;
    
    applicationInfoForMIMEType([[dataSource response] MIMEType], &appName, &appIcon);
    if (!appName)
        appName = UI_STRING("Finder", "Default application name for Open With context menu");
    
    NSString *title = [NSString stringWithFormat:UI_STRING("Open with %@", "context menu item for PDF"), appName];
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:@selector(openWithFinder:) keyEquivalent:@""];
    [item setTag:WebMenuItemTagOpenWithDefaultApplication];
    if (appIcon)
        [item setImage:appIcon];
    [items insertObject:item atIndex:0];
    [item release];
    
    [items insertObject:[NSMenuItem separatorItem] atIndex:1];
    
    // pass the items off to the WebKit context menu mechanism
    WebView *webView = [[dataSource webFrame] webView];
    ASSERT(webView);
    // Currently clicks anywhere in the PDF view are treated the same, so we just pass NSZeroPoint;
    // we implement elementAtPoint: here just to be slightly forward-looking.
    NSMenu *menu = [webView _menuForElement:[self elementAtPoint:NSZeroPoint] defaultItems:items];
    
    // The delegate has now had the opportunity to add items to the standard PDF-related items, or to
    // remove or modify some of the PDF-related items. In 10.4, the PDF context menu did not go through 
    // the standard WebKit delegate path, and so the standard PDF-related items always appeared. For
    // clients that create their own context menu by hand-picking specific items from the default list, such as
    // Safari, none of the PDF-related items will appear until the client is rewritten to explicitly
    // include these items. So for backwards compatibility we're going to include the entire set of PDF-related
    // items if the executable was linked in 10.4 or earlier and the menu returned from the delegate mechanism
    // contains none of the PDF-related items.
    if (WKExecutableLinkedInTigerOrEarlier()) {
        if (![self _anyPDFTagsFoundInMenu:menu]) {
            [menu addItem:[NSMenuItem separatorItem]];
            NSEnumerator *e = [items objectEnumerator];
            NSMenuItem *menuItem;
            while ((menuItem = [e nextObject]) != nil) {
                // copy menuItem since a given menuItem can be in only one menu at a time, and we don't
                // want to mess with the menu returned from PDFKit.
                [menu addItem:[menuItem copy]];
            }
        }
    }
    
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

// FIXME 4182876: We can eliminate this function in favor if -isEqual: if [PDFSelection isEqual:] is overridden
// to compare contents.
static BOOL PDFSelectionsAreEqual(PDFSelection *selectionA, PDFSelection *selectionB)
{
    NSArray *aPages = [selectionA pages];
    NSArray *bPages = [selectionB pages];
    
    if (![aPages isEqual:bPages]) {
        return NO;
    }
    
    int count = [aPages count];
    int i;
    for (i = 0; i < count; ++i) {
        NSRect aBounds = [selectionA boundsForPage:[aPages objectAtIndex:i]];
        NSRect bBounds = [selectionB boundsForPage:[bPages objectAtIndex:i]];
        if (!NSEqualRects(aBounds, bBounds)) {
            return NO;
        }
    }
    
    return YES;
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag
{
    // Our search algorithm, used in WebCore also, is to search in the selection first. If the found text is the
    // entire selection, then we search again from just past the selection.

    int options = 0;
    if (!forward) {
        options |= NSBackwardsSearch;
    }
    if (!caseFlag) {
        options |= NSCaseInsensitiveSearch;
    }
    PDFDocument *document = [PDFSubview document];
    PDFSelection *oldSelection = [PDFSubview currentSelection];
    
    // Initially we want to include the selected text in the search. PDFDocument's API always searches from just
    // past the passed-in selection, so we need to pass a selection that's modified appropriately. 
    // FIXME 4182863: Ideally we'd use a zero-length selection at the edge of the current selection, but zero-length
    // selections don't work in PDFDocument. So instead we make a one-length selection just before or after the
    // current selection, which works for our purposes even when the current selection is at an edge of the
    // document.
    PDFSelection *selectionForInitialSearch = [oldSelection copy];
    int oldSelectionLength = [[oldSelection string] length];
    if (forward) {
        [selectionForInitialSearch extendSelectionAtStart:1];
        [selectionForInitialSearch extendSelectionAtEnd:-oldSelectionLength];
    } else {
        [selectionForInitialSearch extendSelectionAtEnd:1];
        [selectionForInitialSearch extendSelectionAtStart:-oldSelectionLength];
    }
    PDFSelection *foundSelection = [document findString:string fromSelection:selectionForInitialSearch withOptions:options];
    [selectionForInitialSearch release];
    
    // If we found the selection, search again from just past the selection
    if (PDFSelectionsAreEqual(foundSelection, oldSelection)) {
        foundSelection = [document findString:string fromSelection:oldSelection withOptions:options];
    }
    
    if (foundSelection == nil && wrapFlag) {
        foundSelection = [document findString:string fromSelection:nil withOptions:options];
    }
    if (foundSelection != nil) {
        [PDFSubview setCurrentSelection:foundSelection];
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

// Delegates implementing the following method will be called to handle clicks on URL
// links within the PDFView.  
- (void)PDFViewWillClickOnLink:(PDFView *)sender withURL:(NSURL *)URL
{
    if (URL != nil) {    
        WebFrame *frame = [[self _web_parentWebFrameView] webFrame];
        [frame _safeLoadURL:URL];
    }
}

/*** WebDocumentText protocol implementation ***/

- (BOOL)supportsTextEncoding
{
    return NO;
}

- (NSString *)string
{
    return [[PDFSubview document] string];
}

- (NSAttributedString *)attributedString
{
    // changing the selection is a hack, but the only way to get an attr string is via PDFSelection
    
    // must copy this selection object because we change the selection which seems to release it
    PDFSelection *savedSelection = [[PDFSubview currentSelection] copy];
    [PDFSubview selectAll:nil];
    NSAttributedString *result = [[PDFSubview currentSelection] attributedString];
    if (savedSelection) {
        [PDFSubview setCurrentSelection:savedSelection];
        [savedSelection release];
    } else {
        // FIXME: behavior of setCurrentSelection:nil is not documented - check 4182934 for progress
        // Otherwise, we could collapse this code with the case above.
        [PDFSubview clearSelection];
    }
    return result;
}

- (NSString *)selectedString
{
    return [[PDFSubview currentSelection] string];
}

- (NSAttributedString *)selectedAttributedString
{
    return [[PDFSubview currentSelection] attributedString];
}

- (void)selectAll
{
    [PDFSubview selectAll:nil];
}

- (void)deselectAll
{
    [PDFSubview clearSelection];
}

- (NSRect)selectionRect
{
    NSRect result = NSZeroRect;
    PDFSelection *selection = [PDFSubview currentSelection];
    NSEnumerator *pages = [[selection pages] objectEnumerator];
    PDFPage *page;
    while ((page = [pages nextObject]) != nil) {
        NSRect selectionOnPageInViewCoordinates = [PDFSubview convertRect:[selection boundsForPage:page] fromPage:page];
        if (NSIsEmptyRect(result)) {
            result = selectionOnPageInViewCoordinates;
        } else {
            result = NSUnionRect(result, selectionOnPageInViewCoordinates);
        }
    }
    
    return result;
}

- (NSArray *)pasteboardTypesForSelection
{
    return [NSArray arrayWithObjects:NSRTFDPboardType, NSRTFPboardType, NSStringPboardType, nil];
}

- (void)writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    NSAttributedString *attributedString = [self selectedAttributedString];
    
    if ([types containsObject:NSRTFDPboardType]) {
        NSData *RTFDData = [attributedString RTFDFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:nil];
        [pasteboard setData:RTFDData forType:NSRTFDPboardType];
    }        
    
    if ([types containsObject:NSRTFPboardType]) {
        if ([attributedString containsAttachments]) {
            attributedString = [attributedString _web_attributedStringByStrippingAttachmentCharacters];
        }
        NSData *RTFData = [attributedString RTFFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:nil];
        [pasteboard setData:RTFData forType:NSRTFPboardType];
    }
    
    if ([types containsObject:NSStringPboardType]) {
        [pasteboard setString:[self selectedString] forType:NSStringPboardType];
    }
}

@end

#endif // OMIT_TIGER_FEATURES
