/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "WebInspector.h"
#import "WebInspectorInternal.h"

#import "WebInspectorPanel.h"
#import "WebInspectorOutlineView.h"
#import "WebNodeHighlight.h"
#import "WebView.h"
#import "WebViewPrivate.h"
#import "WebHTMLView.h"
#import "WebFrame.h"
#import "WebLocalizableStrings.h"
#import "WebKitNSStringExtras.h"

#import <WebKit/DOMCore.h>
#import <WebKit/DOMHTML.h>
#import <WebKit/DOMCSS.h>
#import <WebKit/DOMExtensions.h>
#import <WebKit/DOMPrivate.h>

static NSMapTable *lengthIgnoringWhitespaceCache = NULL;
static NSMapTable *lastChildIndexIgnoringWhitespaceCache = NULL;
static NSMapTable *lastChildIgnoringWhitespaceCache = NULL;

@interface NSWindow (NSWindowPrivate)
- (void)_setContentHasShadow:(BOOL)hasShadow;
@end

#pragma mark -

@implementation WebInspector
+ (WebInspector *)sharedWebInspector
{
    static WebInspector *_sharedWebInspector = nil;
    if (!_sharedWebInspector) {
        _sharedWebInspector = [[self alloc] initWithWebFrame:nil];
        _sharedWebInspector->_private->isSharedInspector = YES;
    }
    return _sharedWebInspector;
}

#pragma mark -

- (id)init
{
    if (![super initWithWindow:nil])
        return nil;

    [self setWindowFrameAutosaveName:@"Web Inspector"];

    _private = [[WebInspectorPrivate alloc] init];
    _private->ignoreWhitespace = YES;

    return self;
}

- (id)initWithWebFrame:(WebFrame *)webFrame
{
    if (![self init])
        return nil;
    [self setWebFrame:webFrame];
    return self;
}

- (void)dealloc
{
    [self close];
    [_private release];
    [super dealloc];
}

#pragma mark -

- (NSWindow *)window
{
    NSWindow *window = [super window];
    if (!window) {
        NSPanel *window = [[WebInspectorPanel alloc] initWithContentRect:NSMakeRect(60.0, 200.0, 350.0, 550.0) styleMask:(NSBorderlessWindowMask | NSUtilityWindowMask) backing:NSBackingStoreBuffered defer:YES];
        [window setBackgroundColor:[NSColor clearColor]];
        [window setOpaque:NO];
        [window setHasShadow:YES];
        [window _setContentHasShadow:NO];
        [window setWorksWhenModal:YES];
        [window setAcceptsMouseMovedEvents:YES];
        [window setIgnoresMouseEvents:NO];
        [window setFloatingPanel:YES];
        [window setReleasedWhenClosed:YES];
        [window setMovableByWindowBackground:YES];
        [window setHidesOnDeactivate:NO];
        [window setDelegate:self];
        [window setMinSize:NSMakeSize(280.0, 450.0)];

        _private->webView = [[WebView alloc] initWithFrame:[[window contentView] frame] frameName:nil groupName:nil];
        [_private->webView setFrameLoadDelegate:self];
        [_private->webView setUIDelegate:self];
        [_private->webView setDrawsBackground:NO];
        [_private->webView _setDashboardBehavior:WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows to:YES];
        [_private->webView _setDashboardBehavior:WebDashboardBehaviorAlwaysAcceptsFirstMouse to:YES];
        [[_private->webView windowScriptObject] setValue:self forKey:@"Inspector"];

        [window setContentView:_private->webView];

        NSString *path = [[NSBundle bundleForClass:[self class]] pathForResource:@"inspector" ofType:@"html" inDirectory:@"webInspector"];
        [[_private->webView mainFrame] loadRequest:[[[NSURLRequest alloc] initWithURL:[NSURL fileURLWithPath:path]] autorelease]];

        [self setWindow:window];
        return window;
    }

    return window;
}

- (void)windowWillClose:(NSNotification *)notification
{
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationWillResignActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationDidBecomeActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:WebNodeHighlightExpiredNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSViewFrameDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSSystemColorsDidChangeNotification object:nil];

    if (!_private->isSharedInspector)
        [self release];
}

- (IBAction)showWindow:(id)sender
{
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_nodeHighlightExpired:) name:WebNodeHighlightExpiredNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_updateSystemColors) name:NSSystemColorsDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationWillResignActive) name:NSApplicationWillResignActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationDidBecomeActive) name:NSApplicationDidBecomeActiveNotification object:nil];

    [self _update];
    [self _updateSystemColors];

    [super showWindow:sender];
}

#pragma mark -

- (void)setWebFrame:(WebFrame *)webFrame
{
    if ([webFrame isEqual:_private->webFrame])
        return;

    if (_private->webFrame) {
        [[NSNotificationCenter defaultCenter] removeObserver:self name:WebViewProgressFinishedNotification object:[_private->webFrame webView]];
        [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowWillCloseNotification object:[[_private->webFrame webView] window]];
    }
    
    [webFrame retain];
    [_private->webFrame release];
    _private->webFrame = webFrame;

    if (_private->webFrame) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(inspectedWebViewProgressFinished:) name:WebViewProgressFinishedNotification object:[_private->webFrame webView]];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(inspectedWindowWillClose:) name:NSWindowWillCloseNotification object:[[_private->webFrame webView] window]];
    }

    [_private->treeOutlineView setAllowsEmptySelection:NO];
    [self setFocusedDOMNode:[webFrame DOMDocument]];
}

- (WebFrame *)webFrame
{
    return _private->webFrame;
}

#pragma mark -

- (void)setRootDOMNode:(DOMNode *)node
{
    if ([node isSameNode:_private->rootNode])
        return;

    [node retain];
    [_private->rootNode release];
    _private->rootNode = node;

    [self _updateRoot];
}

- (DOMNode *)rootDOMNode
{
    return _private->rootNode;
}

#pragma mark -

- (void)setFocusedDOMNode:(DOMNode *)node
{
    if ([node isSameNode:_private->focusedNode])
        return;

    [[NSRunLoop currentRunLoop] cancelPerformSelector:@selector(_highlightNode:) target:self argument:_private->focusedNode];

    [node retain];
    [_private->focusedNode release];
    _private->focusedNode = node;

    DOMNode *root = _private->rootNode;
    if (!root || (![root isSameNode:node] && ![root _isAncestorOfNode:node]))
        [self setRootDOMNode:node];

    if (!_private->webViewLoaded)
        return;

    [self _revealAndSelectNodeInTree:node];
    [self _update];

    if (!_private->preventHighlight) {
        NSRect bounds = [node boundingBox];
        if (!NSIsEmptyRect(bounds)) {
            NSRect visible = [[[_private->webFrame frameView] documentView] visibleRect];
            BOOL needsScroll = !NSContainsRect(visible, bounds) && !NSContainsRect(bounds, visible);

            // only scroll if the bounds isn't in the visible rect and dosen't contain the visible rect
            if (needsScroll) {
                // scroll to the parent element if we arn't focued on an element
                DOMElement *element = (DOMElement *)_private->focusedNode;
                if (![element isKindOfClass:[DOMElement class]])
                    element = (DOMElement *)[element parentNode];

                if ([element isKindOfClass:[DOMElement class]])
                    [element scrollIntoViewIfNeeded:YES];

                // give time for the scroll to happen
                [self performSelector:@selector(_highlightNode:) withObject:node afterDelay:0.25];
            } else
                [self _highlightNode:node];
        } else
            [_private->currentHighlight expire];
    }
}

- (DOMNode *)focusedDOMNode
{
    return _private->focusedNode;
}

#pragma mark -

- (void)setSearchQuery:(NSString *)query
{
    if (_private->webViewLoaded) {
        if (!query)
            query = @"";
        [[_private->webView windowScriptObject] callWebScriptMethod:@"performSearch" withArguments:[NSArray arrayWithObject:query]];
        DOMHTMLInputElement *search = (DOMHTMLInputElement *)[_private->domDocument getElementById:@"search"];
        [search setValue:query];
    } else {
        [query retain];
        [_private->searchQuery release];
        _private->searchQuery = query;
    }
}

- (NSString *)searchQuery
{
    return _private->searchQuery;
}

#pragma mark -

- (NSArray *)searchResults
{
    return [NSArray arrayWithArray:_private->searchResults];
}
@end

#pragma mark -

@implementation WebInspector (WebInspectorScripting)
- (void)showOptionsMenu
{
    NSMenu *menu = [[NSMenu alloc] init];
    [menu setAutoenablesItems:NO];

    NSMenuItem *item = [[[NSMenuItem alloc] init] autorelease];
    [item setTitle:@"Ignore Whitespace"];
    [item setTarget:self];
    [item setAction:@selector(_toggleIgnoreWhitespace:)];
    [item setState:_private->ignoreWhitespace];
    [menu addItem:item];

    [NSMenu popUpContextMenu:menu withEvent:[[self window] currentEvent] forView:_private->webView];
    [menu release];

    // hack to force a layout and balance mouse events
    NSEvent *currentEvent = [[self window] currentEvent];
    NSEvent *event = [NSEvent mouseEventWithType:NSLeftMouseUp location:[currentEvent locationInWindow] modifierFlags:[currentEvent modifierFlags] timestamp:[currentEvent timestamp] windowNumber:[[currentEvent window] windowNumber] context:[currentEvent context] eventNumber:[currentEvent eventNumber] clickCount:[currentEvent clickCount] pressure:[currentEvent pressure]];
    [[[[_private->webView mainFrame] frameView] documentView] mouseUp:event];
}

- (void)selectNewRoot:(DOMHTMLSelectElement *)popup
{
    unsigned index = [popup selectedIndex];
    unsigned count = 0;

    DOMNode *currentNode = [self rootDOMNode];
    while (currentNode) {
        if (count == index) {
            [self setRootDOMNode:currentNode];
            break;
        }
        count++;
        currentNode = [currentNode parentNode];
    }
}

- (void)resizeTopArea
{
    NSWindow *window = [self window];
    NSEvent *event = [window currentEvent];
    NSPoint lastLocation = [window convertBaseToScreen:[event locationInWindow]];
    NSRect lastFrame = [window frame];
    NSSize minSize = [window minSize];
    NSSize maxSize = [window maxSize];
    BOOL mouseUpOccurred = NO;

    DOMHTMLElement *topArea = (DOMHTMLElement *)[_private->domDocument getElementById:@"top"];
    DOMHTMLElement *splitter = (DOMHTMLElement *)[_private->domDocument getElementById:@"splitter"];
    DOMHTMLElement *bottomArea = (DOMHTMLElement *)[_private->domDocument getElementById:@"bottom"];

    while (!mouseUpOccurred) {
        // set mouseUp flag here, but process location of event before exiting from loop, leave mouseUp in queue
        event = [window nextEventMatchingMask:(NSLeftMouseDraggedMask | NSLeftMouseUpMask) untilDate:[NSDate distantFuture] inMode:NSEventTrackingRunLoopMode dequeue:YES];

        if ([event type] == NSLeftMouseUp)
            mouseUpOccurred = YES;

        NSPoint newLocation = [window convertBaseToScreen:[event locationInWindow]];
        if (NSEqualPoints(newLocation, lastLocation))
            continue;

        NSRect proposedRect = lastFrame;
        long delta = newLocation.y - lastLocation.y;
        proposedRect.size.height -= delta;
        proposedRect.origin.y += delta;

        if (proposedRect.size.height < minSize.height) {
            proposedRect.origin.y -= minSize.height - proposedRect.size.height;
            proposedRect.size.height = minSize.height;
        } else if (proposedRect.size.height > maxSize.height) {
            proposedRect.origin.y -= maxSize.height - proposedRect.size.height;
            proposedRect.size.height = maxSize.height;
        }

        NSNumber *baseValue = [topArea valueForKey:@"clientHeight"];
        NSString *newValue = [NSString stringWithFormat:@"%dpx", [baseValue unsignedLongValue] - delta];
        [[topArea style] setProperty:@"height" :newValue :@""];

        baseValue = [splitter valueForKey:@"offsetTop"];
        newValue = [NSString stringWithFormat:@"%dpx", [baseValue unsignedLongValue] - delta];
        [[splitter style] setProperty:@"top" :newValue :@""];

        baseValue = [bottomArea valueForKey:@"offsetTop"];
        newValue = [NSString stringWithFormat:@"%dpx", [baseValue unsignedLongValue] - delta];
        [[bottomArea style] setProperty:@"top" :newValue :@""];

        [window setFrame:proposedRect display:YES];
        lastLocation = newLocation;
        lastFrame = proposedRect;

        [self _updateTreeScrollbar];
    }

    // post the mouse up event back to the queue so the WebView can get it
    [window postEvent:event atStart:YES];
}

- (void)treeViewScrollTo:(float)number
{
    float bottom = NSHeight([_private->treeOutlineView frame]) - NSHeight([_private->treeScrollView documentVisibleRect]);
    number = MAX(0.0, MIN(bottom, number));
    [[_private->treeScrollView contentView] scrollToPoint:NSMakePoint(0.0, number)];
}

- (float)treeViewOffsetTop
{
    return NSMinY([_private->treeScrollView documentVisibleRect]);
}

- (float)treeViewScrollHeight
{
    return NSHeight([_private->treeOutlineView frame]);
}

- (void)traverseTreeBackward
{
    if (_private->searchResultsVisible) {
        // if we have a search showing, we will only walk up and down the results
        int row = [_private->treeOutlineView selectedRow];
        if (row == -1)
            return;
        if ((row - 1) >= 0) {
            row--;
            [_private->treeOutlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];
            [_private->treeOutlineView scrollRowToVisible:row];
        }
        return;
    }

    DOMNode *node =  nil;
    // traverse backward, holding opton will traverse only to the previous sibling
    if ([[[self window] currentEvent] modifierFlags] & NSAlternateKeyMask) {
        if (!_private->ignoreWhitespace)
            node = [_private->focusedNode previousSibling];
        else
            node = [_private->focusedNode _previousSiblingSkippingWhitespace];
    } else {
        if (!_private->ignoreWhitespace)
            node = [_private->focusedNode _traversePreviousNode];
        else
            node = [_private->focusedNode _traversePreviousNodeSkippingWhitespace];
    }

    if (node) {
        DOMNode *root = [self rootDOMNode];
        if (![root isSameNode:node] && ![root _isAncestorOfNode:node])
            [self setRootDOMNode:[[self focusedDOMNode] _firstAncestorCommonWithNode:node]];
        [self setFocusedDOMNode:node];
    }
}

- (void)traverseTreeForward
{
    if (_private->searchResultsVisible) {
        // if we have a search showing, we will only walk up and down the results
        int row = [_private->treeOutlineView selectedRow];
        if (row == -1)
            return;
        if ((row + 1) < (int)[_private->searchResults count]) {
            row++;
            [_private->treeOutlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];
            [_private->treeOutlineView scrollRowToVisible:row];
        }
        return;
    }

    DOMNode *node =  nil;
    // traverse forward, holding opton will traverse only to the next sibling
    if ([[[self window] currentEvent] modifierFlags] & NSAlternateKeyMask) {
        if (!_private->ignoreWhitespace)
            node = [_private->focusedNode nextSibling];
        else
            node = [_private->focusedNode _nextSiblingSkippingWhitespace];
    } else {
        if (!_private->ignoreWhitespace)
            node = [_private->focusedNode _traverseNextNodeStayingWithin:nil];
        else
            node = [_private->focusedNode _traverseNextNodeSkippingWhitespaceStayingWithin:nil];
    }

    if (node) {
        DOMNode *root = [self rootDOMNode];
        if (![root isSameNode:node] && ![root _isAncestorOfNode:node])
            [self setRootDOMNode:[[self focusedDOMNode] _firstAncestorCommonWithNode:node]];
        [self setFocusedDOMNode:node];
    }
}

- (void)searchPerformed:(NSString *)query
{
    [query retain];
    [_private->searchQuery release];
    _private->searchQuery = query;

    BOOL show = [query length];
    if (show != _private->searchResultsVisible) {
        [_private->treeOutlineView setDoubleAction:(show ? @selector(_exitSearch:) : @selector(_focusRootNode:))];
        _private->preventRevealOnFocus = show;
        _private->searchResultsVisible = show;
    }

    [self _refreshSearch];
}
@end

#pragma mark -

@implementation WebInspector (WebInspectorPrivate)
- (IBAction)_toggleIgnoreWhitespace:(id)sender
{
    _private->ignoreWhitespace = !_private->ignoreWhitespace;
    [_private->treeOutlineView reloadData];
    [self _updateTreeScrollbar];
}

- (void)_highlightNode:(DOMNode *)node
{
    if (_private->currentHighlight) {
        [_private->currentHighlight expire];
        [_private->currentHighlight release];
        _private->currentHighlight = nil;
    }

    NSView *view = [[_private->webFrame frameView] documentView];
    NSRect bounds = NSIntersectionRect([node boundingBox], [view visibleRect]);
    if (!NSIsEmptyRect(bounds)) {
        NSArray *rects = nil;
        if ([node isKindOfClass:[DOMElement class]]) {
            DOMCSSStyleDeclaration *style = [_private->domDocument getComputedStyle:(DOMElement *)node :@""];
            if ([[style getPropertyValue:@"display"] isEqualToString:@"inline"])
                rects = [node lineBoxRects];
        } else if ([node isKindOfClass:[DOMText class]])
            rects = [node lineBoxRects];

        if (![rects count])
            rects = [NSArray arrayWithObject:[NSValue valueWithRect:bounds]];

        if ([view window])      // skip the highlight if we have no window (e.g. hidden tab)
            _private->currentHighlight = [[WebNodeHighlight alloc] initWithBounds:bounds andRects:rects forView:view];
    }
}

- (void)_nodeHighlightExpired:(NSNotification *)notification
{
    if (_private->currentHighlight == [notification object]) {
        [_private->currentHighlight release];
        _private->currentHighlight = nil;
    }
}

- (void)_exitSearch:(id)sender
{
    [self setSearchQuery:nil];
}

- (void)_focusRootNode:(id)sender
{
    int index = [sender selectedRow];
    if (index == -1 || !sender)
        return;
    DOMNode *node = [sender itemAtRow:index];
    if (![node isSameNode:[self rootDOMNode]])
        [self setRootDOMNode:node];
}

- (void)_revealAndSelectNodeInTree:(DOMNode *)node
{
    if (!_private->webViewLoaded)
        return;

    if (!_private->preventRevealOnFocus) {
        NSMutableArray *ancestors = [[NSMutableArray alloc] init];
        DOMNode *currentNode = [node parentNode];
        while (currentNode) {
            [ancestors addObject:currentNode];
            if ([currentNode isSameNode:[self rootDOMNode]])
                break;
            currentNode = [currentNode parentNode];
        }

        NSEnumerator *enumerator = [ancestors reverseObjectEnumerator];
        while ((currentNode = [enumerator nextObject]))
            [_private->treeOutlineView expandItem:currentNode];

        [ancestors release];
    }

    int index = [_private->treeOutlineView rowForItem:node];
    if (index != -1) {
        [_private->treeOutlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:index] byExtendingSelection:NO];
        [_private->treeOutlineView scrollRowToVisible:index];
    } else {
        [_private->treeOutlineView deselectAll:self];
    }
    [self _updateTreeScrollbar];
}

- (void)_refreshSearch
{
    BOOL selectFirst = ![_private->searchResults count];

    if (!_private->searchResults)
        _private->searchResults = [[NSMutableArray alloc] initWithCapacity:100];
    else
        [_private->searchResults removeAllObjects];

    NSString *query = _private->searchQuery;
    if (![query length]) {
        // switch back to the DOM tree and reveal the focused node
        DOMNode *node = [[self focusedDOMNode] retain];
        DOMNode *root = [self rootDOMNode];
        if (![root _isAncestorOfNode:node])
            [self setRootDOMNode:[node parentNode]];

        _private->preventSelectionRefocus = YES;
        [_private->treeOutlineView reloadData];
        _private->preventSelectionRefocus = NO;

        [self _revealAndSelectNodeInTree:node];
        [self _updateTraversalButtons];
        [node release];

        [[_private->webView windowScriptObject] callWebScriptMethod:@"toggleNoSelection" withArguments:[NSArray arrayWithObject:[NSNumber numberWithBool:NO]]];
        return;
    }

    unsigned count = 0;
    DOMNode *node = nil;

    if ([query hasPrefix:@"/"]) {
        // search document by Xpath query
        id nodeList = [[_private->webView windowScriptObject] callWebScriptMethod:@"resultsWithXpathQuery" withArguments:[NSArray arrayWithObject:query]];
        if ([nodeList isKindOfClass:[WebScriptObject class]]) {
            if ([[nodeList valueForKey:@"snapshotLength"] isKindOfClass:[NSNumber class]]) {
                count = [[nodeList valueForKey:@"snapshotLength"] unsignedLongValue];
                for( unsigned i = 0; i < count; i++ ) {
                    NSNumber *index = [[NSNumber alloc] initWithUnsignedLong:i];
                    NSArray *args = [[NSArray alloc] initWithObjects:&index count:1];
                    node = [nodeList callWebScriptMethod:@"snapshotItem" withArguments:args];
                    if (node)
                        [_private->searchResults addObject:node];
                    [args release];
                    [index release];
                }
            }
        }
    } else {
        // search nodes by node name, node value, id and class name
        node = [_private->webFrame DOMDocument];
        while ((node = [node _traverseNextNodeStayingWithin:nil])) {
            BOOL matched = NO;
            if ([[node nodeName] _webkit_hasCaseInsensitiveSubstring:query])
                matched = YES;
            else if ([node nodeType] == DOM_TEXT_NODE && [[node nodeValue] _webkit_hasCaseInsensitiveSubstring:query])
                matched = YES;
            else if ([node isKindOfClass:[DOMHTMLElement class]] && [[(DOMHTMLElement *)node idName] _webkit_hasCaseInsensitiveSubstring:query])
                matched = YES;
            else if ([node isKindOfClass:[DOMHTMLElement class]] && [[(DOMHTMLElement *)node className] _webkit_hasCaseInsensitiveSubstring:query])
                matched = YES;
            if (matched) {
                [_private->searchResults addObject:node];
                count++;
            }
        }
    }

    DOMHTMLElement *searchCount = (DOMHTMLElement *)[_private->domDocument getElementById:@"searchCount"];
    if (count == 1)
        [searchCount setInnerText:@"1 node"];
    else
        [searchCount setInnerText:[NSString stringWithFormat:@"%u nodes", count]];

    [_private->treeOutlineView reloadData];
    [self _updateTreeScrollbar];

    [[_private->webView windowScriptObject] callWebScriptMethod:@"toggleNoSelection" withArguments:[NSArray arrayWithObject:[NSNumber numberWithBool:!count]]];

    if (selectFirst && count)
        [_private->treeOutlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
}

- (void)_update
{
    if (_private->webViewLoaded) {
        if ([self focusedDOMNode]) {
            [[_private->webView windowScriptObject] callWebScriptMethod:@"toggleNoSelection" withArguments:[NSArray arrayWithObject:[NSNumber numberWithBool:NO]]];
            [[_private->webView windowScriptObject] callWebScriptMethod:@"updatePanes" withArguments:nil];
        } else
            [[_private->webView windowScriptObject] callWebScriptMethod:@"toggleNoSelection" withArguments:[NSArray arrayWithObject:[NSNumber numberWithBool:YES]]];
    }
}

- (void)_updateTraversalButtons
{
    DOMNode *node = [self focusedDOMNode];
    DOMHTMLButtonElement *back = (DOMHTMLButtonElement *)[_private->domDocument getElementById:@"traverseUp"];
    DOMHTMLButtonElement *forward = (DOMHTMLButtonElement *)[_private->domDocument getElementById:@"traverseDown"];
    if (_private->searchResultsVisible) {
        int row = [_private->treeOutlineView selectedRow];
        if (row != -1) {
            [forward setDisabled:!((row + 1) < (int)[_private->searchResults count])];
            [back setDisabled:!((row - 1) >= 0)];
        }
    } else {
        if (!_private->ignoreWhitespace)
            node = [_private->focusedNode _traverseNextNodeStayingWithin:nil];
        else
            node = [_private->focusedNode _traverseNextNodeSkippingWhitespaceStayingWithin:nil];
        [forward setDisabled:!node];

        if (!_private->ignoreWhitespace)
            node = [_private->focusedNode _traversePreviousNode];
        else
            node = [_private->focusedNode _traversePreviousNodeSkippingWhitespace];
        [back setDisabled:!node];
    }
}

- (void)_updateRoot
{
    if (!_private->webViewLoaded || _private->searchResultsVisible)
        return;

    DOMHTMLElement *titleArea = (DOMHTMLElement *)[_private->domDocument getElementById:@"treePopupTitleArea"];
    [titleArea setTextContent:[[self rootDOMNode] _displayName]];

    DOMHTMLElement *popup = (DOMHTMLElement *)[_private->domDocument getElementById:@"realTreePopup"];
    [popup setInnerHTML:@""]; // reset the list

    DOMNode *currentNode = [self rootDOMNode];
    while (currentNode) {
        DOMHTMLOptionElement *option = (DOMHTMLOptionElement *)[_private->domDocument createElement:@"option"];
        [option setTextContent:[currentNode _displayName]];
        [popup appendChild:option];
        currentNode = [currentNode parentNode];
    }

    _private->preventSelectionRefocus = YES;
    DOMNode *focusedNode = [[self focusedDOMNode] retain];
    [_private->treeOutlineView reloadData];
    if ([self rootDOMNode])
        [_private->treeOutlineView expandItem:[self rootDOMNode]];
    [self _revealAndSelectNodeInTree:focusedNode];
    [focusedNode release];
    _private->preventSelectionRefocus = NO;
}

- (void)_updateTreeScrollbar
{
    if (_private->webViewLoaded)
        [[_private->webView windowScriptObject] evaluateWebScript:@"treeScrollbar.refresh()"];
}

- (void)_updateSystemColors
{
    if (!_private->webViewLoaded)
        return;

    float red = 0.0, green = 0.0, blue = 0.0;
    NSColor *color = [[NSColor alternateSelectedControlColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    [color getRed:&red green:&green blue:&blue alpha:NULL];

    NSString *colorText = [NSString stringWithFormat:@"background-color: rgba(%d, %d, %d, 0.4) !important", (int)(red * 255), (int)(green * 255), (int)(blue * 255)];
    NSString *styleText = [NSString stringWithFormat:@"#styleRulesScrollview > .row.focused { %@ }; .treeList li.focused { %1$@ }", colorText];
    DOMElement *style = [_private->domDocument getElementById:@"systemColors"];
    if (!style) {
        style = [_private->domDocument createElement:@"style"];
        [[[_private->domDocument getElementsByTagName:@"head"] item:0] appendChild:style];
    }

    [style setAttribute:@"id" :@"systemColors"];
    [style setTextContent:styleText];
}

- (void)_applicationWillResignActive
{
    [(NSPanel *)[self window] setFloatingPanel:NO];
}

- (void)_applicationDidBecomeActive
{
    [(NSPanel *)[self window] setFloatingPanel:YES];
}

#pragma mark -

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    return NO;
}

+ (NSString *)webScriptNameForSelector:(SEL)aSelector {
    NSMutableString *name = [[NSStringFromSelector(aSelector) mutableCopy] autorelease];
    [name replaceOccurrencesOfString:@":" withString:@"_" options:NSLiteralSearch range:NSMakeRange(0, [name length])];
    if ([name hasSuffix:@"_"])
        return [name substringToIndex:[name length] - 1];
    return name;
}

+ (BOOL)isKeyExcludedFromWebScript:(const char *)name
{
    return NO;
}

#pragma mark -

- (void)inspectedWebViewProgressFinished:(NSNotification *)notification
{
    if ([notification object] == [[self webFrame] webView]) {
        [self setFocusedDOMNode:[[self webFrame] DOMDocument]];
        [self _update];
        [self _updateRoot];
        [self _highlightNode:[self focusedDOMNode]];
    }
}

- (void)inspectedWindowWillClose:(NSNotification *)notification
{
    [self setFocusedDOMNode:nil];
    [self setWebFrame:nil];
    [_private->treeOutlineView setAllowsEmptySelection:YES];
    [_private->treeOutlineView deselectAll:self];
    [self _update];
    [self _updateRoot];
}

#pragma mark -

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    // note: this is the Inspector's own WebView, not the one being inspected
    _private->webViewLoaded = YES;
    [[sender windowScriptObject] setValue:self forKey:@"Inspector"];

    [_private->domDocument release];
    _private->domDocument = (DOMHTMLDocument *)[[[sender mainFrame] DOMDocument] retain];

    [[self window] invalidateShadow];

    [self _update];
    [self _updateRoot];
    [self _updateSystemColors];

    if ([[self searchQuery] length])
        [self setSearchQuery:[self searchQuery]];

    [self _highlightNode:[self focusedDOMNode]];
}

- (NSView *)webView:(WebView *)sender plugInViewWithArguments:(NSDictionary *)arguments
{
    NSDictionary *attributes = [arguments objectForKey:@"WebPlugInAttributesKey"];
    if ([[attributes objectForKey:@"type"] isEqualToString:@"application/x-inspector-tree"]) {
        if (!_private->treeOutlineView) {
            _private->treeScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 250.0, 100.0)];
            [_private->treeScrollView setDrawsBackground:NO];
            [_private->treeScrollView setBorderType:NSNoBorder];
            [_private->treeScrollView setVerticalScroller:NO];
            [_private->treeScrollView setHasHorizontalScroller:NO];
            [_private->treeScrollView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
            [_private->treeScrollView setFocusRingType:NSFocusRingTypeNone];

            _private->treeOutlineView = [[WebInspectorOutlineView alloc] initWithFrame:[_private->treeScrollView frame]];
            [_private->treeOutlineView setHeaderView:nil];
            [_private->treeOutlineView setAllowsMultipleSelection:NO];
            [_private->treeOutlineView setAllowsEmptySelection:NO];
            [_private->treeOutlineView setDelegate:self];
            [_private->treeOutlineView setDataSource:self];
            [_private->treeOutlineView sendActionOn:(NSLeftMouseUpMask | NSLeftMouseDownMask | NSLeftMouseDraggedMask)];
            [_private->treeOutlineView setFocusRingType:NSFocusRingTypeNone];
            [_private->treeOutlineView setAutoresizesOutlineColumn:NO];
            [_private->treeOutlineView setRowHeight:15.0];
            [_private->treeOutlineView setTarget:self];
            [_private->treeOutlineView setDoubleAction:@selector(_focusRootNode:)];
            [_private->treeOutlineView setIndentationPerLevel:12.0];
            [_private->treeScrollView setDocumentView:_private->treeOutlineView];

            NSCell *headerCell = [[NSCell alloc] initTextCell:@""];
            NSCell *dataCell = [[NSCell alloc] initTextCell:@""];
            [dataCell setFont:[NSFont systemFontOfSize:11.0]];

            NSTableColumn *tableColumn = [[NSTableColumn alloc] initWithIdentifier:@"node"];
            [tableColumn setHeaderCell:headerCell];
            [tableColumn setDataCell:dataCell];
            [tableColumn setMinWidth:50];
            [tableColumn setWidth:300];
            [tableColumn setEditable:NO];
            [_private->treeOutlineView addTableColumn:tableColumn];
            [_private->treeOutlineView setOutlineTableColumn:tableColumn];

            [_private->treeOutlineView sizeLastColumnToFit];

            [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_updateTreeScrollbar) name:NSViewFrameDidChangeNotification object:_private->treeOutlineView]; 
        }

        return [_private->treeOutlineView enclosingScrollView];
    }

    return nil;
}

#pragma mark -

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    if (outlineView == _private->treeOutlineView) {
        if (!item && _private->searchResultsVisible)
            return [_private->searchResults count];
        if (!item)
            return 1;
        if (!_private->ignoreWhitespace)
            return [[item childNodes] length];
        return [item _lengthOfChildNodesIgnoringWhitespace];
    }
    return 0;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    if (outlineView == _private->treeOutlineView) {
        if (!_private->ignoreWhitespace)
            return [item hasChildNodes];
        return ([item _firstChildSkippingWhitespace] ? YES : NO);
    }
    return NO;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
    if (outlineView == _private->treeOutlineView) {
        if (!item && _private->searchResultsVisible)
            return [_private->searchResults objectAtIndex:index];
        if (!item)
            return [self rootDOMNode];
        id node = nil;
        if (!_private->ignoreWhitespace)
            node = [[item childNodes] item:index];
        else
            node = [item _childNodeAtIndexIgnoringWhitespace:index];
        if (!node)
            return nil;
        // cache the node because NSOutlineView assumes we hold on to it
        // if we don't hold on to the node it will be released before the next time the NSOutlineView needs it
        [_private->nodeCache addObject:node];
        return node;
    }
    return nil;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    if (outlineView == _private->treeOutlineView && item) {
        NSShadow *shadow = [[NSShadow alloc] init];
        [shadow setShadowColor:[NSColor blackColor]];
        [shadow setShadowBlurRadius:2.0];
        [shadow setShadowOffset:NSMakeSize(2.0,-2.0)];
        NSMutableParagraphStyle *para = [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
        [para setLineBreakMode:NSLineBreakByTruncatingTail];
        
        NSDictionary *attrs = [[NSDictionary alloc] initWithObjectsAndKeys:[NSColor whiteColor], NSForegroundColorAttributeName, shadow, NSShadowAttributeName, para, NSParagraphStyleAttributeName, nil];
        NSMutableAttributedString *string = [[NSMutableAttributedString alloc] initWithString:[item _displayName] attributes:attrs];
        [attrs release];

        if ([item hasChildNodes] && ![outlineView isItemExpanded:item]) {
            attrs = [[NSDictionary alloc] initWithObjectsAndKeys:[NSColor colorWithCalibratedRed:1.0 green:1.0 blue:1.0 alpha:0.5], NSForegroundColorAttributeName, shadow, NSShadowAttributeName, para, NSParagraphStyleAttributeName, nil];
            NSAttributedString *preview = [[NSAttributedString alloc] initWithString:[item _contentPreview] attributes:attrs];
            [string appendAttributedString:preview];
            [attrs release];
            [preview release];
        }

        [para release];
        [shadow release];
        return [string autorelease];
    }

    return nil;
}

- (void)outlineView:(NSOutlineView *)outlineView willDisplayOutlineCell:(id)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
    if (outlineView == _private->treeOutlineView)
        [cell setImage:([cell state] ? _private->downArrowImage : _private->rightArrowImage)];
}

- (void)outlineViewItemDidCollapse:(NSNotification *)notification
{
    if ([notification object] == _private->treeOutlineView) {
        DOMNode *node = [[notification userInfo] objectForKey:@"NSObject"];
        if (!node)
            return;

        // remove all child nodes from the node cache when the parent collapses
        node = [node firstChild];
        while (node) {
            NSMapRemove(lengthIgnoringWhitespaceCache, node);
            NSMapRemove(lastChildIndexIgnoringWhitespaceCache, node);
            NSMapRemove(lastChildIgnoringWhitespaceCache, node);
            [_private->nodeCache removeObject:node];
            node = [node nextSibling];
        }
    }
}

- (void)outlineViewSelectionDidChange:(NSNotification *)notification
{
    if ([notification object] == _private->treeOutlineView && !_private->preventSelectionRefocus && _private->webViewLoaded) {
        int index = [_private->treeOutlineView selectedRow];
        if (index != -1)
            [self setFocusedDOMNode:[_private->treeOutlineView itemAtRow:index]];
        [self _updateTreeScrollbar];
        [self _updateTraversalButtons];
    }
}
@end

#pragma mark -

@implementation WebInspectorPrivate
- (id)init
{
    [super init];
    nodeCache = [[NSMutableSet alloc] initWithCapacity:100];
    rightArrowImage = [[NSImage alloc] initWithContentsOfFile:[[NSBundle bundleForClass:[self class]] pathForResource:@"rightTriangle" ofType:@"png" inDirectory:@"webInspector/Images"]];
    downArrowImage = [[NSImage alloc] initWithContentsOfFile:[[NSBundle bundleForClass:[self class]] pathForResource:@"downTriangle" ofType:@"png" inDirectory:@"webInspector/Images"]];
    return self;
}

- (void)dealloc
{
    [webView release];
    [domDocument release];
    [webFrame release];
    [rootNode release];
    [focusedNode release];
    [searchQuery release];
    [nodeCache release];
    [treeScrollView release];
    [treeOutlineView release];
    [currentHighlight release];
    [rightArrowImage release];
    [downArrowImage release];
    [super dealloc];
}
@end

#pragma mark -

@implementation DOMNode (DOMNodeInspectorAdditions)
- (NSString *)_contentPreview
{
    if (![self hasChildNodes])
        return @"";

    unsigned limit = 0;
    NSMutableString *preview = [[NSMutableString alloc] initWithCapacity:100];
    [preview appendString:@" "];

    // always skip whitespace here
    DOMNode *currentNode = [self _traverseNextNodeSkippingWhitespaceStayingWithin:self];
    while (currentNode) {
        if ([currentNode nodeType] == DOM_TEXT_NODE)
            [preview appendString:[[currentNode nodeValue] _webkit_stringByCollapsingWhitespaceCharacters]];
        else
            [preview appendString:[currentNode _displayName]];
        currentNode = [currentNode _traverseNextNodeStayingWithin:self];
        if (++limit > 4) {
            unichar ellipsis = 0x2026;
            [preview appendString:[NSString stringWithCharacters:&ellipsis length:1]];
            break;
        }
    }

    return [preview autorelease];
}

#pragma mark -

- (BOOL)_isAncestorOfNode:(DOMNode *)node
{
    DOMNode *currentNode = [node parentNode];
    while (currentNode) {
        if ([self isSameNode:currentNode])
            return YES;
        currentNode = [currentNode parentNode];
    }
    return NO;
}

- (BOOL)_isDescendantOfNode:(DOMNode *)node
{
    return [node _isAncestorOfNode:self];
}

#pragma mark -

- (BOOL)_isWhitespace
{
    static NSCharacterSet *characters = nil;
    if (!characters)
        characters = [[[NSCharacterSet whitespaceAndNewlineCharacterSet] invertedSet] retain];
    NSRange range = [[self nodeValue] rangeOfCharacterFromSet:characters];
    return (range.location == NSNotFound);
}

- (unsigned long)_lengthOfChildNodesIgnoringWhitespace
{
    if (!lengthIgnoringWhitespaceCache)
        lengthIgnoringWhitespaceCache = NSCreateMapTable(NSObjectMapKeyCallBacks, NSIntMapValueCallBacks, 300);

    void *lookup = NSMapGet(lengthIgnoringWhitespaceCache, self);
    if (lookup)
        return (unsigned long)lookup;

    unsigned long count = 0;
    DOMNode *node = [self _firstChildSkippingWhitespace];
    while (node) {
        node = [node _nextSiblingSkippingWhitespace];
        count++;
    }

    NSMapInsert(lengthIgnoringWhitespaceCache, self, (void *)count);
    return count;
}

- (DOMNode *)_childNodeAtIndexIgnoringWhitespace:(unsigned long)nodeIndex
{
    unsigned long count = 0;
    DOMNode *node = nil;

    if (!lastChildIndexIgnoringWhitespaceCache)
        lastChildIndexIgnoringWhitespaceCache = NSCreateMapTable(NSObjectMapKeyCallBacks, NSIntMapValueCallBacks, 300);
    if (!lastChildIgnoringWhitespaceCache)
        lastChildIgnoringWhitespaceCache = NSCreateMapTable(NSObjectMapKeyCallBacks, NSObjectMapValueCallBacks, 300);

    void *cachedLastIndex = NSMapGet(lastChildIndexIgnoringWhitespaceCache, self);
    if (cachedLastIndex) {
        DOMNode *lastChild = (DOMNode *)NSMapGet(lastChildIgnoringWhitespaceCache, self);
        if (lastChild) {
            unsigned long cachedIndex = (unsigned long)cachedLastIndex;
            if (nodeIndex == cachedIndex) {
                return lastChild;
            } else if (nodeIndex > cachedIndex) {
                node = lastChild;
                count = cachedIndex;
            }
        }
    }

    if (!node)
        node = [self _firstChildSkippingWhitespace];

    while (node && count < nodeIndex) {
        node = [node _nextSiblingSkippingWhitespace];
        count++;
    }

    if (node) {
        NSMapInsert(lastChildIndexIgnoringWhitespaceCache, self, (void *)count);
        NSMapInsert(lastChildIgnoringWhitespaceCache, self, node);
    } else {
        NSMapRemove(lastChildIndexIgnoringWhitespaceCache, self);
        NSMapRemove(lastChildIgnoringWhitespaceCache, self);
    }

    return node;
}

#pragma mark -

- (DOMNode *)_nextSiblingSkippingWhitespace
{
    DOMNode *node = [self nextSibling];
    while ([node nodeType] == DOM_TEXT_NODE && [node _isWhitespace])
        node = [node nextSibling];
    return node;
}

- (DOMNode *)_previousSiblingSkippingWhitespace
{
    DOMNode *node = [self previousSibling];
    while ([node nodeType] == DOM_TEXT_NODE && [node _isWhitespace])
        node = [node previousSibling];
    return node;
}

- (DOMNode *)_firstChildSkippingWhitespace
{
    DOMNode *node = [self firstChild];
    while ([node nodeType] == DOM_TEXT_NODE && [node _isWhitespace])
        node = [node _nextSiblingSkippingWhitespace];
    return node;
}

- (DOMNode *)_lastChildSkippingWhitespace
{
    DOMNode *node = [self lastChild];
    while ([node nodeType] == DOM_TEXT_NODE && [node _isWhitespace])
        node = [node _previousSiblingSkippingWhitespace];
    return node;
}

#pragma mark -

- (DOMNode *)_firstAncestorCommonWithNode:(DOMNode *)node
{
    if ([[self parentNode] isSameNode:[node parentNode]])
        return [self parentNode];

    NSMutableArray *ancestorsOne = [[[NSMutableArray alloc] init] autorelease];
    NSMutableArray *ancestorsTwo = [[[NSMutableArray alloc] init] autorelease];

    DOMNode *currentNode = [self parentNode];
    while (currentNode) {
        [ancestorsOne addObject:currentNode];
        currentNode = [currentNode parentNode];
    }

    currentNode = [node parentNode];
    while (currentNode) {
        [ancestorsTwo addObject:currentNode];
        currentNode = [currentNode parentNode];
    }

    return [ancestorsOne firstObjectCommonWithArray:ancestorsTwo];
}

#pragma mark -

- (DOMNode *)_traverseNextNodeStayingWithin:(DOMNode *)stayWithin
{
    DOMNode *node = [self firstChild];
    if (node)
        return node;

    if ([self isSameNode:stayWithin])
        return 0;

    node = [self nextSibling];
    if (node)
        return node;

    node = self;
    while (node && ![node nextSibling] && (!stayWithin || ![[node parentNode] isSameNode:stayWithin]))
        node = [node parentNode];

    return [node nextSibling];
}

- (DOMNode *)_traverseNextNodeSkippingWhitespaceStayingWithin:(DOMNode *)stayWithin
{
    DOMNode *node = [self _firstChildSkippingWhitespace];
    if (node)
        return node;

    if ([self isSameNode:stayWithin])
        return 0;

    node = [self _nextSiblingSkippingWhitespace];
    if (node)
        return node;

    node = self;
    while (node && ![node _nextSiblingSkippingWhitespace] && (!stayWithin || ![[node parentNode] isSameNode:stayWithin]))
        node = [node parentNode];

    return [node _nextSiblingSkippingWhitespace];
}

- (DOMNode *)_traversePreviousNode
{
    DOMNode *node = [self previousSibling];
    while ([node lastChild])
        node = [node lastChild];
    if (node)
        return node;
    return [self parentNode];
}

- (DOMNode *)_traversePreviousNodeSkippingWhitespace
{
    DOMNode *node = [self _previousSiblingSkippingWhitespace];
    while ([node _lastChildSkippingWhitespace])
        node = [node _lastChildSkippingWhitespace];
    if (node)
        return node;
    return [self parentNode];
}

#pragma mark -

- (NSString *)_displayName
{
    switch([self nodeType]) {
        case DOM_DOCUMENT_NODE:
            return @"Document";
        case DOM_ELEMENT_NODE: {
            if ([self hasAttributes]) {
                NSMutableString *name = [NSMutableString stringWithFormat:@"<%@", [[self nodeName] lowercaseString]];
                NSString *value = [(DOMElement *)self getAttribute:@"id"];
                if ([value length])
                    [name appendFormat:@" id=\"%@\"", value];
                value = [(DOMElement *)self getAttribute:@"class"];
                if ([value length])
                    [name appendFormat:@" class=\"%@\"", value];
                if ([[self nodeName] caseInsensitiveCompare:@"a"] == NSOrderedSame) {
                    value = [(DOMElement *)self getAttribute:@"name"];
                    if ([value length])
                        [name appendFormat:@" name=\"%@\"", value];
                    value = [(DOMElement *)self getAttribute:@"href"];
                    if ([value length])
                        [name appendFormat:@" href=\"%@\"", value];
                } else if ([[self nodeName] caseInsensitiveCompare:@"img"] == NSOrderedSame) {
                    value = [(DOMElement *)self getAttribute:@"src"];
                    if ([value length])
                        [name appendFormat:@" src=\"%@\"", value];
                } else if ([[self nodeName] caseInsensitiveCompare:@"iframe"] == NSOrderedSame) {
                    value = [(DOMElement *)self getAttribute:@"src"];
                    if ([value length])
                        [name appendFormat:@" src=\"%@\"", value];
                } else if ([[self nodeName] caseInsensitiveCompare:@"input"] == NSOrderedSame) {
                    value = [(DOMElement *)self getAttribute:@"name"];
                    if ([value length])
                        [name appendFormat:@" name=\"%@\"", value];
                    value = [(DOMElement *)self getAttribute:@"type"];
                    if ([value length])
                        [name appendFormat:@" type=\"%@\"", value];
                } else if ([[self nodeName] caseInsensitiveCompare:@"form"] == NSOrderedSame) {
                    value = [(DOMElement *)self getAttribute:@"action"];
                    if ([value length])
                        [name appendFormat:@" action=\"%@\"", value];
                }
                [name appendString:@">"];
                return name;
            }
            return [NSString stringWithFormat:@"<%@>", [[self nodeName] lowercaseString]];
        }
        case DOM_TEXT_NODE: {
            if ([self _isWhitespace])
                return @"(whitespace)";
            NSString *value = [[self nodeValue] _webkit_stringByCollapsingWhitespaceCharacters];
            CFStringTrimWhitespace((CFMutableStringRef)value);
            return [NSString stringWithFormat:@"\"%@\"", value];
        }
    }
    return [[self nodeName] lowercaseString];
}
@end
