/*
    WebHTMLView.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebHTMLView.h>

#import <WebKit/DOM.h>
#import <WebKit/DOMExtensions.h>
#import <WebKit/WebArchive.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebClipView.h>
#import <WebKit/WebDataProtocol.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDocumentInternal.h>
#import <WebKit/WebDOMOperationsPrivate.h>
#import <WebKit/WebEditingDelegate.h>
#import <WebKit/WebException.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebHTMLViewInternal.h>
#import <WebKit/WebHTMLRepresentationPrivate.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNSEventExtras.h>
#import <WebKit/WebNSImageExtras.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSPrintOperationExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebResourcePrivate.h>
#import <WebKit/WebStringTruncator.h>
#import <WebKit/WebTextRenderer.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebUIDelegatePrivate.h>
#import <WebKit/WebUnicode.h>
#import <WebKit/WebViewInternal.h>
#import <WebKit/WebViewPrivate.h>

#import <AppKit/NSAccessibility.h>
#import <AppKit/NSGraphicsContextPrivate.h>
#import <AppKit/NSResponder_Private.h>

#import <Foundation/NSFileManager_NSURLExtras.h>
#import <Foundation/NSURL_NSURLExtras.h>
#import <Foundation/NSURLFileTypeMappings.h>

#import <CoreGraphics/CGContextGState.h>

// Included to help work around this bug:
// <rdar://problem/3630640>: "Calling interpretKeyEvents: in a custom text view can fail to process keys right after app startup"
#import <AppKit/NSKeyBindingManager.h>

// By imaging to a width a little wider than the available pixels,
// thin pages will be scaled down a little, matching the way they
// print in IE and Camino. This lets them use fewer sheets than they
// would otherwise, which is presumably why other browsers do this.
// Wide pages will be scaled down more than this.
#define PrintingMinimumShrinkFactor     1.25

// This number determines how small we are willing to reduce the page content
// in order to accommodate the widest line. If the page would have to be
// reduced smaller to make the widest line fit, we just clip instead (this
// behavior matches MacIE and Mozilla, at least)
#define PrintingMaximumShrinkFactor     2.0

#define AUTOSCROLL_INTERVAL             0.1

#define DRAG_LABEL_BORDER_X             4.0
#define DRAG_LABEL_BORDER_Y             2.0
#define DRAG_LABEL_RADIUS               5.0
#define DRAG_LABEL_BORDER_Y_OFFSET              2.0

#define MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP        120.0
#define MAX_DRAG_LABEL_WIDTH                    320.0

#define DRAG_LINK_LABEL_FONT_SIZE   11.0
#define DRAG_LINK_URL_FONT_SIZE   10.0

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3
#define BUILT_ON_TIGER_OR_LATER
#endif

static BOOL forceRealHitTest = NO;

@interface WebHTMLView (WebTextSizing) <_web_WebDocumentTextSizing>
@end

@interface WebHTMLView (WebFileInternal)
- (BOOL)_imageExistsAtPaths:(NSArray *)paths;
- (DOMDocumentFragment *)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard allowPlainText:(BOOL)allowPlainText;
- (void)_pasteWithPasteboard:(NSPasteboard *)pasteboard allowPlainText:(BOOL)allowPlainText;
- (BOOL)_shouldInsertFragment:(DOMDocumentFragment *)fragment replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action;
- (BOOL)_shouldReplaceSelectionWithText:(NSString *)text givenAction:(WebViewInsertAction)action;
@end

@interface WebHTMLView (WebHTMLViewPrivate)
- (void)_setPrinting:(BOOL)printing minimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustViewSize:(BOOL)adjustViewSize;
- (void)_updateTextSizeMultiplier;
- (float)_calculatePrintHeight;
- (float)_scaleFactorForPrintOperation:(NSPrintOperation *)printOperation;
@end

@interface WebHTMLView (WebNSTextInputSupport) <NSTextInput>
- (void)_updateSelectionForInputManager;
@end

// Any non-zero value will do, but using somethign recognizable might help us debug some day.
#define TRACKING_RECT_TAG 0xBADFACE

@interface NSView (AppKitSecretsIKnowAbout)
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView;
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect;
- (NSRect)_dirtyRect;
- (void)_setDrawsOwnDescendants:(BOOL)drawsOwnDescendants;
@end

@interface NSApplication (AppKitSecretsIKnowAbout)
- (void)speakString:(NSString *)string;
@end

@interface NSWindow (AppKitSecretsIKnowAbout)
- (id)_newFirstResponderAfterResigning;
@end

@interface NSView (WebHTMLViewPrivate)
- (void)_web_setPrintingModeRecursive;
- (void)_web_clearPrintingModeRecursive;
- (void)_web_layoutIfNeededRecursive:(NSRect)rect testDirtyRect:(bool)testDirtyRect;
@end

@interface NSMutableDictionary (WebHTMLViewPrivate)
- (void)_web_setObjectIfNotNil:(id)object forKey:(id)key;
@end

@interface WebElementOrTextFilter : NSObject <DOMNodeFilter>
+ (WebElementOrTextFilter *)filter;
@end

@interface NSAttributedString(AppKitSecretsIKnowAbout)
- (id)_initWithDOMRange:(DOMRange *)domRange;
- (DOMDocumentFragment *)_documentFromRange:(NSRange)range document:(DOMDocument *)document documentAttributes:(NSDictionary *)dict subresources:(NSArray **)subresources;
@end

static WebElementOrTextFilter *elementOrTextFilterInstance = nil;

// Handles the complete: text command
@interface WebTextCompleteController : NSObject
{
@private
    WebHTMLView *_view;
    NSWindow *_popupWindow;
    NSTableView *_tableView;
    NSArray *_completions;
    NSString *_originalString;
    int prefixLength;
}
- (id)initWithHTMLView:(WebHTMLView *)view;
- (void)doCompletion;
- (void)endRevertingChange:(BOOL)revertChange moveLeft:(BOOL)goLeft;
- (BOOL)filterKeyDown:(NSEvent *)event;
- (void)_reflectSelection;
@end

@implementation WebHTMLViewPrivate

- (void)dealloc
{
    ASSERT(autoscrollTimer == nil);
    ASSERT(autoscrollTriggerEvent == nil);
    
    [mouseDownEvent release];
    [draggingImageURL release];
    [pluginController release];
    [toolTip release];
    [compController release];

    [super dealloc];
}

@end

@implementation WebHTMLView (WebFileInternal)

- (BOOL)_imageExistsAtPaths:(NSArray *)paths
{
    NSURLFileTypeMappings *mappings = [NSURLFileTypeMappings sharedMappings];
    NSArray *imageMIMETypes = [[WebImageRendererFactory sharedFactory] supportedMIMETypes];
    NSEnumerator *enumerator = [paths objectEnumerator];
    NSString *path;
    
    while ((path = [enumerator nextObject]) != nil) {
        NSString *MIMEType = [mappings MIMETypeForExtension:[path pathExtension]];
        if ([imageMIMETypes containsObject:MIMEType]) {
            return YES;
        }
    }
    
    return NO;
}

- (DOMDocumentFragment *)_documentFragmentWithPaths:(NSArray *)paths
{
    DOMDocumentFragment *fragment = [[[self _bridge] DOMDocument] createDocumentFragment];
    NSURLFileTypeMappings *mappings = [NSURLFileTypeMappings sharedMappings];
    NSArray *imageMIMETypes = [[WebImageRendererFactory sharedFactory] supportedMIMETypes];
    NSEnumerator *enumerator = [paths objectEnumerator];
    WebDataSource *dataSource = [self _dataSource];
    NSString *path;
    
    while ((path = [enumerator nextObject]) != nil) {
        NSString *MIMEType = [mappings MIMETypeForExtension:[path pathExtension]];
        if ([imageMIMETypes containsObject:MIMEType]) {
            WebResource *resource = [[WebResource alloc] initWithData:[NSData dataWithContentsOfFile:path]
                                                                  URL:[NSURL fileURLWithPath:path]
                                                             MIMEType:MIMEType 
                                                     textEncodingName:nil
                                                            frameName:nil];
            if (resource) {
                [fragment appendChild:[dataSource _imageElementWithImageResource:resource]];
                [resource release];
            }
        }
    }
    
    return [fragment firstChild] != nil ? fragment : nil;
}

- (DOMDocumentFragment *)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard allowPlainText:(BOOL)allowPlainText
{
    NSArray *types = [pasteboard types];

    if ([types containsObject:WebArchivePboardType]) {
        WebArchive *archive = [[WebArchive alloc] initWithData:[pasteboard dataForType:WebArchivePboardType]];
        if (archive) {
            DOMDocumentFragment *fragment = [[self _dataSource] _documentFragmentWithArchive:archive];
            [archive release];
            if (fragment) {
                return fragment;
            }
        }
    }
    
    if ([types containsObject:NSFilenamesPboardType]) {
        DOMDocumentFragment *fragment = [self _documentFragmentWithPaths:[pasteboard propertyListForType:NSFilenamesPboardType]];
        if (fragment != nil) {
            return fragment;
        }
    }
    
    NSURL *URL;
    
    if ([types containsObject:NSHTMLPboardType]) {
        return [[self _bridge] documentFragmentWithMarkupString:[pasteboard stringForType:NSHTMLPboardType] baseURLString:nil];
    }
    
    if ([types containsObject:NSTIFFPboardType]) {
        WebResource *resource = [[WebResource alloc] initWithData:[pasteboard dataForType:NSTIFFPboardType]
                                                              URL:[NSURL _web_uniqueWebDataURLWithRelativeString:@"/image.tiff"]
                                                         MIMEType:@"image/tiff" 
                                                 textEncodingName:nil
                                                        frameName:nil];
        DOMDocumentFragment *fragment = [[self _dataSource] _documentFragmentWithImageResource:resource];
        [resource release];
        return fragment;
    }
    
    if ([types containsObject:NSPICTPboardType]) {
        WebResource *resource = [[WebResource alloc] initWithData:[pasteboard dataForType:NSPICTPboardType]
                                                              URL:[NSURL _web_uniqueWebDataURLWithRelativeString:@"/image.pict"]
                                                         MIMEType:@"image/pict" 
                                                 textEncodingName:nil
                                                        frameName:nil];
        DOMDocumentFragment *fragment = [[self _dataSource] _documentFragmentWithImageResource:resource];
        [resource release];
        return fragment;
    }
    
#ifdef BUILT_ON_TIGER_OR_LATER
    NSAttributedString *string = nil;
    if ([types containsObject:NSRTFDPboardType]) {
        string = [[NSAttributedString alloc] initWithRTFD:[pasteboard dataForType:NSRTFDPboardType] documentAttributes:NULL];
    }
    if (string == nil && [types containsObject:NSRTFPboardType]) {
        string = [[NSAttributedString alloc] initWithRTF:[pasteboard dataForType:NSRTFPboardType] documentAttributes:NULL];
    }
    if (string != nil) {
        NSArray *elements = [[NSArray alloc] initWithObjects:@"style", nil];
        NSDictionary *documentAttributes = [[NSDictionary alloc] initWithObjectsAndKeys:elements, NSExcludedElementsDocumentAttribute, nil];
        [elements release];
        NSArray *subresources;
        DOMDocumentFragment *fragment = [string _documentFromRange:NSMakeRange(0, [string length]) 
                                                          document:[[self _bridge] DOMDocument] 
                                                documentAttributes:documentAttributes
                                                      subresources:&subresources];
        [documentAttributes release];
        [string release];
        if (fragment) {
            if ([subresources count] != 0) {
                [[self _dataSource] _addSubresources:subresources];
            }
            return fragment;
        }
    }
#endif
    
    if ((URL = [NSURL URLFromPasteboard:pasteboard])) {
        NSString *URLString = [URL _web_userVisibleString];
        if ([URLString length] > 0) {
            return [[self _bridge] documentFragmentWithText:URLString];
        }
    }
    
    if (allowPlainText && [types containsObject:NSStringPboardType]) {
        return [[self _bridge] documentFragmentWithText:[pasteboard stringForType:NSStringPboardType]];
    }
    
    return nil;
}

- (void)_pasteWithPasteboard:(NSPasteboard *)pasteboard allowPlainText:(BOOL)allowPlainText
{
    DOMDocumentFragment *fragment = [self _documentFragmentFromPasteboard:pasteboard allowPlainText:allowPlainText];
    WebBridge *bridge = [self _bridge];
    if (fragment && [self _shouldInsertFragment:fragment replacingDOMRange:[bridge selectedDOMRange] givenAction:WebViewInsertActionPasted]) {
        [bridge replaceSelectionWithFragment:fragment selectReplacement:NO];
    }
}

- (BOOL)_shouldInsertFragment:(DOMDocumentFragment *)fragment replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action
{
    WebView *webView = [self _webView];
    DOMNode *child = [fragment firstChild];
    if ([fragment lastChild] == child && [child isKindOfClass:[DOMCharacterData class]]) {
        return [[webView _editingDelegateForwarder] webView:webView shouldInsertText:[(DOMCharacterData *)child data] replacingDOMRange:range givenAction:action];
    } else {
        return [[webView _editingDelegateForwarder] webView:webView shouldInsertNode:fragment replacingDOMRange:range givenAction:action];
    }
}

- (BOOL)_shouldReplaceSelectionWithText:(NSString *)text givenAction:(WebViewInsertAction)action
{
    WebView *webView = [self _webView];
    DOMRange *selectedRange = [[self _bridge] selectedDOMRange];

    return [[webView _editingDelegateForwarder] webView:webView shouldInsertText:text replacingDOMRange:selectedRange givenAction:action];
}

@end

@implementation WebHTMLView (WebPrivate)

- (void)_reset
{
    [WebImageRenderer stopAnimationsInView:self];
}

- (WebView *)_webView
{
    // We used to use the view hierarchy exclusively here, but that won't work
    // right when the first viewDidMoveToSuperview call is done, and this wil.
    return [[self _frame] webView];
}

- (WebFrame *)_frame
{
    WebFrameView *webFrameView = [self _web_parentWebFrameView];
    return [webFrameView webFrame];
}

// Required so view can access the part's selection.
- (WebBridge *)_bridge
{
    return [[self _frame] _bridge];
}

- (WebDataSource *)_dataSource
{
    return [[self _frame] dataSource];
}

+ (void)_postFlagsChangedEvent:(NSEvent *)flagsChangedEvent
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved
        location:[[flagsChangedEvent window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[flagsChangedEvent modifierFlags]
        timestamp:[flagsChangedEvent timestamp]
        windowNumber:[flagsChangedEvent windowNumber]
        context:[flagsChangedEvent context]
        eventNumber:0 clickCount:0 pressure:0];

    // Pretend it's a mouse move.
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSMouseMovedNotification object:self
        userInfo:[NSDictionary dictionaryWithObject:fakeEvent forKey:@"NSEvent"]];
}

- (void)_updateMouseoverWithFakeEvent
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved
        location:[[self window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[[NSApp currentEvent] modifierFlags]
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[[self window] windowNumber]
        context:[[NSApp currentEvent] context]
        eventNumber:0 clickCount:0 pressure:0];
    
    [self _updateMouseoverWithEvent:fakeEvent];
}

- (void)_frameOrBoundsChanged
{
    if (!NSEqualSizes(_private->lastLayoutSize, [(NSClipView *)[self superview] documentVisibleRect].size)) {
        [self setNeedsLayout:YES];
        [self setNeedsDisplay:YES];
        [_private->compController endRevertingChange:NO moveLeft:NO];
    }

    NSPoint origin = [[self superview] bounds].origin;
    if (!NSEqualPoints(_private->lastScrollPosition, origin)) {
        [[self _bridge] sendScrollEvent];
        [_private->compController endRevertingChange:NO moveLeft:NO];
    }
    _private->lastScrollPosition = origin;

    SEL selector = @selector(_updateMouseoverWithFakeEvent);
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:selector object:nil];
    [self performSelector:selector withObject:nil afterDelay:0];
}

- (void)_setAsideSubviews
{
    ASSERT(!_private->subviewsSetAside);
    ASSERT(_private->savedSubviews == nil);
    _private->savedSubviews = _subviews;
    _subviews = nil;
    _private->subviewsSetAside = YES;
 }
 
 - (void)_restoreSubviews
 {
    ASSERT(_private->subviewsSetAside);
    ASSERT(_subviews == nil);
    _subviews = _private->savedSubviews;
    _private->savedSubviews = nil;
    _private->subviewsSetAside = NO;
}

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView
{
    // This helps when we print as part of a larger print process.
    // If the WebHTMLView itself is what we're printing, then we will never have to do this.
    BOOL wasInPrintingMode = _private->printing;
    BOOL isPrinting = ![NSGraphicsContext currentContextDrawingToScreen];
    if (wasInPrintingMode != isPrinting) {
        if (isPrinting) {
            [self _web_setPrintingModeRecursive];
        } else {
            [self _web_clearPrintingModeRecursive];
        }
    }

    [self _web_layoutIfNeededRecursive: rect testDirtyRect:YES];

    [self _setAsideSubviews];
    [super _recursiveDisplayRectIfNeededIgnoringOpacity:rect isVisibleRect:isVisibleRect
        rectIsVisibleRectForView:visibleView topView:topView];
    [self _restoreSubviews];

    if (wasInPrintingMode != isPrinting) {
        if (wasInPrintingMode) {
            [self _web_setPrintingModeRecursive];
        } else {
            [self _web_clearPrintingModeRecursive];
        }
    }
}

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect
{
    BOOL needToSetAsideSubviews = !_private->subviewsSetAside;

    BOOL wasInPrintingMode = _private->printing;
    BOOL isPrinting = ![NSGraphicsContext currentContextDrawingToScreen];

    if (needToSetAsideSubviews) {
        // This helps when we print as part of a larger print process.
        // If the WebHTMLView itself is what we're printing, then we will never have to do this.
        if (wasInPrintingMode != isPrinting) {
            if (isPrinting) {
                [self _web_setPrintingModeRecursive];
            } else {
                [self _web_clearPrintingModeRecursive];
            }
        }

        [self _web_layoutIfNeededRecursive: visRect testDirtyRect:NO];

        [self _setAsideSubviews];
    }
    
    [super _recursiveDisplayAllDirtyWithLockFocus:needsLockFocus visRect:visRect];
    
    if (needToSetAsideSubviews) {
        if (wasInPrintingMode != isPrinting) {
            if (wasInPrintingMode) {
                [self _web_setPrintingModeRecursive];
            } else {
                [self _web_clearPrintingModeRecursive];
            }
        }

        [self _restoreSubviews];
    }
}

- (BOOL)_insideAnotherHTMLView
{
    NSView *view = self;
    while ((view = [view superview])) {
        if ([view isKindOfClass:[WebHTMLView class]]) {
            return YES;
        }
    }
    return NO;
}

- (void)scrollPoint:(NSPoint)point
{
    // Since we can't subclass NSTextView to do what we want, we have to second guess it here.
    // If we get called during the handling of a key down event, we assume the call came from
    // NSTextView, and ignore it and use our own code to decide how to page up and page down
    // We are smarter about how far to scroll, and we have "superview scrolling" logic.
    NSEvent *event = [[self window] currentEvent];
    if ([event type] == NSKeyDown) {
        const unichar pageUp = NSPageUpFunctionKey;
        if ([[event characters] rangeOfString:[NSString stringWithCharacters:&pageUp length:1]].length == 1) {
            [self tryToPerform:@selector(scrollPageUp:) with:nil];
            return;
        }
        const unichar pageDown = NSPageDownFunctionKey;
        if ([[event characters] rangeOfString:[NSString stringWithCharacters:&pageDown length:1]].length == 1) {
            [self tryToPerform:@selector(scrollPageDown:) with:nil];
            return;
        }
    }
    
    [super scrollPoint:point];
}

- (NSView *)hitTest:(NSPoint)point
{
    // WebHTMLView objects handle all left mouse clicks for objects inside them.
    // That does not include left mouse clicks with the control key held down.
    BOOL captureHitsOnSubviews;
    if (forceRealHitTest) {
        captureHitsOnSubviews = NO;
    } else {
        NSEvent *event = [[self window] currentEvent];
        captureHitsOnSubviews = [event type] == NSLeftMouseDown && ([event modifierFlags] & NSControlKeyMask) == 0;
    }
    if (!captureHitsOnSubviews) {
        return [super hitTest:point];
    }
    if ([[self superview] mouse:point inRect:[self frame]]) {
        return self;
    }
    return nil;
}

static WebHTMLView *lastHitView = nil;

- (void)_clearLastHitViewIfSelf
{
    if (lastHitView == self) {
        lastHitView = nil;
    }
}

- (NSTrackingRectTag)addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside
{
    ASSERT(_private->trackingRectOwner == nil);
    _private->trackingRectOwner = owner;
    _private->trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside useTrackingNum:(int)tag
{
    ASSERT(tag == TRACKING_RECT_TAG);
    return [self addTrackingRect:rect owner:owner userData:data assumeInside:assumeInside];
}

- (void)removeTrackingRect:(NSTrackingRectTag)tag
{
    ASSERT(tag == TRACKING_RECT_TAG);
    if (_private != nil) {
        _private->trackingRectOwner = nil;
    }
}

- (void)_sendToolTipMouseExited
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseExited
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_private->trackingRectUserData];
    [_private->trackingRectOwner mouseExited:fakeEvent];
}

- (void)_sendToolTipMouseEntered
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseEntered
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_private->trackingRectUserData];
    [_private->trackingRectOwner mouseEntered:fakeEvent];
}

- (void)_setToolTip:(NSString *)string
{
    NSString *toolTip = [string length] == 0 ? nil : string;
    NSString *oldToolTip = _private->toolTip;
    if ((toolTip == nil || oldToolTip == nil) ? toolTip == oldToolTip : [toolTip isEqualToString:oldToolTip]) {
        return;
    }
    if (oldToolTip) {
        [self _sendToolTipMouseExited];
        [oldToolTip release];
    }
    _private->toolTip = [toolTip copy];
    if (toolTip) {
        [self removeAllToolTips];
        NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
        [self addToolTipRect:wideOpenRect owner:self userData:NULL];
        [self _sendToolTipMouseEntered];
    }
}

- (NSString *)view:(NSView *)view stringForToolTip:(NSToolTipTag)tag point:(NSPoint)point userData:(void *)data
{
    return [[_private->toolTip copy] autorelease];
}

- (void)_updateMouseoverWithEvent:(NSEvent *)event
{
    WebHTMLView *view = nil;
    if ([event window] == [self window]) {
        forceRealHitTest = YES;
        NSView *hitView = [[[self window] contentView] hitTest:[event locationInWindow]];
        forceRealHitTest = NO;
        while (hitView) {
            if ([hitView isKindOfClass:[WebHTMLView class]]) {
                view = (WebHTMLView *)hitView;
                break;
            }
            hitView = [hitView superview];
        }
    }

    if (lastHitView != view && lastHitView != nil) {
        // If we are moving out of a view (or frame), let's pretend the mouse moved
        // all the way out of that view. But we have to account for scrolling, because
        // khtml doesn't understand our clipping.
        NSRect visibleRect = [[[[lastHitView _frame] frameView] _scrollView] documentVisibleRect];
        float yScroll = visibleRect.origin.y;
        float xScroll = visibleRect.origin.x;

        event = [NSEvent mouseEventWithType:NSMouseMoved
                         location:NSMakePoint(-1 - xScroll, -1 - yScroll )
                         modifierFlags:[[NSApp currentEvent] modifierFlags]
                         timestamp:[NSDate timeIntervalSinceReferenceDate]
                         windowNumber:[[self window] windowNumber]
                         context:[[NSApp currentEvent] context]
                         eventNumber:0 clickCount:0 pressure:0];
        [[lastHitView _bridge] mouseMoved:event];
    }

    lastHitView = view;
    
    NSDictionary *element;
    if (view == nil) {
        element = nil;
    } else {
        [[view _bridge] mouseMoved:event];

        NSPoint point = [view convertPoint:[event locationInWindow] fromView:nil];
        element = [view elementAtPoint:point];
    }

    // Have the web view send a message to the delegate so it can do status bar display.
    [[self _webView] _mouseDidMoveOverElement:element modifierFlags:[event modifierFlags]];

    // Set a tool tip; it won't show up right away but will if the user pauses.
    [self _setToolTip:[element objectForKey:WebCoreElementTitleKey]];
}

+ (NSArray *)_insertablePasteboardTypes
{
    static NSArray *types = nil;
    if (!types) {
        types = [[NSArray alloc] initWithObjects:WebArchivePboardType, NSHTMLPboardType,
            NSFilenamesPboardType, NSTIFFPboardType, NSPICTPboardType, NSURLPboardType, 
            NSRTFDPboardType, NSRTFPboardType, NSStringPboardType, nil];
    }
    return types;
}

+ (NSArray *)_selectionPasteboardTypes
{
    // FIXME: We should put data for NSHTMLPboardType on the pasteboard but Microsoft Excel doesn't like our format of HTML (3640423).
    return [NSArray arrayWithObjects:WebArchivePboardType, NSRTFPboardType, NSRTFDPboardType, NSStringPboardType, nil];
}

- (WebArchive *)_selectedArchive
{
    NSArray *nodes;
    NSString *markupString = [[self _bridge] markupStringFromRange:[[self _bridge] selectedDOMRange] nodes:&nodes];
    return [[self _dataSource] _archiveWithMarkupString:markupString nodes:nodes];
}

- (NSData *)_selectedRTFData
{
    NSAttributedString *attributedString = [self selectedAttributedString];
    NSRange range = NSMakeRange(0, [attributedString length]);
    return [attributedString RTFFromRange:range documentAttributes:nil];
}

- (void)_writeSelectionToPasteboard:(NSPasteboard *)pasteboard
{
    ASSERT([self _hasSelection]);
    NSArray *types = [[self class] _selectionPasteboardTypes];
    [pasteboard declareTypes:types owner:nil];
    [self writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard];
}

- (NSImage *)_dragImageForLinkElement:(NSDictionary *)element
{
    NSURL *linkURL = [element objectForKey: WebElementLinkURLKey];

    BOOL drawURLString = YES;
    BOOL clipURLString = NO, clipLabelString = NO;
    
    NSString *label = [element objectForKey: WebElementLinkLabelKey];
    NSString *urlString = [linkURL _web_userVisibleString];
    
    if (!label) {
        drawURLString = NO;
        label = urlString;
    }
    
    NSFont *labelFont = [[NSFontManager sharedFontManager] convertFont:[NSFont systemFontOfSize:DRAG_LINK_LABEL_FONT_SIZE]
                                                   toHaveTrait:NSBoldFontMask];
    NSFont *urlFont = [NSFont systemFontOfSize: DRAG_LINK_URL_FONT_SIZE];
    NSSize labelSize;
    labelSize.width = [label _web_widthWithFont: labelFont];
    labelSize.height = [labelFont ascender] - [labelFont descender];
    if (labelSize.width > MAX_DRAG_LABEL_WIDTH){
        labelSize.width = MAX_DRAG_LABEL_WIDTH;
        clipLabelString = YES;
    }
    
    NSSize imageSize, urlStringSize;
    imageSize.width = labelSize.width + DRAG_LABEL_BORDER_X * 2;
    imageSize.height = labelSize.height + DRAG_LABEL_BORDER_Y * 2;
    if (drawURLString) {
        urlStringSize.width = [urlString _web_widthWithFont: urlFont];
        urlStringSize.height = [urlFont ascender] - [urlFont descender];
        imageSize.height += urlStringSize.height;
        if (urlStringSize.width > MAX_DRAG_LABEL_WIDTH) {
            imageSize.width = MAX(MAX_DRAG_LABEL_WIDTH + DRAG_LABEL_BORDER_X * 2, MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP);
            clipURLString = YES;
        } else {
            imageSize.width = MAX(labelSize.width + DRAG_LABEL_BORDER_X * 2, urlStringSize.width + DRAG_LABEL_BORDER_X * 2);
        }
    }
    NSImage *dragImage = [[[NSImage alloc] initWithSize: imageSize] autorelease];
    [dragImage lockFocus];
    
    [[NSColor colorWithCalibratedRed: 0.7 green: 0.7 blue: 0.7 alpha: 0.8] set];
    
    // Drag a rectangle with rounded corners/
    NSBezierPath *path = [NSBezierPath bezierPath];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0,0, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0,imageSize.height - DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS * 2, imageSize.height - DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS * 2,0, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    
    [path appendBezierPathWithRect: NSMakeRect(DRAG_LABEL_RADIUS, 0, imageSize.width - DRAG_LABEL_RADIUS * 2, imageSize.height)];
    [path appendBezierPathWithRect: NSMakeRect(0, DRAG_LABEL_RADIUS, DRAG_LABEL_RADIUS + 10, imageSize.height - 2 * DRAG_LABEL_RADIUS)];
    [path appendBezierPathWithRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS - 20,DRAG_LABEL_RADIUS, DRAG_LABEL_RADIUS + 20, imageSize.height - 2 * DRAG_LABEL_RADIUS)];
    [path fill];
        
    NSColor *topColor = [NSColor colorWithCalibratedWhite:0.0 alpha:0.75];
    NSColor *bottomColor = [NSColor colorWithCalibratedWhite:1.0 alpha:0.5];
    if (drawURLString) {
        if (clipURLString)
            urlString = [WebStringTruncator centerTruncateString: urlString toWidth:imageSize.width - (DRAG_LABEL_BORDER_X * 2) withFont:urlFont];

        [urlString _web_drawDoubledAtPoint:NSMakePoint(DRAG_LABEL_BORDER_X, DRAG_LABEL_BORDER_Y - [urlFont descender]) 
             withTopColor:topColor bottomColor:bottomColor font:urlFont];
    }

    if (clipLabelString)
        label = [WebStringTruncator rightTruncateString: label toWidth:imageSize.width - (DRAG_LABEL_BORDER_X * 2) withFont:labelFont];
    [label _web_drawDoubledAtPoint:NSMakePoint (DRAG_LABEL_BORDER_X, imageSize.height - DRAG_LABEL_BORDER_Y_OFFSET - [labelFont pointSize])
             withTopColor:topColor bottomColor:bottomColor font:labelFont];
    
    [dragImage unlockFocus];
    
    return dragImage;
}

- (BOOL)_startDraggingImage:(NSImage *)wcDragImage at:(NSPoint)wcDragLoc operation:(NSDragOperation)op event:(NSEvent *)mouseDraggedEvent sourceIsDHTML:(BOOL)srcIsDHTML DHTMLWroteData:(BOOL)dhtmlWroteData
{
    NSPoint mouseDownPoint = [self convertPoint:[_private->mouseDownEvent locationInWindow] fromView:nil];
    NSDictionary *element = [self elementAtPoint:mouseDownPoint];

    NSURL *linkURL = [element objectForKey:WebElementLinkURLKey];
    NSURL *imageURL = [element objectForKey:WebElementImageURLKey];
    BOOL isSelected = [[element objectForKey:WebElementIsSelectedKey] boolValue];

    [_private->draggingImageURL release];
    _private->draggingImageURL = nil;

    NSPoint mouseDraggedPoint = [self convertPoint:[mouseDraggedEvent locationInWindow] fromView:nil];
    _private->webCoreDragOp = op;     // will be DragNone if WebCore doesn't care
    NSImage *dragImage = nil;
    NSPoint dragLoc;

    // We allow WebCore to override the drag image, even if its a link, image or text we're dragging.
    // This is in the spirit of the IE API, which allows overriding of pasteboard data and DragOp.
    // We could verify that ActionDHTML is allowed, although WebCore does claim to respect the action.
    if (wcDragImage) {
        dragImage = wcDragImage;
        // wcDragLoc is the cursor position relative to the lower-left corner of the image.
        // We add in the Y dimension because we are a flipped view, so adding moves the image down.
        if (linkURL) {
            // see HACK below
            dragLoc = NSMakePoint(mouseDraggedPoint.x - wcDragLoc.x, mouseDraggedPoint.y + wcDragLoc.y);
        } else {
            dragLoc = NSMakePoint(mouseDownPoint.x - wcDragLoc.x, mouseDownPoint.y + wcDragLoc.y);
        }
        _private->dragOffset = wcDragLoc;
    }
    
    WebView *webView = [self _webView];
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    WebImageRenderer *image = [element objectForKey:WebElementImageKey];
    BOOL startedDrag = YES;  // optimism - we almost always manage to start the drag

    // note per kwebster, the offset arg below is always ignored in positioning the image
    if (imageURL != nil && image != nil && (_private->dragSourceActionMask & WebDragSourceActionImage)) {
        id source = self;
        if (!dhtmlWroteData) {
            _private->draggingImageURL = [imageURL retain];
            source = [pasteboard _web_declareAndWriteDragImage:image
                                                           URL:linkURL ? linkURL : imageURL
                                                         title:[element objectForKey:WebElementImageAltStringKey]
                                                       archive:[[element objectForKey:WebElementDOMNodeKey] webArchive]
                                                        source:self];
        }
        [[webView _UIDelegateForwarder] webView:webView willPerformDragSourceAction:WebDragSourceActionImage fromPoint:mouseDownPoint withPasteboard:pasteboard];
        if (dragImage == nil) {
            [self _web_dragImage:[element objectForKey:WebElementImageKey]
                            rect:[[element objectForKey:WebElementImageRectKey] rectValue]
                           event:_private->mouseDownEvent
                      pasteboard:pasteboard
                          source:source
                          offset:&_private->dragOffset];
        } else {
            [self dragImage:dragImage
                         at:dragLoc
                     offset:NSZeroSize
                      event:_private->mouseDownEvent
                 pasteboard:pasteboard
                     source:source
                  slideBack:YES];
        }
    } else if (linkURL && (_private->dragSourceActionMask & WebDragSourceActionLink)) {
        if (!dhtmlWroteData) {
            NSArray *types = [NSPasteboard _web_writableTypesForURL];
            [pasteboard declareTypes:types owner:self];
            [pasteboard _web_writeURL:linkURL andTitle:[element objectForKey:WebElementLinkLabelKey] types:types];            
        }
        [[webView _UIDelegateForwarder] webView:webView willPerformDragSourceAction:WebDragSourceActionLink fromPoint:mouseDownPoint withPasteboard:pasteboard];
        if (dragImage == nil) {
            dragImage = [self _dragImageForLinkElement:element];
            NSSize offset = NSMakeSize([dragImage size].width / 2, -DRAG_LABEL_BORDER_Y);
            dragLoc = NSMakePoint(mouseDraggedPoint.x - offset.width, mouseDraggedPoint.y - offset.height);
            _private->dragOffset.x = offset.width;
            _private->dragOffset.y = -offset.height;        // inverted because we are flipped
        }
        // HACK:  We should pass the mouseDown event instead of the mouseDragged!  This hack gets rid of
        // a flash of the image at the mouseDown location when the drag starts.
        [self dragImage:dragImage
                     at:dragLoc
                 offset:NSZeroSize
                  event:mouseDraggedEvent
             pasteboard:pasteboard
                 source:self
              slideBack:YES];
    } else if (isSelected && (_private->dragSourceActionMask & WebDragSourceActionSelection)) {
        if (!dhtmlWroteData) {
            [self _writeSelectionToPasteboard:pasteboard];
        }
        [[webView _UIDelegateForwarder] webView:webView willPerformDragSourceAction:WebDragSourceActionSelection fromPoint:mouseDownPoint withPasteboard:pasteboard];
        if (dragImage == nil) {
            dragImage = [[self _bridge] selectionImage];
            [dragImage _web_dissolveToFraction:WebDragImageAlpha];
            NSRect visibleSelectionRect = [[self _bridge] visibleSelectionRect];
            dragLoc = NSMakePoint(NSMinX(visibleSelectionRect), NSMaxY(visibleSelectionRect));
            _private->dragOffset.x = mouseDownPoint.x - dragLoc.x;
            _private->dragOffset.y = dragLoc.y - mouseDownPoint.y;        // inverted because we are flipped
        }
        [self dragImage:dragImage
                     at:dragLoc
                 offset:NSZeroSize
                  event:_private->mouseDownEvent
             pasteboard:pasteboard
                 source:self
              slideBack:YES];
    } else if (srcIsDHTML) {
        ASSERT(_private->dragSourceActionMask & WebDragSourceActionDHTML);
        [[webView _UIDelegateForwarder] webView:webView willPerformDragSourceAction:WebDragSourceActionDHTML fromPoint:mouseDownPoint withPasteboard:pasteboard];
        if (dragImage == nil) {
            // WebCore should have given us an image, but we'll make one up
            NSString *imagePath = [[NSBundle bundleForClass:[self class]] pathForResource:@"missing_image" ofType:@"tiff"];
            dragImage = [[[NSImage alloc] initWithContentsOfFile:imagePath] autorelease];
            NSSize imageSize = [dragImage size];
            dragLoc = NSMakePoint(mouseDownPoint.x - imageSize.width/2, mouseDownPoint.y + imageSize.height/2);
            _private->dragOffset.x = imageSize.width/2;
            _private->dragOffset.y = imageSize.height/2;        // inverted because we are flipped
        }
        [self dragImage:dragImage
                     at:dragLoc
                 offset:NSZeroSize
                  event:_private->mouseDownEvent
             pasteboard:pasteboard
                 source:self
              slideBack:YES];
    } else {
        // Only way I know if to get here is if the original element clicked on in the mousedown is no longer
        // under the mousedown point, so linkURL, imageURL and isSelected are all false/nil.
        startedDrag = NO;
    }
    return startedDrag;
}

- (void)_handleAutoscrollForMouseDragged:(NSEvent *)event
{
    [self autoscroll:event];
    [self _startAutoscrollTimer:event];
}

- (BOOL)_mayStartDragAtEventLocation:(NSPoint)location
{
    NSPoint mouseDownPoint = [self convertPoint:location fromView:nil];
    NSDictionary *mouseDownElement = [self elementAtPoint:mouseDownPoint];

    if ([mouseDownElement objectForKey: WebElementImageKey] != nil &&
        [mouseDownElement objectForKey: WebElementImageURLKey] != nil && 
        [[WebPreferences standardPreferences] loadsImagesAutomatically] && 
        (_private->dragSourceActionMask & WebDragSourceActionImage)) {
        return YES;
    }
    
    if ([mouseDownElement objectForKey:WebElementLinkURLKey] != nil && 
        (_private->dragSourceActionMask & WebDragSourceActionLink)) {
        return YES;
    }
    
    if ([[mouseDownElement objectForKey:WebElementIsSelectedKey] boolValue] &&
        (_private->dragSourceActionMask & WebDragSourceActionSelection)) {
        return YES;
    }
    
    return NO;
}

- (WebPluginController *)_pluginController
{
    return _private->pluginController;
}

- (void)_web_setPrintingModeRecursive
{
    [self _setPrinting:YES minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:NO];
    [super _web_setPrintingModeRecursive];
}

- (void)_web_clearPrintingModeRecursive
{
    [self _setPrinting:NO minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:NO];
    [super _web_clearPrintingModeRecursive];
}

- (void)_web_layoutIfNeededRecursive:(NSRect)displayRect testDirtyRect:(bool)testDirtyRect
{
    ASSERT(!_private->subviewsSetAside);
    displayRect = NSIntersectionRect(displayRect, [self bounds]);

    if (!testDirtyRect || [self needsDisplay]) {
        if (testDirtyRect) {
            NSRect dirtyRect = [self _dirtyRect];
            displayRect = NSIntersectionRect(displayRect, dirtyRect);
        }
        if (!NSIsEmptyRect(displayRect)) {
            if ([[self _bridge] needsLayout])
                _private->needsLayout = YES;
            if (_private->needsToApplyStyles || _private->needsLayout)
                [self layout];
        }
    }

    [super _web_layoutIfNeededRecursive: displayRect testDirtyRect: NO];
}

- (NSRect)_selectionRect
{
    return [[self _bridge] selectionRect];
}

- (void)_startAutoscrollTimer: (NSEvent *)triggerEvent
{
    if (_private->autoscrollTimer == nil) {
        _private->autoscrollTimer = [[NSTimer scheduledTimerWithTimeInterval:AUTOSCROLL_INTERVAL
            target:self selector:@selector(_autoscroll) userInfo:nil repeats:YES] retain];
        _private->autoscrollTriggerEvent = [triggerEvent retain];
    }
}

- (void)_stopAutoscrollTimer
{
    NSTimer *timer = _private->autoscrollTimer;
    _private->autoscrollTimer = nil;
    [_private->autoscrollTriggerEvent release];
    _private->autoscrollTriggerEvent = nil;
    [timer invalidate];
    [timer release];
}

- (void)_autoscroll
{
    int isStillDown;
    
    // Guarantee that the autoscroll timer is invalidated, even if we don't receive
    // a mouse up event.
    PSstilldown([_private->autoscrollTriggerEvent eventNumber], &isStillDown);
    if (!isStillDown){
        [self _stopAutoscrollTimer];
        return;
    }

    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSLeftMouseDragged
        location:[[self window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[[NSApp currentEvent] modifierFlags]
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[[self window] windowNumber]
        context:[[NSApp currentEvent] context]
        eventNumber:0 clickCount:0 pressure:0];
    [self mouseDragged:fakeEvent];
}

- (BOOL)_canCopy
{
    // Copying can be done regardless of whether you can edit.
    return [self _hasSelection];
}

- (BOOL)_canCut
{
    return [self _hasSelection] && [self _isEditable];
}

- (BOOL)_canDelete
{
    return [self _hasSelection] && [self _isEditable];
}

- (BOOL)_canPaste
{
    return [self _hasSelectionOrInsertionPoint] && [self _isEditable];
}

- (BOOL)_canType
{
    return [self _hasSelectionOrInsertionPoint] && [self _isEditable];
}

- (BOOL)_hasSelection
{
    return [[self _bridge] selectionState] == WebSelectionStateRange;
}

- (BOOL)_hasSelectionOrInsertionPoint
{
    return [[self _bridge] selectionState] != WebSelectionStateNone;
}

- (BOOL)_isEditable
{
    return [[self _webView] isEditable] || [[self _bridge] isSelectionEditable];
}

@end

@implementation NSView (WebHTMLViewPrivate)

- (void)_web_setPrintingModeRecursive
{
    [_subviews makeObjectsPerformSelector:@selector(_web_setPrintingModeRecursive)];
}

- (void)_web_clearPrintingModeRecursive
{
    [_subviews makeObjectsPerformSelector:@selector(_web_clearPrintingModeRecursive)];
}

- (void)_web_layoutIfNeededRecursive: (NSRect)rect testDirtyRect:(bool)testDirtyRect
{
    unsigned index, count;
    for (index = 0, count = [_subviews count]; index < count; index++) {
        NSView *subview = [_subviews objectAtIndex:index];
        NSRect dirtiedSubviewRect = [subview convertRect: rect fromView: self];
        [subview _web_layoutIfNeededRecursive: dirtiedSubviewRect testDirtyRect:testDirtyRect];
    }
}

@end

@implementation NSMutableDictionary (WebHTMLViewPrivate)

- (void)_web_setObjectIfNotNil:(id)object forKey:(id)key
{
    if (object == nil) {
        [self removeObjectForKey:key];
    } else {
        [self setObject:object forKey:key];
    }
}

@end

// The following is a workaround for
// <rdar://problem/3429631> window stops getting mouse moved events after first tooltip appears
// The trick is to define a category on NSToolTipPanel that implements setAcceptsMouseMovedEvents:.
// Since the category will be searched before the real class, we'll prevent the flag from being
// set on the tool tip panel.

@interface NSToolTipPanel : NSPanel
@end

@interface NSToolTipPanel (WebHTMLViewPrivate)
@end

@implementation NSToolTipPanel (WebHTMLViewPrivate)

- (void)setAcceptsMouseMovedEvents:(BOOL)flag
{
    // Do nothing, preventing the tool tip panel from trying to accept mouse-moved events.
}

@end


@interface WebHTMLView (TextSizing) <_web_WebDocumentTextSizing>
@end

@interface NSArray (WebHTMLView)
- (void)_web_makePluginViewsPerformSelector:(SEL)selector withObject:(id)object;
@end

@implementation WebHTMLView

+ (void)initialize
{
    WebKitInitializeUnicode();
    [NSApp registerServicesMenuSendTypes:[[self class] _selectionPasteboardTypes] returnTypes:nil];
}

- (id)initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];
    
    // Make all drawing go through us instead of subviews.
    // The bulk of the code to handle this is in WebHTMLViewPrivate.m.
    if (NSAppKitVersionNumber >= 711) {
        [self _setDrawsOwnDescendants:YES];
    }
    
    _private = [[WebHTMLViewPrivate alloc] init];

    _private->pluginController = [[WebPluginController alloc] initWithHTMLView:self];
    _private->needsLayout = YES;

    return self;
}

- (void)dealloc
{
    [self _clearLastHitViewIfSelf];
    [self _reset];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_private->pluginController destroyAllPlugins];
    [_private release];
    _private = nil;
    [super dealloc];
}

- (void)finalize
{
    [self _clearLastHitViewIfSelf];
    [self _reset];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_private->pluginController destroyAllPlugins];
    _private = nil;
    [super finalize];
}

- (IBAction)takeFindStringFromSelection:(id)sender
{
    if (![self _hasSelection]) {
        NSBeep();
        return;
    }

    [NSPasteboard _web_setFindPasteboardString:[self selectedString] withOwner:self];
}

- (NSArray *)pasteboardTypesForSelection
{
    return [[self class] _selectionPasteboardTypes];
}

- (void)writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    // Put HTML on the pasteboard.
    if ([types containsObject:WebArchivePboardType]) {
        WebArchive *archive = [self _selectedArchive];
        [pasteboard setData:[archive data] forType:WebArchivePboardType];
    }
    
    // Put attributed string on the pasteboard (RTF format).
    NSData *RTFData = nil;
    if ([types containsObject:NSRTFPboardType]) {
        RTFData = [self _selectedRTFData];
        [pasteboard setData:RTFData forType:NSRTFPboardType];
    }
    if ([types containsObject:NSRTFDPboardType]) {
        if (!RTFData) {
            RTFData = [self _selectedRTFData];
        }
        [pasteboard setData:RTFData forType:NSRTFDPboardType];
    }
    
    // Put plain string on the pasteboard.
    if ([types containsObject:NSStringPboardType]) {
        // Map &nbsp; to a plain old space because this is better for source code, other browsers do it,
        // and because HTML forces you to do this any time you want two spaces in a row.
        NSMutableString *s = [[self selectedString] mutableCopy];
        const unichar NonBreakingSpaceCharacter = 0xA0;
        NSString *NonBreakingSpaceString = [NSString stringWithCharacters:&NonBreakingSpaceCharacter length:1];
        [s replaceOccurrencesOfString:NonBreakingSpaceString withString:@" " options:0 range:NSMakeRange(0, [s length])];
        [pasteboard setString:s forType:NSStringPboardType];
        [s release];
    }
}

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pasteboard types:(NSArray *)types
{
    [self _writeSelectionToPasteboard:pasteboard];
    return YES;
}

- (void)selectAll:(id)sender
{
    [self selectAll];
}

- (void)jumpToSelection: sender
{
    [[self _bridge] jumpToSelection];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item 
{
    SEL action = [item action];
    WebBridge *bridge = [self _bridge];

    if (action == @selector(cut:)) {
        return [bridge mayDHTMLCut] || [self _canDelete];
    } else if (action == @selector(copy:)) {
        return [bridge mayDHTMLCopy] || [self _canCopy];
    } else if (action == @selector(delete:)) {
        return [self _canDelete];
    } else if (action == @selector(paste:)) {
        return [bridge mayDHTMLPaste] || [self _canPaste];
    } else if (action == @selector(takeFindStringFromSelection:)) {
        return [self _hasSelection];
    } else if (action == @selector(jumpToSelection:)) {
        return [self _hasSelection];
    } else if (action == @selector(checkSpelling:)
               || action == @selector(showGuessPanel:)
               || action == @selector(changeSpelling:)
               || action == @selector(ignoreSpelling:)) {
        return [[self _bridge] isSelectionEditable];
    }

    return YES;
}

- (id)validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType
{
    if (sendType && ([[[self class] _selectionPasteboardTypes] containsObject:sendType]) && [self _hasSelection]){
        return self;
    }

    return [super validRequestorForSendType:sendType returnType:returnType];
}

- (BOOL)acceptsFirstResponder
{
    // Don't accept first responder when we first click on this view.
    // We have to pass the event down through WebCore first to be sure we don't hit a subview.
    // Do accept first responder at any other time, for example from keyboard events,
    // or from calls back from WebCore once we begin mouse-down event handling.
    NSEvent *event = [NSApp currentEvent];
    if ([event type] == NSLeftMouseDown && event != _private->mouseDownEvent && 
        NSPointInRect([event locationInWindow], [self convertRect:[self visibleRect] toView:nil])) {
        return NO;
    }
    return YES;
}

- (BOOL)maintainsInactiveSelection
{
    // This method helps to determing whether the view should maintain
    // an inactive selection when the view is not first responder.
    // Traditionally, these views have not maintained such selections,
    // clearing them when the view was not first responder. However,
    // to fix bugs like this one:
    // <rdar://problem/3672088>: "Editable WebViews should maintain a selection even 
    //                            when they're not firstResponder"
    // it was decided to add a switch to act more like an NSTextView.
    // For now, however, the view only acts in this way when the
    // web view is set to be editable. This will maintain traditional
    // behavior for WebKit clients dating back to before this change,
    // and will likely be a decent switch for the long term, since
    // clients to ste the web view to be editable probably want it
    // to act like a "regular" Cocoa view in terms of its selection
    // behavior.
    if (![[self _webView] isEditable])
        return NO;
        
    id nextResponder = [[self window] _newFirstResponderAfterResigning];
    return !nextResponder || ![nextResponder isKindOfClass:[NSView class]] || ![nextResponder isDescendantOf:[self _webView]];
}

- (void)addMouseMovedObserver
{
    if ([[self window] isKeyWindow] && ![self _insideAnotherHTMLView]) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(mouseMovedNotification:)
            name:NSMouseMovedNotification object:nil];
        [self _frameOrBoundsChanged];
    }
}

- (void)removeMouseMovedObserver
{
    [[self _webView] _mouseDidMoveOverElement:nil modifierFlags:0];
    [[NSNotificationCenter defaultCenter] removeObserver:self
        name:NSMouseMovedNotification object:nil];
}

- (void)updateFocusDisplay
{
    // This method does the job of updating the view based on the view's firstResponder-ness and
    // the window key-ness of the window containing this view. This involves three kinds of 
    // drawing updates right now, all handled in WebCore in response to the call over the bridge. 
    // 
    // The three display attributes are as follows:
    // 
    // 1. The background color used to draw behind selected content (active | inactive color)
    // 2. Caret blinking (blinks | does not blink)
    // 3. The drawing of a focus ring around links in web pages.

    BOOL flag = !_private->resigningFirstResponder && [[self window] isKeyWindow] && [self _web_firstResponderCausesFocusDisplay];
    [[self _bridge] setDisplaysWithFocusAttributes:flag];
}

- (void)addSuperviewObservers
{
    // We watch the bounds of our superview, so that we can do a layout when the size
    // of the superview changes. This is different from other scrollable things that don't
    // need this kind of thing because their layout doesn't change.
    
    // We need to pay attention to both height and width because our "layout" has to change
    // to extend the background the full height of the space and because some elements have
    // sizes that are based on the total size of the view.
    
    NSView *superview = [self superview];
    if (superview && [self window]) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_frameOrBoundsChanged) 
            name:NSViewFrameDidChangeNotification object:superview];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_frameOrBoundsChanged) 
            name:NSViewBoundsDidChangeNotification object:superview];
    }
}

- (void)removeSuperviewObservers
{
    NSView *superview = [self superview];
    if (superview && [self window]) {
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSViewFrameDidChangeNotification object:superview];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSViewBoundsDidChangeNotification object:superview];
    }
}

- (void)addWindowObservers
{
    NSWindow *window = [self window];
    if (window) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidBecomeKey:)
            name:NSWindowDidBecomeKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidResignKey:)
            name:NSWindowDidResignKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowWillClose:)
            name:NSWindowWillCloseNotification object:window];
    }
}

- (void)removeWindowObservers
{
    NSWindow *window = [self window];
    if (window) {
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidBecomeKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidResignKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowWillCloseNotification object:window];
    }
}

- (void)viewWillMoveToSuperview:(NSView *)newSuperview
{
    [self removeSuperviewObservers];
}

- (void)viewDidMoveToSuperview
{
    // Do this here in case the text size multiplier changed when a non-HTML
    // view was installed.
    if ([self superview] != nil) {
        [self _updateTextSizeMultiplier];
        [self addSuperviewObservers];
    }
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (_private){
        // FIXME: Some of these calls may not work because this view may be already removed from it's superview.
        [self removeMouseMovedObserver];
        [self removeWindowObservers];
        [self removeSuperviewObservers];
        [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_updateMouseoverWithFakeEvent) object:nil];
    
        [[self _pluginController] stopAllPlugins];
    }
}

- (void)viewDidMoveToWindow
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (_private) {
        [self _stopAutoscrollTimer];
        if ([self window]) {
            _private->lastScrollPosition = [[self superview] bounds].origin;
            [self addWindowObservers];
            [self addSuperviewObservers];
            [self addMouseMovedObserver];
            // Schedule this update, rather than making the call right now.
            // The reason is that placing the caret in the just-installed view requires
            // the HTML/XML document to be available on the WebCore side, but it is not
            // at the time this code is running. However, it will be there on the next
            // crank of the run loop. Doing this helps to make a blinking caret appear 
            // in a new, empty window "automatic".
            [self performSelector:@selector(updateFocusDisplay) withObject:nil afterDelay:0];
    
            [[self _pluginController] startAllPlugins];
    
            _private->lastScrollPosition = NSZeroPoint;
            
            _private->inWindow = YES;
        } else {
            // Reset when we are moved out of a window after being moved into one.
            // Without this check, we reset ourselves before we even start.
            // This is only needed because viewDidMoveToWindow is called even when
            // the window is not changing (bug in AppKit).
            if (_private->inWindow) {
                [self _reset];
                _private->inWindow = NO;
            }
        }
    }
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    [[self subviews] _web_makePluginViewsPerformSelector:@selector(viewWillMoveToHostWindow:) withObject:hostWindow];
}

- (void)viewDidMoveToHostWindow
{
    [[self subviews] _web_makePluginViewsPerformSelector:@selector(viewDidMoveToHostWindow) withObject:nil];
}


- (void)addSubview:(NSView *)view
{
    [super addSubview:view];

    if ([[view class] respondsToSelector:@selector(plugInViewWithArguments:)] || [view respondsToSelector:@selector(pluginInitialize)] || [view respondsToSelector:@selector(webPlugInInitialize)]) {
        [[self _pluginController] addPlugin:view];
    }
}

- (void)reapplyStyles
{
    if (!_private->needsToApplyStyles) {
        return;
    }
    
#ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
#endif

    [[self _bridge] reapplyStylesForDeviceType:
        _private->printing ? WebCoreDevicePrinter : WebCoreDeviceScreen];
    
#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s apply style seconds = %f", [self URL], thisTime);
#endif

    _private->needsToApplyStyles = NO;
}

// Do a layout, but set up a new fixed width for the purposes of doing printing layout.
// minPageWidth==0 implies a non-printing layout
- (void)layoutToMinimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustingViewSize:(BOOL)adjustViewSize
{
    [self reapplyStyles];
    
    // Ensure that we will receive mouse move events.  Is this the best place to put this?
    [[self window] setAcceptsMouseMovedEvents: YES];
    [[self window] _setShouldPostEventNotifications: YES];

    if (!_private->needsLayout) {
        return;
    }

#ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
#endif

    LOG(View, "%@ doing layout", self);

    if (minPageWidth > 0.0) {
        [[self _bridge] forceLayoutWithMinimumPageWidth:minPageWidth maximumPageWidth:maxPageWidth adjustingViewSize:adjustViewSize];
    } else {
        [[self _bridge] forceLayoutAdjustingViewSize:adjustViewSize];
    }
    _private->needsLayout = NO;
    
    if (!_private->printing) {
        // get size of the containing dynamic scrollview, so
        // appearance and disappearance of scrollbars will not show up
        // as a size change
        NSSize newLayoutFrameSize = [[[self superview] superview] frame].size;
        if (_private->laidOutAtLeastOnce && !NSEqualSizes(_private->lastLayoutFrameSize, newLayoutFrameSize)) {
            [[self _bridge] sendResizeEvent];
        }
        _private->laidOutAtLeastOnce = YES;
        _private->lastLayoutSize = [(NSClipView *)[self superview] documentVisibleRect].size;
        _private->lastLayoutFrameSize = newLayoutFrameSize;
    }

#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s layout seconds = %f", [self URL], thisTime);
#endif
}

- (void)layout
{
    [self layoutToMinimumPageWidth:0.0 maximumPageWidth:0.0 adjustingViewSize:NO];
}

- (NSMenu *)menuForEvent:(NSEvent *)event
{
    [_private->compController endRevertingChange:NO moveLeft:NO];

    if ([[self _bridge] sendContextMenuEvent:event]) {
        return nil;
    }
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    NSDictionary *element = [self elementAtPoint:point];
    return [[self _webView] _menuForElement:element];
}

// Search from the end of the currently selected location, or from the beginning of the
// document if nothing is selected.
- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag;
{
    return [[self _bridge] searchFor:string direction:forward caseSensitive:caseFlag wrap:wrapFlag];
}

- (NSString *)string
{
    // FIXME: We should be using WebCore logic for converting the document into a string and not creating an attributed string to do this.
    return [[self attributedString] string];
}

- (NSAttributedString *)_attributeStringFromDOMRange:(DOMRange *)range
{
    NSAttributedString *attributedString = nil;
#ifdef BUILT_ON_TIGER_OR_LATER
    attributedString = [[[NSAttributedString alloc] _initWithDOMRange:range] autorelease];
#endif
    return attributedString;
}

- (NSAttributedString *)attributedString
{
    WebBridge *bridge = [self _bridge];
    DOMDocument *document = [bridge DOMDocument];
    NSAttributedString *attributedString = [self _attributeStringFromDOMRange:[document _documentRange]];
    if (attributedString == nil) {
        attributedString = [bridge attributedStringFrom:document startOffset:0 to:nil endOffset:0];
    }
    return attributedString;
}

- (NSString *)selectedString
{
    return [[self _bridge] selectedString];
}

- (NSAttributedString *)selectedAttributedString
{
    WebBridge *bridge = [self _bridge];
    NSAttributedString *attributedString = [self _attributeStringFromDOMRange:[bridge selectedDOMRange]];
    if (attributedString == nil) {
        attributedString = [bridge selectedAttributedString];
    }
    return attributedString;
}

- (void)selectAll
{
    [[self _bridge] selectAll];
}

// Remove the selection.
- (void)deselectAll
{
    [[self _bridge] deselectAll];
}

- (void)deselectText
{
    [[self _bridge] deselectText];
}

- (BOOL)isOpaque
{
    return [[self _webView] drawsBackground];
}

- (void)setNeedsDisplay:(BOOL)flag
{
    LOG(View, "%@ flag = %d", self, (int)flag);
    [super setNeedsDisplay: flag];
}

- (void)setNeedsLayout: (BOOL)flag
{
    LOG(View, "%@ flag = %d", self, (int)flag);
    _private->needsLayout = flag;
}


- (void)setNeedsToApplyStyles: (BOOL)flag
{
    LOG(View, "%@ flag = %d", self, (int)flag);
    _private->needsToApplyStyles = flag;
}

- (void)drawRect:(NSRect)rect
{
    LOG(View, "%@ drawing", self);
    
    BOOL subviewsWereSetAside = _private->subviewsSetAside;
    if (subviewsWereSetAside) {
        [self _restoreSubviews];
    }

#ifdef _KWQ_TIMING
    double start = CFAbsoluteTimeGetCurrent();
#endif

    [NSGraphicsContext saveGraphicsState];
    NSRectClip(rect);
        
    ASSERT([[self superview] isKindOfClass:[WebClipView class]]);

    [(WebClipView *)[self superview] setAdditionalClip:rect];

    NS_DURING {
        WebTextRendererFactory *textRendererFactoryIfCoalescing = nil;
        if ([WebTextRenderer shouldBufferTextDrawing] && [NSView focusView]) {
            textRendererFactoryIfCoalescing = [WebTextRendererFactory sharedFactory];
            [textRendererFactoryIfCoalescing startCoalesceTextDrawing];
        }

        if (![[self _webView] drawsBackground]) {
            [[NSColor clearColor] set];
            NSRectFill (rect);
        }
        
        //double start = CFAbsoluteTimeGetCurrent();
        [[self _bridge] drawRect:rect];
        //LOG(Timing, "draw time %e", CFAbsoluteTimeGetCurrent() - start);

        if (textRendererFactoryIfCoalescing != nil) {
            [textRendererFactoryIfCoalescing endCoalesceTextDrawing];
        }

        [(WebClipView *)[self superview] resetAdditionalClip];

        [NSGraphicsContext restoreGraphicsState];
    } NS_HANDLER {
        [(WebClipView *)[self superview] resetAdditionalClip];
        [NSGraphicsContext restoreGraphicsState];
        ERROR("Exception caught while drawing: %@", localException);
        [localException raise];
    } NS_ENDHANDLER

#ifdef DEBUG_LAYOUT
    NSRect vframe = [self frame];
    [[NSColor blackColor] set];
    NSBezierPath *path;
    path = [NSBezierPath bezierPath];
    [path setLineWidth:(float)0.1];
    [path moveToPoint:NSMakePoint(0, 0)];
    [path lineToPoint:NSMakePoint(vframe.size.width, vframe.size.height)];
    [path closePath];
    [path stroke];
    path = [NSBezierPath bezierPath];
    [path setLineWidth:(float)0.1];
    [path moveToPoint:NSMakePoint(0, vframe.size.height)];
    [path lineToPoint:NSMakePoint(vframe.size.width, 0)];
    [path closePath];
    [path stroke];
#endif

#ifdef _KWQ_TIMING
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s draw seconds = %f", widget->part()->baseURL().URL().latin1(), thisTime);
#endif

    if (subviewsWereSetAside) {
        [self _setAsideSubviews];
    }
}

// Turn off the additional clip while computing our visibleRect.
- (NSRect)visibleRect
{
    if (!([[self superview] isKindOfClass:[WebClipView class]]))
        return [super visibleRect];
        
    WebClipView *clipView = (WebClipView *)[self superview];

    BOOL hasAdditionalClip = [clipView hasAdditionalClip];
    if (!hasAdditionalClip) {
        return [super visibleRect];
    }
    
    NSRect additionalClip = [clipView additionalClip];
    [clipView resetAdditionalClip];
    NSRect visibleRect = [super visibleRect];
    [clipView setAdditionalClip:additionalClip];
    return visibleRect;
}

- (BOOL)isFlipped 
{
    return YES;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    ASSERT([notification object] == [self window]);
    [self addMouseMovedObserver];
    [self updateFocusDisplay];
}

- (void)windowDidResignKey: (NSNotification *)notification
{
    ASSERT([notification object] == [self window]);
    [_private->compController endRevertingChange:NO moveLeft:NO];
    [self removeMouseMovedObserver];
    [self updateFocusDisplay];
}

- (void)windowWillClose:(NSNotification *)notification
{
    [_private->compController endRevertingChange:NO moveLeft:NO];
    [[self _pluginController] destroyAllPlugins];
}

- (BOOL)_isSelectionEvent:(NSEvent *)event
{
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    return [[[self elementAtPoint:point] objectForKey:WebElementIsSelectedKey] boolValue];
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    // We hack AK's hitTest method to catch all events at the topmost WebHTMLView.  However, for
    // the purposes of this method we want to really query the deepest view, so we forward to it.
    forceRealHitTest = YES;
    NSView *hitView = [[[self window] contentView] hitTest:[event locationInWindow]];
    forceRealHitTest = NO;
    
    if ([hitView isKindOfClass:[self class]]) {
        WebHTMLView *hitHTMLView = (WebHTMLView *)hitView;
        [[hitHTMLView _bridge] setActivationEventNumber:[event eventNumber]];
        return [self _isSelectionEvent:event] ? [[hitHTMLView _bridge] eventMayStartDrag:event] : NO;
    } else {
        return [hitView acceptsFirstMouse:event];
    }
}

- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent *)event
{
    // We hack AK's hitTest method to catch all events at the topmost WebHTMLView.  However, for
    // the purposes of this method we want to really query the deepest view, so we forward to it.
    forceRealHitTest = YES;
    NSView *hitView = [[[self window] contentView] hitTest:[event locationInWindow]];
    forceRealHitTest = NO;
    
    if ([hitView isKindOfClass:[self class]]) {
        WebHTMLView *hitHTMLView = (WebHTMLView *)hitView;
        return [self _isSelectionEvent:event] ? [[hitHTMLView _bridge] eventMayStartDrag:event] : NO;
    } else {
        return [hitView shouldDelayWindowOrderingForEvent:event];
    }
}

- (void)mouseDown:(NSEvent *)event
{    
    // TEXTINPUT: if there is marked text and the current input
    // manager wants to handle mouse events, we need to make sure to
    // pass it to them. If not, then we need to notify the input
    // manager when the marked text is abandoned (user clicks outside
    // the marked area)

    [_private->compController endRevertingChange:NO moveLeft:NO];

    // If the web page handles the context menu event and menuForEvent: returns nil, we'll get control click events here.
    // We don't want to pass them along to KHTML a second time.
    if ([event modifierFlags] & NSControlKeyMask) {
        return;
    }
    
    _private->ignoringMouseDraggedEvents = NO;
    
    // Record the mouse down position so we can determine drag hysteresis.
    [_private->mouseDownEvent release];
    _private->mouseDownEvent = [event retain];

    // Don't do any mouseover while the mouse is down.
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_updateMouseoverWithFakeEvent) object:nil];

    // Let KHTML get a chance to deal with the event. This will call back to us
    // to start the autoscroll timer if appropriate.
    [[self _bridge] mouseDown:event];
}

- (void)dragImage:(NSImage *)dragImage
               at:(NSPoint)at
           offset:(NSSize)offset
            event:(NSEvent *)event
       pasteboard:(NSPasteboard *)pasteboard
           source:(id)source
        slideBack:(BOOL)slideBack
{   
    [self _stopAutoscrollTimer];
    
    _private->initiatedDrag = YES;
    [[self _webView] _setInitiatedDrag:YES];
    
    // Retain this view during the drag because it may be released before the drag ends.
    [self retain];

    [super dragImage:dragImage at:at offset:offset event:event pasteboard:pasteboard source:source slideBack:slideBack];
}

- (void)mouseDragged:(NSEvent *)event
{
    // TEXTINPUT: if there is marked text and the current input
    // manager wants to handle mouse events, we need to make sure to
    // pass it to them.

    if (!_private->ignoringMouseDraggedEvents) {
        [[self _bridge] mouseDragged:event];
    }
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal
{
    if (_private->webCoreDragOp == NSDragOperationNone) {
        return (NSDragOperationGeneric | NSDragOperationCopy);
    } else {
        return _private->webCoreDragOp;
    }
}

- (void)draggedImage:(NSImage *)image movedTo:(NSPoint)screenLoc
{
    NSPoint windowImageLoc = [[self window] convertScreenToBase:screenLoc];
    NSPoint windowMouseLoc = NSMakePoint(windowImageLoc.x + _private->dragOffset.x, windowImageLoc.y + _private->dragOffset.y);
    [[self _bridge] dragSourceMovedTo:windowMouseLoc];
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
    NSPoint windowImageLoc = [[self window] convertScreenToBase:aPoint];
    NSPoint windowMouseLoc = NSMakePoint(windowImageLoc.x + _private->dragOffset.x, windowImageLoc.y + _private->dragOffset.y);
    [[self _bridge] dragSourceEndedAt:windowMouseLoc operation:operation];

    _private->initiatedDrag = NO;
    [[self _webView] _setInitiatedDrag:NO];
    
    // Prevent queued mouseDragged events from coming after the drag and fake mouseUp event.
    _private->ignoringMouseDraggedEvents = YES;
    
    // Once the dragging machinery kicks in, we no longer get mouse drags or the up event.
    // khtml expects to get balanced down/up's, so we must fake up a mouseup.
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSLeftMouseUp
                                            location:windowMouseLoc
                                       modifierFlags:[[NSApp currentEvent] modifierFlags]
                                           timestamp:[NSDate timeIntervalSinceReferenceDate]
                                        windowNumber:[[self window] windowNumber]
                                             context:[[NSApp currentEvent] context]
                                         eventNumber:0 clickCount:0 pressure:0];
    [self mouseUp:fakeEvent]; // This will also update the mouseover state.
    
    // Balance the previous retain from when the drag started.
    [self release];
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    ASSERT(_private->draggingImageURL);
    
    NSFileWrapper *wrapper = [[self _dataSource] _fileWrapperForURL:_private->draggingImageURL];
    ASSERT(wrapper);    
    
    // FIXME: Report an error if we fail to create a file.
    NSString *path = [[dropDestination path] stringByAppendingPathComponent:[wrapper preferredFilename]];
    path = [[NSFileManager defaultManager] _web_pathWithUniqueFilenameForPath:path];
    if (![wrapper writeToFile:path atomically:NO updateFilenames:YES]) {
        ERROR("Failed to create image file via -[NSFileWrapper writeToFile:atomically:updateFilenames:]");
    }
    
    return [NSArray arrayWithObject:[path lastPathComponent]];
}

- (BOOL)_canProcessDragWithDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
    NSPasteboard *pasteboard = [draggingInfo draggingPasteboard];
    NSMutableSet *types = [NSMutableSet setWithArray:[pasteboard types]];
    [types intersectSet:[NSSet setWithArray:[WebHTMLView _insertablePasteboardTypes]]];
    if ([types count] == 0) {
        return NO;
    } else if ([types count] == 1 && 
               [types containsObject:NSFilenamesPboardType] && 
               ![self _imageExistsAtPaths:[pasteboard propertyListForType:NSFilenamesPboardType]]) {
        return NO;
    }
    
    NSPoint point = [self convertPoint:[draggingInfo draggingLocation] fromView:nil];
    NSDictionary *element = [self elementAtPoint:point];
    if ([[self _webView] isEditable] || [[element objectForKey:WebElementDOMNodeKey] isContentEditable]) {
        if (_private->initiatedDrag && [[element objectForKey:WebElementIsSelectedKey] boolValue]) {
            // Can't drag onto the selection being dragged.
            return NO;
        }
        return YES;
    }
    
    return NO;
}

- (BOOL)_isMoveDrag
{
    return _private->initiatedDrag && 
    [[self _bridge] isSelectionEditable] &&
    ([[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask) == 0;
}

- (NSDragOperation)draggingUpdatedWithDraggingInfo:(id <NSDraggingInfo>)draggingInfo actionMask:(unsigned int)actionMask
{
    NSDragOperation operation = NSDragOperationNone;
    
    if (actionMask & WebDragDestinationActionDHTML) {
        operation = [[self _bridge] dragOperationForDraggingInfo:draggingInfo];
    }
    _private->webCoreHandlingDrag = (operation != NSDragOperationNone);
    
    if ((actionMask & WebDragDestinationActionEdit) &&
        !_private->webCoreHandlingDrag
        && [self _canProcessDragWithDraggingInfo:draggingInfo]) {
        WebView *webView = [self _webView];
        [webView moveDragCaretToPoint:[webView convertPoint:[draggingInfo draggingLocation] fromView:nil]];
        operation = [self _isMoveDrag] ? NSDragOperationMove : NSDragOperationCopy;
    } else {
        [[self _webView] removeDragCaret];
    }
    
    return operation;
}

- (void)draggingCancelledWithDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
    [[self _bridge] dragExitedWithDraggingInfo:draggingInfo];
    [[self _webView] removeDragCaret];
}

- (BOOL)concludeDragForDraggingInfo:(id <NSDraggingInfo>)draggingInfo actionMask:(unsigned int)actionMask
{
    WebView *webView = [self _webView];
    WebBridge *bridge = [self _bridge];
    if (_private->webCoreHandlingDrag) {
        ASSERT(actionMask & WebDragDestinationActionDHTML);
        [[webView _UIDelegateForwarder] webView:webView willPerformDragDestinationAction:WebDragDestinationActionDHTML forDraggingInfo:draggingInfo];
        [bridge concludeDragForDraggingInfo:draggingInfo];
        return YES;
    } else if (actionMask & WebDragDestinationActionEdit) {
        BOOL didInsert = NO;
        if ([self _canProcessDragWithDraggingInfo:draggingInfo]) {
            DOMDocumentFragment *fragment = [self _documentFragmentFromPasteboard:[draggingInfo draggingPasteboard] allowPlainText:YES];
            if (fragment && [self _shouldInsertFragment:fragment replacingDOMRange:[bridge dragCaretDOMRange] givenAction:WebViewInsertActionDropped]) {
                [[webView _UIDelegateForwarder] webView:webView willPerformDragDestinationAction:WebDragDestinationActionEdit forDraggingInfo:draggingInfo];
                if ([self _isMoveDrag]) {
                    [bridge moveSelectionToDragCaret:fragment];
                } else {
                    [bridge setSelectionToDragCaret];
                    [bridge replaceSelectionWithFragment:fragment selectReplacement:YES];
                }
                didInsert = YES;
            }
        }
        [webView removeDragCaret];
        return didInsert;
    }
    return NO;
}

- (NSDictionary *)elementAtPoint:(NSPoint)point
{
    NSDictionary *elementInfoWC = [[self _bridge] elementAtPoint:point];
    NSMutableDictionary *elementInfo = [elementInfoWC mutableCopy];
    
    // Convert URL strings to NSURLs
    [elementInfo _web_setObjectIfNotNil:[NSURL _web_URLWithDataAsString:[elementInfoWC objectForKey:WebElementLinkURLKey]] forKey:WebElementLinkURLKey];
    [elementInfo _web_setObjectIfNotNil:[NSURL _web_URLWithDataAsString:[elementInfoWC objectForKey:WebElementImageURLKey]] forKey:WebElementImageURLKey];
    
    WebFrameView *webFrameView = [self _web_parentWebFrameView];
    ASSERT(webFrameView);
    WebFrame *webFrame = [webFrameView webFrame];
    
    if (webFrame) {
        NSString *frameName = [elementInfoWC objectForKey:WebElementLinkTargetFrameKey];
        if ([frameName length] == 0) {
            [elementInfo setObject:webFrame forKey:WebElementLinkTargetFrameKey];
        } else {
            WebFrame *wf = [webFrame findFrameNamed:frameName];
            if (wf != nil)
                [elementInfo setObject:wf forKey:WebElementLinkTargetFrameKey];
            else
                [elementInfo removeObjectForKey:WebElementLinkTargetFrameKey];
        }
        
        [elementInfo setObject:webFrame forKey:WebElementFrameKey];
    }
    
    return [elementInfo autorelease];
}

- (void)mouseUp:(NSEvent *)event
{
    // TEXTINPUT: if there is marked text and the current input
    // manager wants to handle mouse events, we need to make sure to
    // pass it to them.

    [self _stopAutoscrollTimer];
    [[self _bridge] mouseUp:event];
    [self _updateMouseoverWithFakeEvent];
}

- (void)mouseMovedNotification:(NSNotification *)notification
{
    [self _updateMouseoverWithEvent:[[notification userInfo] objectForKey:@"NSEvent"]];
}

- (BOOL)supportsTextEncoding
{
    return YES;
}

- (NSView *)nextValidKeyView
{
    NSView *view = nil;
    if (![self isHiddenOrHasHiddenAncestor]) {
        view = [[self _bridge] nextKeyViewInsideWebFrameViews];
    }
    if (view == nil) {
        view = [super nextValidKeyView];
    }
    return view;
}

- (NSView *)previousValidKeyView
{
    NSView *view = nil;
    if (![self isHiddenOrHasHiddenAncestor]) {
        view = [[self _bridge] previousKeyViewInsideWebFrameViews];
    }
    if (view == nil) {
        view = [super previousValidKeyView];
    }
    return view;
}

- (BOOL)becomeFirstResponder
{
    NSView *view = nil;
    if (![[self _webView] _isPerformingProgrammaticFocus]) {
        switch ([[self window] keyViewSelectionDirection]) {
        case NSDirectSelection:
            break;
        case NSSelectingNext:
            view = [[self _bridge] nextKeyViewInsideWebFrameViews];
            break;
        case NSSelectingPrevious:
            view = [[self _bridge] previousKeyViewInsideWebFrameViews];
            break;
        }
    }
    if (view) {
        [[self window] makeFirstResponder:view];
    }
    [self updateFocusDisplay];
    return YES;
}

// This approach could be relaxed when dealing with 3228554.
// Some alteration to the selection behavior was done to deal with 3672088.
- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign) {
        [_private->compController endRevertingChange:NO moveLeft:NO];
        _private->resigningFirstResponder = YES;
        if (![self maintainsInactiveSelection]) { 
            if ([[self _webView] _isPerformingProgrammaticFocus]) {
                [self deselectText];
            }
            else {
                [self deselectAll];
            }
        }
        [self updateFocusDisplay];
        _private->resigningFirstResponder = NO;
    }
    return resign;
}

//------------------------------------------------------------------------------------
// WebDocumentView protocol
//------------------------------------------------------------------------------------
- (void)setDataSource:(WebDataSource *)dataSource 
{
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

// Does setNeedsDisplay:NO as a side effect when printing is ending.
// pageWidth != 0 implies we will relayout to a new width
- (void)_setPrinting:(BOOL)printing minimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustViewSize:(BOOL)adjustViewSize
{
    WebFrame *frame = [self _frame];
    NSArray *subframes = [frame childFrames];
    unsigned n = [subframes count];
    unsigned i;
    for (i = 0; i != n; ++i) {
        WebFrame *subframe = [subframes objectAtIndex:i];
        WebFrameView *frameView = [subframe frameView];
        if ([[subframe dataSource] _isDocumentHTML]) {
            [(WebHTMLView *)[frameView documentView] _setPrinting:printing minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:adjustViewSize];
        }
    }

    if (printing != _private->printing) {
        [_private->pageRects release];
        _private->pageRects = nil;
        _private->printing = printing;
        [self setNeedsToApplyStyles:YES];
        [self setNeedsLayout:YES];
        [self layoutToMinimumPageWidth:minPageWidth maximumPageWidth:maxPageWidth adjustingViewSize:adjustViewSize];
        if (printing) {
            [[self _webView] _adjustPrintingMarginsForHeaderAndFooter];
        } else {
            // Can't do this when starting printing or nested printing won't work, see 3491427.
            [self setNeedsDisplay:NO];
        }
    }
}

// This is needed for the case where the webview is embedded in the view that's being printed.
// It shouldn't be called when the webview is being printed directly.
- (void)adjustPageHeightNew:(float *)newBottom top:(float)oldTop bottom:(float)oldBottom limit:(float)bottomLimit
{
    // This helps when we print as part of a larger print process.
    // If the WebHTMLView itself is what we're printing, then we will never have to do this.
    BOOL wasInPrintingMode = _private->printing;
    if (!wasInPrintingMode) {
        [self _setPrinting:YES minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:NO];
    }
    
    [[self _bridge] adjustPageHeightNew:newBottom top:oldTop bottom:oldBottom limit:bottomLimit];
    
    if (!wasInPrintingMode) {
        [self _setPrinting:NO minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:NO];
    }
}

- (float)_availablePaperWidthForPrintOperation:(NSPrintOperation *)printOperation
{
    NSPrintInfo *printInfo = [printOperation printInfo];
    return [printInfo paperSize].width - [printInfo leftMargin] - [printInfo rightMargin];
}

- (float)_scaleFactorForPrintOperation:(NSPrintOperation *)printOperation
{
    float viewWidth = NSWidth([self bounds]);
    if (viewWidth < 1) {
        ERROR("%@ has no width when printing", self);
        return 1.0;
    }

    float userScaleFactor = [printOperation _web_pageSetupScaleFactor];
    float maxShrinkToFitScaleFactor = 1/PrintingMaximumShrinkFactor;
    float shrinkToFitScaleFactor = [self _availablePaperWidthForPrintOperation:printOperation]/viewWidth;
    return userScaleFactor * MAX(maxShrinkToFitScaleFactor, shrinkToFitScaleFactor);
}

// FIXME 3491344: This is a secret AppKit-internal method that we need to override in order
// to get our shrink-to-fit to work with a custom pagination scheme. We can do this better
// if AppKit makes it SPI/API.
- (float)_provideTotalScaleFactorForPrintOperation:(NSPrintOperation *)printOperation 
{
    return [self _scaleFactorForPrintOperation:printOperation];
}

- (void)setPageWidthForPrinting:(float)pageWidth
{
    [self _setPrinting:NO minimumPageWidth:0. maximumPageWidth:0. adjustViewSize:NO];
    [self _setPrinting:YES minimumPageWidth:pageWidth maximumPageWidth:pageWidth adjustViewSize:YES];
}


// Return the number of pages available for printing
- (BOOL)knowsPageRange:(NSRangePointer)range {
    // Must do this explicit display here, because otherwise the view might redisplay while the print
    // sheet was up, using printer fonts (and looking different).
    [self displayIfNeeded];
    [[self window] setAutodisplay:NO];
    
    // If we are a frameset just print with the layout we have onscreen, otherwise relayout
    // according to the paper size
    float minLayoutWidth = 0.0;
    float maxLayoutWidth = 0.0;
    if (![[self _bridge] isFrameSet]) {
        float paperWidth = [self _availablePaperWidthForPrintOperation:[NSPrintOperation currentOperation]];
        minLayoutWidth = paperWidth*PrintingMinimumShrinkFactor;
        maxLayoutWidth = paperWidth*PrintingMaximumShrinkFactor;
    }
    [self _setPrinting:YES minimumPageWidth:minLayoutWidth maximumPageWidth:maxLayoutWidth adjustViewSize:YES]; // will relayout
    
    // There is a theoretical chance that someone could do some drawing between here and endDocument,
    // if something caused setNeedsDisplay after this point. If so, it's not a big tragedy, because
    // you'd simply see the printer fonts on screen. As of this writing, this does not happen with Safari.

    range->location = 1;
    NSPrintOperation *printOperation = [NSPrintOperation currentOperation];
    float totalScaleFactor = [self _scaleFactorForPrintOperation:printOperation];
    float userScaleFactor = [printOperation _web_pageSetupScaleFactor];
    [_private->pageRects release];
    NSArray *newPageRects = [[self _bridge] computePageRectsWithPrintWidthScaleFactor:userScaleFactor
                                                                          printHeight:[self _calculatePrintHeight]/totalScaleFactor];
    // AppKit gets all messed up if you give it a zero-length page count (see 3576334), so if we
    // hit that case we'll pass along a degenerate 1 pixel square to print. This will print
    // a blank page (with correct-looking header and footer if that option is on), which matches
    // the behavior of IE and Camino at least.
    if ([newPageRects count] == 0) {
        newPageRects = [NSArray arrayWithObject:[NSValue valueWithRect: NSMakeRect(0, 0, 1, 1)]];
    }
    _private->pageRects = [newPageRects retain];
    
    range->length = [_private->pageRects count];
    
    return YES;
}

// Return the drawing rectangle for a particular page number
- (NSRect)rectForPage:(int)page {
    return [[_private->pageRects objectAtIndex: (page-1)] rectValue];
}

// Calculate the vertical size of the view that fits on a single page
- (float)_calculatePrintHeight {
    // Obtain the print info object for the current operation
    NSPrintInfo *pi = [[NSPrintOperation currentOperation] printInfo];
    
    // Calculate the page height in points
    NSSize paperSize = [pi paperSize];
    return paperSize.height - [pi topMargin] - [pi bottomMargin];
}

- (void)drawPageBorderWithSize:(NSSize)borderSize
{
    ASSERT(NSEqualSizes(borderSize, [[[NSPrintOperation currentOperation] printInfo] paperSize]));    
    [[self _webView] _drawHeaderAndFooter];
}

- (void)endDocument
{
    [super endDocument];
    // Note sadly at this point [NSGraphicsContext currentContextDrawingToScreen] is still NO 
    [self _setPrinting:NO minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:YES];
    [[self window] setAutodisplay:YES];
}

- (void)_updateTextSizeMultiplier
{
    [[self _bridge] setTextSizeMultiplier:[[self _webView] textSizeMultiplier]];    
}

- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    // Pass command-key combos through WebCore if there is a key binding available for
    // this event. This lets web pages have a crack at intercepting command-modified keypresses.
    if ([self _web_firstResponderIsSelfOrDescendantView] && [[self _bridge] interceptKeyEvent:event toView:self]) {
        return YES;
    }
    return [super performKeyEquivalent:event];
}

- (BOOL)_interceptEditingKeyEvent:(NSEvent *)event
{   
    // Work around this bug:
    // <rdar://problem/3630640>: "Calling interpretKeyEvents: in a custom text view can fail to process keys right after app startup"
    [NSKeyBindingManager sharedKeyBindingManager];
    
    // Use the isEditable state to determine whether or not to process tab key events.
    // The idea here is that isEditable will be NO when this WebView is being used
    // in a browser, and we desire the behavior where tab moves to the next element
    // in tab order. If isEditable is YES, it is likely that the WebView is being
    // embedded as the whole view, as in Mail, and tabs should input tabs as expected
    // in a text editor.
    if (![[self _webView] isEditable] && [event _web_isTabKeyEvent]) 
        return NO;
    
    // Now process the key normally
    [self interpretKeyEvents:[NSArray arrayWithObject:event]];
    return YES;
}

- (void)keyDown:(NSEvent *)event
{
    WebBridge *bridge = [self _bridge];
    if ([bridge interceptKeyEvent:event toView:self]) {
        // WebCore processed a key event, bail on any outstanding complete: UI
        [_private->compController endRevertingChange:YES moveLeft:NO];
        return;
    }

    if (_private->compController && [_private->compController filterKeyDown:event])
        return;     // consumed by complete: popup window

    // We're going to process a key event, bail on any outstanding complete: UI
    [_private->compController endRevertingChange:YES moveLeft:NO];
    
    if ([self _canType] && [self _interceptEditingKeyEvent:event]) 
        return;
    
    [super keyDown:event];
}

- (void)keyUp:(NSEvent *)event
{
    if (![[self _bridge] interceptKeyEvent:event toView:self]) {
        [super keyUp:event];
    }
}

- (id)accessibilityAttributeValue:(NSString*)attributeName
{
    if ([attributeName isEqualToString: NSAccessibilityChildrenAttribute]) {
        id accTree = [[self _bridge] accessibilityTree];
        if (accTree)
            return [NSArray arrayWithObject: accTree];
        return nil;
    }
    return [super accessibilityAttributeValue:attributeName];
}

- (id)accessibilityHitTest:(NSPoint)point
{
    id accTree = [[self _bridge] accessibilityTree];
    if (accTree) {
        NSPoint windowCoord = [[self window] convertScreenToBase: point];
        return [accTree accessibilityHitTest: [self convertPoint:windowCoord fromView:nil]];
    }
    else
        return self;
}

- (void)centerSelectionInVisibleArea:(id)sender
{
    // FIXME: Does this do the right thing when the selection is not a caret?
    [[self _bridge] ensureCaretVisible];
}

- (void)_alterCurrentSelection:(WebSelectionAlteration)alteration direction:(WebSelectionDirection)direction granularity:(WebSelectionGranularity)granularity
{
    WebBridge *bridge = [self _bridge];
    DOMRange *proposedRange = [bridge rangeByAlteringCurrentSelection:alteration direction:direction granularity:granularity];
    WebView *webView = [self _webView];
    if ([[webView _editingDelegateForwarder] webView:webView shouldChangeSelectedDOMRange:[bridge selectedDOMRange] toDOMRange:proposedRange affinity:[bridge selectionAffinity] stillSelecting:NO]) {
        [bridge alterCurrentSelection:alteration direction:direction granularity:granularity];
    }
}

- (void)moveBackward:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectBackward granularity:WebSelectByCharacter];
}

- (void)moveBackwardAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectBackward granularity:WebSelectByCharacter];
}

- (void)moveDown:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectForward granularity:WebSelectByLine];
}

- (void)moveDownAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectForward granularity:WebSelectByLine];
}

- (void)moveForward:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectForward granularity:WebSelectByCharacter];
}

- (void)moveForwardAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectForward granularity:WebSelectByCharacter];
}

- (void)moveLeft:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectLeft granularity:WebSelectByCharacter];
}

- (void)moveLeftAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectLeft granularity:WebSelectByCharacter];
}

- (void)moveRight:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectRight granularity:WebSelectByCharacter];
}

- (void)moveRightAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectRight granularity:WebSelectByCharacter];
}

- (void)moveToBeginningOfDocument:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectLeft granularity:WebSelectByDocument];
}

- (void)moveToBeginningOfDocumentAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectLeft granularity:WebSelectByDocument];
}

- (void)moveToBeginningOfLine:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectLeft granularity:WebSelectToLineBoundary];
}

- (void)moveToBeginningOfLineAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectLeft granularity:WebSelectToLineBoundary];
}

- (void)moveToBeginningOfParagraph:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectLeft granularity:WebSelectByParagraph];
}

- (void)moveToBeginningOfParagraphAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectLeft granularity:WebSelectByParagraph];
}

- (void)moveToEndOfDocument:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectRight granularity:WebSelectByDocument];
}

- (void)moveToEndOfDocumentAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectRight granularity:WebSelectByDocument];
}

- (void)moveToEndOfLine:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectRight granularity:WebSelectToLineBoundary];
}

- (void)moveToEndOfLineAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectRight granularity:WebSelectToLineBoundary];
}

- (void)moveToEndOfParagraph:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectRight granularity:WebSelectByParagraph];
}

- (void)moveToEndOfParagraphAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectRight granularity:WebSelectByParagraph];
}

- (void)moveUp:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectBackward granularity:WebSelectByLine];
}

- (void)moveUpAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectBackward granularity:WebSelectByLine];
}

- (void)moveWordBackward:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectBackward granularity:WebSelectByWord];
}

- (void)moveWordBackwardAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectBackward granularity:WebSelectByWord];
}

- (void)moveWordForward:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectForward granularity:WebSelectByWord];
}

- (void)moveWordForwardAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectForward granularity:WebSelectByWord];
}

- (void)moveWordLeft:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectLeft granularity:WebSelectByWord];
}

- (void)moveWordLeftAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectLeft granularity:WebSelectByWord];
}

- (void)moveWordRight:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebSelectRight granularity:WebSelectByWord];
}

- (void)moveWordRightAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebSelectRight granularity:WebSelectByWord];
}

- (void)pageDown:(id)sender
{
    // FIXME: This should to scroll page down, then move the caret to the top left.
    ERROR("unimplemented");
}

- (void)pageUp:(id)sender
{
    // FIXME: This should to scroll page up, then move the caret to the top left.
    ERROR("unimplemented");
}

- (void)_expandSelectionToGranularity:(WebSelectionGranularity)granularity
{
    WebBridge *bridge = [self _bridge];
    DOMRange *range = [bridge rangeByExpandingSelectionWithGranularity:granularity];
    if (range && ![range collapsed]) {
        WebView *webView = [self _webView];
        if ([[webView _editingDelegateForwarder] webView:webView shouldChangeSelectedDOMRange:[bridge selectedDOMRange] toDOMRange:range affinity:[bridge selectionAffinity] stillSelecting:NO]) {
            [bridge setSelectedDOMRange:range affinity:[bridge selectionAffinity]];
        }
    }
}

- (void)selectParagraph:(id)sender
{
    [self _expandSelectionToGranularity:WebSelectByParagraph];
}

- (void)selectLine:(id)sender
{
    [self _expandSelectionToGranularity:WebSelectByLine];
}

- (void)selectWord:(id)sender
{
    [self _expandSelectionToGranularity:WebSelectByWord];
}

- (void)copy:(id)sender
{
    if ([[self _bridge] tryDHTMLCopy]) {
        return;     // DHTML did the whole operation
    }

    if (![self _canCopy]) {
        NSBeep();
        return;
    }
    [self _writeSelectionToPasteboard:[NSPasteboard generalPasteboard]];
}

- (void)cut:(id)sender
{
    if ([[self _bridge] tryDHTMLCut]) {
        return;     // DHTML did the whole operation
    }

    if (![self _canCut]) {
        NSBeep();
        return;
    }
    [self copy:sender];
    [[self _bridge] deleteSelection];
}

- (void)delete:(id)sender
{
    if (![self _canDelete]) {
        NSBeep();
        return;
    }
    [[self _bridge] deleteSelection];
}

- (void)paste:(id)sender
{
    if ([[self _bridge] tryDHTMLPaste]) {
        return;     // DHTML did the whole operation
    }

    if (![self _canPaste]) {
        NSBeep();
        return;
    }

    [self _pasteWithPasteboard:[NSPasteboard generalPasteboard] allowPlainText:YES];
}

- (void)copyFont:(id)sender
{
    // Font is RTF with a single character in it.
    // NSTextView uses the first character in the selection, or a space if there are no characters.
    ERROR("unimplemented");
}

- (void)pasteFont:(id)sender
{
    // FIXME: Support RTF to HTML (or DOM) conversion.
    // Font is RTF. Call fontAttributesInRange to extract the attributes to apply.
    // Then convert the RTF into CSS.
    ERROR("unimplemented");
}

- (void)pasteAsPlainText:(id)sender
{
    NSString *text = [[NSPasteboard generalPasteboard] stringForType:NSStringPboardType];
    WebBridge *bridge = [self _bridge];
    if ([self _shouldReplaceSelectionWithText:text givenAction:WebViewInsertActionPasted]) {
        [bridge replaceSelectionWithText:text selectReplacement:NO];
    }
}

- (void)pasteAsRichText:(id)sender
{
    [self _pasteWithPasteboard:[NSPasteboard generalPasteboard] allowPlainText:NO];
}

- (DOMCSSStyleDeclaration *)_fontManagerOperationAsStyle
{
    WebBridge *bridge = [self _bridge];
    DOMCSSStyleDeclaration *style = [[bridge DOMDocument] createCSSStyleDeclaration];

    NSFontManager *fm = [NSFontManager sharedFontManager];

    NSFont *a = [fm convertFont:[fm fontWithFamily:@"Helvetica" traits:0 weight:5 size:10]];
    NSFont *b = [fm convertFont:[fm fontWithFamily:@"Times" traits:(NSBoldFontMask | NSItalicFontMask) weight:10 size:12]];
    
    NSString *fa = [a familyName];
    NSString *fb = [b familyName];
    if ([fa isEqualToString:fb]) {
        [style setFontFamily:fa];
    }

    int sa = [a pointSize];
    int sb = [b pointSize];
    if (sa == sb) {
        [style setFontSize:[NSString stringWithFormat:@"%dpx", sa]];
    }

    int wa = [fm weightOfFont:a];
    int wb = [fm weightOfFont:b];
    if (wa == wb) {
        if (wa >= 9) {
            [style setFontWeight:@"bold"];
        } else {
            [style setFontWeight:@"normal"];
        }
    }

    BOOL ia = ([fm traitsOfFont:a] & NSItalicFontMask) != 0;
    BOOL ib = ([fm traitsOfFont:b] & NSItalicFontMask) != 0;
    if (ia == ib) {
        if (ia) {
            [style setFontStyle:@"italic"];
        } else {
            [style setFontStyle:@"normal"];
        }
    }

    return style;
}

- (void)changeFont:(id)sender
{
    DOMCSSStyleDeclaration *style = [self _fontManagerOperationAsStyle];
    WebView *webView = [self _webView];
    WebBridge *bridge = [self _bridge];
    if ([[webView _editingDelegateForwarder] webView:webView shouldApplyStyle:style toElementsInDOMRange:[bridge selectedDOMRange]]) {
        [bridge applyStyle:style];
    }
}

- (void)changeAttributes:(id)sender
{
    // Used for various fancy stuff from the font panel.
    // Turn the attributes into CSS and then call applyStyle.
    ERROR("unimplemented");
}

- (DOMCSSStyleDeclaration *)_colorPanelColorAsStyleUsingSelector:(SEL)selector
{
    WebBridge *bridge = [self _bridge];
    DOMCSSStyleDeclaration *style = [[bridge DOMDocument] createCSSStyleDeclaration];
    NSColor *color = [[NSColorPanel sharedColorPanel] color];
    NSString *colorAsString = [NSString stringWithFormat:@"rgb(%.0f,%.0f,%.0f)", [color redComponent]*255, [color greenComponent]*255, [color blueComponent]*255];
    ASSERT([style respondsToSelector:selector]);
    [style performSelector:selector withObject:colorAsString];
    
    return style;
}

- (void)_changeCSSColorUsingSelector:(SEL)selector inRange:(DOMRange *)range
{
    DOMCSSStyleDeclaration *style = [self _colorPanelColorAsStyleUsingSelector:selector];
    WebView *webView = [self _webView];
    if ([[webView _editingDelegateForwarder] webView:webView shouldApplyStyle:style toElementsInDOMRange:range]) {
        [[self _bridge] applyStyle:style];
    }
}

- (DOMRange *)_entireDOMRange
{
    DOMDocument *document = [[self _bridge] DOMDocument];
    DOMRange *range = [document createRange];
    [range selectNode:[document documentElement]];
    return range;
}

- (void)changeDocumentBackgroundColor:(id)sender
{
    // Mimicking NSTextView, this method sets the background color for the
    // entire document. There is no NSTextView API for setting the background
    // color on the selected range only. Note that this method is currently
    // never called from the UI (see comment in changeColor:).
    // FIXME: this actually has no effect when called, probably due to 3654850. _entireDOMRange seems
    // to do the right thing because it works in startSpeaking:, and I know setBackgroundColor: does the
    // right thing because I tested it with [[self _bridge] selectedDOMRange].
    // FIXME: This won't actually apply the style to the entire range here, because it ends up calling
    // [bridge applyStyle:], which operates on the current selection. To make this work right, we'll
    // need to save off the selection, temporarily set it to the entire range, make the change, then
    // restore the old selection.
    [self _changeCSSColorUsingSelector:@selector(setBackgroundColor:) inRange:[self _entireDOMRange]];
}

- (void)changeColor:(id)sender
{
    // FIXME: in NSTextView, this method calls changeDocumentBackgroundColor: when a
    // private call has earlier been made by [NSFontFontEffectsBox changeColor:], see 3674493. 
    // AppKit will have to be revised to allow this to work with anything that isn't an 
    // NSTextView. However, this might not be required for Tiger, since the background-color 
    // changing box in the font panel doesn't work in Mail (3674481), though it does in TextEdit.
    [self _changeCSSColorUsingSelector:@selector(setColor:) inRange:[[self _bridge] selectedDOMRange]];
}

- (void)_alignSelectionUsingCSSValue:(NSString *)CSSAlignmentValue
{
    // FIXME 3675191: This doesn't work yet. Maybe it's blocked by 3654850, or maybe something other than
    // just applyStyle: needs to be called for block-level attributes like this.
    WebBridge *bridge = [self _bridge];
    DOMCSSStyleDeclaration *style = [[bridge DOMDocument] createCSSStyleDeclaration];
    [style setTextAlign:CSSAlignmentValue];
    WebView *webView = [self _webView];
    if ([[webView _editingDelegateForwarder] webView:webView shouldApplyStyle:style toElementsInDOMRange:[bridge selectedDOMRange]]) {
        [[self _bridge] applyStyle:style];
    }
}

- (void)alignCenter:(id)sender
{
    [self _alignSelectionUsingCSSValue:@"center"];
}

- (void)alignJustified:(id)sender
{
    [self _alignSelectionUsingCSSValue:@"justify"];
}

- (void)alignLeft:(id)sender
{
    [self _alignSelectionUsingCSSValue:@"left"];
}

- (void)alignRight:(id)sender
{
    [self _alignSelectionUsingCSSValue:@"right"];
}

- (void)indent:(id)sender
{
    // Figure out current indent level.
    // Turn new indent level into CSS, then call applyStyle:.
    ERROR("unimplemented");
}

- (void)insertTab:(id)sender
{
    WebBridge *bridge = [self _bridge];
    if ([self _shouldReplaceSelectionWithText:@"\t" givenAction:WebViewInsertActionPasted]) {
        [bridge insertText:@"\t"];
    }
}

- (void)insertBacktab:(id)sender
{
    // Doing nothing matches normal NSTextView behavior. If we ever use WebView for a field-editor-type purpose
    // we might add code here.
}

- (void)insertNewline:(id)sender
{
    // Perhaps we should make this delegate call sensitive to the real DOM operation we actually do.
    WebBridge *bridge = [self _bridge];
    if ([self _shouldReplaceSelectionWithText:@"\n" givenAction:WebViewInsertActionTyped]) {
        [bridge insertNewline];
    }
}

- (void)insertParagraphSeparator:(id)sender
{
    // FIXME: Should this do something different?
    [self insertNewline:sender];
}

- (void)changeCaseOfLetter:(id)sender
{
    ERROR("unimplemented");
}

- (void)_changeWordCaseWithSelector:(SEL)selector
{
    WebBridge *bridge = [self _bridge];
    [self selectWord:nil];
    NSString *word = [[bridge selectedString] performSelector:selector];
    // FIXME: Does this need a different action context other than "typed"?
    if ([self _shouldReplaceSelectionWithText:word givenAction:WebViewInsertActionTyped]) {
        [bridge replaceSelectionWithText:word selectReplacement:NO];
    }
}

- (void)uppercaseWord:(id)sender
{
    [self _changeWordCaseWithSelector:@selector(uppercaseString)];
}

- (void)lowercaseWord:(id)sender
{
    [self _changeWordCaseWithSelector:@selector(lowercaseString)];
}

- (void)capitalizeWord:(id)sender
{
    [self _changeWordCaseWithSelector:@selector(capitalizedString)];
}

- (void)deleteForward:(id)sender
{
    ERROR("unimplemented");
}

- (void)deleteBackward:(id)sender
{
    WebBridge *bridge = [self _bridge];
    if ([bridge isSelectionEditable]) {
        WebView *webView = [self _webView];
        if ([[webView _editingDelegateForwarder] webView:webView shouldDeleteDOMRange:[bridge selectedDOMRange]]) {
            [bridge deleteKeyPressed];
        }
    }
}

- (void)deleteBackwardByDecomposingPreviousCharacter:(id)sender
{
    ERROR("unimplemented");
}

- (void)deleteWordForward:(id)sender
{
    [self moveWordForwardAndModifySelection:sender];
    [self delete:sender];
}

- (void)deleteWordBackward:(id)sender
{
    [self moveWordBackwardAndModifySelection:sender];
    [self delete:sender];
}

- (void)deleteToBeginningOfLine:(id)sender
{
    [self moveToBeginningOfLine:sender];
    [self delete:sender];
}

- (void)deleteToEndOfLine:(id)sender
{
    [self moveToEndOfLine:sender];
    [self delete:sender];
}

- (void)deleteToBeginningOfParagraph:(id)sender
{
    [self moveToBeginningOfParagraph:sender];
    [self delete:sender];
}

- (void)deleteToEndOfParagraph:(id)sender
{
    [self moveToEndOfParagraph:sender];
    [self delete:sender];
}

- (void)complete:(id)sender
{
    if (!_private->compController) {
        _private->compController = [[WebTextCompleteController alloc] initWithHTMLView:self];
    }
    [_private->compController doCompletion];
}

- (void)checkSpelling:(id)sender
{
    // WebCore does everything but update the spelling panel
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        ERROR("No NSSpellChecker");
        return;
    }
    NSString *badWord = [[self _bridge] advanceToNextMisspelling];
    if (badWord) {
        [checker updateSpellingPanelWithMisspelledWord:badWord];
    }
}
#if APPKIT_CODE_FOR_REFERENCE
    NSTextStorage *text = _getTextStorage(self);
    NSTextViewSharedData *sharedData = _getSharedData(self);
    if (text && ([text length] > 0) && [self isSelectable]) {
        NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
        NSRange selCharRange = [self selectedRange];
        NSRange misspellCharRange = {0, 0}, grammarCharRange = {0, 0};
        unsigned i, count;
        NSArray *grammarRanges = nil, *grammarDescriptions = nil;

        if (!checker || [checker windowIsSpellingPanel:[self window]]) return;

        misspellCharRange = [checker checkSpellingOfString:[text string] startingAt:NSMaxRange(selCharRange) language:nil wrap:YES inSpellDocumentWithTag:[self spellCheckerDocumentTag] wordCount:NULL];
#if GRAMMAR_CHECKING
        grammarCharRange = [checker checkGrammarOfString:[text string] startingAt:NSMaxRange(selCharRange) language:nil wrap:YES inSpellDocumentWithTag:[self spellCheckerDocumentTag] ranges:&grammarRanges descriptions:&grammarDescriptions reconnectOnError:YES];
#endif

        if (misspellCharRange.length > 0 && (grammarCharRange.length == 0 || misspellCharRange.location <= grammarCharRange.location)) {
            // select the text and drive the panel
            [self setSelectedRange:misspellCharRange affinity:NSSelectionAffinityUpstream stillSelecting:NO];
            if ([self isEditable]) {
                [self _addSpellingAttributeForRange:misspellCharRange];
                sharedData->_excludedSpellingCharRange = misspellCharRange;
            }
            [self scrollRangeToVisible:misspellCharRange];
            [checker updateSpellingPanelWithMisspelledWord:[[text string] substringWithRange:misspellCharRange]];
        } else if (grammarCharRange.length > 0) {
            [self setSelectedRange:grammarCharRange affinity:NSSelectionAffinityUpstream stillSelecting:NO];
            count = [grammarRanges count];
            if ([self isEditable]) {
                for (i = 0; i < count; i++) {
                    NSRange range = [grammarRanges rangeAtIndex:i];
                    range.location += grammarCharRange.location;
                    [self _addSpellingAttributeForRange:range];
                    [_getLayoutManager(self) addTemporaryAttributes:[NSDictionary dictionaryWithObjectsAndKeys:[grammarDescriptions objectAtIndex:i], NSToolTipAttributeName, nil] forCharacterRange:range];
                }
            }
            [self scrollRangeToVisible:grammarCharRange];
        } else {
            // Cause the beep to indicate there are no more misspellings.
            [checker updateSpellingPanelWithMisspelledWord:@""];
        }
    }
#endif


- (void)showGuessPanel:(id)sender
{
    // WebCore does everything but update the spelling panel
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        ERROR("No NSSpellChecker");
        return;
    }

    NSString *badWord = [[self _bridge] advanceToNextMisspelling];
    if (badWord) {
        [checker updateSpellingPanelWithMisspelledWord:badWord];
    }
    [[checker spellingPanel] orderFront:sender];
}
#if APPKIT_CODE_FOR_REFERENCE
    NSTextStorage *text = _getTextStorage(self);
    NSTextViewSharedData *sharedData = _getSharedData(self);
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (text && ([text length] > 0) && [self isSelectable]) {
        NSRange selCharRange = [self selectedRange];
        NSRange misspellCharRange = {0, 0};
        if (checker && ![checker windowIsSpellingPanel:[self window]]) {
            misspellCharRange = [checker checkSpellingOfString:[text string] startingAt:((selCharRange.location > 0) ? (selCharRange.location - 1) : 0) language:nil wrap:YES inSpellDocumentWithTag:[self spellCheckerDocumentTag] wordCount:NULL];
            if (misspellCharRange.length) {
                // select the text and drive the panel
                [self setSelectedRange:misspellCharRange affinity:NSSelectionAffinityUpstream stillSelecting:NO];
                if ([self isEditable]) {
                    [self _addSpellingAttributeForRange:misspellCharRange];
                    sharedData->_excludedSpellingCharRange = misspellCharRange;
                }
                [self scrollRangeToVisible:misspellCharRange];
                [checker updateSpellingPanelWithMisspelledWord:[[text string] substringWithRange:misspellCharRange]];
            }
        }
    }
    if (checker) {
        [[checker spellingPanel] orderFront:sender];
    }
#endif

- (void)_changeSpellingToWord:(NSString *)newWord
{
    WebBridge *bridge = [self _bridge];
    if (![bridge isSelectionEditable]) {
        return;
    }
    
    // Don't correct to empty string.  (AppKit checked this, we might as well too.)
    if (![NSSpellChecker sharedSpellChecker]) {
        ERROR("No NSSpellChecker");
        return;
    }
    
    if ([newWord isEqualToString:@""]) {
        return;
    }

    if ([self _shouldReplaceSelectionWithText:newWord givenAction:WebViewInsertActionPasted]) {
        [bridge replaceSelectionWithText:newWord selectReplacement:YES];
    }
}
#if APPKIT_CODE_FOR_REFERENCE
    NSRange charRange = [self rangeForUserTextChange];
    if ([self isEditable] && charRange.location != NSNotFound) {
        NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
        if (!checker || [checker windowIsSpellingPanel:[self window]]) return;
        
        // Don't correct to empty string.
        if ([newWord isEqualToString:@""]) return;
        
        if ([self shouldChangeTextInRange:charRange replacementString:newWord]) {
            [self replaceCharactersInRange:charRange withString:newWord];
            charRange.length = [newWord length];
            [self setSelectedRange:charRange affinity:NSSelectionAffinityUpstream stillSelecting:NO];
            [self didChangeText];
        }
    }
#endif

- (void)changeSpelling:(id)sender {
    [self _changeSpellingToWord:[[sender selectedCell] stringValue]];
}

- (void)ignoreSpelling:(id)sender {
    WebBridge *bridge = [self _bridge];
    if (![bridge isSelectionEditable]) {
        return;
    }
    
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        ERROR("No NSSpellChecker");
        return;
    }
    
    NSString *stringToIgnore = [sender stringValue];
    unsigned int length = [stringToIgnore length];
    if (stringToIgnore && length > 0) {
        [checker ignoreWord:stringToIgnore inSpellDocumentWithTag:[[self _webView] spellCheckerDocumentTag]];
        //??? need to clear any special markup for misspelled words
    }
}
#if APPKIT_CODE_FOR_REFERENCE
    NSRange charRange = [self rangeForUserTextChange];
    if ([self isEditable]) {
        NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
        NSString *stringToIgnore;
        unsigned int length;
        
        if (!checker) return;
        
        stringToIgnore = [sender stringValue];
        length = [stringToIgnore length];
        if (stringToIgnore && length > 0) {
            [checker ignoreWord:stringToIgnore inSpellDocumentWithTag:[self spellCheckerDocumentTag]];
            if (length == charRange.length && [stringToIgnore isEqualToString:[[_getTextStorage(self) string] substringWithRange:charRange]]) {
                [self _removeSpellingAttributeForRange:charRange];
            }
        }
    }
#endif


#if APPKIT_CODE_FOR_REFERENCE

- (void)_changeSpellingFromMenu:(id)sender {
    [self _changeSpellingToWord:[sender title]];
}

- (void)_ignoreSpellingFromMenu:(id)sender {
    NSRange charRange = [self rangeForUserTextChange];
    if ([self isEditable] && charRange.location != NSNotFound && charRange.length > 0) {
        NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
        if (!checker || [checker windowIsSpellingPanel:[self window]]) return;
        [self _removeSpellingAttributeForRange:charRange];
        [checker ignoreWord:[[_getTextStorage(self) string] substringWithRange:charRange] inSpellDocumentWithTag:[self spellCheckerDocumentTag]];
    }
}

- (void)_learnSpellingFromMenu:(id)sender {
    NSRange charRange = [self rangeForUserTextChange];
    if ([self isEditable] && charRange.location != NSNotFound && charRange.length > 0) {
        NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
        if (!checker || [checker windowIsSpellingPanel:[self window]]) return;
        [self _removeSpellingAttributeForRange:charRange];
        [checker learnWord:[[_getTextStorage(self) string] substringWithRange:charRange]];
    }
}

- (void)_forgetSpellingFromMenu:(id)sender {
    NSRange charRange = [self rangeForUserTextChange];
    if ([self isEditable] && charRange.location != NSNotFound && charRange.length > 0) {
        NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
        if (!checker || [checker windowIsSpellingPanel:[self window]]) return;
        [checker forgetWord:[[_getTextStorage(self) string] substringWithRange:charRange]];
    }
}

- (void)_openLinkFromMenu:(id)sender {
    NSTextStorage *text = _getTextStorage(self);
    NSRange charRange = [self selectedRange];
    if (charRange.location != NSNotFound && charRange.length > 0) {
        id link = [text attribute:NSLinkAttributeName atIndex:charRange.location effectiveRange:NULL];
        if (link) {
            [self clickedOnLink:link atIndex:charRange.location];
        } else {
            NSString *string = [[text string] substringWithRange:charRange];
            link = [NSURL URLWithString:string];
            if (link) [[NSWorkspace sharedWorkspace] openURL:link];
        }
    }
}

#endif

- (void)performFindPanelAction:(id)sender
{
    ERROR("unimplemented");
}

- (void)startSpeaking:(id)sender
{
    WebBridge *bridge = [self _bridge];
    DOMRange *range = [bridge selectedDOMRange];
    if (!range || [range collapsed]) {
        range = [self _entireDOMRange];
    }
    [NSApp speakString:[bridge stringForRange:range]];
}

- (void)stopSpeaking:(id)sender
{
    [NSApp stopSpeaking:sender];
}

#if 0

- (void)transpose:(id)sender;
- (void)transposeWords:(id)sender;
- (void)insertNewlineIgnoringFieldEditor:(id)sender;
- (void)insertTabIgnoringFieldEditor:(id)sender;
- (void)yank:(id)sender;
- (void)yankAndSelect:(id)sender;
- (void)setMark:(id)sender;
- (void)deleteToMark:(id)sender;
- (void)selectToMark:(id)sender;
- (void)swapWithMark:(id)sender;
- (void)moveParagraphBackwardAndModifySelection:(id)sender;
- (void)moveParagraphForwardAndModifySelection:(id)sender;
- (void)pageUpAndModifySelection:(id)sender;
- (void)pageDownAndModifySelection:(id)sender;
- (void)insertTable:(id)sender;

#endif

@end

@implementation WebHTMLView (WebTextSizing)

- (void)_web_textSizeMultiplierChanged
{
    [self _updateTextSizeMultiplier];
}

@end

@implementation NSArray (WebHTMLView)

- (void)_web_makePluginViewsPerformSelector:(SEL)selector withObject:(id)object
{
    NSEnumerator *enumerator = [self objectEnumerator];
    WebNetscapePluginEmbeddedView *view;
    while ((view = [enumerator nextObject]) != nil) {
        if ([view isKindOfClass:[WebNetscapePluginEmbeddedView class]]) {
            [view performSelector:selector withObject:object];
        }
    }
}

@end

@implementation WebElementOrTextFilter

+ (WebElementOrTextFilter *)filter 
{
    if (!elementOrTextFilterInstance)
        elementOrTextFilterInstance = [[WebElementOrTextFilter alloc] init];
    return elementOrTextFilterInstance;
}

- (short)acceptNode:(DOMNode *)n
{
    return ([n isKindOfClass:[DOMElement class]] || [n isKindOfClass:[DOMText class]]) ? DOM_FILTER_ACCEPT : DOM_FILTER_SKIP;
}

@end

@implementation WebHTMLView (WebInternal)

- (void)_selectionChanged
{
    [self _updateSelectionForInputManager];
    [self _updateFontPanel];
}

- (void)_updateFontPanel
{
    // FIXME: NSTextView bails out if becoming or resigning first responder, for which it has ivar flags. Not
    // sure if we need to do something similar.
    
    WebBridge *bridge = [self _bridge];

    if (![bridge isSelectionEditable]) {
        return;
    }
    
    NSWindow *window = [self window];
    // FIXME: is this first-responder check correct? What happens if a subframe is editable and is first responder?
    if ([NSApp keyWindow] != window || [window firstResponder] != self) {
        return;
    }
    
    BOOL onlyOneFontInSelection = YES;
    NSFont *font = nil;
    
    if (![self _hasSelection]) {
        font = [bridge fontForCurrentPosition];
    } 
    else {
        DOMRange *selection = [bridge selectedDOMRange];
        DOMNode *startContainer = [selection startContainer];
        DOMNode *endContainer = [selection endContainer];
        
        ASSERT(startContainer);
        ASSERT(endContainer);
        ASSERT([[WebElementOrTextFilter filter] acceptNode:startContainer] == DOM_FILTER_ACCEPT);
        ASSERT([[WebElementOrTextFilter filter] acceptNode:endContainer] == DOM_FILTER_ACCEPT);
        
        font = [bridge renderedFontForNode:startContainer];
        
        if (startContainer != endContainer) {
            DOMDocument *document = [bridge DOMDocument];
            DOMTreeWalker *treeWalker = [document createTreeWalker:document :DOM_SHOW_ALL :[WebElementOrTextFilter filter] :NO];
            DOMNode *node = startContainer;
            [treeWalker setCurrentNode:node];
            while (node) {
                NSFont *otherFont = [bridge renderedFontForNode:node];
                if (![font isEqual:otherFont]) {
                    onlyOneFontInSelection = NO;
                    break;
                }
                if (node == endContainer)
                    break;
                node = [treeWalker nextNode];
            }
        }
    }
    
    // FIXME: for now, return a bogus font that distinguishes the empty selection from the non-empty
    // selection. We should be able to remove this once the rest of this code works properly.
    if (font == nil) {
        if (![self _hasSelection]) {
            font = [NSFont toolTipsFontOfSize:17];
        } else {
            font = [NSFont menuFontOfSize:23];
        }
    }
    ASSERT(font != nil);
        
    NSFontManager *fm = [NSFontManager sharedFontManager];
    [fm setSelectedFont:font isMultiple:!onlyOneFontInSelection];
    // FIXME: we don't keep track of selected attributes, or set them on the font panel. This
    // appears to have no effect on the UI. E.g., underlined text in Mail or TextEdit is
    // not reflected in the font panel. Maybe someday this will change.
}

- (unsigned int)_delegateDragSourceActionMask
{
    WebView *webView = [self _webView];
    NSPoint point = [webView convertPoint:[_private->mouseDownEvent locationInWindow] fromView:nil];
    _private->dragSourceActionMask = [[webView _UIDelegateForwarder] webView:webView dragSourceActionMaskForPoint:point];
    return _private->dragSourceActionMask;
}

@end


@implementation WebHTMLView (WebNSTextInputSupport)

- (NSArray *)validAttributesForMarkedText
{
    // FIXME: TEXTINPUT: validAttributesForMarkedText not yet implemented
    return [NSArray array];
}

- (unsigned int)characterIndexForPoint:(NSPoint)thePoint
{
    ERROR("TEXTINPUT: characterIndexForPoint: not yet implemented");
    return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange
{
    ERROR("TEXTINPUT: firstRectForCharacterRange: not yet implemented");
    return NSMakeRect(0,0,0,0);
}

- (NSRange)selectedRange
{
    ERROR("TEXTINPUT: selectedRange not yet implemented");
    return NSMakeRange(0,0);
}

- (NSRange)markedRange
{
    if (![self hasMarkedText]) {
	return NSMakeRange(NSNotFound,0);
    }

    DOMRange *markedDOMRange = [[self _bridge] markedDOMRange];

    unsigned rangeLocation = [markedDOMRange startOffset];
    unsigned rangeLength = [markedDOMRange endOffset] - rangeLocation;

    return NSMakeRange(rangeLocation, rangeLength);
}

- (NSAttributedString *)attributedSubstringFromRange:(NSRange)theRange
{
    ERROR("TEXTINPUT: attributedSubstringFromRange: not yet implemented");
    return nil;
}

- (long)conversationIdentifier
{
    return (long)self;
}

- (BOOL)hasMarkedText
{
    return [[self _bridge] markedDOMRange] != nil;
}

- (void)unmarkText
{
    [[self _bridge] clearMarkedDOMRange];
}

- (void)_selectMarkedText
{
    if ([self hasMarkedText]) {
	WebBridge *bridge = [self _bridge];
	DOMRange *markedTextRange = [bridge markedDOMRange];
	[bridge setSelectedDOMRange:markedTextRange affinity:NSSelectionAffinityUpstream];
    }
}

- (void)_selectRangeInMarkedText:(NSRange)range
{
    ASSERT([self hasMarkedText]);

    WebBridge *bridge = [self _bridge];
    DOMRange *selectedRange = [[bridge DOMDocument] createRange];
    DOMRange *markedRange = [bridge markedDOMRange];
    
    ASSERT([markedRange startContainer] == [markedRange endContainer]);

    unsigned selectionStart = [markedRange startOffset] + range.location;
    unsigned selectionEnd = selectionStart + range.length;

    [selectedRange setStart:[markedRange startContainer] :selectionStart];
    [selectedRange setEnd:[markedRange startContainer] :selectionEnd];

    [bridge setSelectedDOMRange:selectedRange affinity:NSSelectionAffinityUpstream];
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)newSelRange
{
    WebBridge *bridge = [self _bridge];

    if (![bridge isSelectionEditable])
	return;

    _private->ignoreMarkedTextSelectionChange = YES;

    // if we had marked text already, we need to make sure to replace
    // that, instead of the selection/caret
    [self _selectMarkedText];

    NSString *text;
    if ([string isKindOfClass:[NSAttributedString class]]) {
	ERROR("TEXTINPUT: requested set marked text with attributed string");
	text = [string string];
    } else {
	text = string;
    }

    [bridge replaceSelectionWithText:text selectReplacement:YES];
    [bridge setMarkedDOMRange:[bridge selectedDOMRange]];
    [self _selectRangeInMarkedText:newSelRange];

    _private->ignoreMarkedTextSelectionChange = NO;
}

- (void)doCommandBySelector:(SEL)aSelector
{
    [super doCommandBySelector:aSelector];
}

- (void)_discardMarkedText
{
    if (![self hasMarkedText])
	return;

    _private->ignoreMarkedTextSelectionChange = YES;

    [self _selectMarkedText];
    [[NSInputManager currentInputManager] markedTextAbandoned:self];
    [self unmarkText];
    [[self _bridge] deleteSelection];

    _private->ignoreMarkedTextSelectionChange = NO;
}
	
- (void)insertText:(id)string
{
    WebBridge *bridge = [self _bridge];

    if (![bridge isSelectionEditable] && ![self hasMarkedText]) {
	return;
    }

    NSString *text;
    if ([string isKindOfClass:[NSAttributedString class]]) {
	ERROR("TEXTINPUT: requested insert of attributed string");
	text = [string string];
    } else {
	text = string;
    }

    if (![self _shouldReplaceSelectionWithText:text givenAction:WebViewInsertActionTyped]) {
	[self _discardMarkedText];
	return;
    }

    _private->ignoreMarkedTextSelectionChange = YES;

    // if we had marked text, we need to make sure to replace
    // that, instead of the selection/caret
    [self _selectMarkedText];

    [bridge insertText:text];

    _private->ignoreMarkedTextSelectionChange = NO;

    // inserting unmarks any marked text
    [self unmarkText];
}

- (BOOL)_selectionIsInsideMarkedText
{
    WebBridge *bridge = [self _bridge];
    DOMRange *selection = [bridge selectedDOMRange];
    DOMRange *markedRange = [bridge markedDOMRange];

    ASSERT([markedRange startContainer] == [markedRange endContainer]);

    if ([selection startContainer] != [markedRange startContainer]) 
	return NO;

    if ([selection endContainer] != [markedRange startContainer])
	return NO;

    if ([selection startOffset] < [markedRange startOffset])
	return NO;

    if ([selection endOffset] > [markedRange endOffset])
	return NO;

    return YES;
}

- (void)_updateSelectionForInputManager
{
    if (![self hasMarkedText] || _private->ignoreMarkedTextSelectionChange)
	return;

    if ([self _selectionIsInsideMarkedText]) {
	DOMRange *selection = [[self _bridge] selectedDOMRange];
	DOMRange *markedDOMRange = [[self _bridge] markedDOMRange];

	unsigned markedSelectionStart = [selection startOffset] - [markedDOMRange startOffset];
	unsigned markedSelectionLength = [selection endOffset] - [selection startOffset];
	NSRange newSelectionRange = NSMakeRange(markedSelectionStart, markedSelectionLength);
	
	[[NSInputManager currentInputManager] markedTextSelectionChanged:newSelectionRange client:self];
    } else {
	[[NSInputManager currentInputManager] markedTextAbandoned:self];
	[self unmarkText];
    }
}

@end

/*
    This class runs the show for handing the complete: NSTextView operation.  It counts on its HTML view
    to call endRevertingChange: whenever the current completion needs to be aborted.
 
    The class is in one of two modes:  PopupWindow showing, or not.  It is shown when a completion yields
    more than one match.  If a completion yields one or zero matches, it is not shown, and **there is no
    state carried across to the next completion**.
 */
@implementation WebTextCompleteController

- (id)initWithHTMLView:(WebHTMLView *)view
{
    [super init];
    _view = view;
    return self;
}

- (void)dealloc
{
    [_popupWindow release];
    [_completions release];
    [_originalString release];
    [super dealloc];
}

- (void)_insertMatch:(NSString *)match
{
    //FIXME: 3769654 - We should preserve case of string being inserted, even in prefix (but then also be
    // able to revert that).  Mimic NSText.
    WebBridge *bridge = [_view _bridge];
    NSString *newText = [match substringFromIndex:prefixLength];
    [bridge replaceSelectionWithText:newText selectReplacement:YES];
}

// mostly lifted from NSTextView_KeyBinding.m
- (void)_buildUI
{
    NSRect scrollFrame = NSMakeRect(0, 0, 100, 100);
    NSRect tableFrame = NSZeroRect;    
    tableFrame.size = [NSScrollView contentSizeForFrameSize:scrollFrame.size hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSNoBorder];
    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:[NSNumber numberWithInt:0]];
    [column setWidth:tableFrame.size.width];
    [column setEditable:NO];
    
    _tableView = [[NSTableView alloc] initWithFrame:tableFrame];
    [_tableView setAutoresizingMask:NSViewWidthSizable];
    [_tableView addTableColumn:column];
    [column release];
    [_tableView setDrawsGrid:NO];
    [_tableView setCornerView:nil];
    [_tableView setHeaderView:nil];
    [_tableView setAutoresizesAllColumnsToFit:YES];
    [_tableView setDelegate:self];
    [_tableView setDataSource:self];
    [_tableView setTarget:self];
    [_tableView setDoubleAction:@selector(tableAction:)];
    
    NSScrollView *scrollView = [[NSScrollView alloc] initWithFrame:scrollFrame];
    [scrollView setBorderType:NSNoBorder];
    [scrollView setHasVerticalScroller:YES];
    [scrollView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [scrollView setDocumentView:_tableView];
    [_tableView release];
    
    _popupWindow = [[NSWindow alloc] initWithContentRect:scrollFrame styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    [_popupWindow setAlphaValue:0.88];
    [_popupWindow setContentView:scrollView];
    [scrollView release];
    [_popupWindow setHasShadow:YES];
    [_popupWindow setOneShot:YES];
    //[_popupWindow _setForceActiveControls:YES];   // AK secret - no known problem from leaving this out
    [_popupWindow setReleasedWhenClosed:NO];
}

// mostly lifted from NSTextView_KeyBinding.m
- (void)_placePopupWindow:(NSPoint)topLeft
{
    int numberToShow = [_completions count];
    if (numberToShow > 20) {
        numberToShow = 20;
    }

    NSRect windowFrame;
    NSPoint wordStart = topLeft;
    windowFrame.origin = [[_view window] convertBaseToScreen:[_view convertPoint:wordStart toView:nil]];
    windowFrame.size.height = numberToShow * [_tableView rowHeight] + (numberToShow + 1) * [_tableView intercellSpacing].height;
    windowFrame.origin.y -= windowFrame.size.height;
    NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:[NSFont systemFontOfSize:12.0], NSFontAttributeName, nil];
    float maxWidth = 0.0;
    int maxIndex = -1;
    int i;
    for (i = 0; i < numberToShow; i++) {
        float width = ceil([[_completions objectAtIndex:i] sizeWithAttributes:attributes].width);
        if (width > maxWidth) {
            maxWidth = width;
            maxIndex = i;
        }
    }
    windowFrame.size.width = 100;
    if (maxIndex >= 0) {
        maxWidth = ceil([NSScrollView frameSizeForContentSize:NSMakeSize(maxWidth, 100) hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSNoBorder].width);
        maxWidth = ceil([NSWindow frameRectForContentRect:NSMakeRect(0, 0, maxWidth, 100) styleMask:NSBorderlessWindowMask].size.width);
        maxWidth += 5.0;
        windowFrame.size.width = MAX(maxWidth, windowFrame.size.width);
        maxWidth = MIN(400.0, windowFrame.size.width);
    }
    [_popupWindow setFrame:windowFrame display:NO];
    
    [_tableView reloadData];
    [_tableView selectRow:0 byExtendingSelection:NO];
    [_tableView scrollRowToVisible:0];
    [self _reflectSelection];
    [_popupWindow setLevel:NSPopUpMenuWindowLevel];
    [_popupWindow orderFront:nil];    
    [[_view window] addChildWindow:_popupWindow ordered:NSWindowAbove];
}

- (void)doCompletion
{
    if (!_popupWindow) {
        NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
        if (!checker) {
            ERROR("No NSSpellChecker");
            return;
        }

        // Get preceeding word stem
        WebBridge *bridge = [_view _bridge];
        DOMRange *selection = [bridge selectedDOMRange];
        DOMRange *wholeWord = [bridge rangeByExpandingSelectionWithGranularity:WebSelectByWord];
        DOMRange *prefix = [wholeWord cloneRange];
        [prefix setEnd:[selection startContainer] :[selection startOffset]];

        // Reject some NOP cases
        if ([prefix collapsed]) {
            NSBeep();
            return;
        }
        NSString *prefixStr = [bridge stringForRange:prefix];
        NSString *trimmedPrefix = [prefixStr stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if ([trimmedPrefix length] == 0) {
            NSBeep();
            return;
        }
        prefixLength = [prefixStr length];

        // Lookup matches
        [_completions release];
        _completions = [checker completionsForPartialWordRange:NSMakeRange(0, [prefixStr length]) inString:prefixStr language:nil inSpellDocumentWithTag:[[_view _webView] spellCheckerDocumentTag]];
        [_completions retain];
    
        if (!_completions || [_completions count] == 0) {
            NSBeep();
        } else if ([_completions count] == 1) {
            [self _insertMatch:[_completions objectAtIndex:0]];
        } else {
            ASSERT(!_originalString);       // this should only be set IFF we have a popup window
            _originalString = [[bridge stringForRange:selection] retain];
            [self _buildUI];
            NSRect wordRect = [bridge caretRectAtNode:[wholeWord startContainer] offset:[wholeWord startOffset]];
            // +1 to be under the word, not the caret
            // FIXME - 3769652 - Wrong positioning for right to left languages.  We should line up the upper
            // right corner with the caret instead of upper left, and the +1 would be a -1.
            NSPoint wordLowerLeft = { NSMinX(wordRect)+1, NSMaxY(wordRect) };
            [self _placePopupWindow:wordLowerLeft];
        }
    } else {
        [self endRevertingChange:YES moveLeft:NO];
    }
}

- (void)endRevertingChange:(BOOL)revertChange moveLeft:(BOOL)goLeft
{
    if (_popupWindow) {
        // tear down UI
        [[_view window] removeChildWindow:_popupWindow];
        [_popupWindow orderOut:self];
        // Must autorelease because event tracking code may be on the stack touching UI
        [_popupWindow autorelease];
        _popupWindow = nil;

        if (revertChange) {
            WebBridge *bridge = [_view _bridge];
            [bridge replaceSelectionWithText:_originalString selectReplacement:YES];
        } else if (goLeft) {
            [_view moveBackward:nil];
        } else {
            [_view moveForward:nil];
        }
        [_originalString release];
        _originalString = nil;
    }
    // else there is no state to abort if the window was not up
}

// WebHTMLView gives us a crack at key events it sees.  Return whether we consumed the event.
// The features for the various keys mimic NSTextView.
- (BOOL)filterKeyDown:(NSEvent *)event {
    if (_popupWindow) {
        NSString *string = [event charactersIgnoringModifiers];
        unichar c = [string characterAtIndex:0];
        if (c == NSUpArrowFunctionKey) {
            int selectedRow = [_tableView selectedRow];
            if (0 < selectedRow) {
                [_tableView selectRow:selectedRow-1 byExtendingSelection:NO];
                [_tableView scrollRowToVisible:selectedRow-1];
            }
            return YES;
        } else if (c == NSDownArrowFunctionKey) {
            int selectedRow = [_tableView selectedRow];
            if (selectedRow < (int)[_completions count]-1) {
                [_tableView selectRow:selectedRow+1 byExtendingSelection:NO];
                [_tableView scrollRowToVisible:selectedRow+1];
            }
            return YES;
        } else if (c == NSRightArrowFunctionKey || c == '\n' || c == '\r' || c == '\t') {
            [self endRevertingChange:NO moveLeft:NO];
            return YES;
        } else if (c == NSLeftArrowFunctionKey) {
            [self endRevertingChange:NO moveLeft:YES];
            return YES;
        } else if (c == 0x1b || c == NSF5FunctionKey) {
            [self endRevertingChange:YES moveLeft:NO];
            return YES;
        } else if (c == ' ' || ispunct(c)) {
            [self endRevertingChange:NO moveLeft:NO];
            return NO;  // let the char get inserted
        }
    }
    return NO;
}

- (void)_reflectSelection
{
    int selectedRow = [_tableView selectedRow];
    ASSERT(selectedRow >= 0 && selectedRow < (int)[_completions count]);
    [self _insertMatch:[_completions objectAtIndex:selectedRow]];
}

- (void)tableAction:(id)sender {
    [self _reflectSelection];
    [self endRevertingChange:NO moveLeft:NO];
}

- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
    return [_completions count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(int)row
{
    return [_completions objectAtIndex:row];
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    [self _reflectSelection];
}

@end
