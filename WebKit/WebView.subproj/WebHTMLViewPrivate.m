/*	WebHTMLViewPrivate.mm
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/

#import <WebKit/WebHTMLViewPrivate.h>

#import <AppKit/NSResponder_Private.h>

#import <WebFoundation/WebAssertions.h>

#import <WebKit/WebBridge.h>
#import <WebKit/WebContextMenuDelegate.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPolicyDelegate.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebWindowOperationsDelegate.h>

@interface NSView (AppKitSecretsIKnowAbout)
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView;
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect;
- (NSRect)_dirtyRect;
- (void)_drawRect:(NSRect)rect clip:(BOOL)clip;
@end

@interface NSView (WebNSViewDisplayExtras)
- (void)_web_stopIfPluginView;
- (void)_web_propagateDirtyRectToAncestor;
- (void)_web_dumpDirtyRects;
@end

@interface WebNSTextView : NSTextView
{
}
@end

@interface WebNSView : NSView
{
}
@end

@interface NSMutableDictionary (WebHTMLViewExtras)
- (void)_web_setObjectIfNotNil:(id)object forKey:(id)key;
@end

@implementation WebHTMLViewPrivate

- (void)dealloc
{
    [draggingImageElement release];
    [super dealloc];
}

@end


@implementation WebHTMLView (WebPrivate)

// Danger Will Robinson.  We have to poseAsClass: as early as possible
// so that any NSViews will be created with the appropriate poser.
+ (void)load
{
    [[WebNSTextView class] poseAsClass:[NSTextView class]];
    [[WebNSView class] poseAsClass:[NSView class]];
}


- (void)_adjustFrames
{
    // Ick!  khtml set the frame size during layout and
    // the frame origins during drawing!  So we have to 
    // layout and do a draw with rendering disabled to
    // correclty adjust the frames.
    [[self _bridge] adjustFrames:[self frame]];
}

- (void)_reset
{
    NSArray *subviews = [[self subviews] copy];
    [subviews makeObjectsPerformSelector:@selector(_web_stopIfPluginView)];
    [subviews release];

    [WebImageRenderer stopAnimationsInView:self];
}

- (WebController *)_controller
{
    return [[self _web_parentWebView] controller];
}

- (WebFrame *)_frame
{
    WebView *webView = [self _web_parentWebView];
    return [[webView controller] frameForView:webView];
}

// Required so view can access the part's selection.
- (WebBridge *)_bridge
{
    return [[self _frame] _bridge];
}

BOOL _modifierTrackingEnabled = FALSE;

+ (void)_setModifierTrackingEnabled:(BOOL)enabled
{
    _modifierTrackingEnabled = enabled;
}

+ (BOOL)_modifierTrackingEnabled
{
    return _modifierTrackingEnabled;
}

+ (void)_postFlagsChangedEvent:(NSEvent *)flagsChangedEvent
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved location:[[flagsChangedEvent window] convertScreenToBase:[NSEvent mouseLocation]] modifierFlags:[flagsChangedEvent modifierFlags] timestamp:[flagsChangedEvent timestamp] windowNumber:[flagsChangedEvent windowNumber] context:[flagsChangedEvent context] eventNumber:0 clickCount:0 pressure:0];

    // pretend it's a mouse move
    [[NSNotificationCenter defaultCenter] postNotificationName:NSMouseMovedNotification object:self userInfo:[NSDictionary dictionaryWithObject:fakeEvent forKey:@"NSEvent"]];
}

- (NSDictionary *)_elementAtPoint:(NSPoint)point
{
    NSDictionary *elementInfoWC = [[self _bridge] elementAtPoint:point];
    NSMutableDictionary *elementInfo = [NSMutableDictionary dictionary];

    [elementInfo _web_setObjectIfNotNil:[elementInfoWC objectForKey:WebCoreElementLinkURL] forKey:WebElementLinkURLKey];
    [elementInfo _web_setObjectIfNotNil:[elementInfoWC objectForKey:WebCoreElementLinkLabel] forKey:WebElementLinkLabelKey];
    [elementInfo _web_setObjectIfNotNil:[elementInfoWC objectForKey:WebCoreElementImageURL] forKey:WebElementImageURLKey];
    [elementInfo _web_setObjectIfNotNil:[elementInfoWC objectForKey:WebCoreElementString] forKey:WebElementStringKey];
    [elementInfo _web_setObjectIfNotNil:[elementInfoWC objectForKey:WebCoreElementImage] forKey:WebElementImageKey];
    [elementInfo _web_setObjectIfNotNil:[elementInfoWC objectForKey:WebCoreElementImageLocation] forKey:WebElementImageLocationKey];
    
    WebView *webView = [self _web_parentWebView];
    WebFrame *webFrame = [[webView controller] frameForView:webView];

    NSString *frameName = [elementInfoWC objectForKey:WebCoreElementLinkTarget];
    if ([frameName length] == 0) {
        [elementInfo setObject:webFrame forKey:WebElementLinkTargetFrameKey];
    } else if (![frameName isEqualToString:@"_blank"]) {
        [elementInfo setObject:[webFrame frameNamed:frameName] forKey:WebElementLinkTargetFrameKey];
    }

    [elementInfo setObject:webFrame forKey:WebElementFrameKey];
    
    return elementInfo;
}

- (BOOL)_continueAfterClickPolicyForEvent:(NSEvent *)event
{
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    WebController *controller = [self _controller];
    WebClickPolicy *clickPolicy;

    clickPolicy = [[controller policyDelegate] clickPolicyForElement:[self _elementAtPoint:point]
                                                              button:[event type]
                                                       modifierFlags:[event modifierFlags]];

    WebPolicyAction clickAction = [clickPolicy policyAction];
    NSURL *URL = [clickPolicy URL];

    switch (clickAction) {
        case WebClickPolicyShow:
            return YES;
        case WebClickPolicyOpenNewWindow:
            [[controller windowOperationsDelegate] openNewWindowWithURL:URL referrer:nil behind:NO];
            break;
        case WebClickPolicyOpenNewWindowBehind:
            [[controller windowOperationsDelegate] openNewWindowWithURL:URL referrer:nil behind:YES];
            break;
        case WebClickPolicySave:
        case WebClickPolicySaveAndOpenExternally:
            [controller _downloadURL:URL toPath:[clickPolicy path]];
            break;
        case WebClickPolicyIgnore:
            break;
        default:
            [NSException raise:NSInvalidArgumentException
                        format:@"clickPolicyForElement:button:modifierFlags: returned an invalid WebClickPolicy"];
    }
    return NO;
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
    [_subviews makeObjectsPerformSelector:@selector(_web_propagateDirtyRectToAncestor)];
    [self _setAsideSubviews];
    [super _recursiveDisplayRectIfNeededIgnoringOpacity:rect isVisibleRect:isVisibleRect
        rectIsVisibleRectForView:visibleView topView:topView];
    [self _restoreSubviews];
}

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect
{
    BOOL needToSetAsideSubviews = !_private->subviewsSetAside;
    
    if (needToSetAsideSubviews) {
        [_subviews makeObjectsPerformSelector:@selector(_web_propagateDirtyRectToAncestor)];
        [self _setAsideSubviews];
    }
    
    [super _recursiveDisplayAllDirtyWithLockFocus:needsLockFocus visRect:visRect];
    
    if (needToSetAsideSubviews) {
        [self _restoreSubviews];
    }
}

- (void)_mouseOverElement:(NSDictionary *)elementInformation modifierFlags:(unsigned)modifierFlags;
{
    if (elementInformation != nil || _private->lastMouseOverElementWasNotNil) {
        [[[self _controller] windowOperationsDelegate]
            mouseDidMoveOverElement:elementInformation modifierFlags:modifierFlags];
    }
    _private->lastMouseOverElementWasNotNil = elementInformation != nil;
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

@end

@implementation NSView (WebHTMLViewPrivate)

- (void)_web_stopIfPluginView
{
    if ([self isKindOfClass:[WebNetscapePluginEmbeddedView class]]) {
	WebNetscapePluginEmbeddedView *pluginView = (WebNetscapePluginEmbeddedView *)self;
        [pluginView stop];
    }
}

- (void)_web_propagateDirtyRectToAncestor
{
    [_subviews makeObjectsPerformSelector:@selector(_web_propagateDirtyRectToAncestor)];
    if ([self needsDisplay]) {
        [[self superview] setNeedsDisplayInRect:[self convertRect:[self _dirtyRect] toView:[self superview]]];
    }
}

@end

@implementation WebNSTextView

static BOOL inNSTextViewDrawRect;

// This code is here to make insertion point drawing work in a way that respects the
// HTML view layering. If we can find a way to make it work without poseAsClass, we
// should do that.

- (BOOL)_web_inHTMLView
{
    NSView *view = self;
    for (;;) {
        NSView *superview = [view superview];
        if (!superview) {
            return NO;
        }
        view = superview;
        if ([view isKindOfClass:[WebHTMLView class]]) {
            return YES;
        }
    }
}

- (BOOL)isOpaque
{
    if (![self _web_inHTMLView]) {
        return [super isOpaque];
    }

    // Text views in the HTML view all say they are not opaque.
    // This prevents the insertion point rect cache from being used,
    // and all the side effects are good since we want the view to act
    // opaque anyway. This could go in NSView instead of NSTextView,
    // but we need to pose as NSTextView anyway for the other override.
    // If we did this in NSView, we wouldn't need to call _web_propagateDirtyRectToAncestor.
    return NO;
}

- (void)drawInsertionPointInRect:(NSRect)rect color:(NSColor *)color turnedOn:(BOOL)turnedOn
{
    if (![self _web_inHTMLView]) {
        [super drawInsertionPointInRect:rect color:color turnedOn:turnedOn];
        return;
    }
    
    // Use the display mechanism to do all insertion point drawing in the web view.
    if (inNSTextViewDrawRect) {
        [super drawInsertionPointInRect:rect color:color turnedOn:turnedOn];
        return;
    }
    [super drawInsertionPointInRect:rect color:color turnedOn:NO];
    if (turnedOn) {
        rect.size.width = 1;
        [self setNeedsDisplayInRect:rect];
    }
}

- (void)_drawRect:(NSRect)rect clip:(BOOL)clip
{
    ASSERT(!inNSTextViewDrawRect);
    inNSTextViewDrawRect = YES;
    [super _drawRect:rect clip:clip];
    inNSTextViewDrawRect = NO;
}

@end

@implementation WebNSView

- (NSView *)opaqueAncestor
{
    if (![self isOpaque]) {
        return [super opaqueAncestor];
    }
    NSView *opaqueAncestor = self;
    NSView *superview = self;
    while ((superview = [superview superview])) {
        if ([superview isKindOfClass:[WebHTMLView class]]) {
            opaqueAncestor = superview;
        }
    }
    return opaqueAncestor;
}

@end

@implementation NSMutableDictionary (WebHTMLViewExtras)

- (void)_web_setObjectIfNotNil:(id)object forKey:(id)key
{
    if (object == nil) {
        return;
    }
    [self setObject:object forKey:key];
}

@end
