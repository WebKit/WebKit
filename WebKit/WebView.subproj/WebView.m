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

#import <WebKit/WebViewInternal.h>

#import <WebKit/DOM.h>
#import <WebKit/DOMExtensions.h>
#import <WebKit/WebAssertions.h>
#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebBaseNetscapePluginView.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebControllerSets.h>
#import <WebKit/WebDashboardRegion.h>
#import <WebKit/WebDataProtocol.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultEditingDelegate.h>
#import <WebKit/WebDefaultFrameLoadDelegate.h>
#import <WebKit/WebDefaultPolicyDelegate.h>
#import <WebKit/WebDefaultResourceLoadDelegate.h>
#import <WebKit/WebDefaultUIDelegate.h>
#import <WebKit/WebDOMOperationsPrivate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDocumentInternal.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebDownload.h>
#import <WebKit/WebEditingDelegate.h>
#import <WebKit/WebFormDelegatePrivate.h>
#import <WebKit/WebFrameInternal.h>
#import <WebKit/WebFrameViewInternal.h>
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebKit/WebHTMLRepresentation.h>
#import <WebKit/WebHTMLViewInternal.h>
#import <WebKit/WebIconDatabase.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebNSDataExtras.h>
#import <WebKit/WebNSDictionaryExtras.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSPrintOperationExtras.h>
#import <WebKit/WebNSEventExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSURLRequestExtras.h>
#import <WebKit/WebNSUserDefaultsExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPolicyDelegate.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebTextView.h>
#import <WebKit/WebTextRepresentation.h>
#import <WebKit/WebTextRenderer.h>
#import <WebKit/WebUIDelegate.h>
#import <WebKit/WebUIDelegatePrivate.h>
#import <WebKitSystemInterface.h>

#import <WebCore/WebCoreEncodings.h>
#import <WebCore/WebCoreSettings.h>
#import <WebCore/WebCoreView.h>

#import <Foundation/NSURLConnection.h>

#define FOR_EACH_RESPONDER_SELECTOR(macro) \
macro(alignCenter) \
macro(alignJustified) \
macro(alignLeft) \
macro(alignRight) \
macro(capitalizeWord) \
macro(centerSelectionInVisibleArea) \
macro(changeAttributes) \
macro(changeColor) \
macro(changeDocumentBackgroundColor) \
macro(changeFont) \
macro(checkSpelling) \
macro(complete) \
macro(copy) \
macro(copyFont) \
macro(cut) \
macro(delete) \
macro(deleteBackward) \
macro(deleteBackwardByDecomposingPreviousCharacter) \
macro(deleteForward) \
macro(deleteToBeginningOfLine) \
macro(deleteToBeginningOfParagraph) \
macro(deleteToEndOfLine) \
macro(deleteToEndOfParagraph) \
macro(deleteWordBackward) \
macro(deleteWordForward) \
macro(ignoreSpelling) \
macro(indent) \
macro(insertBacktab) \
macro(insertNewline) \
macro(insertNewlineIgnoringFieldEditor) \
macro(insertParagraphSeparator) \
macro(insertTab) \
macro(insertTabIgnoringFieldEditor) \
macro(lowercaseWord) \
macro(moveBackward) \
macro(moveBackwardAndModifySelection) \
macro(moveDown) \
macro(moveDownAndModifySelection) \
macro(moveForward) \
macro(moveForwardAndModifySelection) \
macro(moveLeft) \
macro(moveLeftAndModifySelection) \
macro(moveRight) \
macro(moveRightAndModifySelection) \
macro(moveToBeginningOfDocument) \
macro(moveToBeginningOfDocumentAndModifySelection) \
macro(moveToBeginningOfLine) \
macro(moveToBeginningOfLineAndModifySelection) \
macro(moveToBeginningOfParagraph) \
macro(moveToBeginningOfParagraphAndModifySelection) \
macro(moveToEndOfDocument) \
macro(moveToEndOfDocumentAndModifySelection) \
macro(moveToEndOfLine) \
macro(moveToEndOfLineAndModifySelection) \
macro(moveToEndOfParagraph) \
macro(moveToEndOfParagraphAndModifySelection) \
macro(moveUp) \
macro(moveUpAndModifySelection) \
macro(moveWordBackward) \
macro(moveWordBackwardAndModifySelection) \
macro(moveWordForward) \
macro(moveWordForwardAndModifySelection) \
macro(moveWordLeft) \
macro(moveWordLeftAndModifySelection) \
macro(moveWordRight) \
macro(moveWordRightAndModifySelection) \
macro(pageDown) \
macro(pageUp) \
macro(paste) \
macro(pasteAsPlainText) \
macro(pasteAsRichText) \
macro(pasteFont) \
macro(performFindPanelAction) \
macro(scrollLineDown) \
macro(scrollLineUp) \
macro(scrollPageDown) \
macro(scrollPageUp) \
macro(selectAll) \
macro(selectLine) \
macro(selectParagraph) \
macro(selectWord) \
macro(showGuessPanel) \
macro(startSpeaking) \
macro(stopSpeaking) \
macro(subscript) \
macro(superscript) \
macro(underline) \
macro(unscript) \
macro(uppercaseWord) \
macro(yank) \
macro(yankAndSelect) \

@interface NSSpellChecker (AppKitSecretsIKnow)
- (void)_preflightChosenSpellServer;
@end

@interface NSView (AppKitSecretsIKnow)
- (NSView *)_hitTest:(NSPoint *)aPoint dragTypes:(NSSet *)types;
- (void)_autoscrollForDraggingInfo:(id)dragInfo timeDelta:(NSTimeInterval)repeatDelta;
- (BOOL)_shouldAutoscrollForDraggingInfo:(id)dragInfo;
@end

@interface NSObject (WebDocumentSearchingHack)
// FIXME: this should be part of a protocol (new version of <WebDocumentSearching>)
- (BOOL)_searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag findInSelection:(BOOL)inSelectionFlag;
@end

@interface WebView (WebFileInternal)
- (void)_preflightSpellChecker;
- (BOOL)_continuousCheckingAllowed;
- (NSResponder *)_responderForResponderOperations;
@end

NSString *WebElementDOMNodeKey =            @"WebElementDOMNode";
NSString *WebElementFrameKey =              @"WebElementFrame";
NSString *WebElementImageKey =              @"WebElementImage";
NSString *WebElementImageAltStringKey =     @"WebElementImageAltString";
NSString *WebElementImageRectKey =          @"WebElementImageRect";
NSString *WebElementImageURLKey =           @"WebElementImageURL";
NSString *WebElementIsSelectedKey =         @"WebElementIsSelected";
NSString *WebElementLinkURLKey =            @"WebElementLinkURL";
NSString *WebElementLinkTargetFrameKey =    @"WebElementTargetFrame";
NSString *WebElementLinkLabelKey =          @"WebElementLinkLabel";
NSString *WebElementLinkTitleKey =          @"WebElementLinkTitle";

NSString *WebViewProgressStartedNotification =          @"WebProgressStartedNotification";
NSString *WebViewProgressEstimateChangedNotification =  @"WebProgressEstimateChangedNotification";
NSString *WebViewProgressFinishedNotification =         @"WebProgressFinishedNotification";

NSString * const WebViewDidBeginEditingNotification =         @"WebViewDidBeginEditingNotification";
NSString * const WebViewDidChangeNotification =               @"WebViewDidChangeNotification";
NSString * const WebViewDidEndEditingNotification =           @"WebViewDidEndEditingNotification";
NSString * const WebViewDidChangeTypingStyleNotification =    @"WebViewDidChangeTypingStyleNotification";
NSString * const WebViewDidChangeSelectionNotification =      @"WebViewDidChangeSelectionNotification";

enum { WebViewVersion = 2 };

#define timedLayoutSize 4096

static NSMutableSet *schemesWithRepresentationsSet;

NSString *_WebCanGoBackKey =            @"canGoBack";
NSString *_WebCanGoForwardKey =         @"canGoForward";
NSString *_WebEstimatedProgressKey =    @"estimatedProgress";
NSString *_WebIsLoadingKey =            @"isLoading";
NSString *_WebMainFrameIconKey =        @"mainFrameIcon";
NSString *_WebMainFrameTitleKey =       @"mainFrameTitle";
NSString *_WebMainFrameURLKey =         @"mainFrameURL";

@interface WebProgressItem : NSObject
{
@public
    long long bytesReceived;
    long long estimatedLength;
}
@end

@implementation WebProgressItem
@end

static BOOL shouldUseFontSmoothing = YES;

@implementation WebViewPrivate

- init 
{
    self = [super init];
    if (!self)
        return nil;
    
    backForwardList = [[WebBackForwardList alloc] init];
    textSizeMultiplier = 1;
    progressNotificationInterval = 0.02;
    progressNotificationTimeInterval = 0.1;
    settings = [[WebCoreSettings alloc] init];
    dashboardBehaviorAllowWheelScrolling = YES;

    return self;
}

- (void)dealloc
{
    ASSERT(mainFrame == nil);
    ASSERT(draggingDocumentView == nil);
    ASSERT(dragCaretBridge == nil);
    
    [backForwardList release];
    [applicationNameForUserAgent release];
    [userAgent release];
    
    [preferences release];
    [settings release];
    [hostWindow release];
    
    [policyDelegateForwarder release];
    [resourceProgressDelegateForwarder release];
    [UIDelegateForwarder release];
    [frameLoadDelegateForwarder release];
    [editingDelegateForwarder release];
    
    [progressItems release];
        
    [mediaStyle release];
    
    [super dealloc];
}

@end

@implementation WebView (WebPrivate)

#ifdef DEBUG_WIDGET_DRAWING
static bool debugWidget = true;
- (void)drawRect:(NSRect)rect
{
    [[NSColor blueColor] set];
    NSRectFill (rect);
    
    NSRect htmlViewRect = [[[[self mainFrame] frameView] documentView] frame];

    if (debugWidget) {
	while (debugWidget) {
	    sleep (1);
	}
    }

    NSLog (@"%s:   rect:  (%0.f,%0.f) %0.f %0.f, htmlViewRect:  (%0.f,%0.f) %0.f %0.f\n", 
	__PRETTY_FUNCTION__, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
	htmlViewRect.origin.x, htmlViewRect.origin.y, htmlViewRect.size.width, htmlViewRect.size.height
	);

    [super drawRect:rect];
}
#endif

+ (NSArray *)_supportedMIMETypes
{
    // Load the plug-in DB allowing plug-ins to install types.
    [WebPluginDatabase installedPlugins];
    return [[WebFrameView _viewTypesAllowImageTypeOmission:NO] allKeys];
}

+ (NSArray *)_supportedFileExtensions
{
    NSMutableSet *extensions = [[NSMutableSet alloc] init];
    NSArray *MIMETypes = [self _supportedMIMETypes];
    NSEnumerator *enumerator = [MIMETypes objectEnumerator];
    NSString *MIMEType;
    while ((MIMEType = [enumerator nextObject]) != nil) {
        NSArray *extensionsForType = WKGetExtensionsForMIMEType(MIMEType);
        if (extensionsForType) {
            [extensions addObjectsFromArray:extensionsForType];
        }
    }
    NSArray *uniqueExtensions = [extensions allObjects];
    [extensions release];
    return uniqueExtensions;
}

+ (BOOL)_viewClass:(Class *)vClass andRepresentationClass:(Class *)rClass forMIMEType:(NSString *)MIMEType;
{
    MIMEType = [MIMEType lowercaseString];
    Class viewClass;
    Class repClass;
    
    // Simple optimization that avoids loading the plug-in DB and image types for the HTML case.
    if ([self canShowMIMETypeAsHTML:MIMEType]) {
        viewClass = [[WebFrameView _viewTypesAllowImageTypeOmission:YES] _webkit_objectForMIMEType:MIMEType];
        repClass = [[WebDataSource _repTypesAllowImageTypeOmission:YES] _webkit_objectForMIMEType:MIMEType];
        if (viewClass && repClass) {
            if (vClass) {
                *vClass = viewClass;
            }
            if (rClass) {
                *rClass = repClass;
            }
            return YES;
        }
    }
    
    // Load the plug-in DB allowing plug-ins to install types.
    [WebPluginDatabase installedPlugins];
        
    // Load the image types and get the view class and rep class. This should be the fullest picture of all handled types.
    viewClass = [[WebFrameView _viewTypesAllowImageTypeOmission:NO] _webkit_objectForMIMEType:MIMEType];
    repClass = [[WebDataSource _repTypesAllowImageTypeOmission:NO] _webkit_objectForMIMEType:MIMEType];
    if (viewClass && repClass) {
        // Special-case WebTextView for text types that shouldn't be shown.
        if (viewClass == [WebTextView class] &&
            repClass == [WebTextRepresentation class] &&
            [[WebTextView unsupportedTextMIMETypes] containsObject:MIMEType]) {
            return NO;
        }
        if (vClass) {
            *vClass = viewClass;
        }
        if (rClass) {
            *rClass = repClass;
        }
        return YES;
    }
    
    return NO;
}

+ (void)_setAlwaysUseATSU:(BOOL)f
{
    [WebTextRenderer _setAlwaysUseATSU:f];
}

+ (BOOL)canShowFile:(NSString *)path
{
    return [[self class] canShowMIMEType:[WebView _MIMETypeForFile:path]];
}

+ (NSString *)suggestedFileExtensionForMIMEType:(NSString *)type
{
    return WKGetPreferredExtensionForMIMEType(type);
}

- (void)_close
{
    if (_private->setName != nil) {
        [WebViewSets removeWebView:self fromSetNamed:_private->setName];
        [_private->setName release];
        _private->setName = nil;
    }

    // To avoid leaks, call removeDragCaret in case it wasn't called after moveDragCaretToPoint.
    [self removeDragCaret];
    
    [_private->mainFrame _detachFromParent];
    [_private->mainFrame release];
    _private->mainFrame = nil;
    
    // Clear the page cache so we call destroy on all the plug-ins in the page cache to break any retain cycles.
    // See comment in [WebHistoryItem _releaseAllPendingPageCaches] for more information.
    [_private->backForwardList _clearPageCache];
    
    if (_private->hasSpellCheckerDocumentTag) {
        [[NSSpellChecker sharedSpellChecker] closeSpellDocumentWithTag:_private->spellCheckerDocumentTag];
        _private->hasSpellCheckerDocumentTag = NO;
    }
}

- (WebFrame *)_createFrameNamed:(NSString *)fname inParent:(WebFrame *)parent allowsScrolling:(BOOL)allowsScrolling
{
    WebFrameView *childView = [[WebFrameView alloc] initWithFrame:NSMakeRect(0,0,0,0)];

    [childView _setWebView:self];
    [childView setAllowsScrolling:allowsScrolling];
    
    WebFrame *newFrame = [[WebFrame alloc] initWithName:fname webFrameView:childView webView:self];

    [childView release];

    [parent _addChild:newFrame];
    
    [newFrame release];
        
    return newFrame;
}

- (void)_finishedLoadingResourceFromDataSource: (WebDataSource *)dataSource
{
    WebFrame *frame = [dataSource webFrame];
    
    ASSERT(dataSource != nil);
    
    // This resource has completed, so check if the load is complete for all frames.
    if (frame != nil) {
        [frame _transitionToLayoutAcceptable];
        [frame _checkLoadComplete];
    }
}

- (void)_mainReceivedBytesSoFar: (unsigned)bytesSoFar fromDataSource: (WebDataSource *)dataSource complete: (BOOL)isComplete
{
    WebFrame *frame = [dataSource webFrame];
    
    ASSERT(dataSource != nil);

    // The frame may be nil if a previously cancelled load is still making progress callbacks.
    if (frame == nil)
        return;
        
    // This resource has completed, so check if the load is complete for all frames.
    if (isComplete){
        // If the load is complete, mark the primary load as done.  The primary load is the load
        // of the main document.  Other resources may still be arriving.
        [dataSource _setPrimaryLoadComplete: YES];
        [frame _checkLoadComplete];
    }
    else {
        // If the frame isn't complete it might be ready for a layout.  Perform that check here.
        // Note that transitioning a frame to this state doesn't guarantee a layout, rather it
        // just indicates that an early layout can be performed.
        if ((int)bytesSoFar > timedLayoutSize)
            [frame _transitionToLayoutAcceptable];
    }
}

- (void)_receivedError: (NSError *)error fromDataSource: (WebDataSource *)dataSource
{
    WebFrame *frame = [dataSource webFrame];

    [frame _checkLoadComplete];
}


- (void)_mainReceivedError:(NSError *)error fromDataSource:(WebDataSource *)dataSource complete:(BOOL)isComplete
{
    ASSERT(dataSource);
#ifndef NDEBUG
    ASSERT([dataSource webFrame]);
#endif
    
    [dataSource _setMainDocumentError: error];

    if (isComplete) {
        [dataSource _setPrimaryLoadComplete:YES];
        [[dataSource webFrame] _checkLoadComplete];
    }
}

+ (NSString *)_MIMETypeForFile:(NSString *)path
{
    NSString *extension = [path pathExtension];
    NSString *MIMEType = nil;

    // FIXME: This is a workaround to make web archive files work with Foundations that
    // are too old to know about web archive files. We should remove this before we ship.
    if ([extension  _webkit_isCaseInsensitiveEqualToString:@"webarchive"]) {
        return @"application/x-webarchive";
    }
    
    // Get the MIME type from the extension.
    if ([extension length] != 0) {
        MIMEType = WKGetMIMETypeForExtension(extension);
    }

    // If we can't get a known MIME type from the extension, sniff.
    if ([MIMEType length] == 0 || [MIMEType isEqualToString:@"application/octet-stream"]) {
        NSFileHandle *handle = [NSFileHandle fileHandleForReadingAtPath:path];
        NSData *data = [handle readDataOfLength:WEB_GUESS_MIME_TYPE_PEEK_LENGTH];
        [handle closeFile];
        if ([data length] != 0) {
            MIMEType = [data _webkit_guessedMIMEType];
        }
        if ([MIMEType length] == 0) {
            MIMEType = @"application/octet-stream";
        }
    }

    return MIMEType;
}

- (void)_downloadURL:(NSURL *)URL
{
    [self _downloadURL:URL toDirectory:nil];
}

- (void)_downloadURL:(NSURL *)URL toDirectory:(NSString *)directory
{
    ASSERT(URL);
    
    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:URL];
    [WebDownload _downloadWithRequest:request
                             delegate:_private->downloadDelegate
                            directory:[directory isAbsolutePath] ? directory : nil];
    [request release];
}

- (BOOL)defersCallbacks
{
    return _private->defersCallbacks;
}

- (void)setDefersCallbacks:(BOOL)defers
{
    if (defers == _private->defersCallbacks) {
        return;
    }

    _private->defersCallbacks = defers;
    [_private->mainFrame _defersCallbacksChanged];
}

- (void)_setTopLevelFrameName:(NSString *)name
{
    [[self mainFrame] _setName:name];
}

- (WebFrame *)_findFrameInThisWindowNamed:(NSString *)name sourceFrame:(WebFrame *)source
{
    return [[self mainFrame] _descendantFrameNamed:name sourceFrame:(WebFrame *)source];
}

- (WebFrame *)_findFrameNamed:(NSString *)name sourceFrame:(WebFrame *)source
{
    // Try this WebView first.
    WebFrame *frame = [self _findFrameInThisWindowNamed:name sourceFrame:source];

    if (frame != nil) {
        return frame;
    }

    // Try other WebViews in the same set
    if (_private->setName != nil) {
        NSEnumerator *enumerator = [WebViewSets webViewsInSetNamed:_private->setName];
        WebView *webView;
        while ((webView = [enumerator nextObject]) != nil && frame == nil) {
	    frame = [webView _findFrameInThisWindowNamed:name sourceFrame:source];
        }
    }

    return frame;
}

- (WebView *)_openNewWindowWithRequest:(NSURLRequest *)request
{
    id wd = [self UIDelegate];
    WebView *newWindowWebView = nil;
    if ([wd respondsToSelector:@selector(webView:createWebViewWithRequest:)])
        newWindowWebView = [wd webView:self createWebViewWithRequest:request];
    else {
        newWindowWebView = [[WebDefaultUIDelegate sharedUIDelegate] webView:self createWebViewWithRequest: request];
    }

    [[newWindowWebView _UIDelegateForwarder] webViewShow: newWindowWebView];

    return newWindowWebView;
}

- (NSMenu *)_menuForElement:(NSDictionary *)element defaultItems:(NSArray *)items
{
    NSArray *defaultMenuItems = [[WebDefaultUIDelegate sharedUIDelegate]
          webView:self contextMenuItemsForElement:element defaultMenuItems:items];
    NSArray *menuItems = defaultMenuItems;
    NSMenu *menu = nil;
    unsigned i;

    if (_private->UIDelegate) {
        id cd = _private->UIDelegate;
        
        if ([cd respondsToSelector:@selector(webView:contextMenuItemsForElement:defaultMenuItems:)])
            menuItems = [cd webView:self contextMenuItemsForElement:element defaultMenuItems:defaultMenuItems];
    } 

    if (menuItems && [menuItems count] > 0) {
        menu = [[[NSMenu alloc] init] autorelease];

        for (i=0; i<[menuItems count]; i++) {
            [menu addItem:[menuItems objectAtIndex:i]];
        }
    }

    return menu;
}

- (void)_mouseDidMoveOverElement:(NSDictionary *)dictionary modifierFlags:(unsigned)modifierFlags
{
    // When the mouse isn't over this view at all, we'll get called with a dictionary of nil over
    // and over again. So it's a good idea to catch that here and not send multiple calls to the delegate
    // for that case.
    
    if (dictionary && _private->lastElementWasNonNil) {
        [[self _UIDelegateForwarder] webView:self mouseDidMoveOverElement:dictionary modifierFlags:modifierFlags];
    }
    _private->lastElementWasNonNil = dictionary != nil;
}

- (void)_goToItem:(WebHistoryItem *)item withLoadType:(WebFrameLoadType)type
{
    // We never go back/forward on a per-frame basis, so the target must be the main frame
    //ASSERT([item target] == nil || [self _findFrameNamed:[item target]] == [self mainFrame]);

    // abort any current load if we're going back/forward
    [[self mainFrame] stopLoading];
    [[self mainFrame] _goToItem:item withLoadType:type];
}

// Not used now, but could be if we ever store frames in bookmarks or history
- (void)_loadItem:(WebHistoryItem *)item
{
    WebHistoryItem *newItem = [item copy];	// Makes a deep copy, happily
    [[self backForwardList] addItem:newItem];
    [self _goToItem:newItem withLoadType:WebFrameLoadTypeIndexedBackForward];
}

- (void)_loadBackForwardListFromOtherView:(WebView *)otherView
{
    // It turns out the right combination of behavior is done with the back/forward load
    // type.  (See behavior matrix at the top of WebFramePrivate.)  So we copy all the items
    // in the back forward list, and go to the current one.

    WebBackForwardList *bfList = [self backForwardList];
    ASSERT(![bfList currentItem]);	// destination list should be empty

    WebBackForwardList *otherBFList = [otherView backForwardList];
    if (![otherBFList currentItem]) {
        return;		// empty back forward list, bail
    }

    WebHistoryItem *newItemToGoTo = nil;
    int lastItemIndex = [otherBFList forwardListCount];
    int i;
    for (i = -[otherBFList backListCount]; i <= lastItemIndex; i++) {
        if (i == 0) {
            // If this item is showing , save away its current scroll and form state,
            // since that might have changed since loading and it is normally not saved
            // until we leave that page.
            [[otherView mainFrame] _saveDocumentAndScrollState];
        }
        WebHistoryItem *newItem = [[otherBFList itemAtIndex:i] copy];
        [bfList addItem:newItem];
        if (i == 0) {
            newItemToGoTo = newItem;
        }
    }
    
    [self _goToItem:newItemToGoTo withLoadType:WebFrameLoadTypeIndexedBackForward];
}

- (void)_setFormDelegate: (id<WebFormDelegate>)delegate
{
    _private->formDelegate = delegate;
}

- (id<WebFormDelegate>)_formDelegate
{
    if (!_private->formDelegate) {
        // create lazily, to give the client a chance to set one before we bother to alloc the shared one
        _private->formDelegate = [WebFormDelegate _sharedWebFormDelegate];
    }
    return _private->formDelegate;
}

- (WebCoreSettings *)_settings
{
    return _private->settings;
}

- (void)_updateWebCoreSettingsFromPreferences: (WebPreferences *)preferences
{
    [_private->settings setCursiveFontFamily:[preferences cursiveFontFamily]];
    [_private->settings setDefaultFixedFontSize:[preferences defaultFixedFontSize]];
    [_private->settings setDefaultFontSize:[preferences defaultFontSize]];
    [_private->settings setDefaultTextEncoding:[preferences defaultTextEncodingName]];
    [_private->settings setFantasyFontFamily:[preferences fantasyFontFamily]];
    [_private->settings setFixedFontFamily:[preferences fixedFontFamily]];
    [_private->settings setJavaEnabled:[preferences isJavaEnabled]];
    [_private->settings setJavaScriptEnabled:[preferences isJavaScriptEnabled]];
    [_private->settings setJavaScriptCanOpenWindowsAutomatically:[preferences javaScriptCanOpenWindowsAutomatically]];
    [_private->settings setMinimumFontSize:[preferences minimumFontSize]];
    [_private->settings setMinimumLogicalFontSize:[preferences minimumLogicalFontSize]];
    [_private->settings setPluginsEnabled:[preferences arePlugInsEnabled]];
    [_private->settings setSansSerifFontFamily:[preferences sansSerifFontFamily]];
    [_private->settings setSerifFontFamily:[preferences serifFontFamily]];
    [_private->settings setStandardFontFamily:[preferences standardFontFamily]];
    [_private->settings setWillLoadImagesAutomatically:[preferences loadsImagesAutomatically]];

    if ([preferences userStyleSheetEnabled]) {
        [_private->settings setUserStyleSheetLocation:[[preferences userStyleSheetLocation] _web_originalDataAsString]];
    } else {
        [_private->settings setUserStyleSheetLocation:@""];
    }
    [_private->settings setShouldPrintBackgrounds:[preferences shouldPrintBackgrounds]];
    [_private->settings setTextAreasAreResizable:[preferences textAreasAreResizable]];
}

- (void)_preferencesChangedNotification: (NSNotification *)notification
{
    WebPreferences *preferences = (WebPreferences *)[notification object];
    
    ASSERT(preferences == [self preferences]);
    if (!_private->userAgentOverridden) {
        [_private->userAgent release];
        _private->userAgent = nil;
    }
    [self _updateWebCoreSettingsFromPreferences: preferences];
}

- _frameLoadDelegateForwarder
{
    if (!_private->frameLoadDelegateForwarder)
        _private->frameLoadDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget: [self frameLoadDelegate]  defaultTarget: [WebDefaultFrameLoadDelegate sharedFrameLoadDelegate] templateClass: [WebDefaultFrameLoadDelegate class]];
    return _private->frameLoadDelegateForwarder;
}

- _resourceLoadDelegateForwarder
{
    if (!_private->resourceProgressDelegateForwarder)
        _private->resourceProgressDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget: [self resourceLoadDelegate] defaultTarget: [WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] templateClass: [WebDefaultResourceLoadDelegate class]];
    return _private->resourceProgressDelegateForwarder;
}

- (void)_cacheResourceLoadDelegateImplementations
{
    WebResourceDelegateImplementationCache *cache = &_private->resourceLoadDelegateImplementations;
    id delegate = [self resourceLoadDelegate];

    cache->delegateImplementsDidCancelAuthenticationChallenge = [delegate respondsToSelector:@selector(webView:resource:didCancelAuthenticationChallenge:fromDataSource:)];
    cache->delegateImplementsDidReceiveAuthenticationChallenge = [delegate respondsToSelector:@selector(webView:resource:didReceiveAuthenticationChallenge:fromDataSource:)];
    cache->delegateImplementsDidFinishLoadingFromDataSource = [delegate respondsToSelector:@selector(webView:resource:didFinishLoadingFromDataSource:)];
    cache->delegateImplementsDidReceiveContentLength = [delegate respondsToSelector:@selector(webView:resource:didReceiveContentLength:fromDataSource:)];
    cache->delegateImplementsDidReceiveResponse = [delegate respondsToSelector:@selector(webView:resource:didReceiveResponse:fromDataSource:)];
    cache->delegateImplementsWillSendRequest = [delegate respondsToSelector:@selector(webView:resource:willSendRequest:redirectResponse:fromDataSource:)];
    cache->delegateImplementsIdentifierForRequest = [delegate respondsToSelector:@selector(webView:identifierForInitialRequest:fromDataSource:)];
}

- (WebResourceDelegateImplementationCache)_resourceLoadDelegateImplementations
{
    return _private->resourceLoadDelegateImplementations;
}

- _policyDelegateForwarder
{
    if (!_private->policyDelegateForwarder)
        _private->policyDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget: [self policyDelegate] defaultTarget: [WebDefaultPolicyDelegate sharedPolicyDelegate] templateClass: [WebDefaultPolicyDelegate class]];
    return _private->policyDelegateForwarder;
}

- _UIDelegateForwarder
{
    if (!_private->UIDelegateForwarder)
        _private->UIDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget: [self UIDelegate] defaultTarget: [WebDefaultUIDelegate sharedUIDelegate] templateClass: [WebDefaultUIDelegate class]];
    return _private->UIDelegateForwarder;
}

- _editingDelegateForwarder
{
    // This can be called during window deallocation by QTMovieView in the QuickTime Cocoa Plug-in.
    // Not sure if that is a bug or not.
    if (!_private)
        return nil;
    if (!_private->editingDelegateForwarder)
        _private->editingDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget: [self editingDelegate] defaultTarget: [WebDefaultEditingDelegate sharedEditingDelegate] templateClass: [WebDefaultEditingDelegate class]];
    return _private->editingDelegateForwarder;
}

- (WebFrame *)_frameForDataSource: (WebDataSource *)dataSource fromFrame: (WebFrame *)frame
{
    NSArray *frames;
    int i, count;
    WebFrame *result, *aFrame;

    if ([frame dataSource] == dataSource)
        return frame;

    if ([frame provisionalDataSource] == dataSource)
        return frame;

    // It's safe to use the internal version because we know this
    // function will not change the set of frames
    frames = [frame _internalChildFrames];
    count = [frames count];
    for (i = 0; i < count; i++){
        aFrame = [frames objectAtIndex: i];
        result = [self _frameForDataSource: dataSource fromFrame: aFrame];
        if (result)
            return result;
    }

    return nil;
}


- (WebFrame *)_frameForDataSource: (WebDataSource *)dataSource
{
    WebFrame *frame = [self mainFrame];

    return [self _frameForDataSource: dataSource fromFrame: frame];
}


- (WebFrame *)_frameForView: (WebFrameView *)aView fromFrame: (WebFrame *)frame
{
    NSArray *frames;
    int i, count;
    WebFrame *result, *aFrame;

    if ([frame frameView] == aView)
        return frame;

    // It's safe to use the internal version because we know this
    // function will not change the set of frames
    frames = [frame _internalChildFrames];
    count = [frames count];
    for (i = 0; i < count; i++){
        aFrame = [frames objectAtIndex: i];
        result = [self _frameForView: aView fromFrame: aFrame];
        if (result)
            return result;
    }

    return nil;
}

- (WebFrame *)_frameForView: (WebFrameView *)aView
{
    WebFrame *frame = [self mainFrame];

    return [self _frameForView: aView fromFrame: frame];
}

- (void)_closeWindow
{
    [[self _UIDelegateForwarder] webViewClose:self];
}

+ (void)_unregisterViewClassAndRepresentationClassForMIMEType:(NSString *)MIMEType;
{
    [[WebFrameView _viewTypesAllowImageTypeOmission:NO] removeObjectForKey:MIMEType];
    [[WebDataSource _repTypesAllowImageTypeOmission:NO] removeObjectForKey:MIMEType];
}

+ (void)_registerViewClass:(Class)viewClass representationClass:(Class)representationClass forURLScheme:(NSString *)URLScheme;
{
    NSString *MIMEType = [self _generatedMIMETypeForURLScheme:URLScheme];
    [self registerViewClass:viewClass representationClass:representationClass forMIMEType:MIMEType];

    // This is used to make _representationExistsForURLScheme faster.
    // Without this set, we'd have to create the MIME type each time.
    if (schemesWithRepresentationsSet == nil) {
        schemesWithRepresentationsSet = [[NSMutableSet alloc] init];
    }
    [schemesWithRepresentationsSet addObject:[[[URLScheme lowercaseString] copy] autorelease]];
}

+ (NSString *)_generatedMIMETypeForURLScheme:(NSString *)URLScheme
{
    return [@"x-apple-web-kit/" stringByAppendingString:[URLScheme lowercaseString]];
}

+ (BOOL)_representationExistsForURLScheme:(NSString *)URLScheme
{
    return [schemesWithRepresentationsSet containsObject:[URLScheme lowercaseString]];
}

+ (BOOL)_canHandleRequest:(NSURLRequest *)request
{
    if ([NSURLConnection canHandleRequest:request]) {
        return YES;
    }
    
    // We're always willing to load alternate content for unreachable URLs
    if ([request _webDataRequestUnreachableURL]) {
        return YES;
    }

    return [self _representationExistsForURLScheme:[[request URL] scheme]];
}

+ (NSString *)_decodeData:(NSData *)data
{
    return [WebCoreEncodings decodeData:data];
}

- (void)_pushPerformingProgrammaticFocus
{
    _private->programmaticFocusCount++;
}

- (void)_popPerformingProgrammaticFocus
{
    _private->programmaticFocusCount--;
}

- (BOOL)_isPerformingProgrammaticFocus
{
    return _private->programmaticFocusCount != 0;
}

#define UnknownTotalBytes -1
#define WebProgressItemDefaultEstimatedLength 1024*16

- (void)_didChangeValueForKey: (NSString *)key
{
    LOG (Bindings, "calling didChangeValueForKey: %@", key);
    [self didChangeValueForKey: key];
}

- (void)_willChangeValueForKey: (NSString *)key
{
    LOG (Bindings, "calling willChangeValueForKey: %@", key);
    [self willChangeValueForKey: key];
}

// Always start progress at INITIAL_PROGRESS_VALUE so it appears progress indicators
// will immediately show some progress.  This helps provide feedback as soon as a load
// starts.
#define INITIAL_PROGRESS_VALUE 0.1

- (void)_resetProgress
{
    [_private->progressItems release];
    _private->progressItems = nil;
    _private->totalPageAndResourceBytesToLoad = 0;
    _private->totalBytesReceived = 0;
    _private->progressValue = 0;
    _private->lastNotifiedProgressValue = 0;
    _private->lastNotifiedProgressTime = 0;
    _private->finalProgressChangedSent = NO;
    _private->numProgressTrackedFrames = 0;
    [_private->orginatingProgressFrame release];
    _private->orginatingProgressFrame = nil;
}
- (void)_progressStarted:(WebFrame *)frame
{
    LOG (Progress, "frame %p(%@), _private->numProgressTrackedFrames %d, _private->orginatingProgressFrame %p", frame, [frame name], _private->numProgressTrackedFrames, _private->orginatingProgressFrame);
    [self _willChangeValueForKey: @"estimatedProgress"];
    if (_private->numProgressTrackedFrames == 0 || _private->orginatingProgressFrame == frame){
        [self _resetProgress];
        _private->progressValue = INITIAL_PROGRESS_VALUE;
        _private->orginatingProgressFrame = [frame retain];
        [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressStartedNotification object:self];
    }
    _private->numProgressTrackedFrames++;
    [self _didChangeValueForKey: @"estimatedProgress"];
}

- (void)_finalProgressComplete
{
    LOG (Progress, "");

    // Before resetting progress value be sure to send client a least one notification
    // with final progress value.
    if (!_private->finalProgressChangedSent) {
        _private->progressValue = 1;
        [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressEstimateChangedNotification object:self];
    }
    
    [self _resetProgress];
    
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressFinishedNotification object:self];
}

- (void)_progressCompleted:(WebFrame *)frame
{    
    LOG (Progress, "frame %p(%@), _private->numProgressTrackedFrames %d, _private->orginatingProgressFrame %p", frame, [frame name], _private->numProgressTrackedFrames, _private->orginatingProgressFrame);

    if (_private->numProgressTrackedFrames <= 0)
        return;

    [self _willChangeValueForKey: @"estimatedProgress"];

    _private->numProgressTrackedFrames--;
    if (_private->numProgressTrackedFrames == 0 ||
        (frame == _private->orginatingProgressFrame && _private->numProgressTrackedFrames != 0)){
        [self _finalProgressComplete];
    }
    [self _didChangeValueForKey: @"estimatedProgress"];
}

- (void)_incrementProgressForConnectionDelegate:(id)connectionDelegate response:(NSURLResponse *)response;
{
    if (!connectionDelegate)
        return;

    LOG (Progress, "_private->numProgressTrackedFrames %d, _private->orginatingProgressFrame %p", _private->numProgressTrackedFrames, _private->orginatingProgressFrame);
    
    if (_private->numProgressTrackedFrames <= 0)
        return;
        
    WebProgressItem *item = [[WebProgressItem alloc] init];

    if (!item)
        return;

    long long length = [response expectedContentLength];
    if (length < 0){
        length = WebProgressItemDefaultEstimatedLength;
    }
    item->estimatedLength = length;
    _private->totalPageAndResourceBytesToLoad += length;
    
    if (!_private->progressItems)
        _private->progressItems = [[NSMutableDictionary alloc] init];
        
    [_private->progressItems _webkit_setObject:item forUncopiedKey:connectionDelegate];
    [item release];
}

- (void)_incrementProgressForConnectionDelegate:(id)connectionDelegate data:(NSData *)data
{
    if (!connectionDelegate)
        return;

    WebProgressItem *item = [_private->progressItems objectForKey:connectionDelegate];

    if (!item)
        return;

    [self _willChangeValueForKey: @"estimatedProgress"];

    unsigned bytesReceived = [data length];
    double increment = 0, percentOfRemainingBytes;
    long long remainingBytes, estimatedBytesForPendingRequests;

    item->bytesReceived += bytesReceived;
    if (item->bytesReceived > item->estimatedLength){
        _private->totalPageAndResourceBytesToLoad += ((item->bytesReceived*2) - item->estimatedLength);
        item->estimatedLength = item->bytesReceived*2;
    }
    
    int numPendingOrLoadingRequests = [[self mainFrame] _numPendingOrLoadingRequests:YES];
    estimatedBytesForPendingRequests = WebProgressItemDefaultEstimatedLength * numPendingOrLoadingRequests;
    remainingBytes = ((_private->totalPageAndResourceBytesToLoad + estimatedBytesForPendingRequests) - _private->totalBytesReceived);
    percentOfRemainingBytes = (double)bytesReceived / (double)remainingBytes;
    increment = (1.0 - _private->progressValue) * percentOfRemainingBytes;
    
    _private->totalBytesReceived += bytesReceived;
    
    _private->progressValue += increment;

    if (_private->progressValue < 0.0)
        _private->progressValue = 0.0;

    if (_private->progressValue > 1.0)
        _private->progressValue = 1.0;

    double now = CFAbsoluteTimeGetCurrent();
    double notifiedProgressTimeDelta = CFAbsoluteTimeGetCurrent() - _private->lastNotifiedProgressTime;
    _private->lastNotifiedProgressTime = now;
    
    LOG (Progress, "_private->progressValue %g, _private->numProgressTrackedFrames %d", _private->progressValue, _private->numProgressTrackedFrames);
    double notificationProgressDelta = _private->progressValue - _private->lastNotifiedProgressValue;
    if ((notificationProgressDelta >= _private->progressNotificationInterval ||
            notifiedProgressTimeDelta >= _private->progressNotificationTimeInterval) &&
            _private->numProgressTrackedFrames > 0) {
        if (!_private->finalProgressChangedSent) {
            if (_private->progressValue == 1)
                _private->finalProgressChangedSent = YES;
            [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressEstimateChangedNotification object:self];
            _private->lastNotifiedProgressValue = _private->progressValue;
        }
    }

    [self _didChangeValueForKey: @"estimatedProgress"];
}

- (void)_completeProgressForConnectionDelegate:(id)connectionDelegate
{
    WebProgressItem *item = [_private->progressItems objectForKey:connectionDelegate];

    if (!item)
        return;
        
    // Adjust the total expected bytes to account for any overage/underage.
    long long delta = item->bytesReceived - item->estimatedLength;
    _private->totalPageAndResourceBytesToLoad += delta;
    item->estimatedLength = item->bytesReceived;
}

// Required to prevent automatic observer notifications.
+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key {
    return NO;
}

- (NSArray *)_declaredKeys {
    static NSArray *declaredKeys = nil;
    
    if (!declaredKeys) {
        declaredKeys = [[NSArray alloc] initWithObjects:_WebMainFrameURLKey, _WebIsLoadingKey, _WebEstimatedProgressKey, _WebCanGoBackKey, _WebCanGoForwardKey, _WebMainFrameTitleKey, _WebMainFrameIconKey, nil];
    }

    return declaredKeys;
}

- (void)setObservationInfo:(void *)info
{
    _private->observationInfo = info;
}

- (void *)observationInfo
{
    return _private->observationInfo;
}

- (void)_willChangeBackForwardKeys
{
    [self _willChangeValueForKey: _WebCanGoBackKey];
    [self _willChangeValueForKey: _WebCanGoForwardKey];
}

- (void)_didChangeBackForwardKeys
{
    [self _didChangeValueForKey: _WebCanGoBackKey];
    [self _didChangeValueForKey: _WebCanGoForwardKey];
}

- (void)_didStartProvisionalLoadForFrame:(WebFrame *)frame
{
    [self _willChangeBackForwardKeys];
    if (frame == [self mainFrame]){
        // Force an observer update by sending a will/did.
        [self _willChangeValueForKey: _WebIsLoadingKey];
        [self _didChangeValueForKey: _WebIsLoadingKey];

        [self _willChangeValueForKey: _WebMainFrameURLKey];
    }
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_didCommitLoadForFrame:(WebFrame *)frame
{
    if (frame == [self mainFrame]){
        [self _didChangeValueForKey: _WebMainFrameURLKey];
    }
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_didFinishLoadForFrame:(WebFrame *)frame
{
    [self _didChangeBackForwardKeys];
    if (frame == [self mainFrame]){
        // Force an observer update by sending a will/did.
        [self _willChangeValueForKey: _WebIsLoadingKey];
        [self _didChangeValueForKey: _WebIsLoadingKey];
    }
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    [self _didChangeBackForwardKeys];
    if (frame == [self mainFrame]){
        // Force an observer update by sending a will/did.
        [self _willChangeValueForKey: _WebIsLoadingKey];
        [self _didChangeValueForKey: _WebIsLoadingKey];
    }
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    [self _didChangeBackForwardKeys];
    if (frame == [self mainFrame]){
        // Force an observer update by sending a will/did.
        [self _willChangeValueForKey: _WebIsLoadingKey];
        [self _didChangeValueForKey: _WebIsLoadingKey];
        
        [self _didChangeValueForKey: _WebMainFrameURLKey];
    }
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_reloadForPluginChanges
{
    [[self mainFrame] _reloadForPluginChanges];
}

- (NSCachedURLResponse *)_cachedResponseForURL:(NSURL *)URL
{
    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:URL];
    [request _web_setHTTPUserAgent:[self userAgentForURL:URL]];
    NSCachedURLResponse *cachedResponse = [[NSURLCache sharedURLCache] cachedResponseForRequest:request];
    [request release];
    return cachedResponse;
}

- (void)_writeImageElement:(NSDictionary *)element withPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    NSURL *linkURL = [element objectForKey:WebElementLinkURLKey];
    [pasteboard _web_writeImage:[element objectForKey:WebElementImageKey] 
                            URL:linkURL ? linkURL : [element objectForKey:WebElementImageURLKey]
                          title:[element objectForKey:WebElementImageAltStringKey] 
                        archive:[[element objectForKey:WebElementDOMNodeKey] webArchive]
                          types:types];
}

- (void)_writeLinkElement:(NSDictionary *)element withPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    [pasteboard _web_writeURL:[element objectForKey:WebElementLinkURLKey]
                     andTitle:[element objectForKey:WebElementLinkLabelKey]
                        types:types];
}

- (void)_setInitiatedDrag:(BOOL)initiatedDrag
{
    _private->initiatedDrag = initiatedDrag;
}

#define DASHBOARD_CONTROL_LABEL @"control"

- (void)_addScrollerDashboardRegions:(NSMutableDictionary *)regions from:(NSArray *)views
{
    // Add scroller regions for NSScroller and KWQScrollBar
    int i, count = [views count];
    
    for (i = 0; i < count; i++) {
        NSView *aView = [views objectAtIndex:i];
        
        if ([aView isKindOfClass:[NSScroller class]] ||
            [aView isKindOfClass:NSClassFromString (@"KWQScrollBar")]) {
            NSRect bounds = [aView bounds];
            NSRect adjustedBounds;
            adjustedBounds.origin = [self convertPoint:bounds.origin fromView:aView];
            adjustedBounds.origin.y = [self bounds].size.height - adjustedBounds.origin.y;
            
            // AppKit has horrible hack of placing absent scrollers at -100,-100
            if (adjustedBounds.origin.y == -100)
                continue;
            adjustedBounds.size = bounds.size;
            NSRect clip = [aView visibleRect];
            NSRect adjustedClip;
            adjustedClip.origin = [self convertPoint:clip.origin fromView:aView];
            adjustedClip.origin.y = [self bounds].size.height - adjustedClip.origin.y;
            adjustedClip.size = clip.size;
            WebDashboardRegion *aRegion = 
                        [[[WebDashboardRegion alloc] initWithRect:adjustedBounds 
                                    clip:adjustedClip type:WebDashboardRegionTypeScrollerRectangle] autorelease];
            NSMutableArray *scrollerRegions;
            scrollerRegions = [regions objectForKey:DASHBOARD_CONTROL_LABEL];
            if (!scrollerRegions) {
                scrollerRegions = [NSMutableArray array];
                [regions setObject:scrollerRegions forKey:DASHBOARD_CONTROL_LABEL];
            }
            [scrollerRegions addObject:aRegion];
        }
        [self _addScrollerDashboardRegions:regions from:[aView subviews]];
    }
}

- (void)_addScrollerDashboardRegions:(NSMutableDictionary *)regions
{
    [self _addScrollerDashboardRegions:regions from:[self subviews]];
}

- (NSDictionary *)_dashboardRegions
{
    // Only return regions from main frame.
    NSMutableDictionary *regions = [[[self mainFrame] _bridge] dashboardRegions];
    [self _addScrollerDashboardRegions:regions];
    return regions;
}

- (void)_setDashboardBehavior:(WebDashboardBehavior)behavior to:(BOOL)flag;
{
    switch (behavior) {
        case WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows: {
            _private->dashboardBehaviorAlwaysSendMouseEventsToAllWindows = flag;
            break;
        }
        case WebDashboardBehaviorAlwaysSendActiveNullEventsToPlugIns: {
            _private->dashboardBehaviorAlwaysSendActiveNullEventsToPlugIns = flag;
            break;
        }
        case WebDashboardBehaviorAlwaysAcceptsFirstMouse: {
            _private->dashboardBehaviorAlwaysAcceptsFirstMouse = flag;
            break;
        }
        case WebDashboardBehaviorAllowWheelScrolling: {
            _private->dashboardBehaviorAllowWheelScrolling = flag;
	    break;
        }
    }
}

- (BOOL)_dashboardBehavior:(WebDashboardBehavior)behavior
{
    switch (behavior) {
        case WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows: {
            return _private->dashboardBehaviorAlwaysSendMouseEventsToAllWindows;
        }
        case WebDashboardBehaviorAlwaysSendActiveNullEventsToPlugIns: {
            return _private->dashboardBehaviorAlwaysSendActiveNullEventsToPlugIns;
        }
        case WebDashboardBehaviorAlwaysAcceptsFirstMouse: {
            return _private->dashboardBehaviorAlwaysAcceptsFirstMouse;
        }
        case WebDashboardBehaviorAllowWheelScrolling: {
            return _private->dashboardBehaviorAllowWheelScrolling;
        }
    }
    return NO;
}

- (void)handleAuthenticationForResource:(id)identifier challenge:(NSURLAuthenticationChallenge *)challenge fromDataSource:(WebDataSource *)dataSource
{
    [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:self resource:identifier didReceiveAuthenticationChallenge:challenge fromDataSource:dataSource];
}

+ (void)_setShouldUseFontSmoothing:(BOOL)f
{
    shouldUseFontSmoothing = f;
}

+ (BOOL)_shouldUseFontSmoothing
{
    return shouldUseFontSmoothing;
}

@end


@implementation _WebSafeForwarder

- initWithTarget: t defaultTarget: dt templateClass: (Class)aClass
{
    self = [super init];
    if (!self)
        return nil;
    
    target = t;		// Non retained.
    defaultTarget = dt;
    templateClass = aClass;
    return self;
}


// Used to send messages to delegates that implement informal protocols.
+ safeForwarderWithTarget: t defaultTarget: dt templateClass: (Class)aClass;
{
    return [[[_WebSafeForwarder alloc] initWithTarget: t defaultTarget: dt templateClass: aClass] autorelease];
}

#ifndef NDEBUG
NSMutableDictionary *countInvocations;
#endif

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
#ifndef NDEBUG
    if (!countInvocations){
        countInvocations = [[NSMutableDictionary alloc] init];
    }
    NSNumber *count = [countInvocations objectForKey: NSStringFromSelector([anInvocation selector])];
    if (!count)
        count = [NSNumber numberWithInt: 1];
    else
        count = [NSNumber numberWithInt: [count intValue] + 1];
    [countInvocations setObject: count forKey: NSStringFromSelector([anInvocation selector])];
#endif
    if ([target respondsToSelector: [anInvocation selector]])
        [anInvocation invokeWithTarget: target];
    else if ([defaultTarget respondsToSelector: [anInvocation selector]])
        [anInvocation invokeWithTarget: defaultTarget];
    // Do nothing quietly if method not implemented.
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    return [templateClass instanceMethodSignatureForSelector: aSelector];
}

@end

@implementation WebView

+ (BOOL)canShowMIMEType:(NSString *)MIMEType
{
    return [self _viewClass:nil andRepresentationClass:nil forMIMEType:MIMEType];
}

+ (BOOL)canShowMIMETypeAsHTML:(NSString *)MIMEType
{
    return [WebFrameView _canShowMIMETypeAsHTML:MIMEType];
}

+ (NSArray *)MIMETypesShownAsHTML
{
    NSMutableDictionary *viewTypes = [WebFrameView _viewTypesAllowImageTypeOmission:YES];
    NSEnumerator *enumerator = [viewTypes keyEnumerator];
    id key;
    NSMutableArray *array = [[[NSMutableArray alloc] init] autorelease];
    
    while ((key = [enumerator nextObject])) {
        if ([viewTypes objectForKey:key] == [WebHTMLView class]) {
            [array addObject:key];
        }
    }
    
    return array;
}

+ (void)setMIMETypesShownAsHTML:(NSArray *)MIMETypes
{
    NSEnumerator *enumerator;
    id key;
    
    NSMutableDictionary *viewTypes = [WebFrameView _viewTypesAllowImageTypeOmission:YES];
    enumerator = [viewTypes keyEnumerator];
    while ((key = [enumerator nextObject])) {
        if ([viewTypes objectForKey:key] == [WebHTMLView class]) {
            [WebView _unregisterViewClassAndRepresentationClassForMIMEType:key];
        }
    }
    
    int i, count = [MIMETypes count];
    for (i = 0; i < count; i++) {
        [WebView registerViewClass:[WebHTMLView class] 
                representationClass:[WebHTMLRepresentation class] 
                forMIMEType:[MIMETypes objectAtIndex:i]];
    }
}

+ (NSURL *)URLFromPasteboard:(NSPasteboard *)pasteboard
{
    return [pasteboard _web_bestURL];
}

+ (NSString *)URLTitleFromPasteboard:(NSPasteboard *)pasteboard
{
    return [pasteboard stringForType:WebURLNamePboardType];
}

- (void)_registerDraggedTypes
{
    NSArray *editableTypes = [WebHTMLView _insertablePasteboardTypes];
    NSArray *URLTypes = [NSPasteboard _web_dragTypesForURL];
    NSMutableSet *types = [[NSMutableSet alloc] initWithArray:editableTypes];
    [types addObjectsFromArray:URLTypes];
    [self registerForDraggedTypes:[types allObjects]];
    [types release];
}

#if !BUILDING_ON_PANTHER
static bool CGContextInitialized = false;
#endif

- (void)_commonInitializationWithFrameName:(NSString *)frameName groupName:(NSString *)groupName
{
#if !BUILDING_ON_PANTHER         
    if (!CGContextInitialized) {
		WKDisableCGDeferredUpdates();
        CGContextInitialized = true;
    }
#endif

    _private->drawsBackground = YES;
    _private->smartInsertDeleteEnabled = YES;

    NSRect f = [self frame];
    WebFrameView *wv = [[WebFrameView alloc] initWithFrame: NSMakeRect(0,0,f.size.width,f.size.height)];
    [wv setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [self addSubview: wv];
    [wv release];

    _private->mainFrame = [[WebFrame alloc] initWithName: frameName webFrameView: wv  webView: self];
    [self setGroupName:groupName];
    
    // If there's already a next key view (e.g., from a nib), wire it up to our
    // contained frame view. In any case, wire our next key view up to the our
    // contained frame view. This works together with our becomeFirstResponder 
    // and setNextKeyView overrides.
    NSView *nextKeyView = [self nextKeyView];
    if (nextKeyView != nil && nextKeyView != wv) {
        [wv setNextKeyView:nextKeyView];
    }
    [super setNextKeyView:wv];

    ++WebViewCount;

    [self _registerDraggedTypes];

    // Update WebCore with preferences.  These values will either come from an archived WebPreferences,
    // or from the standard preferences, depending on whether this method was called from initWithCoder:
    // or initWithFrame, respectively.
    [self _updateWebCoreSettingsFromPreferences: [self preferences]];
    
    // Register to receive notifications whenever preference values change.
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesChangedNotification:)
                                                 name:WebPreferencesChangedNotification object:[self preferences]];
}

- init
{
    return [self initWithFrame: NSZeroRect frameName: nil groupName: nil];
}

- initWithFrame: (NSRect)f
{
    return [self initWithFrame: f frameName:nil groupName:nil];
}

- initWithFrame: (NSRect)f frameName: (NSString *)frameName groupName: (NSString *)groupName;
{
    self = [super initWithFrame:f];
    if (!self)
        return nil;
    
    _private = [[WebViewPrivate alloc] init];
    [self _commonInitializationWithFrameName:frameName groupName:groupName];
    [self setMaintainsBackForwardList: YES];
    return self;
}

- (id)initWithCoder:(NSCoder *)decoder
{
    WebView *result = nil;

NS_DURING

    NSString *frameName;
    NSString *groupName;
    
    result = [super initWithCoder:decoder];
    result->_private = [[WebViewPrivate alloc] init];
    
    // We don't want any of the archived subviews.  The subviews will always
    // be created in _commonInitializationFrameName:groupName:.
    [[result subviews] makeObjectsPerformSelector:@selector(removeFromSuperview)];
    
    if ([decoder allowsKeyedCoding]){
        frameName = [decoder decodeObjectForKey:@"FrameName"];
        groupName = [decoder decodeObjectForKey:@"GroupName"];
                
        [result setPreferences: [decoder decodeObjectForKey:@"Preferences"]];
	result->_private->useBackForwardList = [decoder decodeBoolForKey:@"UseBackForwardList"];

        LOG (Encoding, "FrameName = %@, GroupName = %@, useBackForwardList = %d\n", frameName, groupName, (int)_private->useBackForwardList);
    }
    else {
        int version;
    
        [decoder decodeValueOfObjCType:@encode(int) at:&version];
        frameName = [decoder decodeObject];
        groupName = [decoder decodeObject];
        [result setPreferences: [decoder decodeObject]];
        if (version > 1)
            [decoder decodeValuesOfObjCTypes:"c",&result->_private->useBackForwardList];
    }
    [result _commonInitializationWithFrameName:frameName groupName:groupName];
    
NS_HANDLER

    result = nil;
    [self release];

NS_ENDHANDLER

    return result;
}

- (void)encodeWithCoder:(NSCoder *)encoder
{
    [super encodeWithCoder:encoder];

    if ([encoder allowsKeyedCoding]){
        [encoder encodeObject:[[self mainFrame] name] forKey:@"FrameName"];
        [encoder encodeObject:[self groupName] forKey:@"GroupName"];
        [encoder encodeObject:[self preferences] forKey:@"Preferences"];
	[encoder encodeBool:_private->useBackForwardList forKey:@"UseBackForwardList"];

        LOG (Encoding, "FrameName = %@, GroupName = %@, useBackForwardList = %d\n", [[self mainFrame] name], [self groupName], (int)_private->useBackForwardList);
    }
    else {
        int version = WebViewVersion;
        [encoder encodeValueOfObjCType:@encode(int) at:&version];
        [encoder encodeObject:[[self mainFrame] name]];
        [encoder encodeObject:[self groupName]];
        [encoder encodeObject:[self preferences]];
        [encoder encodeValuesOfObjCTypes:"c",&_private->useBackForwardList];
    }
}

- (void)dealloc
{
    [self _close];
    
    --WebViewCount;
    
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
    [WebPreferences _removeReferenceForIdentifier: [self preferencesIdentifier]];
    
    [_private release];
    // [super dealloc] can end up dispatching against _private (3466082)
    _private = nil;

    [super dealloc];
}

- (void)finalize
{
    [self _close];

    --WebViewCount;

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [WebPreferences _removeReferenceForIdentifier: [self preferencesIdentifier]];

    [super finalize];
}

- (void)setPreferences: (WebPreferences *)prefs
{
    if (_private->preferences != prefs){
        [[NSNotificationCenter defaultCenter] removeObserver: self name: WebPreferencesChangedNotification object: [self preferences]];
        [WebPreferences _removeReferenceForIdentifier: [_private->preferences identifier]];
        [_private->preferences release];
        _private->preferences = [prefs retain];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesChangedNotification:)
                                                    name:WebPreferencesChangedNotification object:[self preferences]];

        [[NSNotificationCenter defaultCenter]
           postNotificationName:WebPreferencesChangedNotification object:prefs userInfo:nil];
    }
}

- (WebPreferences *)preferences
{
    return _private->preferences ? _private->preferences : [WebPreferences standardPreferences];
}

- (void)setPreferencesIdentifier:(NSString *)anIdentifier
{
    if (![anIdentifier isEqual: [[self preferences] identifier]]){
        [self setPreferences: [[WebPreferences alloc] initWithIdentifier:anIdentifier]];
    }
}

- (NSString *)preferencesIdentifier
{
    return [[self preferences] identifier];
}


- (void)setUIDelegate:delegate
{
    _private->UIDelegate = delegate;
    [_private->UIDelegateForwarder release];
    _private->UIDelegateForwarder = nil;
}

- UIDelegate
{
    return _private->UIDelegate;
}

- (void)setResourceLoadDelegate: delegate
{
    _private->resourceProgressDelegate = delegate;
    [_private->resourceProgressDelegateForwarder release];
    _private->resourceProgressDelegateForwarder = nil;
    [self _cacheResourceLoadDelegateImplementations];
}


- resourceLoadDelegate
{
    return _private->resourceProgressDelegate;
}


- (void)setDownloadDelegate: delegate
{
    _private->downloadDelegate = delegate;
}


- downloadDelegate
{
    return _private->downloadDelegate;
}

- (void)setPolicyDelegate:delegate
{
    _private->policyDelegate = delegate;
    [_private->policyDelegateForwarder release];
    _private->policyDelegateForwarder = nil;
}

- policyDelegate
{
    return _private->policyDelegate;
}

- (void)setFrameLoadDelegate:delegate
{
    _private->frameLoadDelegate = delegate;
    [_private->frameLoadDelegateForwarder release];
    _private->frameLoadDelegateForwarder = nil;
}

- frameLoadDelegate
{
    return _private->frameLoadDelegate;
}

- (WebFrame *)mainFrame
{
    // This can be called in initialization, before _private has been set up (3465613)
    if (_private != nil) {
        return _private->mainFrame;
    }
    return nil;
}

- (WebBackForwardList *)backForwardList
{
    if (_private->useBackForwardList)
        return _private->backForwardList;
    return nil;
}

- (void)setMaintainsBackForwardList: (BOOL)flag
{
    _private->useBackForwardList = flag;
}

- (BOOL)goBack
{
    WebHistoryItem *item = [[self backForwardList] backItem];
    
    if (item){
        [self _goToItem: item withLoadType: WebFrameLoadTypeBack];
        return YES;
    }
    return NO;
}

- (BOOL)goForward
{
    WebHistoryItem *item = [[self backForwardList] forwardItem];
    
    if (item){
        [self _goToItem: item withLoadType: WebFrameLoadTypeForward];
        return YES;
    }
    return NO;
}

- (BOOL)goToBackForwardItem:(WebHistoryItem *)item
{
    [self _goToItem: item withLoadType: WebFrameLoadTypeIndexedBackForward];
    return YES;
}

- (void)setTextSizeMultiplier:(float)m
{
    if (_private->textSizeMultiplier == m) {
        return;
    }
    _private->textSizeMultiplier = m;
    [[self mainFrame] _textSizeMultiplierChanged];
}

- (float)textSizeMultiplier
{
    return _private->textSizeMultiplier;
}

- (void)setApplicationNameForUserAgent:(NSString *)applicationName
{
    NSString *name = [applicationName copy];
    [_private->applicationNameForUserAgent release];
    _private->applicationNameForUserAgent = name;
    if (!_private->userAgentOverridden) {
        [_private->userAgent release];
        _private->userAgent = nil;
    }
}

- (NSString *)applicationNameForUserAgent
{
    return [[_private->applicationNameForUserAgent retain] autorelease];
}

- (void)setCustomUserAgent:(NSString *)userAgentString
{
    NSString *override = [userAgentString copy];
    [_private->userAgent release];
    _private->userAgent = override;
    _private->userAgentOverridden = override != nil;
}

- (NSString *)customUserAgent
{
    return _private->userAgentOverridden ? [[_private->userAgent retain] autorelease] : nil;
}

- (void)setMediaStyle:(NSString *)mediaStyle
{
    if (_private->mediaStyle != mediaStyle) {
        [_private->mediaStyle release];
        _private->mediaStyle = [mediaStyle copy];
    }
}

- (NSString *)mediaStyle
{
    return _private->mediaStyle;
}

- (BOOL)supportsTextEncoding
{
    id documentView = [[[self mainFrame] frameView] documentView];
    return [documentView conformsToProtocol:@protocol(WebDocumentText)]
        && [documentView supportsTextEncoding];
}

- (void)setCustomTextEncodingName:(NSString *)encoding
{
    NSString *oldEncoding = [self customTextEncodingName];
    if (encoding == oldEncoding || [encoding isEqualToString:oldEncoding]) {
        return;
    }
    [[self mainFrame] _reloadAllowingStaleDataWithOverrideEncoding:encoding];
}

- (NSString *)_mainFrameOverrideEncoding
{
    WebDataSource *dataSource = [[self mainFrame] provisionalDataSource];
    if (dataSource == nil) {
        dataSource = [[self mainFrame] dataSource];
    }
    if (dataSource == nil) {
        return nil;
    }
    return [dataSource _overrideEncoding];
}

- (NSString *)customTextEncodingName
{
    return [self _mainFrameOverrideEncoding];
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)script
{
    return [[[self mainFrame] _bridge] stringByEvaluatingJavaScriptFromString:script];
}

- (WebScriptObject *)windowScriptObject
{
    return [[[self mainFrame] _bridge] windowScriptObject];
}


// Get the appropriate user-agent string for a particular URL.
// Since we no longer automatically spoof, this no longer requires looking at the URL.
- (NSString *)userAgentForURL:(NSURL *)URL
{
    NSString *userAgent = _private->userAgent;
    if (userAgent) {
        return [[userAgent retain] autorelease];
    }
    
    // FIXME: Some day we will start reporting the actual CPU here instead of hardcoding PPC.

    NSString *language = [NSUserDefaults _webkit_preferredLanguageCode];
    id sourceVersion = [[NSBundle bundleForClass:[WebView class]]
        objectForInfoDictionaryKey:(id)kCFBundleVersionKey];
    NSString *applicationName = _private->applicationNameForUserAgent;

    if ([applicationName length]) {
        userAgent = [NSString stringWithFormat:@"Mozilla/5.0 (Macintosh; U; PPC Mac OS X; %@) AppleWebKit/%@ (KHTML, like Gecko) %@",
            language, sourceVersion, applicationName];
    } else {
        userAgent = [NSString stringWithFormat:@"Mozilla/5.0 (Macintosh; U; PPC Mac OS X; %@) AppleWebKit/%@ (KHTML, like Gecko)",
            language, sourceVersion];
    }

    _private->userAgent = [userAgent retain];
    return userAgent;
}

- (void)setHostWindow:(NSWindow *)hostWindow
{
    if (hostWindow != _private->hostWindow) {
        [[self mainFrame] _viewWillMoveToHostWindow:hostWindow];
        [_private->hostWindow release];
        _private->hostWindow = [hostWindow retain];
        [[self mainFrame] _viewDidMoveToHostWindow];
    }
}

- (NSWindow *)hostWindow
{
    return _private->hostWindow;
}

- (NSView <WebDocumentView> *)documentViewAtWindowPoint:(NSPoint)point
{
    return [[self _frameViewAtWindowPoint:point] documentView];
}

- (NSView <WebDocumentDragging> *)_draggingDocumentViewAtWindowPoint:(NSPoint)point
{
    NSView <WebDocumentView> *documentView = [self documentViewAtWindowPoint:point];
    if ([documentView conformsToProtocol:@protocol(WebDocumentDragging)]) {
        return (NSView <WebDocumentDragging> *)documentView;
    }
    return nil;
}

- (NSDictionary *)_elementAtWindowPoint:(NSPoint)windowPoint
{
    WebFrameView *frameView = [self _frameViewAtWindowPoint:windowPoint];
    if (!frameView)
        return nil;
    NSView <WebDocumentView> *documentView = [frameView documentView];
    if ([documentView conformsToProtocol:@protocol(WebDocumentElement)]) {
        NSPoint point = [documentView convertPoint:windowPoint fromView:nil];
        return [(NSView <WebDocumentElement> *)documentView elementAtPoint:point];
    }
    return [NSDictionary dictionaryWithObject:[frameView webFrame] forKey:WebElementFrameKey];
}

- (NSDictionary *)elementAtPoint:(NSPoint)point
{
    return [self _elementAtWindowPoint:[self convertPoint:point toView:nil]];
}

- (void)_setDraggingDocumentView:(NSView <WebDocumentDragging> *)newDraggingView
{
    if (_private->draggingDocumentView != newDraggingView) {
        [_private->draggingDocumentView release];
        _private->draggingDocumentView = [newDraggingView retain];
    }
}

- (NSDragOperation)_loadingDragOperationForDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
    if (_private->dragDestinationActionMask & WebDragDestinationActionLoad) {
        NSPoint windowPoint = [draggingInfo draggingLocation];
        NSView *view = [self hitTest:[[self superview] convertPoint:windowPoint toView:nil]];
        // Don't accept the drag over a plug-in since plug-ins may want to handle it.
        if (![view isKindOfClass:[WebBaseNetscapePluginView class]] && !_private->editable && !_private->initiatedDrag) {
            // If not editing or dragging, use _web_dragOperationForDraggingInfo to find a URL to load on the pasteboard.
            return [self _web_dragOperationForDraggingInfo:draggingInfo];
        }
    }
    return NSDragOperationNone;
}

- (NSDragOperation)_delegateDragOperationForDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
    NSPoint windowPoint = [draggingInfo draggingLocation];
    NSView <WebDocumentDragging> *newDraggingView = [self _draggingDocumentViewAtWindowPoint:windowPoint];
    if (_private->draggingDocumentView != newDraggingView) {
        [_private->draggingDocumentView draggingCancelledWithDraggingInfo:draggingInfo];
        [self _setDraggingDocumentView:newDraggingView];
    }
    
    _private->dragDestinationActionMask = [[self _UIDelegateForwarder] webView:self dragDestinationActionMaskForDraggingInfo:draggingInfo];
    NSDragOperation operation = NSDragOperationNone;
    
    if (_private->dragDestinationActionMask == WebDragDestinationActionNone) {
        [_private->draggingDocumentView draggingCancelledWithDraggingInfo:draggingInfo];
    } else {
        operation = [_private->draggingDocumentView draggingUpdatedWithDraggingInfo:draggingInfo actionMask:_private->dragDestinationActionMask];
        if (operation == NSDragOperationNone) {
            return [self _loadingDragOperationForDraggingInfo:draggingInfo];
        }
    }
    
    return operation;
}

// The following 2 internal NSView methods are called on the drag destination by make scrolling while dragging work.
// Scrolling while dragging will only work if the drag destination is in a scroll view. The WebView is the drag destination. 
// When dragging to a WebView, the document subview should scroll, but it doesn't because it is not the drag destination. 
// Forward these calls to the document subview to make its scroll view scroll.
- (void)_autoscrollForDraggingInfo:(id)draggingInfo timeDelta:(NSTimeInterval)repeatDelta
{
    NSView <WebDocumentView> *documentView = [self documentViewAtWindowPoint:[draggingInfo draggingLocation]];
    [documentView _autoscrollForDraggingInfo:draggingInfo timeDelta:repeatDelta];
}

- (BOOL)_shouldAutoscrollForDraggingInfo:(id)draggingInfo
{
    NSView <WebDocumentView> *documentView = [self documentViewAtWindowPoint:[draggingInfo draggingLocation]];
    return [documentView _shouldAutoscrollForDraggingInfo:draggingInfo];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)draggingInfo
{
    return [self _delegateDragOperationForDraggingInfo:draggingInfo];
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)draggingInfo
{
    return [self _delegateDragOperationForDraggingInfo:draggingInfo];
}

- (void)draggingExited:(id <NSDraggingInfo>)draggingInfo
{
    [_private->draggingDocumentView draggingCancelledWithDraggingInfo:draggingInfo];
    [self _setDraggingDocumentView:nil];
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)draggingInfo
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)draggingInfo
{
    ASSERT(_private->draggingDocumentView == [self _draggingDocumentViewAtWindowPoint:[draggingInfo draggingLocation]]);
    
    if ([_private->draggingDocumentView concludeDragForDraggingInfo:draggingInfo actionMask:_private->dragDestinationActionMask]) {
        [self _setDraggingDocumentView:nil];
        return YES;
    }
    
    [self _setDraggingDocumentView:nil];
        
    if ([self _loadingDragOperationForDraggingInfo:draggingInfo] != NSDragOperationNone) {    
        NSURL *URL = [[self class] URLFromPasteboard:[draggingInfo draggingPasteboard]];
        if (URL != nil) {
            [[self _UIDelegateForwarder] webView:self willPerformDragDestinationAction:WebDragDestinationActionLoad forDraggingInfo:draggingInfo];
            NSURLRequest *request = [[NSURLRequest alloc] initWithURL:URL];
            [[self mainFrame] loadRequest:request];
            [request release];
            return YES;
        }
    }
    
    return NO;
}

- (NSView *)_hitTest:(NSPoint *)aPoint dragTypes:(NSSet *)types
{
    NSView *hitView = [super _hitTest:aPoint dragTypes:types];
    if (!hitView && [[self superview] mouse:*aPoint inRect:[self frame]]) {
        return self;
    } else {
        return hitView;
    }
}

- (BOOL)acceptsFirstResponder
{
    return [[[self mainFrame] frameView] acceptsFirstResponder];
}

- (BOOL)becomeFirstResponder
{
    // This works together with setNextKeyView to splice the WebView into
    // the key loop similar to the way NSScrollView does this. Note that
    // WebFrameView has very similar code.
    NSWindow *window = [self window];
    WebFrameView *mainFrameView = [[self mainFrame] frameView];
    
    if ([window keyViewSelectionDirection] == NSSelectingPrevious) {
        NSView *previousValidKeyView = [self previousValidKeyView];
        if ((previousValidKeyView != self) && (previousValidKeyView != mainFrameView)) {
            [window makeFirstResponder:previousValidKeyView];
            return YES;
        } else {
            return NO;
        }
    }
    
    if ([mainFrameView acceptsFirstResponder]) {
        [window makeFirstResponder:mainFrameView];
        return YES;
    } 
    
    return NO;
}

- (NSView *)_webcore_effectiveFirstResponder
{
    WebFrameView *frameView = [[self mainFrame] frameView];
    return frameView ? [frameView _webcore_effectiveFirstResponder] : [super _webcore_effectiveFirstResponder];
}

- (void)setNextKeyView:(NSView *)aView
{
    // This works together with becomeFirstResponder to splice the WebView into
    // the key loop similar to the way NSScrollView does this. Note that
    // WebFrameView has very similar code.
    WebFrameView *mainFrameView = [[self mainFrame] frameView];
    if (mainFrameView != nil) {
        [mainFrameView setNextKeyView:aView];
    } else {
        [super setNextKeyView:aView];
    }
}

static WebFrame *incrementFrame(WebFrame *curr, BOOL forward, BOOL wrapFlag)
{
    return forward ? [curr _nextFrameWithWrap:wrapFlag]
                   : [curr _previousFrameWithWrap:wrapFlag];
}

// Search from the end of the currently selected location, or from the beginning of the
// document if nothing is selected.  Deals with subframes.
- (BOOL)_searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag findInSelection:(BOOL)findInSelection
{
    // Get the frame holding the selection, or start with the main frame
    WebFrame *startFrame = [self _frameForCurrentSelection];

    // Search the first frame, then all the other frames, in order
    NSView <WebDocumentSearching> *startSearchView = nil;
    BOOL startHasSelection = NO;
    WebFrame *frame = startFrame;
    do {
        id <WebDocumentView> view = [[frame frameView] documentView];
        if ([view conformsToProtocol:@protocol(WebDocumentSearching)]) {
            NSView <WebDocumentSearching> *searchView = (NSView <WebDocumentSearching> *)view;

            // first time through
            if (frame == startFrame) {
                // Remember if start even has a selection, to know if we need to search more later
                if ([searchView isKindOfClass:[WebHTMLView class]]) {
                    // optimization for the common case, to avoid making giant string for selection
                    startHasSelection = [[startFrame _bridge] selectedDOMRange] != nil;
                } else if ([searchView conformsToProtocol:@protocol(WebDocumentText)]) {
                    startHasSelection = [(id <WebDocumentText>)searchView selectedString] != nil;
                }
                startSearchView = searchView;
            }
            
            // Note at this point we are assuming the search will be done top-to-bottom,
            // not starting at any selection that exists.  See 3228554.
            BOOL success;
            if ([searchView respondsToSelector:@selector(_searchFor:direction:caseSensitive:wrap:findInSelection:)]) {
                success = [searchView _searchFor:string direction:forward caseSensitive:caseFlag wrap:NO findInSelection:findInSelection];
            } else {
                success = [searchView searchFor:string direction:forward caseSensitive:caseFlag wrap:NO];
            }
            if (success) {
                [[self window] makeFirstResponder:searchView];
                return YES;
            }
        }
        frame = incrementFrame(frame, forward, wrapFlag);
    } while (frame != nil && frame != startFrame);

    // Search contents of startFrame, on the other side of the selection that we did earlier.
    // We cheat a bit and just research with wrap on
    if (wrapFlag && startHasSelection && startSearchView) {
        BOOL success;
        if ([startSearchView respondsToSelector:@selector(_searchFor:direction:caseSensitive:wrap:findInSelection:)]) {
            success = [startSearchView _searchFor:string direction:forward caseSensitive:caseFlag wrap:YES findInSelection:findInSelection];
        } else {
            success = [startSearchView searchFor:string direction:forward caseSensitive:caseFlag wrap:YES];
        }
        if (success) {
            [[self window] makeFirstResponder:startSearchView];
            return YES;
        }
    }
    return NO;
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag
{
    return [self _searchFor:string direction:forward caseSensitive:caseFlag wrap:wrapFlag findInSelection:NO];
}


+ (void)registerViewClass:(Class)viewClass representationClass:(Class)representationClass forMIMEType:(NSString *)MIMEType
{
    [[WebFrameView _viewTypesAllowImageTypeOmission:YES] setObject:viewClass forKey:MIMEType];
    [[WebDataSource _repTypesAllowImageTypeOmission:YES] setObject:representationClass forKey:MIMEType];
}

- (void)setGroupName:(NSString *)groupName
{
    if (groupName != _private->setName){
        [_private->setName release];
        _private->setName = [groupName copy];
        [WebViewSets addWebView:self toSetNamed:_private->setName];
    }
}

- (NSString *)groupName
{
    return _private->setName;
}

- (double)estimatedProgress
{
    return _private->progressValue;
}

- (NSArray *)pasteboardTypesForSelection
{
    NSView <WebDocumentView> *documentView = [[[self _frameForCurrentSelection] frameView] documentView];
    if ([documentView conformsToProtocol:@protocol(WebDocumentSelection)]) {
        return [(NSView <WebDocumentSelection> *)documentView pasteboardTypesForSelection];
    }
    return [NSArray array];
}

- (void)writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    WebBridge *bridge = [self _bridgeForCurrentSelection];
    if ([bridge selectionState] != WebSelectionStateRange) {
        NSView <WebDocumentView> *documentView = [[[bridge webFrame] frameView] documentView];
        if ([documentView conformsToProtocol:@protocol(WebDocumentSelection)]) {
            [(NSView <WebDocumentSelection> *)documentView writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard];
        }
    }
}

- (NSArray *)pasteboardTypesForElement:(NSDictionary *)element
{
    if ([element objectForKey:WebElementImageURLKey] != nil) {
        return [NSPasteboard _web_writableTypesForImageIncludingArchive:([element objectForKey:WebElementDOMNodeKey] != nil)];
    } else if ([element objectForKey:WebElementLinkURLKey] != nil) {
        return [NSPasteboard _web_writableTypesForURL];
    } else if ([[element objectForKey:WebElementIsSelectedKey] boolValue]) {
        return [self pasteboardTypesForSelection];
    }
    return [NSArray array];
}

- (void)writeElement:(NSDictionary *)element withPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    if ([element objectForKey:WebElementImageURLKey] != nil) {
        [self _writeImageElement:element withPasteboardTypes:types toPasteboard:pasteboard];
    } else if ([element objectForKey:WebElementLinkURLKey] != nil) {
        [self _writeLinkElement:element withPasteboardTypes:types toPasteboard:pasteboard];
    } else if ([[element objectForKey:WebElementIsSelectedKey] boolValue]) {
        [self writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard];
    }
}

- (void)moveDragCaretToPoint:(NSPoint)point
{
    WebBridge *bridge = [self _bridgeAtPoint:point];
    if (bridge != _private->dragCaretBridge) {
        [_private->dragCaretBridge removeDragCaret];
        _private->dragCaretBridge = [bridge retain];
    }
    [_private->dragCaretBridge moveDragCaretToPoint:[self convertPoint:point toView:[[[_private->dragCaretBridge webFrame] frameView] documentView]]];
}

- (void)removeDragCaret
{
    [_private->dragCaretBridge removeDragCaret];
    [_private->dragCaretBridge release];
    _private->dragCaretBridge = nil;
}

@end

@implementation WebView (WebIBActions)

- (IBAction)takeStringURLFrom: sender
{
    NSString *URLString = [sender stringValue];
    
    [[self mainFrame] loadRequest: [NSURLRequest requestWithURL: [NSURL _web_URLWithDataAsString: URLString]]];
}

- (BOOL)canGoBack
{
    return [[self backForwardList] backItem] != nil;
}

- (BOOL)canGoForward
{
    return [[self backForwardList] forwardItem] != nil;
}

- (IBAction)goBack:(id)sender
{
    [self goBack];
}

- (IBAction)goForward:(id)sender
{
    [self goForward];
}

- (IBAction)stopLoading:(id)sender
{
    [[self mainFrame] stopLoading];
}

- (IBAction)reload:(id)sender
{
    [[self mainFrame] reload];
}

#define MinimumTextSizeMultiplier	0.5
#define MaximumTextSizeMultiplier	3.0
#define TextSizeMultiplierRatio		1.2

- (BOOL)canMakeTextSmaller
{
    if ([[self mainFrame] dataSource] == nil) {
        return NO;
    }
    // FIXME: This will prevent text sizing in subframes if the main frame doesn't support it
    if (![[[[self mainFrame] frameView] documentView] conformsToProtocol:@protocol(_web_WebDocumentTextSizing)]) {
        return NO;
    }
    if ([self textSizeMultiplier]/TextSizeMultiplierRatio < MinimumTextSizeMultiplier) {
        return NO;
    }
    return YES;
}

- (BOOL)canMakeTextLarger
{
    if ([[self mainFrame] dataSource] == nil) {
        return NO;
    }
    // FIXME: This will prevent text sizing in subframes if the main frame doesn't support it
    if (![[[[self mainFrame] frameView] documentView] conformsToProtocol:@protocol(_web_WebDocumentTextSizing)]) {
        return NO;
    }
    if ([self textSizeMultiplier]*TextSizeMultiplierRatio > MaximumTextSizeMultiplier) {
        return NO;
    }
    return YES;
}

- (IBAction)makeTextSmaller:(id)sender
{
    if (![self canMakeTextSmaller]) {
        return;
    }
    [self setTextSizeMultiplier:[self textSizeMultiplier]/TextSizeMultiplierRatio];
}

- (IBAction)makeTextLarger:(id)sender
{
    if (![self canMakeTextLarger]) {
        return;
    }
    [self setTextSizeMultiplier:[self textSizeMultiplier]*TextSizeMultiplierRatio];
}

- (BOOL)_responderValidateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    id responder = [self _responderForResponderOperations];
    if (responder != self && [responder respondsToSelector:[item action]]) {
        if ([responder respondsToSelector:@selector(validateUserInterfaceItem:)]) {
            return [responder validateUserInterfaceItem:item];
        }
        return YES;
    }
    return NO;
}

#define VALIDATE(name) \
    else if (action == @selector(name:)) { return [self _responderValidateUserInterfaceItem:item]; }

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    SEL action = [item action];

    if (action == @selector(goBack:)) {
	return [self canGoBack];
    } else if (action == @selector(goForward:)) {
	return [self canGoForward];
    } else if (action == @selector(makeTextLarger:)) {
	return [self canMakeTextLarger];
    } else if (action == @selector(makeTextSmaller:)) {
	return [self canMakeTextSmaller];
    } else if (action == @selector(makeTextStandardSize:)) {
	return [self canMakeTextStandardSize];
    } else if (action == @selector(reload:)) {
	return [[self mainFrame] dataSource] != nil;
    } else if (action == @selector(stopLoading:)) {
	return [self _isLoading];
    } else if (action == @selector(toggleContinuousSpellChecking:)) {
        BOOL checkMark = NO;
        BOOL retVal = NO;
	if ([self isEditable] && [self _continuousCheckingAllowed]) {
            checkMark = [self isContinuousSpellCheckingEnabled];
            retVal = YES;
        }
        if ([(NSObject *)item isKindOfClass:[NSMenuItem class]]) {
            NSMenuItem *menuItem = (NSMenuItem *)item;
            [menuItem setState:checkMark ? NSOnState : NSOffState];
        }
        return retVal;
    }
    FOR_EACH_RESPONDER_SELECTOR(VALIDATE)

    return YES;
}

@end

@implementation WebView (WebPendingPublic)

- (void)setMainFrameURL:(NSString *)URLString
{
    [[self mainFrame] loadRequest: [NSURLRequest requestWithURL: [NSURL _web_URLWithDataAsString: URLString]]];
}

- (NSString *)mainFrameURL
{
    WebDataSource *ds;
    ds = [[self mainFrame] provisionalDataSource];
    if (!ds)
        ds = [[self mainFrame] dataSource];
    return [[[ds request] URL] _web_originalDataAsString];
}

- (BOOL)isLoading
{
    LOG (Bindings, "isLoading = %d", (int)[self _isLoading]);
    return [self _isLoading];
}

- (NSString *)mainFrameTitle
{
    NSString *mainFrameTitle = [[[self mainFrame] dataSource] pageTitle];
    return (mainFrameTitle != nil) ? mainFrameTitle : @"";
}

- (NSImage *)mainFrameIcon
{
    return [[WebIconDatabase sharedIconDatabase] iconForURL:[[[[self mainFrame] dataSource] _URL] _web_originalDataAsString] withSize:WebIconSmallSize];
}

- (void)setDrawsBackground:(BOOL)drawsBackground
{
    if (_private->drawsBackground == drawsBackground)
        return;
    _private->drawsBackground = drawsBackground;
    [[self mainFrame] _updateDrawsBackground];
}

- (BOOL)drawsBackground
{
    return _private->drawsBackground;
}

- (void)toggleSmartInsertDelete:(id)sender
{
    if ([self isEditable]) {
        [self setSmartInsertDeleteEnabled:![self smartInsertDeleteEnabled]];
    }
}

- (IBAction)toggleContinuousSpellChecking:(id)sender
{
    if ([self isEditable]) {
        [self setContinuousSpellCheckingEnabled:![self isContinuousSpellCheckingEnabled]];
    }
}

- (BOOL)isContinuousGrammarCheckingEnabled
{
    return _private->continuousGrammarCheckingEnabled && [self _continuousCheckingAllowed];
}

- (void)setContinuousGrammarCheckingEnabled:(BOOL)flag
{
    _private->continuousGrammarCheckingEnabled = flag;
    if ([self isContinuousGrammarCheckingEnabled]) {
        [self _preflightSpellChecker];
    } else {
        // FIXME: Put code here to remove underlines for bad grammar.
    }
}

- (void)toggleContinuousGrammarChecking:(id)sender
{
    if ([self isEditable]) {
        [self setContinuousGrammarCheckingEnabled:![self isContinuousGrammarCheckingEnabled]];
    }
}


- (BOOL)canMakeTextStandardSize
{
    if ([[self mainFrame] dataSource] == nil) {
        return NO;
    }
    // FIXME: This will prevent text sizing in subframes if the main frame doesn't support it
    if (![[[[self mainFrame] frameView] documentView] conformsToProtocol:@protocol(_web_WebDocumentTextSizing)]) {
        return NO;
    }
    
    return [self textSizeMultiplier] != 1;
}

- (IBAction)makeTextStandardSize:(id)sender
{
    if (![self canMakeTextStandardSize]) {
        return;
    }
    [self setTextSizeMultiplier:1];
}

- (BOOL)maintainsInactiveSelection
{
    return [self isEditable];
}

@end

@implementation WebView (WebViewPrintingPrivate)

- (float)_headerHeight
{
    if ([[self UIDelegate] respondsToSelector:@selector(webViewHeaderHeight:)]) {
        return [[self UIDelegate] webViewHeaderHeight:self];
    }
    
#ifdef DEBUG_HEADER_AND_FOOTER
    return 25;
#else
    return 0;
#endif
}

- (float)_footerHeight
{
    if ([[self UIDelegate] respondsToSelector:@selector(webViewFooterHeight:)]) {
        return [[self UIDelegate] webViewFooterHeight:self];
    }
    
#ifdef DEBUG_HEADER_AND_FOOTER
    return 50;
#else
    return 0;
#endif
}

- (void)_drawHeaderInRect:(NSRect)rect
{
#ifdef DEBUG_HEADER_AND_FOOTER
    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    [currentContext saveGraphicsState];
    [[NSColor yellowColor] set];
    NSRectFill(rect);
    [currentContext restoreGraphicsState];
#endif
    
    if ([[self UIDelegate] respondsToSelector:@selector(webView:drawHeaderInRect:)]) {
        NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
        [currentContext saveGraphicsState];
        NSRectClip(rect);
        [[self UIDelegate] webView:self drawHeaderInRect:rect]; 
        [currentContext restoreGraphicsState];
    }
}

- (void)_drawFooterInRect:(NSRect)rect
{
#ifdef DEBUG_HEADER_AND_FOOTER
    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    [currentContext saveGraphicsState];
    [[NSColor cyanColor] set];
    NSRectFill(rect);
    [currentContext restoreGraphicsState];
#endif
    
    if ([[self UIDelegate] respondsToSelector:@selector(webView:drawFooterInRect:)]) {
        NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
        [currentContext saveGraphicsState];
        NSRectClip(rect);
        [[self UIDelegate] webView:self drawFooterInRect:rect];
        [currentContext restoreGraphicsState];
    }
}

- (void)_adjustPrintingMarginsForHeaderAndFooter
{
    NSPrintOperation *op = [NSPrintOperation currentOperation];
    NSPrintInfo *info = [op printInfo];
    float scale = [op _web_pageSetupScaleFactor];
    [info setTopMargin:[info topMargin] + [self _headerHeight]*scale];
    [info setBottomMargin:[info bottomMargin] + [self _footerHeight]*scale];
}

- (void)_drawHeaderAndFooter
{
    // The header and footer rect height scales with the page, but the width is always
    // all the way across the printed page (inset by printing margins).
    NSPrintOperation *op = [NSPrintOperation currentOperation];
    float scale = [op _web_pageSetupScaleFactor];
    NSPrintInfo *printInfo = [op printInfo];
    NSSize paperSize = [printInfo paperSize];
    float headerFooterLeft = [printInfo leftMargin]/scale;
    float headerFooterWidth = (paperSize.width - ([printInfo leftMargin] + [printInfo rightMargin]))/scale;
    NSRect footerRect = NSMakeRect(headerFooterLeft, [printInfo bottomMargin]/scale - [self _footerHeight] , 
                                   headerFooterWidth, [self _footerHeight]);
    NSRect headerRect = NSMakeRect(headerFooterLeft, (paperSize.height - [printInfo topMargin])/scale, 
                                   headerFooterWidth, [self _headerHeight]);
    
    [self _drawHeaderInRect:headerRect];
    [self _drawFooterInRect:footerRect];
}
@end

@implementation WebView (WebDebugBinding)

- (void)addObserver:(NSObject *)anObserver forKeyPath:(NSString *)keyPath options:(NSKeyValueObservingOptions)options context:(void *)context
{
    LOG (Bindings, "addObserver:%p forKeyPath:%@ options:%x context:%p", anObserver, keyPath, options, context);
    [super addObserver:anObserver forKeyPath:keyPath options:options context:context];
}

- (void)removeObserver:(NSObject *)anObserver forKeyPath:(NSString *)keyPath
{
    LOG (Bindings, "removeObserver:%p forKeyPath:%@", anObserver, keyPath);
    [super removeObserver:anObserver forKeyPath:keyPath];
}

@end

//==========================================================================================
// Editing

@implementation WebView (WebViewCSS)

- (DOMCSSStyleDeclaration *)computedStyleForElement:(DOMElement *)element pseudoElement:(NSString *)pseudoElement
{
    // FIXME: is this the best level for this conversion?
    if (pseudoElement == nil) {
        pseudoElement = @"";
    }
    return [[element ownerDocument] getComputedStyle:element :pseudoElement];
}

@end

@implementation WebView (WebInternal)

// Return the frame holding first responder
- (WebFrame *)_frameForCurrentSelection
{
    // Find the frame holding the first responder, or holding the first form in the doc
    NSResponder *resp = [[self window] firstResponder];
    if (!resp || ![resp isKindOfClass:[NSView class]] || ![(NSView *)resp isDescendantOf:self]) {
        return [self mainFrame];	// first responder outside our view tree
    } else {
        WebFrameView *frameView = (WebFrameView *)[(NSView *)resp _web_superviewOfClass:[WebFrameView class]];
        return [frameView webFrame];
    }
}

- (WebBridge *)_bridgeForCurrentSelection
{
    return [[self _frameForCurrentSelection] _bridge];
}

- (BOOL)_isLoading
{
    WebFrame *mainFrame = [self mainFrame];
    return [[mainFrame dataSource] isLoading]
        || [[mainFrame provisionalDataSource] isLoading];
}

- (WebFrameView *)_frameViewAtWindowPoint:(NSPoint)point
{
    NSView *view = [self hitTest:[[self superview] convertPoint:point fromView:nil]];
    return (WebFrameView *)[view _web_superviewOfClass:[WebFrameView class] stoppingAtClass:[self class]];
}

- (WebBridge *)_bridgeAtPoint:(NSPoint)point
{
    return [[[self _frameViewAtWindowPoint:[self convertPoint:point toView:nil]] webFrame] _bridge];
}

@end

@implementation WebView (WebViewEditing)

- (DOMRange *)editableDOMRangeForPoint:(NSPoint)point
{
    WebBridge *bridge = [self _bridgeAtPoint:point];
    return [bridge editableDOMRangeForPoint:[self convertPoint:point toView:[[[bridge webFrame] frameView] documentView]]];
}

- (BOOL)_shouldBeginEditingInDOMRange:(DOMRange *)range
{
    return [[self _editingDelegateForwarder] webView:self shouldBeginEditingInDOMRange:range];
}

- (BOOL)_shouldEndEditingInDOMRange:(DOMRange *)range
{
    return [[self _editingDelegateForwarder] webView:self shouldEndEditingInDOMRange:range];
}

- (BOOL)_canPaste
{
    id documentView = [[[self mainFrame] frameView] documentView];
    return [documentView respondsToSelector:@selector(_canPaste)] && [documentView _canPaste];
}

- (void)setSelectedDOMRange:(DOMRange *)range affinity:(NSSelectionAffinity)selectionAffinity
{
    if (range == nil) {
        [[self _bridgeForCurrentSelection] deselectText];
    } else {
        // Derive the bridge to use from the range passed in.
        // Using _bridgeForCurrentSelection could give us a different document than
        // the one the range uses.
        [[[range startContainer] _bridge] setSelectedDOMRange:range affinity:selectionAffinity closeTyping:YES];
    }
}

- (DOMRange *)selectedDOMRange
{
    return [[self _bridgeForCurrentSelection] selectedDOMRange];
}

- (NSSelectionAffinity)selectionAffinity
{
    return [[self _bridgeForCurrentSelection] selectionAffinity];
}

- (void)setEditable:(BOOL)flag
{
    if (_private->editable != flag) {
        _private->editable = flag;
        WebBridge *bridge = [[self mainFrame] _bridge];
        if (flag) {
            [bridge applyEditingStyleToBodyElement];
            // If the WebView is made editable and the selection is empty, set it to something.
            if ([self selectedDOMRange] == nil)
                [bridge setSelectionFromNone];
        }
        else {
            [bridge removeEditingStyleFromBodyElement];
        }
    }
}

- (BOOL)isEditable
{
    return _private->editable;
}

- (void)setTypingStyle:(DOMCSSStyleDeclaration *)style
{
    // We don't know enough at thls level to pass in a relevant WebUndoAction; we'd have to
    // change the API to allow this.
    [[self _bridgeForCurrentSelection] setTypingStyle:style withUndoAction:WebUndoActionUnspecified];
}

- (DOMCSSStyleDeclaration *)typingStyle
{
    return [[self _bridgeForCurrentSelection] typingStyle];
}

- (void)setSmartInsertDeleteEnabled:(BOOL)flag
{
    _private->smartInsertDeleteEnabled = flag;
}

- (BOOL)smartInsertDeleteEnabled
{
    return _private->smartInsertDeleteEnabled;
}

- (void)setContinuousSpellCheckingEnabled:(BOOL)flag
{
    _private->continuousSpellCheckingEnabled = flag;
    if ([self isContinuousSpellCheckingEnabled]) {
        [self _preflightSpellChecker];
    } else {
        [[self mainFrame] _unmarkAllMisspellings];
    }
}

- (BOOL)isContinuousSpellCheckingEnabled
{
    return _private->continuousSpellCheckingEnabled && [self _continuousCheckingAllowed];
}

- (int)spellCheckerDocumentTag
{
    if (!_private->hasSpellCheckerDocumentTag) {
        _private->spellCheckerDocumentTag = [NSSpellChecker uniqueSpellDocumentTag];
        _private->hasSpellCheckerDocumentTag = YES;
    }
    return _private->spellCheckerDocumentTag;
}

- (NSUndoManager *)undoManager
{
    NSUndoManager *undoManager = [[self _editingDelegateForwarder] undoManagerForWebView:self];
    if (undoManager) {
        return undoManager;
    }
    return [super undoManager];
}

- (void)registerForEditingDelegateNotification:(NSString *)name selector:(SEL)selector
{
    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
    if ([_private->editingDelegate respondsToSelector:selector])
        [defaultCenter addObserver:_private->editingDelegate selector:selector name:name object:self];
}

- (void)setEditingDelegate:(id)delegate
{
    if (_private->editingDelegate == delegate)
        return;

    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];

    // remove notifications from current delegate
    [defaultCenter removeObserver:_private->editingDelegate name:WebViewDidBeginEditingNotification object:self];
    [defaultCenter removeObserver:_private->editingDelegate name:WebViewDidChangeNotification object:self];
    [defaultCenter removeObserver:_private->editingDelegate name:WebViewDidEndEditingNotification object:self];
    [defaultCenter removeObserver:_private->editingDelegate name:WebViewDidChangeTypingStyleNotification object:self];
    [defaultCenter removeObserver:_private->editingDelegate name:WebViewDidChangeSelectionNotification object:self];
    
    _private->editingDelegate = delegate;
    [_private->editingDelegateForwarder release];
    _private->editingDelegateForwarder = nil;
    
    // add notifications for new delegate
    [self registerForEditingDelegateNotification:WebViewDidBeginEditingNotification selector:@selector(webViewDidBeginEditing:)];
    [self registerForEditingDelegateNotification:WebViewDidChangeNotification selector:@selector(webViewDidChange:)];
    [self registerForEditingDelegateNotification:WebViewDidEndEditingNotification selector:@selector(webViewDidEndEditing:)];
    [self registerForEditingDelegateNotification:WebViewDidChangeTypingStyleNotification selector:@selector(webViewDidChangeTypingStyle:)];
    [self registerForEditingDelegateNotification:WebViewDidChangeSelectionNotification selector:@selector(webViewDidChangeSelection:)];
}

- (id)editingDelegate
{
    return _private->editingDelegate;
}

- (DOMCSSStyleDeclaration *)styleDeclarationWithText:(NSString *)text
{
    // FIXME: Should this really be attached to the document with the current selection?
    DOMCSSStyleDeclaration *decl = [[[self _bridgeForCurrentSelection] DOMDocument] createCSSStyleDeclaration];
    [decl setCssText:text];
    return decl;
}

@end

@implementation WebView (WebViewUndoableEditing)

- (void)replaceSelectionWithNode:(DOMNode *)node
{
    [[self _bridgeForCurrentSelection] replaceSelectionWithNode:node selectReplacement:YES smartReplace:NO];
}    

- (void)replaceSelectionWithText:(NSString *)text
{
    [[self _bridgeForCurrentSelection] replaceSelectionWithText:text selectReplacement:YES smartReplace:NO];
}

- (void)replaceSelectionWithMarkupString:(NSString *)markupString
{
    [[self _bridgeForCurrentSelection] replaceSelectionWithMarkupString:markupString baseURLString:nil selectReplacement:YES smartReplace:NO];
}

- (void)replaceSelectionWithArchive:(WebArchive *)archive
{
    [[[[self _bridgeForCurrentSelection] webFrame] dataSource] _replaceSelectionWithArchive:archive selectReplacement:YES];
}

- (void)deleteSelection
{
    WebBridge *bridge = [self _bridgeForCurrentSelection];
    [bridge deleteSelectionWithSmartDelete:[(WebHTMLView *)[[[bridge webFrame] frameView] documentView] _canSmartCopyOrDelete]];
}
    
- (void)applyStyle:(DOMCSSStyleDeclaration *)style
{
    // We don't know enough at thls level to pass in a relevant WebUndoAction; we'd have to
    // change the API to allow this.
    [[self _bridgeForCurrentSelection] applyStyle:style withUndoAction:WebUndoActionUnspecified];
}

@end

@implementation WebView (WebViewEditingActions)

- (void)_performResponderOperation:(SEL)selector with:(id)parameter
{
    static BOOL reentered = NO;
    if (reentered) {
        [[self nextResponder] tryToPerform:selector with:parameter];
        return;
    }

    // There are two possibilities here.
    //
    // One is that WebView has been called in its role as part of the responder chain.
    // In that case, it's fine to call the first responder and end up calling down the
    // responder chain again. Later we will return here with reentered = YES and continue
    // past the WebView.
    //
    // The other is that we are being called directly, in which case we want to pass the
    // selector down to the view inside us that can handle it, and continue down the
    // responder chain as usual.

    // Pass this selector down to the first responder.
    NSResponder *responder = [self _responderForResponderOperations];
    reentered = YES;
    [responder tryToPerform:selector with:parameter];
    reentered = NO;
}

#define FORWARD(name) \
    - (void)name:(id)sender { [self _performResponderOperation:_cmd with:sender]; }

FOR_EACH_RESPONDER_SELECTOR(FORWARD)

- (void)insertText:(NSString *)text
{
    [self _performResponderOperation:_cmd with:text];
}

@end

@implementation WebView (WebViewEditingInMail)

- (void)_insertNewlineInQuotedContent;
{
    [[self _bridgeForCurrentSelection] insertParagraphSeparatorInQuotedContent];
}

- (BOOL)_selectWordBeforeMenuEvent
{
    return _private->selectWordBeforeMenuEvent;
}

- (void)_setSelectWordBeforeMenuEvent:(BOOL)flag
{
    _private->selectWordBeforeMenuEvent = flag;
}

@end

@implementation WebView (WebFileInternal)

- (void)_preflightSpellCheckerNow:(id)sender
{
    [[NSSpellChecker sharedSpellChecker] _preflightChosenSpellServer];
}

- (void)_preflightSpellChecker
{
    // As AppKit does, we wish to delay tickling the shared spellchecker into existence on application launch.
    if ([NSSpellChecker sharedSpellCheckerExists]) {
        [self _preflightSpellCheckerNow:self];
    } else {
        [self performSelector:@selector(_preflightSpellCheckerNow:) withObject:self afterDelay:2.0];
    }
}

- (BOOL)_continuousCheckingAllowed
{
    static BOOL allowContinuousSpellChecking = YES;
    static BOOL readAllowContinuousSpellCheckingDefault = NO;
    if (!readAllowContinuousSpellCheckingDefault) {
        if ([[NSUserDefaults standardUserDefaults] objectForKey:@"NSAllowContinuousSpellChecking"]) {
            allowContinuousSpellChecking = [[NSUserDefaults standardUserDefaults] boolForKey:@"NSAllowContinuousSpellChecking"];
        }
        readAllowContinuousSpellCheckingDefault = YES;
    }
    return allowContinuousSpellChecking;
}

- (NSResponder *)_responderForResponderOperations
{
    NSResponder *responder = [[self window] firstResponder];
    if (![self _web_firstResponderIsSelfOrDescendantView]) {
        responder = [[[self mainFrame] frameView] documentView];
        if (!responder) {
            responder = [[self mainFrame] frameView];
        }
    }
    return responder;
}

@end
