/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 David Smith (catfish.man@gmail.com)
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

#import "WebViewInternal.h"

#import "DOMRangeInternal.h"
#import "WebBackForwardList.h"
#import "WebBackForwardListInternal.h"
#import "WebBaseNetscapePluginView.h"
#import "WebChromeClient.h"
#import "WebContextMenuClient.h"
#import "WebDOMOperationsPrivate.h"
#import "WebDashboardRegion.h"
#import "WebDataSourceInternal.h"
#import "WebDefaultEditingDelegate.h"
#import "WebDefaultFrameLoadDelegate.h"
#import "WebDefaultPolicyDelegate.h"
#import "WebDefaultResourceLoadDelegate.h"
#import "WebDefaultScriptDebugDelegate.h"
#import "WebDefaultUIDelegate.h"
#import "WebDocument.h"
#import "WebDocumentInternal.h"
#import "WebDownload.h"
#import "WebDownloadInternal.h"
#import "WebDragClient.h"
#import "WebDynamicScrollBarsView.h"
#import "WebEditingDelegate.h"
#import "WebEditorClient.h"
#import "WebFormDelegatePrivate.h"
#import "WebFrameBridge.h"
#import "WebFrameInternal.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLRepresentation.h"
#import "WebHTMLViewInternal.h"
#import "WebHistoryItemInternal.h"
#import "WebIconDatabase.h"
#import "WebInspector.h"
#import "WebKitErrors.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitStatisticsPrivate.h"
#import "WebLocalizableStrings.h"
#import "WebNSDataExtras.h"
#import "WebNSDataExtrasPrivate.h"
#import "WebNSDictionaryExtras.h"
#import "WebNSEventExtras.h"
#import "WebNSObjectExtras.h"
#import "WebNSPasteboardExtras.h"
#import "WebNSPrintOperationExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebNSUserDefaultsExtras.h"
#import "WebNSViewExtras.h"
#import "WebPasteboardHelper.h"
#import "WebPDFView.h"
#import "WebPluginDatabase.h"
#import "WebPolicyDelegate.h"
#import "WebPreferenceKeysPrivate.h"
#import "WebPreferencesPrivate.h"
#import "WebResourceLoadDelegate.h"
#import "WebScriptDebugDelegatePrivate.h"
#import "WebScriptDebugServerPrivate.h"
#import "WebUIDelegate.h"
#import "WebUIDelegatePrivate.h"
#import <CoreFoundation/CFSet.h>
#import <Foundation/NSURLConnection.h>
#import <JavaScriptCore/Assertions.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/DragController.h>
#import <WebCore/DragData.h>
#import <WebCore/Editor.h>
#import <WebCore/ExceptionHandlers.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameMac.h>
#import <WebCore/FrameTree.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/Logging.h>
#import <WebCore/Page.h>
#import <WebCore/PlatformMouseEvent.h>
#import <WebCore/ProgressTracker.h>
#import <WebCore/SelectionController.h>
#import <WebCore/Settings.h>
#import <WebCore/TextResourceDecoder.h>
#import <WebCore/WebCoreFrameBridge.h>
#import <WebCore/WebCoreTextRenderer.h>
#import <WebCore/WebCoreView.h>
#import <WebKit/DOM.h>
#import <WebKit/DOMExtensions.h>
#import <WebKit/DOMPrivate.h>
#import <WebKitSystemInterface.h>
#import <objc/objc-runtime.h>
#import <wtf/RefPtr.h>
#import <wtf/HashTraits.h>

using namespace WebCore;

#if defined(__ppc__) || defined(__ppc64__)
#define PROCESSOR "PPC"
#elif defined(__i386__) || defined(__x86_64__)
#define PROCESSOR "Intel"
#else
#error Unknown architecture
#endif

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
macro(moveToBeginningOfSentence) \
macro(moveToBeginningOfSentenceAndModifySelection) \
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
macro(moveToEndOfSentence) \
macro(moveToEndOfSentenceAndModifySelection) \
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
macro(outdent) \
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
macro(scrollToBeginningOfDocument) \
macro(scrollToEndOfDocument) \
macro(selectAll) \
macro(selectWord) \
macro(selectSentence) \
macro(selectLine) \
macro(selectParagraph) \
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

@interface WebViewPrivate : NSObject
{
@public
    Page* page;
    
    id UIDelegate;
    id UIDelegateForwarder;
    id resourceProgressDelegate;
    id resourceProgressDelegateForwarder;
    id downloadDelegate;
    id policyDelegate;
    id policyDelegateForwarder;
    id frameLoadDelegate;
    id frameLoadDelegateForwarder;
    id <WebFormDelegate> formDelegate;
    id editingDelegate;
    id editingDelegateForwarder;
    id scriptDebugDelegate;
    id scriptDebugDelegateForwarder;
    
    BOOL useBackForwardList;
    BOOL allowsUndo;
        
    float textSizeMultiplier;

    NSString *applicationNameForUserAgent;
    String* userAgent;
    BOOL userAgentOverridden;
    
    WebPreferences *preferences;
        
    BOOL lastElementWasNonNil;

    NSWindow *hostWindow;

    int programmaticFocusCount;
    
    WebResourceDelegateImplementationCache resourceLoadDelegateImplementations;

    void *observationInfo;
    
    BOOL closed;
    BOOL shouldCloseWithWindow;
    BOOL mainFrameDocumentReady;
    BOOL drawsBackground;
    BOOL editable;
    BOOL tabKeyCyclesThroughElementsChanged;
    BOOL becomingFirstResponder;
    BOOL hoverFeedbackSuspended;

    NSColor *backgroundColor;

    NSString *mediaStyle;
    
    BOOL hasSpellCheckerDocumentTag;
    WebNSInteger spellCheckerDocumentTag;

    BOOL smartInsertDeleteEnabled;
        
    BOOL dashboardBehaviorAlwaysSendMouseEventsToAllWindows;
    BOOL dashboardBehaviorAlwaysSendActiveNullEventsToPlugIns;
    BOOL dashboardBehaviorAlwaysAcceptsFirstMouse;
    BOOL dashboardBehaviorAllowWheelScrolling;
    
    BOOL selectWordBeforeMenuEvent;
    
    WebPluginDatabase *pluginDatabase;
    
    HashMap<unsigned long, RetainPtr<id> >* identifierMap;
}
@end

@interface WebView (WebFileInternal)
- (WebFrame *)_selectedOrMainFrame;
- (WebFrameBridge *)_bridgeForSelectedOrMainFrame;
- (BOOL)_isLoading;
- (WebFrameView *)_frameViewAtWindowPoint:(NSPoint)point;
- (WebFrameBridge *)_bridgeAtPoint:(NSPoint)point;
- (WebFrame *)_focusedFrame;
+ (void)_preflightSpellChecker;
- (BOOL)_continuousCheckingAllowed;
- (NSResponder *)_responderForResponderOperations;
- (BOOL)_performTextSizingSelector:(SEL)sel withObject:(id)arg onTrackingDocs:(BOOL)doTrackingViews selForNonTrackingDocs:(SEL)testSel newScaleFactor:(float)newScaleFactor;
- (void)_notifyTextSizeMultiplierChanged;
@end

NSString *WebElementDOMNodeKey =            @"WebElementDOMNode";
NSString *WebElementFrameKey =              @"WebElementFrame";
NSString *WebElementImageKey =              @"WebElementImage";
NSString *WebElementImageAltStringKey =     @"WebElementImageAltString";
NSString *WebElementImageRectKey =          @"WebElementImageRect";
NSString *WebElementImageURLKey =           @"WebElementImageURL";
NSString *WebElementIsSelectedKey =         @"WebElementIsSelected";
NSString *WebElementLinkLabelKey =          @"WebElementLinkLabel";
NSString *WebElementLinkTargetFrameKey =    @"WebElementTargetFrame";
NSString *WebElementLinkTitleKey =          @"WebElementLinkTitle";
NSString *WebElementLinkURLKey =            @"WebElementLinkURL";
NSString *WebElementSpellingToolTipKey =    @"WebElementSpellingToolTip";
NSString *WebElementTitleKey =              @"WebElementTitle";
NSString *WebElementLinkIsLiveKey =         @"WebElementLinkIsLive";
NSString *WebElementIsContentEditableKey =  @"WebElementIsContentEditableKey";

NSString *WebViewProgressStartedNotification =          @"WebProgressStartedNotification";
NSString *WebViewProgressEstimateChangedNotification =  @"WebProgressEstimateChangedNotification";
NSString *WebViewProgressFinishedNotification =         @"WebProgressFinishedNotification";

NSString * const WebViewDidBeginEditingNotification =         @"WebViewDidBeginEditingNotification";
NSString * const WebViewDidChangeNotification =               @"WebViewDidChangeNotification";
NSString * const WebViewDidEndEditingNotification =           @"WebViewDidEndEditingNotification";
NSString * const WebViewDidChangeTypingStyleNotification =    @"WebViewDidChangeTypingStyleNotification";
NSString * const WebViewDidChangeSelectionNotification =      @"WebViewDidChangeSelectionNotification";

enum { WebViewVersion = 3 };

#define timedLayoutSize 4096

static NSMutableSet *schemesWithRepresentationsSet;

NSString *_WebCanGoBackKey =            @"canGoBack";
NSString *_WebCanGoForwardKey =         @"canGoForward";
NSString *_WebEstimatedProgressKey =    @"estimatedProgress";
NSString *_WebIsLoadingKey =            @"isLoading";
NSString *_WebMainFrameIconKey =        @"mainFrameIcon";
NSString *_WebMainFrameTitleKey =       @"mainFrameTitle";
NSString *_WebMainFrameURLKey =         @"mainFrameURL";
NSString *_WebMainFrameDocumentKey =    @"mainFrameDocument";

@interface WebProgressItem : NSObject
{
@public
    long long bytesReceived;
    long long estimatedLength;
}
@end

@implementation WebProgressItem
@end

static BOOL continuousSpellCheckingEnabled;
#ifndef BUILDING_ON_TIGER
static BOOL grammarCheckingEnabled;
#endif

@implementation WebViewPrivate

- init 
{
    self = [super init];
    if (!self)
        return nil;
    
    allowsUndo = YES;
    textSizeMultiplier = 1;
    dashboardBehaviorAllowWheelScrolling = YES;
    shouldCloseWithWindow = objc_collecting_enabled();
    continuousSpellCheckingEnabled = [[NSUserDefaults standardUserDefaults] boolForKey:WebContinuousSpellCheckingEnabled];
#ifndef BUILDING_ON_TIGER
    grammarCheckingEnabled = [[NSUserDefaults standardUserDefaults] boolForKey:WebGrammarCheckingEnabled];
#endif
    userAgent = new String;
    
    identifierMap = new HashMap<unsigned long, RetainPtr<id> >();

    return self;
}

- (void)dealloc
{
    ASSERT(!page);

    delete userAgent;
    delete identifierMap;
    
    [applicationNameForUserAgent release];
    [backgroundColor release];
    
    [preferences release];
    [hostWindow release];
    
    [policyDelegateForwarder release];
    [resourceProgressDelegateForwarder release];
    [UIDelegateForwarder release];
    [frameLoadDelegateForwarder release];
    [editingDelegateForwarder release];
    [scriptDebugDelegateForwarder release];
    
    [mediaStyle release];
    
    [super dealloc];
}

- (void)finalize
{
    delete userAgent;
    [super finalize];
}

@end

@implementation WebView (AllWebViews)

static CFSetCallBacks NonRetainingSetCallbacks = {
    0,
    NULL,
    NULL,
    CFCopyDescription,
    CFEqual,
    CFHash
};

static CFMutableSetRef allWebViewsSet;

+ (void)_makeAllWebViewsPerformSelector:(SEL)selector
{
    if (!allWebViewsSet)
        return;

    [(NSMutableSet *)allWebViewsSet makeObjectsPerformSelector:selector];
}

- (void)_removeFromAllWebViewsSet
{
    if (allWebViewsSet)
        CFSetRemoveValue(allWebViewsSet, self);
}

- (void)_addToAllWebViewsSet
{
    if (!allWebViewsSet)
        allWebViewsSet = CFSetCreateMutable(NULL, 0, &NonRetainingSetCallbacks);

    CFSetSetValue(allWebViewsSet, self);
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

+ (BOOL)_developerExtrasEnabled
{
#ifdef NDEBUG
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    BOOL enableDebugger = [defaults boolForKey:@"WebKitDeveloperExtras"];
    if (!enableDebugger)
        enableDebugger = [defaults boolForKey:@"IncludeDebugMenu"];
    return enableDebugger;
#else
    return YES; // always enable in debug builds
#endif
}

+ (BOOL)_scriptDebuggerEnabled
{
#ifdef NDEBUG
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitScriptDebuggerEnabled"];
#else
    return YES; // always enable in debug builds
#endif
}

+ (NSArray *)_supportedMIMETypes
{
    // Load the plug-in DB allowing plug-ins to install types.
    [WebPluginDatabase sharedDatabase];
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
    Class viewClass = [[WebFrameView _viewTypesAllowImageTypeOmission:YES] _webkit_objectForMIMEType:MIMEType];
    Class repClass = [[WebDataSource _repTypesAllowImageTypeOmission:YES] _webkit_objectForMIMEType:MIMEType];
    
    if (!viewClass || !repClass || [[WebPDFView supportedMIMETypes] containsObject:MIMEType]) {
        // Our optimization to avoid loading the plug-in DB and image types for the HTML case failed.
        // Load the plug-in DB allowing plug-ins to install types.
        [WebPluginDatabase sharedDatabase];
            
        // Load the image types and get the view class and rep class. This should be the fullest picture of all handled types.
        viewClass = [[WebFrameView _viewTypesAllowImageTypeOmission:NO] _webkit_objectForMIMEType:MIMEType];
        repClass = [[WebDataSource _repTypesAllowImageTypeOmission:NO] _webkit_objectForMIMEType:MIMEType];
    }
    
    if (viewClass && repClass) {
        // Special-case WebHTMLView for text types that shouldn't be shown.
        if (viewClass == [WebHTMLView class] &&
            repClass == [WebHTMLRepresentation class] &&
            [[WebHTMLView unsupportedTextMIMETypes] containsObject:MIMEType]) {
            return NO;
        }
        if (vClass)
            *vClass = viewClass;
        if (rClass)
            *rClass = repClass;
        return YES;
    }
    
    return NO;
}

- (BOOL)_viewClass:(Class *)vClass andRepresentationClass:(Class *)rClass forMIMEType:(NSString *)MIMEType;
{
    if ([[self class] _viewClass:vClass andRepresentationClass:rClass forMIMEType:MIMEType])
        return YES;

    if (_private->pluginDatabase) {
        WebBasePluginPackage *pluginPackage = [_private->pluginDatabase pluginForMIMEType:MIMEType];
        if (pluginPackage) {
            if (vClass)
                *vClass = [WebHTMLView class];
            if (rClass)
                *rClass = [WebHTMLRepresentation class];
            return YES;
        }
    }
    
    return NO;
}

+ (void)_setAlwaysUseATSU:(BOOL)f
{
    WebCoreSetAlwaysUseATSU(f);
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
    if (!_private || _private->closed)
        return;
    _private->closed = YES;

    [self _removeFromAllWebViewsSet];

    [self setGroupName:nil];
    [self setHostWindow:nil];
    [self setDownloadDelegate:nil];
    [self setEditingDelegate:nil];
    [self setFrameLoadDelegate:nil];
    [self setPolicyDelegate:nil];
    [self setResourceLoadDelegate:nil];
    [self setScriptDebugDelegate:nil];
    [self setUIDelegate:nil];

    // To avoid leaks, call removeDragCaret in case it wasn't called after moveDragCaretToPoint.
    [self removeDragCaret];

    FrameLoader* mainFrameLoader = [[self mainFrame] _frameLoader];
    if (mainFrameLoader)
        mainFrameLoader->detachFromParent();
    
    // Deleteing the WebCore::Page will clear the page cache so we call destroy on 
    // all the plug-ins in the page cache to break any retain cycles.
    // See comment in HistoryItem::releaseAllPendingPageCaches() for more information.
    delete _private->page;
    _private->page = 0;

    if (_private->hasSpellCheckerDocumentTag) {
        [[NSSpellChecker sharedSpellChecker] closeSpellDocumentWithTag:_private->spellCheckerDocumentTag];
        _private->hasSpellCheckerDocumentTag = NO;
    }

    if (_private->pluginDatabase) {
        [_private->pluginDatabase close];
        [_private->pluginDatabase release];
        _private->pluginDatabase = nil;
    }

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [WebPreferences _removeReferenceForIdentifier: [self preferencesIdentifier]];
}

+ (NSString *)_MIMETypeForFile:(NSString *)path
{
    NSString *extension = [path pathExtension];
    NSString *MIMEType = nil;

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
    ASSERT(URL);
    
    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:URL];
    [WebDownload _downloadWithRequest:request
                             delegate:_private->downloadDelegate
                            directory:nil];
    [request release];
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

- (WebCore::Page*)page
{
    return _private->page;
}

- (NSMenu *)_menuForElement:(NSDictionary *)element defaultItems:(NSArray *)items
{
    NSArray *defaultMenuItems = [[WebDefaultUIDelegate sharedUIDelegate]
          webView:self contextMenuItemsForElement:element defaultMenuItems:items];
    NSArray *menuItems = defaultMenuItems;
    NSMenu *menu = nil;
    unsigned i;

    if ([_private->UIDelegate respondsToSelector:@selector(webView:contextMenuItemsForElement:defaultMenuItems:)])
        menuItems = [_private->UIDelegate webView:self contextMenuItemsForElement:element defaultMenuItems:defaultMenuItems];

    if (menuItems && [menuItems count] > 0) {
        menu = [[[NSMenu alloc] init] autorelease];

        for (i=0; i<[menuItems count]; i++) {
            [menu addItem:[menuItems objectAtIndex:i]];
        }
    }

    return menu;
}

- (void)_mouseDidMoveOverElement:(NSDictionary *)dictionary modifierFlags:(WebNSUInteger)modifierFlags
{
    // When the mouse isn't over this view at all, we'll get called with a dictionary of nil over
    // and over again. So it's a good idea to catch that here and not send multiple calls to the delegate
    // for that case.
    
    if (dictionary && _private->lastElementWasNonNil) {
        [[self _UIDelegateForwarder] webView:self mouseDidMoveOverElement:dictionary modifierFlags:modifierFlags];
    }
    _private->lastElementWasNonNil = dictionary != nil;
}



- (void)_loadBackForwardListFromOtherView:(WebView *)otherView
{
    // It turns out the right combination of behavior is done with the back/forward load
    // type.  (See behavior matrix at the top of WebFramePrivate.)  So we copy all the items
    // in the back forward list, and go to the current one.

    BackForwardList* backForwardList = _private->page->backForwardList();
    ASSERT(!backForwardList->currentItem()); // destination list should be empty

    BackForwardList* otherBackForwardList = otherView->_private->page->backForwardList();
    if (!otherBackForwardList->currentItem())
        return; // empty back forward list, bail
    
    HistoryItem* newItemToGoTo = 0;

    int lastItemIndex = otherBackForwardList->forwardListCount();
    for (int i = -otherBackForwardList->backListCount(); i <= lastItemIndex; ++i) {
        if (i == 0) {
            // If this item is showing , save away its current scroll and form state,
            // since that might have changed since loading and it is normally not saved
            // until we leave that page.
            otherView->_private->page->mainFrame()->loader()->saveDocumentAndScrollState();
        }
        RefPtr<HistoryItem> newItem = otherBackForwardList->itemAtIndex(i)->copy();
        if (i == 0) 
            newItemToGoTo = newItem.get();
        backForwardList->addItem(newItem.release());
    }
    
    ASSERT(newItemToGoTo);
    _private->page->goToItem(newItemToGoTo, FrameLoadTypeIndexedBackForward);
}

- (void)_setFormDelegate: (id<WebFormDelegate>)delegate
{
    _private->formDelegate = delegate;
}

- (id<WebFormDelegate>)_formDelegate
{
    return _private->formDelegate;
}

- (void)_updateWebCoreSettingsFromPreferences:(WebPreferences *)preferences
{
    Settings* settings = _private->page->settings();
    
    settings->setCursiveFontFamily([preferences cursiveFontFamily]);
    settings->setDefaultFixedFontSize([preferences defaultFixedFontSize]);
    settings->setDefaultFontSize([preferences defaultFontSize]);
    settings->setDefaultTextEncodingName([preferences defaultTextEncodingName]);
    settings->setFantasyFontFamily([preferences fantasyFontFamily]);
    settings->setFixedFontFamily([preferences fixedFontFamily]);
    settings->setJavaEnabled([preferences isJavaEnabled]);
    settings->setJavaScriptEnabled([preferences isJavaScriptEnabled]);
    settings->setJavaScriptCanOpenWindowsAutomatically([preferences javaScriptCanOpenWindowsAutomatically]);
    settings->setMinimumFontSize([preferences minimumFontSize]);
    settings->setMinimumLogicalFontSize([preferences minimumLogicalFontSize]);
    settings->setPluginsEnabled([preferences arePlugInsEnabled]);
    settings->setPrivateBrowsingEnabled([preferences privateBrowsingEnabled]);
    settings->setSansSerifFontFamily([preferences sansSerifFontFamily]);
    settings->setSerifFontFamily([preferences serifFontFamily]);
    settings->setStandardFontFamily([preferences standardFontFamily]);
    settings->setLoadsImagesAutomatically([preferences loadsImagesAutomatically]);
    settings->setShouldPrintBackgrounds([preferences shouldPrintBackgrounds]);
    settings->setTextAreasAreResizable([preferences textAreasAreResizable]);
    settings->setEditableLinkBehavior(core([preferences editableLinkBehavior]));
    if ([preferences userStyleSheetEnabled]) {
        NSString* location = [[preferences userStyleSheetLocation] _web_originalDataAsString];
        settings->setUserStyleSheetLocation([NSURL URLWithString:(location ? location : @"")]);
    } else
        settings->setUserStyleSheetLocation([NSURL URLWithString:@""]);
}

- (void)_preferencesChangedNotification: (NSNotification *)notification
{
    WebPreferences *preferences = (WebPreferences *)[notification object];
    
    ASSERT(preferences == [self preferences]);
    if (!_private->userAgentOverridden)
        *_private->userAgent = String();
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
    Class delegateClass = [delegate class];

    cache->delegateImplementsDidCancelAuthenticationChallenge = [delegate respondsToSelector:@selector(webView:resource:didCancelAuthenticationChallenge:fromDataSource:)];
    cache->delegateImplementsDidReceiveAuthenticationChallenge = [delegate respondsToSelector:@selector(webView:resource:didReceiveAuthenticationChallenge:fromDataSource:)];
    cache->delegateImplementsDidFinishLoadingFromDataSource = [delegate respondsToSelector:@selector(webView:resource:didFinishLoadingFromDataSource:)];
    cache->delegateImplementsDidReceiveContentLength = [delegate respondsToSelector:@selector(webView:resource:didReceiveContentLength:fromDataSource:)];
    cache->delegateImplementsDidReceiveResponse = [delegate respondsToSelector:@selector(webView:resource:didReceiveResponse:fromDataSource:)];
    cache->delegateImplementsWillSendRequest = [delegate respondsToSelector:@selector(webView:resource:willSendRequest:redirectResponse:fromDataSource:)];
    cache->delegateImplementsIdentifierForRequest = [delegate respondsToSelector:@selector(webView:identifierForInitialRequest:fromDataSource:)];
    cache->delegateImplementsDidLoadResourceFromMemoryCache = [delegate respondsToSelector:@selector(webView:didLoadResourceFromMemoryCache:response:length:fromDataSource:)];

#if defined(OBJC_API_VERSION) && OBJC_API_VERSION > 0
#define GET_OBJC_METHOD_IMP(class,selector) class_getMethodImplementation(class, selector)
#else
#define GET_OBJC_METHOD_IMP(class,selector) class_getInstanceMethod(class, selector)->method_imp
#endif

    if (cache->delegateImplementsDidCancelAuthenticationChallenge)
        cache->didCancelAuthenticationChallengeFunc = (WebDidCancelAuthenticationChallengeFunc)GET_OBJC_METHOD_IMP(delegateClass, @selector(webView:resource:didReceiveAuthenticationChallenge:fromDataSource:));
    if (cache->delegateImplementsDidReceiveAuthenticationChallenge)
        cache->didReceiveAuthenticationChallengeFunc = (WebDidReceiveAuthenticationChallengeFunc)GET_OBJC_METHOD_IMP(delegateClass, @selector(webView:resource:didReceiveAuthenticationChallenge:fromDataSource:));
    if (cache->delegateImplementsDidFinishLoadingFromDataSource)
        cache->didFinishLoadingFromDataSourceFunc = (WebDidFinishLoadingFromDataSourceFunc)GET_OBJC_METHOD_IMP(delegateClass, @selector(webView:resource:didFinishLoadingFromDataSource:));
    if (cache->delegateImplementsDidReceiveContentLength)
        cache->didReceiveContentLengthFunc = (WebDidReceiveContentLengthFunc)GET_OBJC_METHOD_IMP(delegateClass, @selector(webView:resource:didReceiveContentLength:fromDataSource:));
    if (cache->delegateImplementsDidReceiveResponse)
        cache->didReceiveResponseFunc = (WebDidReceiveResponseFunc)GET_OBJC_METHOD_IMP(delegateClass, @selector(webView:resource:didReceiveResponse:fromDataSource:));
    if (cache->delegateImplementsWillSendRequest)
        cache->willSendRequestFunc = (WebWillSendRequestFunc)GET_OBJC_METHOD_IMP(delegateClass, @selector(webView:resource:willSendRequest:redirectResponse:fromDataSource:));
    if (cache->delegateImplementsIdentifierForRequest)
        cache->identifierForRequestFunc = (WebIdentifierForRequestFunc)GET_OBJC_METHOD_IMP(delegateClass, @selector(webView:identifierForInitialRequest:fromDataSource:));
    if (cache->delegateImplementsDidLoadResourceFromMemoryCache)
        cache->didLoadResourceFromMemoryCacheFunc = (WebDidLoadResourceFromMemoryCacheFunc)GET_OBJC_METHOD_IMP(delegateClass, @selector(webView:didLoadResourceFromMemoryCache:response:length:fromDataSource:));

#undef GET_OBJC_METHOD_IMP
}

id WebViewGetResourceLoadDelegate(WebView *webView)
{
    return webView->_private->resourceProgressDelegate;
}

WebResourceDelegateImplementationCache WebViewGetResourceLoadDelegateImplementations(WebView *webView)
{
    return webView->_private->resourceLoadDelegateImplementations;
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

- _scriptDebugDelegateForwarder
{
    if (!_private->scriptDebugDelegateForwarder)
        _private->scriptDebugDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget: [self scriptDebugDelegate] defaultTarget: [WebDefaultScriptDebugDelegate sharedScriptDebugDelegate] templateClass: [WebDefaultScriptDebugDelegate class]];
    return _private->scriptDebugDelegateForwarder;
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

    return [self _representationExistsForURLScheme:[[request URL] scheme]];
}

+ (NSString *)_decodeData:(NSData *)data
{
    HTMLNames::init(); // this method is used for importing bookmarks at startup, so HTMLNames are likely to be uninitialized yet
    RefPtr<TextResourceDecoder> decoder = new TextResourceDecoder("text/html"); // bookmark files are HTML
    String result = decoder->decode(static_cast<const char*>([data bytes]), [data length]);
    result += decoder->flush();
    return result;
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

// Required to prevent automatic observer notifications.
+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key {
    return NO;
}

- (NSArray *)_declaredKeys {
    static NSArray *declaredKeys = nil;
    
    if (!declaredKeys) {
        declaredKeys = [[NSArray alloc] initWithObjects:_WebMainFrameURLKey, _WebIsLoadingKey, _WebEstimatedProgressKey, _WebCanGoBackKey, _WebCanGoForwardKey, _WebMainFrameTitleKey, _WebMainFrameIconKey, _WebMainFrameDocumentKey, nil];
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
    [request _web_setHTTPUserAgent:[self _userAgent]];
    NSCachedURLResponse *cachedResponse = [[NSURLCache sharedURLCache] cachedResponseForRequest:request];
    [request release];
    return cachedResponse;
}

- (void)_writeImageForElement:(NSDictionary *)element withPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    NSURL *linkURL = [element objectForKey:WebElementLinkURLKey];
    DOMElement *domElement = [element objectForKey:WebElementDOMNodeKey];
    [pasteboard _web_writeImage:(NSImage *)(domElement ? nil : [element objectForKey:WebElementImageKey])
                        element:domElement
                            URL:linkURL ? linkURL : (NSURL *)[element objectForKey:WebElementImageURLKey]
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
    [self page]->dragController()->setDidInitiateDrag(initiatedDrag);
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
    FrameMac* mainFrame = [[[self mainFrame] _bridge] _frame];
    if (!mainFrame)
        return nil;
    NSMutableDictionary *regions = mainFrame->dashboardRegionsDictionary();
    [self _addScrollerDashboardRegions:regions];
    return regions;
}

- (void)_setDashboardBehavior:(WebDashboardBehavior)behavior to:(BOOL)flag
{
    // FIXME: Remove this blanket assignment once Dashboard and Dashcode implement 
    // specific support for the backward compatibility mode flag.
    if (behavior == WebDashboardBehaviorAllowWheelScrolling && flag == NO)
        _private->page->settings()->setUsesDashboardBackwardCompatibilityMode(true);
    
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
        case WebDashboardBehaviorUseBackwardCompatibilityMode: {
            _private->page->settings()->setUsesDashboardBackwardCompatibilityMode(flag);
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
        case WebDashboardBehaviorUseBackwardCompatibilityMode: {
            return _private->page->settings()->usesDashboardBackwardCompatibilityMode();
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
    WebCoreSetShouldUseFontSmoothing(f);
}

+ (BOOL)_shouldUseFontSmoothing
{
    return WebCoreShouldUseFontSmoothing();
}

+ (NSString *)_minimumRequiredSafariBuildNumber
{
    return @"420+";
}

- (void)setAlwaysShowVerticalScroller:(BOOL)flag
{
    WebDynamicScrollBarsView *scrollview = (WebDynamicScrollBarsView *)[[[self mainFrame] frameView] _scrollView];
    if (flag) {
        [scrollview setVerticalScrollingMode:WebCoreScrollbarAlwaysOn];
        [scrollview setVerticalScrollingModeLocked:YES];
    } else {
        [scrollview setVerticalScrollingModeLocked:NO];
        [scrollview setVerticalScrollingMode:WebCoreScrollbarAuto];
    }
}

- (BOOL)alwaysShowVerticalScroller
{
    WebDynamicScrollBarsView *scrollview = (WebDynamicScrollBarsView *)[[[self mainFrame] frameView] _scrollView];
    return [scrollview verticalScrollingModeLocked] && [scrollview verticalScrollingMode] == WebCoreScrollbarAlwaysOn;
}

- (void)setAlwaysShowHorizontalScroller:(BOOL)flag
{
    WebDynamicScrollBarsView *scrollview = (WebDynamicScrollBarsView *)[[[self mainFrame] frameView] _scrollView];
    if (flag) {
        [scrollview setHorizontalScrollingMode:WebCoreScrollbarAlwaysOn];
        [scrollview setHorizontalScrollingModeLocked:YES];
    } else {
        [scrollview setHorizontalScrollingModeLocked:NO];
        [scrollview setHorizontalScrollingMode:WebCoreScrollbarAuto];
    }
}

- (void)setProhibitsMainFrameScrolling:(BOOL)prohibits
{
    FrameMac* mainFrame = [[[self mainFrame] _bridge] _frame];
    if (mainFrame)
        mainFrame->setProhibitsScrolling(prohibits);
}

- (BOOL)alwaysShowHorizontalScroller
{
    WebDynamicScrollBarsView *scrollview = (WebDynamicScrollBarsView *)[[[self mainFrame] frameView] _scrollView];
    return [scrollview horizontalScrollingModeLocked] && [scrollview horizontalScrollingMode] == WebCoreScrollbarAlwaysOn;
}

- (void)_setInViewSourceMode:(BOOL)flag
{
    FrameMac* mainFrame = [[[self mainFrame] _bridge] _frame];
    if (mainFrame)
        mainFrame->setInViewSourceMode(flag);
}

- (BOOL)_inViewSourceMode
{
    FrameMac* mainFrame = [[[self mainFrame] _bridge] _frame];
    return mainFrame && mainFrame->inViewSourceMode();
}

- (void)_setAdditionalWebPlugInPaths:(NSArray *)newPaths
{
    if (!_private->pluginDatabase)
        _private->pluginDatabase = [[WebPluginDatabase alloc] init];
        
    [_private->pluginDatabase setPlugInPaths:newPaths];
    [_private->pluginDatabase refresh];
}

- (void)_attachScriptDebuggerToAllFrames
{
    for (Frame* frame = core([self mainFrame]); frame; frame = frame->tree()->traverseNext())
        [kit(frame) _attachScriptDebugger];
}

- (void)_detachScriptDebuggerFromAllFrames
{
    for (Frame* frame = core([self mainFrame]); frame; frame = frame->tree()->traverseNext())
        [kit(frame) _detachScriptDebugger];
}

- (void)setBackgroundColor:(NSColor *)backgroundColor
{
    if ([_private->backgroundColor isEqual:backgroundColor])
        return;

    id old = _private->backgroundColor;
    _private->backgroundColor = [backgroundColor retain];
    [old release];

    [[self mainFrame] _updateBackground];
}

- (NSColor *)backgroundColor
{
    return _private->backgroundColor;
}

- (BOOL)defersCallbacks
{
    if (!_private->page)
        return NO;
    return _private->page->defersLoading();
}

- (void)setDefersCallbacks:(BOOL)defer
{
    if (!_private->page)
        return;
    return _private->page->setDefersLoading(defer);
}

@end

@implementation _WebSafeForwarder

- initWithTarget: t defaultTarget: dt templateClass: (Class)aClass
{
    self = [super init];
    if (!self)
        return nil;
    
    target = t; // Non retained.
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

#ifdef REMOVE_SAFARI_DOM_TREE_DEBUG_ITEM
// this prevents open source users from crashing when using the Show DOM Tree menu item in Safari
// FIXME: remove this when it is no longer needed to prevent Safari from crashing
+(void)initialize
{
    static BOOL tooLate = NO;
    if (!tooLate) {
        if ([[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.Safari"] && [[NSUserDefaults standardUserDefaults] boolForKey:@"IncludeDebugMenu"])
            [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_finishedLaunching) name:NSApplicationDidFinishLaunchingNotification object:NSApp];
        tooLate = YES;
    }
}

+(void)_finishedLaunching
{
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_removeDOMTreeMenuItem:) name:NSMenuDidAddItemNotification object:[NSApp mainMenu]];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationDidFinishLaunchingNotification object:NSApp];
}

+(void)_removeDOMTreeMenuItem:(NSNotification *)notification
{
    NSMenu *debugMenu = [[[[NSApp mainMenu] itemArray] lastObject] submenu];
    NSMenuItem *domTree = [debugMenu itemWithTitle:@"Show DOM Tree"];
    if (domTree)
        [debugMenu removeItem:domTree];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSMenuDidAddItemNotification object:[NSApp mainMenu]];
}
#endif

#ifdef DISABLE_EDITABLE_LINKS_IN_MAIL
+(void)initialize
{
    static BOOL tooLate = NO;
    if (!tooLate) {
        if ([[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.mail"] && [[WebPreferences standardPreferences] editableLinkBehavior] == WebKitEditableLinkDefaultBehavior)
            [[WebPreferences standardPreferences] setEditableLinkBehavior:WebKitEditableLinkOnlyLiveWithShiftKey];
        tooLate = YES;
    }
}
#endif

+ (BOOL)canShowMIMEType:(NSString *)MIMEType
{
    return [self _viewClass:nil andRepresentationClass:nil forMIMEType:MIMEType];
}

- (WebBasePluginPackage *)_pluginForMIMEType:(NSString *)MIMEType
{
    WebBasePluginPackage *pluginPackage = [[WebPluginDatabase sharedDatabase] pluginForMIMEType:MIMEType];
    if (pluginPackage)
        return pluginPackage;
    
    if (_private->pluginDatabase)
        return [_private->pluginDatabase pluginForMIMEType:MIMEType];
    
    return nil;
}

- (WebBasePluginPackage *)_pluginForExtension:(NSString *)extension
{
    WebBasePluginPackage *pluginPackage = [[WebPluginDatabase sharedDatabase] pluginForExtension:extension];
    if (pluginPackage)
        return pluginPackage;
    
    if (_private->pluginDatabase)
        return [_private->pluginDatabase pluginForExtension:extension];
    
    return nil;
}

- (BOOL)_isMIMETypeRegisteredAsPlugin:(NSString *)MIMEType
{
    if ([[WebPluginDatabase sharedDatabase] isMIMETypeRegistered:MIMEType])
        return YES;
        
    if (_private->pluginDatabase && [_private->pluginDatabase isMIMETypeRegistered:MIMEType])
        return YES;
    
    return NO;
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
        if ([viewTypes objectForKey:key] == [WebHTMLView class])
            [array addObject:key];
    }
    
    return array;
}

+ (void)setMIMETypesShownAsHTML:(NSArray *)MIMETypes
{
    NSMutableDictionary *viewTypes = [WebFrameView _viewTypesAllowImageTypeOmission:YES];
    NSEnumerator *enumerator = [viewTypes keyEnumerator];
    id key;
    while ((key = [enumerator nextObject])) {
        if ([viewTypes objectForKey:key] == [WebHTMLView class])
            [WebView _unregisterViewClassAndRepresentationClassForMIMEType:key];
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

- (void)_commonInitializationWithFrameName:(NSString *)frameName groupName:(NSString *)groupName
{
    _private->mainFrameDocumentReady = NO;
    _private->drawsBackground = YES;
    _private->smartInsertDeleteEnabled = YES;
    _private->backgroundColor = [[NSColor whiteColor] retain];

    NSRect f = [self frame];
    WebFrameView *frameView = [[WebFrameView alloc] initWithFrame: NSMakeRect(0,0,f.size.width,f.size.height)];
    [frameView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self addSubview:frameView];
    [frameView release];

    WebKitInitializeLoggingChannelsIfNecessary();
    WebCore::InitializeLoggingChannelsIfNecessary();
    [WebBackForwardList setDefaultPageCacheSizeIfNecessary];
    [WebHistoryItem initWindowWatcherIfNecessary];

    _private->page = new Page(new WebChromeClient(self), new WebContextMenuClient(self), new WebEditorClient(self), new WebDragClient(self));
    [[[WebFrameBridge alloc] initMainFrameWithPage:_private->page frameName:frameName frameView:frameView] release];

    [self _addToAllWebViewsSet];
    [self setGroupName:groupName];
    
    // If there's already a next key view (e.g., from a nib), wire it up to our
    // contained frame view. In any case, wire our next key view up to the our
    // contained frame view. This works together with our becomeFirstResponder 
    // and setNextKeyView overrides.
    NSView *nextKeyView = [self nextKeyView];
    if (nextKeyView != nil && nextKeyView != frameView) {
        [frameView setNextKeyView:nextKeyView];
    }
    [super setNextKeyView:frameView];

    ++WebViewCount;

    [self _registerDraggedTypes];

    // initialize WebScriptDebugServer here so listeners can register before any pages are loaded.
    if ([WebView _scriptDebuggerEnabled])
        [WebScriptDebugServer sharedScriptDebugServer];

    // Update WebCore with preferences.  These values will either come from an archived WebPreferences,
    // or from the standard preferences, depending on whether this method was called from initWithCoder:
    // or initWithFrame, respectively.
    [self _updateWebCoreSettingsFromPreferences: [self preferences]];
    
    // Register to receive notifications whenever preference values change.
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesChangedNotification:)
                                                 name:WebPreferencesChangedNotification object:[self preferences]];
}

- (id)initWithFrame:(NSRect)f
{
    return [self initWithFrame:f frameName:nil groupName:nil];
}

- (id)initWithFrame:(NSRect)f frameName:(NSString *)frameName groupName:(NSString *)groupName;
{
    self = [super initWithFrame:f];
    if (!self)
        return nil;

#ifdef ENABLE_WEBKIT_UNSET_DYLD_FRAMEWORK_PATH
    // DYLD_FRAMEWORK_PATH is used so Safari will load the development version of WebKit, which
    // may not work with other WebKit applications.  Unsetting DYLD_FRAMEWORK_PATH removes the
    // need for Safari to unset it to prevent it from being passed to applications it launches.
    // Unsetting it when a WebView is first created is as good a place as any.
    // See <http://bugzilla.opendarwin.org/show_bug.cgi?id=4286> for more details.
    if (getenv("WEBKIT_UNSET_DYLD_FRAMEWORK_PATH")) {
        unsetenv("DYLD_FRAMEWORK_PATH");
        unsetenv("WEBKIT_UNSET_DYLD_FRAMEWORK_PATH");
    }
#endif

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
    WebPreferences *preferences;
    BOOL useBackForwardList = NO;
    BOOL allowsUndo = YES;
    
    result = [super initWithCoder:decoder];
    result->_private = [[WebViewPrivate alloc] init];

    // We don't want any of the archived subviews. The subviews will always
    // be created in _commonInitializationFrameName:groupName:.
    [[result subviews] makeObjectsPerformSelector:@selector(removeFromSuperview)];

    if ([decoder allowsKeyedCoding]) {
        frameName = [decoder decodeObjectForKey:@"FrameName"];
        groupName = [decoder decodeObjectForKey:@"GroupName"];
        preferences = [decoder decodeObjectForKey:@"Preferences"];
        useBackForwardList = [decoder decodeBoolForKey:@"UseBackForwardList"];
        if ([decoder containsValueForKey:@"AllowsUndo"])
            allowsUndo = [decoder decodeBoolForKey:@"AllowsUndo"];
    } else {
        int version;
        [decoder decodeValueOfObjCType:@encode(int) at:&version];
        frameName = [decoder decodeObject];
        groupName = [decoder decodeObject];
        preferences = [decoder decodeObject];
        if (version > 1)
            [decoder decodeValuesOfObjCTypes:"c", &useBackForwardList];
        if (version > 2)
            [decoder decodeValuesOfObjCTypes:"c", &allowsUndo];
    }

    if (![frameName isKindOfClass:[NSString class]])
        frameName = nil;
    if (![groupName isKindOfClass:[NSString class]])
        groupName = nil;
    if (![preferences isKindOfClass:[WebPreferences class]])
        preferences = nil;

    LOG(Encoding, "FrameName = %@, GroupName = %@, useBackForwardList = %d\n", frameName, groupName, (int)useBackForwardList);
    [result _commonInitializationWithFrameName:frameName groupName:groupName];
    result->_private->useBackForwardList = useBackForwardList;
    result->_private->allowsUndo = allowsUndo;
    if (preferences)
        [result setPreferences:preferences];

NS_HANDLER

    result = nil;
    [self release];

NS_ENDHANDLER

    return result;
}

- (void)encodeWithCoder:(NSCoder *)encoder
{
    [super encodeWithCoder:encoder];

    if ([encoder allowsKeyedCoding]) {
        [encoder encodeObject:[[self mainFrame] name] forKey:@"FrameName"];
        [encoder encodeObject:[self groupName] forKey:@"GroupName"];
        [encoder encodeObject:[self preferences] forKey:@"Preferences"];
        [encoder encodeBool:_private->useBackForwardList forKey:@"UseBackForwardList"];
        [encoder encodeBool:_private->allowsUndo forKey:@"AllowsUndo"];
    } else {
        int version = WebViewVersion;
        [encoder encodeValueOfObjCType:@encode(int) at:&version];
        [encoder encodeObject:[[self mainFrame] name]];
        [encoder encodeObject:[self groupName]];
        [encoder encodeObject:[self preferences]];
        [encoder encodeValuesOfObjCTypes:"c", &_private->useBackForwardList];
        [encoder encodeValuesOfObjCTypes:"c", &_private->allowsUndo];
    }

    LOG(Encoding, "FrameName = %@, GroupName = %@, useBackForwardList = %d\n", [[self mainFrame] name], [self groupName], (int)_private->useBackForwardList);
}

- (void)dealloc
{
    // call close to ensure we tear-down completly
    // this maintains our old behavior for existing applications
    [self _close];

    --WebViewCount;
    
    [_private release];
    // [super dealloc] can end up dispatching against _private (3466082)
    _private = nil;

    [super dealloc];
}

- (void)finalize
{
    ASSERT(_private->closed);

    --WebViewCount;

    [super finalize];
}

- (void)close
{
    [self _close];
}

- (void)setShouldCloseWithWindow:(BOOL)close
{
    _private->shouldCloseWithWindow = close;
}

- (BOOL)shouldCloseWithWindow
{
    return _private->shouldCloseWithWindow;
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    // Don't do anything if we aren't initialized.  This happens when decoding a WebView.
    if (!_private)
        return;
    
    if ([self window])
        [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowWillCloseNotification object:[self window]];

    if (window) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowWillClose:) name:NSWindowWillCloseNotification object:window];

        // Ensure that we will receive the events that WebHTMLView (at least) needs. It's expensive enough
        // that we don't want to call it over and over.
        [window setAcceptsMouseMovedEvents:YES];
        WKSetNSWindowShouldPostEventNotifications(window, YES);
    }
}

- (void)_windowWillClose:(NSNotification *)notification
{
    if ([self shouldCloseWithWindow] && ([self window] == [self hostWindow] || ([self window] && ![self hostWindow]) || (![self window] && [self hostWindow])))
        [self _close];
}

- (void)setPreferences:(WebPreferences *)prefs
{
    if (_private->preferences != prefs) {
#ifdef DISABLE_EDITABLE_LINKS_IN_MAIL
        if ([[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.mail"] && [prefs editableLinkBehavior] == WebKitEditableLinkDefaultBehavior)
            [prefs setEditableLinkBehavior:WebKitEditableLinkOnlyLiveWithShiftKey];
#endif
        [[NSNotificationCenter defaultCenter] removeObserver:self name:WebPreferencesChangedNotification object:[self preferences]];
        [WebPreferences _removeReferenceForIdentifier:[_private->preferences identifier]];
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
    if (!_private->closed && ![anIdentifier isEqual:[[self preferences] identifier]]) {
        WebPreferences *prefs = [[WebPreferences alloc] initWithIdentifier:anIdentifier];
        [self setPreferences:prefs];
        [prefs release];
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
    if (!_private)
        return nil;
    if (!_private->page)
        return nil;
    return kit(_private->page->mainFrame());
}

- (WebFrame *)selectedFrame
{
    // If the first responder is a view in our tree, we get the frame containing the first responder.
    // This is faster than searching the frame hierarchy, and will give us a result even in the case
    // where the focused frame doesn't actually contain a selection.
    WebFrame *focusedFrame = [self _focusedFrame];
    if (focusedFrame)
        return focusedFrame;
    
    // If the first responder is outside of our view tree, we search for a frame containing a selection.
    // There should be at most only one of these.
    return [[self mainFrame] _findFrameWithSelection];
}

- (WebBackForwardList *)backForwardList
{
    if (_private->useBackForwardList)
        return kit([self page]->backForwardList());
    return nil;
}

- (void)setMaintainsBackForwardList: (BOOL)flag
{
    _private->useBackForwardList = flag;
}

- (BOOL)goBack
{
    return _private->page->goBack();
}

- (BOOL)goForward
{
    return _private->page->goForward();
}

- (BOOL)goToBackForwardItem:(WebHistoryItem *)item
{
    _private->page->goToItem(core(item), FrameLoadTypeIndexedBackForward);
    return YES;
}

- (void)setTextSizeMultiplier:(float)m
{
    // NOTE: This has no visible effect when viewing a PDF (see <rdar://problem/4737380>)
    if (_private->textSizeMultiplier == m)
        return;

    _private->textSizeMultiplier = m;
    [self _notifyTextSizeMultiplierChanged];
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
    if (!_private->userAgentOverridden)
        *_private->userAgent = String();
}

- (NSString *)applicationNameForUserAgent
{
    return [[_private->applicationNameForUserAgent retain] autorelease];
}

- (void)setCustomUserAgent:(NSString *)userAgentString
{
    *_private->userAgent = userAgentString;
    _private->userAgentOverridden = userAgentString != nil;
}

- (NSString *)customUserAgent
{
    if (!_private->userAgentOverridden)
        return nil;
    return *_private->userAgent;
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
    if (encoding == oldEncoding || [encoding isEqualToString:oldEncoding])
        return;
    FrameLoader* mainFrameLoader = [[self mainFrame] _frameLoader];
    if (mainFrameLoader)
        mainFrameLoader->reloadAllowingStaleData(encoding);
}

- (NSString *)_mainFrameOverrideEncoding
{
    WebDataSource *dataSource = [[self mainFrame] provisionalDataSource];
    if (dataSource == nil)
        dataSource = [[self mainFrame] dataSource];
    if (dataSource == nil)
        return nil;
    return [dataSource _documentLoader]->overrideEncoding();
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
    return core([self mainFrame])->windowScriptObject();
}

// Get the appropriate user-agent string for a particular URL.
// Since we no longer automatically spoof, this no longer requires looking at the URL.
- (NSString *)userAgentForURL:(NSURL *)URL
{
    return [self _userAgent];
}

- (void)setHostWindow:(NSWindow *)hostWindow
{
    if (!_private->closed && hostWindow != _private->hostWindow) {
        [[self mainFrame] _viewWillMoveToHostWindow:hostWindow];
        if (_private->hostWindow)
            [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowWillCloseNotification object:_private->hostWindow];
        if (hostWindow)
            [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowWillClose:) name:NSWindowWillCloseNotification object:hostWindow];
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
    NSView <WebDocumentView>* view = [self documentViewAtWindowPoint:[draggingInfo draggingLocation]];
    WebPasteboardHelper helper([view isKindOfClass:[WebHTMLView class]] ? (WebHTMLView*)view : nil);
    IntPoint client([draggingInfo draggingLocation]);
    IntPoint global(globalPoint([draggingInfo draggingLocation], [self window]));
    DragData dragData(draggingInfo, client, global, (DragOperation)[draggingInfo draggingSourceOperationMask], &helper);
    return core(self)->dragController()->dragEntered(&dragData);
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)draggingInfo
{
    NSView <WebDocumentView>* view = [self documentViewAtWindowPoint:[draggingInfo draggingLocation]];
    WebPasteboardHelper helper([view isKindOfClass:[WebHTMLView class]] ? (WebHTMLView*)view : nil);
    IntPoint client([draggingInfo draggingLocation]);
    IntPoint global(globalPoint([draggingInfo draggingLocation], [self window]));
    DragData dragData(draggingInfo, client, global, (DragOperation)[draggingInfo draggingSourceOperationMask], &helper);
    return core(self)->dragController()->dragUpdated(&dragData);
}

- (void)draggingExited:(id <NSDraggingInfo>)draggingInfo
{
    NSView <WebDocumentView>* view = [self documentViewAtWindowPoint:[draggingInfo draggingLocation]];
    WebPasteboardHelper helper([view isKindOfClass:[WebHTMLView class]] ? (WebHTMLView*)view : nil);
    IntPoint client([draggingInfo draggingLocation]);
    IntPoint global(globalPoint([draggingInfo draggingLocation], [self window]));
    DragData dragData(draggingInfo, client, global, (DragOperation)[draggingInfo draggingSourceOperationMask], &helper);
    core(self)->dragController()->dragExited(&dragData);
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)draggingInfo
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)draggingInfo
{
    NSView <WebDocumentView>* view = [self documentViewAtWindowPoint:[draggingInfo draggingLocation]];
    WebPasteboardHelper helper([view isKindOfClass:[WebHTMLView class]]? (WebHTMLView*)view : nil);
    IntPoint client([draggingInfo draggingLocation]);
    IntPoint global(globalPoint([draggingInfo draggingLocation], [self window]));
    DragData dragData(draggingInfo, client, global, (DragOperation)[draggingInfo draggingSourceOperationMask], &helper);
    return core(self)->dragController()->performDrag(&dragData);
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
    if (_private->becomingFirstResponder) {
        // Fix for unrepro infinite recursion reported in radar 4448181. If we hit this assert on
        // a debug build, we should figure out what causes the problem and do a better fix.
        ASSERT_NOT_REACHED();
        return NO;
    }
    
    // This works together with setNextKeyView to splice the WebView into
    // the key loop similar to the way NSScrollView does this. Note that
    // WebFrameView has very similar code.
    NSWindow *window = [self window];
    WebFrameView *mainFrameView = [[self mainFrame] frameView];
    
    if ([window keyViewSelectionDirection] == NSSelectingPrevious) {
        NSView *previousValidKeyView = [self previousValidKeyView];
        if ((previousValidKeyView != self) && (previousValidKeyView != mainFrameView)) {
            _private->becomingFirstResponder = YES;
            [window makeFirstResponder:previousValidKeyView];
            _private->becomingFirstResponder = NO;
            return YES;
        } else {
            return NO;
        }
    }
    
    if ([mainFrameView acceptsFirstResponder]) {
        _private->becomingFirstResponder = YES;
        [window makeFirstResponder:mainFrameView];
        _private->becomingFirstResponder = NO;
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
    Frame* coreFrame = core(curr);
    return kit(forward
        ? coreFrame->tree()->traverseNextWithWrap(wrapFlag)
        : coreFrame->tree()->traversePreviousWithWrap(wrapFlag));
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag
{
    return [self searchFor:string direction:forward caseSensitive:caseFlag wrap:wrapFlag startInSelection:NO];
}

+ (void)registerViewClass:(Class)viewClass representationClass:(Class)representationClass forMIMEType:(NSString *)MIMEType
{
    [[WebFrameView _viewTypesAllowImageTypeOmission:YES] setObject:viewClass forKey:MIMEType];
    [[WebDataSource _repTypesAllowImageTypeOmission:YES] setObject:representationClass forKey:MIMEType];
}

- (void)setGroupName:(NSString *)groupName
{
    if (!_private->page)
        return;
    _private->page->setGroupName(groupName);
}

- (NSString *)groupName
{
    if (!_private->page)
        return nil;
    return _private->page->groupName();
}

- (double)estimatedProgress
{
    if (!_private->page)
        return 0.0;

    return _private->page->progress()->estimatedProgress();
}

- (NSArray *)pasteboardTypesForSelection
{
    NSView <WebDocumentView> *documentView = [[[self _selectedOrMainFrame] frameView] documentView];
    if ([documentView conformsToProtocol:@protocol(WebDocumentSelection)]) {
        return [(NSView <WebDocumentSelection> *)documentView pasteboardTypesForSelection];
    }
    return [NSArray array];
}

- (void)writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    WebFrame *frame = [self _selectedOrMainFrame];
    if (frame && [frame _hasSelection]) {
        NSView <WebDocumentView> *documentView = [[frame frameView] documentView];
        if ([documentView conformsToProtocol:@protocol(WebDocumentSelection)])
            [(NSView <WebDocumentSelection> *)documentView writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard];
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
        [self _writeImageForElement:element withPasteboardTypes:types toPasteboard:pasteboard];
    } else if ([element objectForKey:WebElementLinkURLKey] != nil) {
        [self _writeLinkElement:element withPasteboardTypes:types toPasteboard:pasteboard];
    } else if ([[element objectForKey:WebElementIsSelectedKey] boolValue]) {
        [self writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard];
    }
}

- (void)moveDragCaretToPoint:(NSPoint)point
{
    [[[self mainFrame] _bridge] moveDragCaretToPoint:[self convertPoint:point toView:[[[self mainFrame] frameView] documentView]]];
}

- (void)removeDragCaret
{
    if (!_private->page)
        return;
    _private->page->dragCaretController()->clear();
}

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
    return (mainFrameTitle != nil) ? mainFrameTitle : (NSString *)@"";
}

- (NSImage *)mainFrameIcon
{
    return [[WebIconDatabase sharedIconDatabase] iconForURL:[[[[self mainFrame] dataSource] _URL] _web_originalDataAsString] withSize:WebIconSmallSize];
}

- (DOMDocument *)mainFrameDocument
{
    // only return the actual value if the state we're in gives NSTreeController
    // enough time to release its observers on the old model
    if (_private->mainFrameDocumentReady)
        return [[self mainFrame] DOMDocument];
    return nil;
}

- (void)setDrawsBackground:(BOOL)drawsBackground
{
    if (_private->drawsBackground == drawsBackground)
        return;
    _private->drawsBackground = drawsBackground;
    [[self mainFrame] _updateBackground];
}

- (BOOL)drawsBackground
{
    return _private->drawsBackground;
}

- (void)_inspectElement:(id)sender
{
    NSDictionary *element = [sender representedObject];
    WebFrame *frame = [element objectForKey:WebElementFrameKey];
    DOMNode *node = [element objectForKey:WebElementDOMNodeKey];
    if (!node || !frame)
        return;

    if ([node nodeType] != DOM_ELEMENT_NODE || [node nodeType] != DOM_DOCUMENT_NODE)
        node = [node parentNode];

    WebInspector *inspector = [WebInspector sharedWebInspector];
    [inspector setWebFrame:frame];
    [inspector setFocusedDOMNode:node];

    node = [node parentNode];
    node = [node parentNode];
    if (node) // set the root node to something relatively close to the focused node
        [inspector setRootDOMNode:node];

    [inspector showWindow:nil];
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
    return !!_private->page->backForwardList()->backItem();
}

- (BOOL)canGoForward
{
    return !!_private->page->backForwardList()->forwardItem();
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

#define MinimumTextSizeMultiplier       0.5f
#define MaximumTextSizeMultiplier       3.0f
#define TextSizeMultiplierRatio         1.2f

- (BOOL)canMakeTextSmaller
{
    BOOL canShrinkMore = _private->textSizeMultiplier/TextSizeMultiplierRatio > MinimumTextSizeMultiplier;
    return [self _performTextSizingSelector:(SEL)0 withObject:nil onTrackingDocs:canShrinkMore selForNonTrackingDocs:@selector(_canMakeTextSmaller) newScaleFactor:0];
}

- (BOOL)canMakeTextLarger
{
    BOOL canGrowMore = _private->textSizeMultiplier*TextSizeMultiplierRatio < MaximumTextSizeMultiplier;
    return [self _performTextSizingSelector:(SEL)0 withObject:nil onTrackingDocs:canGrowMore selForNonTrackingDocs:@selector(_canMakeTextLarger) newScaleFactor:0];
}

- (IBAction)makeTextSmaller:(id)sender
{
    float newScale = _private->textSizeMultiplier / TextSizeMultiplierRatio;
    BOOL canShrinkMore = newScale > MinimumTextSizeMultiplier;
    [self _performTextSizingSelector:@selector(_makeTextSmaller:) withObject:sender onTrackingDocs:canShrinkMore selForNonTrackingDocs:@selector(_canMakeTextSmaller) newScaleFactor:newScale];
}

- (IBAction)makeTextLarger:(id)sender
{
    float newScale = _private->textSizeMultiplier*TextSizeMultiplierRatio;
    BOOL canGrowMore = newScale < MaximumTextSizeMultiplier;
    [self _performTextSizingSelector:@selector(_makeTextLarger:) withObject:sender onTrackingDocs:canGrowMore selForNonTrackingDocs:@selector(_canMakeTextLarger) newScaleFactor:newScale];
}

- (IBAction)toggleSmartInsertDelete:(id)sender
{
    [self setSmartInsertDeleteEnabled:![self smartInsertDeleteEnabled]];
}

- (IBAction)toggleContinuousSpellChecking:(id)sender
{
    [self setContinuousSpellCheckingEnabled:![self isContinuousSpellCheckingEnabled]];
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

- (BOOL)canMakeTextStandardSize
{
    BOOL notAlreadyStandard = _private->textSizeMultiplier != 1.0f;
    return [self _performTextSizingSelector:(SEL)0 withObject:nil onTrackingDocs:notAlreadyStandard selForNonTrackingDocs:@selector(_canMakeTextStandardSize) newScaleFactor:0.0f];
}

- (IBAction)makeTextStandardSize:(id)sender
{
    BOOL notAlreadyStandard = _private->textSizeMultiplier != 1.0f;
    [self _performTextSizingSelector:@selector(_makeTextStandardSize:) withObject:sender onTrackingDocs:notAlreadyStandard selForNonTrackingDocs:@selector(_canMakeTextStandardSize) newScaleFactor:1.0f];
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
        if ([self _continuousCheckingAllowed]) {
            checkMark = [self isContinuousSpellCheckingEnabled];
            retVal = YES;
        }
        if ([(NSObject *)item isKindOfClass:[NSMenuItem class]]) {
            NSMenuItem *menuItem = (NSMenuItem *)item;
            [menuItem setState:checkMark ? NSOnState : NSOffState];
        }
        return retVal;
#ifndef BUILDING_ON_TIGER
    } else if (action == @selector(toggleGrammarChecking:)) {
        BOOL checkMark = [self isGrammarCheckingEnabled];
        if ([(NSObject *)item isKindOfClass:[NSMenuItem class]]) {
            NSMenuItem *menuItem = (NSMenuItem *)item;
            [menuItem setState:checkMark ? NSOnState : NSOffState];
        }
        return YES;
#endif
    }
    FOR_EACH_RESPONDER_SELECTOR(VALIDATE)

    return YES;
}

@end

@implementation WebView (WebPendingPublic)

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag startInSelection:(BOOL)startInSelection
{
    if (_private->closed)
        return NO;
    
    // Get the frame holding the selection, or start with the main frame
    WebFrame *startFrame = [self _selectedOrMainFrame];
    
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
                if ([searchView isKindOfClass:[WebHTMLView class]])
                    // optimization for the common case, to avoid making giant string for selection
                    startHasSelection = [startFrame _hasSelection];
                else if ([searchView conformsToProtocol:@protocol(WebDocumentText)])
                    startHasSelection = [(id <WebDocumentText>)searchView selectedString] != nil;
                startSearchView = searchView;
            }
            
            BOOL foundString;
            if ([searchView conformsToProtocol:@protocol(WebDocumentIncrementalSearching)])
                foundString = [(NSView <WebDocumentIncrementalSearching> *)searchView searchFor:string direction:forward caseSensitive:caseFlag wrap:NO startInSelection:startInSelection];
            else
                foundString = [searchView searchFor:string direction:forward caseSensitive:caseFlag wrap:NO];
            
            if (foundString) {
                if (frame != startFrame)
                    [startFrame _clearSelection];
                [[self window] makeFirstResponder:searchView];
                return YES;
            }
        }
        frame = incrementFrame(frame, forward, wrapFlag);
    } while (frame != nil && frame != startFrame);
    
    // Search contents of startFrame, on the other side of the selection that we did earlier.
    // We cheat a bit and just research with wrap on
    if (wrapFlag && startHasSelection && startSearchView) {
        BOOL foundString;
        if ([startSearchView conformsToProtocol:@protocol(WebDocumentIncrementalSearching)])
            foundString = [(NSView <WebDocumentIncrementalSearching> *)startSearchView searchFor:string direction:forward caseSensitive:caseFlag wrap:YES startInSelection:startInSelection];
        else
            foundString = [startSearchView searchFor:string direction:forward caseSensitive:caseFlag wrap:YES];
        if (foundString) {
            [[self window] makeFirstResponder:startSearchView];
            return YES;
        }
    }
    return NO;
}

- (void)setHoverFeedbackSuspended:(BOOL)newValue
{
    if (_private->hoverFeedbackSuspended == newValue)
        return;
    
    _private->hoverFeedbackSuspended = newValue;
    id <WebDocumentView> documentView = [[[self mainFrame] frameView] documentView];
    // FIXME: in a perfect world we'd do this in a general way that worked with any document view,
    // such as by calling a protocol method or using respondsToSelector or sending a notification.
    // But until there is any need for these more general solutions, we'll just hardwire it to work
    // with WebHTMLView.
    // Note that _hoverFeedbackSuspendedChanged needs to be called only on the main WebHTMLView, not
    // on each subframe separately.
    if ([documentView isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView *)documentView _hoverFeedbackSuspendedChanged];
}

- (BOOL)isHoverFeedbackSuspended
{
    return _private->hoverFeedbackSuspended;
}

- (void)setMainFrameDocumentReady:(BOOL)mainFrameDocumentReady
{
    // by setting this to NO, calls to mainFrameDocument are forced to return nil
    // setting this to YES lets it return the actual DOMDocument value
    // we use this to tell NSTreeController to reset its observers and clear its state
    if (_private->mainFrameDocumentReady == mainFrameDocumentReady)
        return;
    [self _willChangeValueForKey:_WebMainFrameDocumentKey];
    _private->mainFrameDocumentReady = mainFrameDocumentReady;
    [self _didChangeValueForKey:_WebMainFrameDocumentKey];
    // this will cause observers to call mainFrameDocument where this flag will be checked
}

// This method name is used by Mail on Tiger (but not post-Tiger), so we shouldn't delete it 
// until the day comes when we're no longer supporting Mail on Tiger.
- (WebFrame *)_frameForCurrentSelection
{
    return [self _selectedOrMainFrame];
}

- (void)setTabKeyCyclesThroughElements:(BOOL)cyclesElements
{
    _private->tabKeyCyclesThroughElementsChanged = YES;
    _private->page->setTabKeyCyclesThroughElements(cyclesElements);
}

- (BOOL)tabKeyCyclesThroughElements
{
    return _private->page->tabKeyCyclesThroughElements();
}

- (void)setScriptDebugDelegate:(id)delegate
{
    _private->scriptDebugDelegate = delegate;
    [_private->scriptDebugDelegateForwarder release];
    _private->scriptDebugDelegateForwarder = nil;
    if (delegate)
        [self _attachScriptDebuggerToAllFrames];
    else
        [self _detachScriptDebuggerFromAllFrames];
}

- (id)scriptDebugDelegate
{
    return _private->scriptDebugDelegate;
}

- (BOOL)shouldClose
{
    FrameMac* coreFrame = core([self mainFrame]);
    if (!coreFrame)
        return YES;
    return coreFrame->shouldClose();
}

- (NSAppleEventDescriptor *)aeDescByEvaluatingJavaScriptFromString:(NSString *)script
{
    return [[[self mainFrame] _bridge] aeDescByEvaluatingJavaScriptFromString:script];
}

- (unsigned)markAllMatchesForText:(NSString *)string caseSensitive:(BOOL)caseFlag highlight:(BOOL)highlight limit:(unsigned)limit
{
    WebFrame *frame = [self mainFrame];
    unsigned matchCount = 0;
    do {
        id <WebDocumentView> view = [[frame frameView] documentView];
        // FIXME: introduce a protocol, or otherwise make this work with other types
        if ([view isKindOfClass:[WebHTMLView class]])
            [(WebHTMLView *)view setMarkedTextMatchesAreHighlighted:highlight];
        
        ASSERT(limit == 0 || matchCount < limit);
        matchCount += [(WebHTMLView *)view markAllMatchesForText:string caseSensitive:caseFlag limit:limit == 0 ? 0 : limit - matchCount];

        // Stop looking if we've reached the limit. A limit of 0 means no limit.
        if (limit > 0 && matchCount >= limit)
            break;
        
        frame = incrementFrame(frame, YES, NO);
    } while (frame);
    
    return matchCount;
}

- (void)unmarkAllTextMatches
{
    WebFrame *frame = [self mainFrame];
    do {
        id <WebDocumentView> view = [[frame frameView] documentView];
        // FIXME: introduce a protocol, or otherwise make this work with other types
        if ([view isKindOfClass:[WebHTMLView class]])
            [(WebHTMLView *)view unmarkAllTextMatches];
        
        frame = incrementFrame(frame, YES, NO);
    } while (frame);
}

- (NSArray *)rectsForTextMatches
{
    NSMutableArray *result = [NSMutableArray array];
    WebFrame *frame = [self mainFrame];
    do {
        id <WebDocumentView> view = [[frame frameView] documentView];
        // FIXME: introduce a protocol, or otherwise make this work with other types
        if ([view isKindOfClass:[WebHTMLView class]]) {
            WebHTMLView *htmlView = (WebHTMLView *)view;
            NSArray *originalRects = [htmlView rectsForTextMatches];
            unsigned rectCount = [originalRects count];
            unsigned rectIndex;
            NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
            for (rectIndex = 0; rectIndex < rectCount; ++rectIndex) {
                NSRect r = [[originalRects objectAtIndex:rectIndex] rectValue];
                if (NSIsEmptyRect(r))
                    continue;
                
                // Convert rect to our coordinate system
                r = [htmlView convertRect:r toView:self];
                [result addObject:[NSValue valueWithRect:r]];
                if (rectIndex % 10 == 0)
                    [pool drain];
            }
            [pool release];
        }
        
        frame = incrementFrame(frame, YES, NO);
    } while (frame);
    
    return result;
}

- (void)scrollDOMRangeToVisible:(DOMRange *)range
{
    [[[range startContainer] _bridge] scrollDOMRangeToVisible:range];
}

- (BOOL)allowsUndo
{
    return _private->allowsUndo;
}

- (void)setAllowsUndo:(BOOL)flag
{
    _private->allowsUndo = flag;
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
    return [[element ownerDocument] getComputedStyle:element pseudoElement:pseudoElement];
}

@end

@implementation WebView (WebViewEditing)

- (DOMRange *)editableDOMRangeForPoint:(NSPoint)point
{
    WebFrameBridge *bridge = [self _bridgeAtPoint:point];
    return [bridge editableDOMRangeForPoint:[self convertPoint:point toView:[[[bridge webFrame] frameView] documentView]]];
}

- (BOOL)_shouldChangeSelectedDOMRange:(DOMRange *)currentRange toDOMRange:(DOMRange *)proposedRange affinity:(NSSelectionAffinity)selectionAffinity stillSelecting:(BOOL)flag;
{
    return [[self _editingDelegateForwarder] webView:self shouldChangeSelectedDOMRange:currentRange toDOMRange:proposedRange affinity:selectionAffinity stillSelecting:flag];
}

- (BOOL)maintainsInactiveSelection
{
    return NO;
}

- (void)setSelectedDOMRange:(DOMRange *)range affinity:(NSSelectionAffinity)selectionAffinity
{
    Frame* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return;

    if (range == nil)
        coreFrame->selectionController()->clear();
    else {
        // Derive the frame to use from the range passed in.
        // Using _bridgeForSelectedOrMainFrame could give us a different document than
        // the one the range uses.
        coreFrame = core([range startContainer])->document()->frame();
        if (!coreFrame)
            return;

        selectRange(coreFrame->selectionController(), [range _range], core(selectionAffinity), true);
    }
}

- (DOMRange *)selectedDOMRange
{
    Frame* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return nil;
    return kit(coreFrame->selectionController()->toRange().get());
}

- (NSSelectionAffinity)selectionAffinity
{
    Frame* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return NSSelectionAffinityDownstream;
    return kit(coreFrame->selectionController()->affinity());
}

- (void)setEditable:(BOOL)flag
{
    if (_private->editable != flag) {
        _private->editable = flag;
        if (!_private->tabKeyCyclesThroughElementsChanged)
            _private->page->setTabKeyCyclesThroughElements(!flag);
        FrameMac* mainFrame = [[[self mainFrame] _bridge] _frame];
        if (mainFrame) {
            if (flag) {
                mainFrame->applyEditingStyleToBodyElement();
                // If the WebView is made editable and the selection is empty, set it to something.
                if (![self selectedDOMRange])
                    mainFrame->setSelectionFromNone();
            } else
                mainFrame->removeEditingStyleFromBodyElement();
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
    [[self _bridgeForSelectedOrMainFrame] setTypingStyle:style withUndoAction:EditActionUnspecified];
}

- (DOMCSSStyleDeclaration *)typingStyle
{
    return [[self _bridgeForSelectedOrMainFrame] typingStyle];
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
    if (continuousSpellCheckingEnabled != flag) {
        continuousSpellCheckingEnabled = flag;
        [[NSUserDefaults standardUserDefaults] setBool:continuousSpellCheckingEnabled forKey:WebContinuousSpellCheckingEnabled];
    }
    
    if ([self isContinuousSpellCheckingEnabled]) {
        [[self class] _preflightSpellChecker];
    } else {
        [[self mainFrame] _unmarkAllMisspellings];
    }
}

- (BOOL)isContinuousSpellCheckingEnabled
{
    return (continuousSpellCheckingEnabled && [self _continuousCheckingAllowed]);
}

- (WebNSInteger)spellCheckerDocumentTag
{
    if (!_private->hasSpellCheckerDocumentTag) {
        _private->spellCheckerDocumentTag = [NSSpellChecker uniqueSpellDocumentTag];
        _private->hasSpellCheckerDocumentTag = YES;
    }
    return _private->spellCheckerDocumentTag;
}

- (NSUndoManager *)undoManager
{
    if (!_private->allowsUndo)
        return nil;

    NSUndoManager *undoManager = [[self _editingDelegateForwarder] undoManagerForWebView:self];
    if (undoManager)
        return undoManager;

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
    DOMCSSStyleDeclaration *decl = [[[self _selectedOrMainFrame] DOMDocument] createCSSStyleDeclaration];
    [decl setCssText:text];
    return decl;
}

@end

@implementation WebView (WebViewGrammarChecking)

// FIXME: This method should be merged into WebViewEditing when we're not in API freeze
- (BOOL)isGrammarCheckingEnabled
{
#if BUILDING_ON_TIGER
    return NO;
#else
    return grammarCheckingEnabled;
#endif
}

#ifndef BUILDING_ON_TIGER
// FIXME: This method should be merged into WebViewEditing when we're not in API freeze
- (void)setGrammarCheckingEnabled:(BOOL)flag
{
    if (grammarCheckingEnabled != flag) {
        grammarCheckingEnabled = flag;
        [[NSUserDefaults standardUserDefaults] setBool:grammarCheckingEnabled forKey:WebGrammarCheckingEnabled];
    }
    
    // We call _preflightSpellChecker when turning continuous spell checking on, but we don't need to do that here
    // because grammar checking only occurs on code paths that already preflight spell checking appropriately.
    
    if (![self isGrammarCheckingEnabled])
        [[self mainFrame] _unmarkAllBadGrammar];
}

// FIXME: This method should be merged into WebIBActions when we're not in API freeze
- (void)toggleGrammarChecking:(id)sender
{
    [self setGrammarCheckingEnabled:![self isGrammarCheckingEnabled]];
}
#endif

@end

@implementation WebView (WebViewUndoableEditing)

- (void)replaceSelectionWithNode:(DOMNode *)node
{
    [[self _bridgeForSelectedOrMainFrame] replaceSelectionWithNode:node selectReplacement:YES smartReplace:NO matchStyle:NO];
}    

- (void)replaceSelectionWithText:(NSString *)text
{
    [[self _bridgeForSelectedOrMainFrame] replaceSelectionWithText:text selectReplacement:YES smartReplace:NO];
}

- (void)replaceSelectionWithMarkupString:(NSString *)markupString
{
    [[self _bridgeForSelectedOrMainFrame] replaceSelectionWithMarkupString:markupString baseURLString:nil selectReplacement:YES smartReplace:NO];
}

- (void)replaceSelectionWithArchive:(WebArchive *)archive
{
    [[[[self _bridgeForSelectedOrMainFrame] webFrame] dataSource] _replaceSelectionWithArchive:archive selectReplacement:YES];
}

- (void)deleteSelection
{
    WebFrame *webFrame = [self _selectedOrMainFrame];
    Frame* coreFrame = core(webFrame);
    if (coreFrame)
        coreFrame->editor()->deleteSelectionWithSmartDelete([(WebHTMLView *)[[webFrame frameView] documentView] _canSmartCopyOrDelete]);
}
    
- (void)applyStyle:(DOMCSSStyleDeclaration *)style
{
    // We don't know enough at thls level to pass in a relevant WebUndoAction; we'd have to
    // change the API to allow this.
    WebFrame *webFrame = [self _selectedOrMainFrame];
    Frame* coreFrame = core(webFrame);
    if (coreFrame)
        coreFrame->editor()->applyStyle(core(style));
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
    [[self _bridgeForSelectedOrMainFrame] insertParagraphSeparatorInQuotedContent];
}

- (BOOL)_selectWordBeforeMenuEvent
{
    return _private->selectWordBeforeMenuEvent;
}

- (void)_setSelectWordBeforeMenuEvent:(BOOL)flag
{
    _private->selectWordBeforeMenuEvent = flag;
}

- (void)_replaceSelectionWithNode:(DOMNode *)node matchStyle:(BOOL)matchStyle
{
    [[self _bridgeForSelectedOrMainFrame] replaceSelectionWithNode:node selectReplacement:YES smartReplace:NO matchStyle:matchStyle];
}

@end

static WebFrameView *containingFrameView(NSView *view)
{
    while (view && ![view isKindOfClass:[WebFrameView class]])
        view = [view superview];
    return (WebFrameView *)view;    
}

@implementation WebView (WebFileInternal)

- (WebFrame *)_focusedFrame
{
    NSResponder *resp = [[self window] firstResponder];
    if (resp && [resp isKindOfClass:[NSView class]] && [(NSView *)resp isDescendantOf:[[self mainFrame] frameView]]) {
        WebFrameView *frameView = containingFrameView((NSView *)resp);
        ASSERT(frameView != nil);
        return [frameView webFrame];
    }
    
    return nil;
}

- (WebFrame *)_selectedOrMainFrame
{
    WebFrame *result = [self selectedFrame];
    if (result == nil)
        result = [self mainFrame];
    return result;
}

- (WebFrameBridge *)_bridgeForSelectedOrMainFrame
{
    return [[self _selectedOrMainFrame] _bridge];
}

- (BOOL)_isLoading
{
    WebFrame *mainFrame = [self mainFrame];
    return [[mainFrame dataSource] isLoading]
        || [[mainFrame provisionalDataSource] isLoading];
}

- (WebFrameView *)_frameViewAtWindowPoint:(NSPoint)point
{
    if (_private->closed)
        return nil;
    NSView *view = [self hitTest:[[self superview] convertPoint:point fromView:nil]];
    if (![view isDescendantOf:[[self mainFrame] frameView]])
        return nil;
    WebFrameView *frameView = containingFrameView(view);
    ASSERT(frameView);
    return frameView;
}

- (WebFrameBridge *)_bridgeAtPoint:(NSPoint)point
{
    return [[[self _frameViewAtWindowPoint:[self convertPoint:point toView:nil]] webFrame] _bridge];
}

+ (void)_preflightSpellCheckerNow:(id)sender
{
    [[NSSpellChecker sharedSpellChecker] _preflightChosenSpellServer];
}

+ (void)_preflightSpellChecker
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
    WebFrameView *mainFrameView = [[self mainFrame] frameView];
    
    // If the current responder is outside of the webview, use our main frameView or its
    // document view. We also do this for subviews of self that are siblings of the main
    // frameView since clients might insert non-webview-related views there (see 4552713).
    if (responder != self && ![mainFrameView _web_firstResponderIsSelfOrDescendantView]) {
        responder = [mainFrameView documentView];
        if (!responder)
            responder = mainFrameView;
    }
    return responder;
}

- (void)_searchWithGoogleFromMenu:(id)sender
{
    id documentView = [[[self selectedFrame] frameView] documentView];
    if (![documentView conformsToProtocol:@protocol(WebDocumentText)]) {
        return;
    }
    
    NSString *selectedString = [(id <WebDocumentText>)documentView selectedString];
    if ([selectedString length] == 0) {
        return;
    }
    
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
    NSMutableString *s = [selectedString mutableCopy];
    const unichar nonBreakingSpaceCharacter = 0xA0;
    NSString *nonBreakingSpaceString = [NSString stringWithCharacters:&nonBreakingSpaceCharacter length:1];
    [s replaceOccurrencesOfString:nonBreakingSpaceString withString:@" " options:0 range:NSMakeRange(0, [s length])];
    [pasteboard setString:s forType:NSStringPboardType];
    [s release];
    
    // FIXME: seems fragile to use the service by name, but this is what AppKit does
    NSPerformService(@"Search With Google", pasteboard);
}

- (void)_searchWithSpotlightFromMenu:(id)sender
{
    id documentView = [[[self selectedFrame] frameView] documentView];
    if (![documentView conformsToProtocol:@protocol(WebDocumentText)]) {
        return;
    }
    
    NSString *selectedString = [(id <WebDocumentText>)documentView selectedString];
    if ([selectedString length] == 0) {
        return;
    }

    (void)HISearchWindowShow((CFStringRef)selectedString, kNilOptions);
}

// Slightly funky method that lets us have one copy of the logic for finding docViews that can do
// text sizing.  It returns whether it found any "suitable" doc views.  It sends sel to any suitable
// doc views, or if sel==0 we do nothing to them.  For doc views that track our size factor, they are
// suitable if doTrackingViews==YES (which in practice means that our size factor isn't at its max or
// min).  For doc views that don't track it, we send them testSel to determine suitablility.  If we
// do find any suitable tracking doc views and newScaleFactor!=0, we will set the common scale factor
// to that new factor before we send sel to any of them. 
- (BOOL)_performTextSizingSelector:(SEL)sel withObject:(id)arg onTrackingDocs:(BOOL)doTrackingViews selForNonTrackingDocs:(SEL)testSel newScaleFactor:(float)newScaleFactor
{
    if ([[self mainFrame] dataSource] == nil)
        return NO;
    
    BOOL foundSome = NO;
    NSArray *docViews = [[self mainFrame] _documentViews];
    for (int i = [docViews count]-1; i >= 0; i--) {
        id docView = [docViews objectAtIndex:i];
        if ([docView conformsToProtocol:@protocol(_WebDocumentTextSizing)]) {
            id <_WebDocumentTextSizing> sizingDocView = (id <_WebDocumentTextSizing>)docView;
            BOOL isSuitable;
            if ([sizingDocView _tracksCommonSizeFactor]) {
                isSuitable = doTrackingViews;
                if (isSuitable && newScaleFactor != 0)
                    _private->textSizeMultiplier = newScaleFactor;
            } else {
                // Incantation to perform a selector returning a BOOL.
                isSuitable = ((BOOL(*)(id, SEL))objc_msgSend)(sizingDocView, testSel);
            }
            
            if (isSuitable) {
                if (sel != 0) {
                    foundSome = YES;
                    [sizingDocView performSelector:sel withObject:arg];
                } else {
                    // if we're just called for the benefit of the return value, we can return at first match
                    return YES;
                }
            }
        }
    }
    
    return foundSome;
}

- (void)_notifyTextSizeMultiplierChanged
{
    if ([[self mainFrame] dataSource] == nil)
        return;

    NSArray *docViews = [[self mainFrame] _documentViews];
    for (int i = [docViews count]-1; i >= 0; i--) {
        id docView = [docViews objectAtIndex:i];
        if ([docView conformsToProtocol:@protocol(_WebDocumentTextSizing)] == NO)
            continue;

        id <_WebDocumentTextSizing> sizingDocView = (id <_WebDocumentTextSizing>)docView;
        if ([sizingDocView _tracksCommonSizeFactor])
            [sizingDocView _textSizeMultiplierChanged];
    }

}

@end

@implementation WebView (WebViewInternal)

- (void)_computeUserAgent
{
    NSString *userAgent;
    NSString *language = [NSUserDefaults _webkit_preferredLanguageCode];
    id sourceVersion = [[NSBundle bundleForClass:[WebView class]]
        objectForInfoDictionaryKey:(id)kCFBundleVersionKey];
    NSString *applicationName = _private->applicationNameForUserAgent;
    if ([applicationName length])
        userAgent = [NSString stringWithFormat:@"Mozilla/5.0 (Macintosh; U; " PROCESSOR " Mac OS X; %@) AppleWebKit/%@ (KHTML, like Gecko) %@",
            language, sourceVersion, applicationName];
    else
        userAgent = [NSString stringWithFormat:@"Mozilla/5.0 (Macintosh; U; " PROCESSOR " Mac OS X; %@) AppleWebKit/%@ (KHTML, like Gecko)",
            language, sourceVersion];
    *_private->userAgent = userAgent;
}

- (WebCore::String&)_userAgent
{
    if (_private->userAgent->isNull())
        [self _computeUserAgent];
    return *_private->userAgent;
}

- (void)_addObject:(id)object forIdentifier:(unsigned long)identifier
{
    ASSERT(!_private->identifierMap->contains(identifier));
    _private->identifierMap->set(identifier, object);
}

- (id)_objectForIdentifier:(unsigned long)identifier
{
    return _private->identifierMap->get(identifier).get();
}

- (void)_removeObjectForIdentifier:(unsigned long)identifier
{
    _private->identifierMap->remove(identifier);
}

@end
