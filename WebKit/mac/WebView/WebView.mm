/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#import "WebBackForwardListInternal.h"
#import "WebCache.h"
#import "WebChromeClient.h"
#import "WebContextMenuClient.h"
#import "WebDOMOperationsPrivate.h"
#import "WebDataSourceInternal.h"
#import "WebDatabaseManagerInternal.h"
#import "WebDefaultEditingDelegate.h"
#import "WebDefaultPolicyDelegate.h"
#import "WebDefaultUIDelegate.h"
#import "WebDocument.h"
#import "WebDocumentInternal.h"
#import "WebDownload.h"
#import "WebDownloadInternal.h"
#import "WebDragClient.h"
#import "WebDynamicScrollBarsViewInternal.h"
#import "WebEditingDelegate.h"
#import "WebEditorClient.h"
#import "WebFormDelegatePrivate.h"
#import "WebFrameInternal.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLRepresentation.h"
#import "WebHTMLViewInternal.h"
#import "WebHistoryItemInternal.h"
#import "WebIconDatabaseInternal.h"
#import "WebInspector.h"
#import "WebInspectorClient.h"
#import "WebKitErrors.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitStatisticsPrivate.h"
#import "WebKitSystemBits.h"
#import "WebKitVersionChecks.h"
#import "WebLocalizableStrings.h"
#import "WebNodeHighlight.h"
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
#import "WebPDFView.h"
#import "WebPanelAuthenticationHandler.h"
#import "WebPasteboardHelper.h"
#import "WebPluginDatabase.h"
#import "WebPolicyDelegate.h"
#import "WebPreferenceKeysPrivate.h"
#import "WebPreferencesPrivate.h"
#import "WebScriptDebugDelegate.h"
#import "WebSystemInterface.h"
#import "WebTextIterator.h"
#import "WebUIDelegate.h"
#import "WebUIDelegatePrivate.h"
#import <CoreFoundation/CFSet.h>
#import <Foundation/NSURLConnection.h>
#import <WebCore/ApplicationCacheStorage.h>
#import <WebCore/Cache.h>
#import <WebCore/ColorMac.h>
#import <WebCore/Cursor.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/DragController.h>
#import <WebCore/DragData.h>
#import <WebCore/Editor.h>
#import <WebCore/EventHandler.h>
#import <WebCore/ExceptionHandlers.h>
#import <WebCore/FocusController.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameView.h>
#import <WebCore/FrameTree.h>
#import <WebCore/GCController.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/IconDatabase.h>
#import <WebCore/Logging.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/Page.h>
#import <WebCore/PageCache.h>
#import <WebCore/PageGroup.h>
#import <WebCore/PlatformMouseEvent.h>
#import <WebCore/ProgressTracker.h>
#import <WebCore/ScriptController.h>
#import <WebCore/ScriptValue.h>
#import <WebCore/SelectionController.h>
#import <WebCore/Settings.h>
#import <WebCore/TextResourceDecoder.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebCoreTextRenderer.h>
#import <WebCore/WebCoreView.h>
#import <WebKit/DOM.h>
#import <WebKit/DOMExtensions.h>
#import <WebKit/DOMPrivate.h>
#import <WebKitSystemInterface.h>
#import <mach-o/dyld.h>
#import <objc/objc-auto.h>
#import <objc/objc-runtime.h>
#import <runtime/ArrayPrototype.h>
#import <runtime/DateInstance.h>
#import <runtime/InitializeThreading.h>
#import <runtime/JSLock.h>
#import <runtime/JSValue.h>
#import <wtf/Assertions.h>
#import <wtf/HashTraits.h>
#import <wtf/RefCountedLeakCounter.h>
#import <wtf/RefPtr.h>
#import <wtf/StdLibExtras.h>

#if ENABLE(DASHBOARD_SUPPORT)
#import <WebKit/WebDashboardRegion.h>
#endif

using namespace WebCore;
using namespace JSC;

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
macro(changeBaseWritingDirection) \
macro(changeBaseWritingDirectionToLTR) \
macro(changeBaseWritingDirectionToRTL) \
macro(changeColor) \
macro(changeDocumentBackgroundColor) \
macro(changeFont) \
macro(changeSpelling) \
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
macro(deleteToMark) \
macro(deleteWordBackward) \
macro(deleteWordForward) \
macro(ignoreSpelling) \
macro(indent) \
macro(insertBacktab) \
macro(insertLineBreak) \
macro(insertNewline) \
macro(insertNewlineIgnoringFieldEditor) \
macro(insertParagraphSeparator) \
macro(insertTab) \
macro(insertTabIgnoringFieldEditor) \
macro(lowercaseWord) \
macro(makeBaseWritingDirectionLeftToRight) \
macro(makeBaseWritingDirectionRightToLeft) \
macro(makeTextWritingDirectionLeftToRight) \
macro(makeTextWritingDirectionNatural) \
macro(makeTextWritingDirectionRightToLeft) \
macro(moveBackward) \
macro(moveBackwardAndModifySelection) \
macro(moveDown) \
macro(moveDownAndModifySelection) \
macro(moveForward) \
macro(moveForwardAndModifySelection) \
macro(moveLeft) \
macro(moveLeftAndModifySelection) \
macro(moveParagraphBackwardAndModifySelection) \
macro(moveParagraphForwardAndModifySelection) \
macro(moveRight) \
macro(moveRightAndModifySelection) \
macro(moveToBeginningOfDocument) \
macro(moveToBeginningOfDocumentAndModifySelection) \
macro(moveToBeginningOfLine) \
macro(moveToBeginningOfLineAndModifySelection) \
macro(moveToBeginningOfParagraph) \
macro(moveToBeginningOfParagraphAndModifySelection) \
macro(moveToBeginningOfSentence) \
macro(moveToBeginningOfSentenceAndModifySelection) \
macro(moveToEndOfDocument) \
macro(moveToEndOfDocumentAndModifySelection) \
macro(moveToEndOfLine) \
macro(moveToEndOfLineAndModifySelection) \
macro(moveToEndOfParagraph) \
macro(moveToEndOfParagraphAndModifySelection) \
macro(moveToEndOfSentence) \
macro(moveToEndOfSentenceAndModifySelection) \
macro(moveToLeftEndOfLine) \
macro(moveToLeftEndOfLineAndModifySelection) \
macro(moveToRightEndOfLine) \
macro(moveToRightEndOfLineAndModifySelection) \
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
macro(pageDownAndModifySelection) \
macro(pageUp) \
macro(pageUpAndModifySelection) \
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
macro(selectLine) \
macro(selectParagraph) \
macro(selectSentence) \
macro(selectToMark) \
macro(selectWord) \
macro(setMark) \
macro(showGuessPanel) \
macro(startSpeaking) \
macro(stopSpeaking) \
macro(subscript) \
macro(superscript) \
macro(swapWithMark) \
macro(takeFindStringFromSelection) \
macro(toggleBaseWritingDirection) \
macro(transpose) \
macro(underline) \
macro(unscript) \
macro(uppercaseWord) \
macro(yank) \
macro(yankAndSelect) \

#define WebKitOriginalTopPrintingMarginKey @"WebKitOriginalTopMargin"
#define WebKitOriginalBottomPrintingMarginKey @"WebKitOriginalBottomMargin"

#define KeyboardUIModeDidChangeNotification @"com.apple.KeyboardUIModeDidChange"
#define AppleKeyboardUIMode CFSTR("AppleKeyboardUIMode")
#define UniversalAccessDomain CFSTR("com.apple.universalaccess")

#if USE(ACCELERATED_COMPOSITING)
#define UsingAcceleratedCompositingProperty @"_isUsingAcceleratedCompositing"
#endif            


static BOOL s_didSetCacheModel;
static WebCacheModel s_cacheModel = WebCacheModelDocumentViewer;

static BOOL applicationIsTerminating;
static int pluginDatabaseClientCount = 0;

#ifndef NDEBUG
static const char webViewIsOpen[] = "At least one WebView is still open.";
#endif

@interface NSSpellChecker (AppKitSecretsIKnow)
- (void)_preflightChosenSpellServer;
@end

@interface NSView (AppKitSecretsIKnow)
- (NSView *)_hitTest:(NSPoint *)aPoint dragTypes:(NSSet *)types;
- (void)_autoscrollForDraggingInfo:(id)dragInfo timeDelta:(NSTimeInterval)repeatDelta;
- (BOOL)_shouldAutoscrollForDraggingInfo:(id)dragInfo;
@end

@interface NSWindow (AppKitSecretsIKnow) 
- (id)_oldFirstResponderBeforeBecoming;
@end

@interface NSObject (ValidateWithoutDelegate)
- (BOOL)validateUserInterfaceItemWithoutDelegate:(id <NSValidatedUserInterfaceItem>)item;
@end

@interface _WebSafeForwarder : NSObject
{
    id target; // Non-retained. Don't retain delegates.
    id defaultTarget;
    BOOL catchExceptions;
}
- (id)initWithTarget:(id)target defaultTarget:(id)defaultTarget catchExceptions:(BOOL)catchExceptions;
@end

@interface WebViewPrivate : NSObject {
@public
    Page* page;
    
    id UIDelegate;
    id UIDelegateForwarder;
    id resourceProgressDelegate;
    id downloadDelegate;
    id policyDelegate;
    id policyDelegateForwarder;
    id frameLoadDelegate;
    id frameLoadDelegateForwarder;
    id <WebFormDelegate> formDelegate;
    id editingDelegate;
    id editingDelegateForwarder;
    id scriptDebugDelegate;

    WebInspector *inspector;
    WebNodeHighlight *currentNodeHighlight;

    BOOL allowsUndo;
        
    float zoomMultiplier;

    NSString *applicationNameForUserAgent;
    String userAgent;
    BOOL userAgentOverridden;
    
    WebPreferences *preferences;
    BOOL useSiteSpecificSpoofing;

    NSWindow *hostWindow;

    int programmaticFocusCount;
    
    WebResourceDelegateImplementationCache resourceLoadDelegateImplementations;
    WebFrameLoadDelegateImplementationCache frameLoadDelegateImplementations;
    WebScriptDebugDelegateImplementationCache scriptDebugDelegateImplementations;

    void *observationInfo;
    
    BOOL closed;
    BOOL shouldCloseWithWindow;
    BOOL mainFrameDocumentReady;
    BOOL drawsBackground;
    BOOL editable;
    BOOL tabKeyCyclesThroughElementsChanged;
    BOOL becomingFirstResponder;
    BOOL becomingFirstResponderFromOutside;
    BOOL hoverFeedbackSuspended;
    BOOL usesPageCache;
    BOOL catchesDelegateExceptions;

    NSColor *backgroundColor;

    NSString *mediaStyle;
    
    BOOL hasSpellCheckerDocumentTag;
    NSInteger spellCheckerDocumentTag;

    BOOL smartInsertDeleteEnabled;
    BOOL selectTrailingWhitespaceEnabled;
        
#if ENABLE(DASHBOARD_SUPPORT)
    BOOL dashboardBehaviorAlwaysSendMouseEventsToAllWindows;
    BOOL dashboardBehaviorAlwaysSendActiveNullEventsToPlugIns;
    BOOL dashboardBehaviorAlwaysAcceptsFirstMouse;
    BOOL dashboardBehaviorAllowWheelScrolling;
#endif
    
    // WebKit has both a global plug-in database and a separate, per WebView plug-in database. Dashboard uses the per WebView database.
    WebPluginDatabase *pluginDatabase;
    
    HashMap<unsigned long, RetainPtr<id> > identifierMap;

    BOOL _keyboardUIModeAccessed;
    KeyboardUIMode _keyboardUIMode;

    BOOL shouldUpdateWhileOffscreen;
    
    // When this flag is set, we will not make any subviews underneath this WebView.  This means no WebFrameViews and no WebHTMLViews.
    BOOL useDocumentViews;

#if USE(ACCELERATED_COMPOSITING)
    // When this flag is set, next time a WebHTMLView draws, it needs to temporarily disable screen updates
    // so that the NSView drawing is visually synchronized with CALayer updates.
    BOOL needsOneShotDrawingSynchronization;
    // Number of WebHTMLViews using accelerated compositing. Used to implement _isUsingAcceleratedCompositing.
    int acceleratedFramesCount;
    // Run loop observer used to implement the compositing equivalent of -viewWillDraw
    CFRunLoopObserverRef viewUpdateRunLoopObserver;
#endif

    NSPasteboard *insertionPasteboard;
}
@end

@interface WebView (WebFileInternal)
- (WebFrame *)_selectedOrMainFrame;
- (BOOL)_isLoading;
- (WebFrameView *)_frameViewAtWindowPoint:(NSPoint)point;
- (WebFrame *)_focusedFrame;
+ (void)_preflightSpellChecker;
- (BOOL)_continuousCheckingAllowed;
- (NSResponder *)_responderForResponderOperations;
@end

@interface WebView (WebCallDelegateFunctions)
@end

static void patchMailRemoveAttributesMethod();

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

enum { WebViewVersion = 4 };

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

+ (void)initialize
{
    JSC::initializeThreading();
#ifndef BUILDING_ON_TIGER
    WebCoreObjCFinalizeOnMainThread(self);
#endif
}

- init 
{
    self = [super init];
    if (!self)
        return nil;
    JSC::initializeThreading();
    allowsUndo = YES;
    zoomMultiplier = 1;
#if ENABLE(DASHBOARD_SUPPORT)
    dashboardBehaviorAllowWheelScrolling = YES;
#endif
    shouldCloseWithWindow = objc_collecting_enabled();
    continuousSpellCheckingEnabled = [[NSUserDefaults standardUserDefaults] boolForKey:WebContinuousSpellCheckingEnabled];

#ifndef BUILDING_ON_TIGER
    grammarCheckingEnabled = [[NSUserDefaults standardUserDefaults] boolForKey:WebGrammarCheckingEnabled];
#endif
    
    usesPageCache = YES;
    
    pluginDatabaseClientCount++;

    shouldUpdateWhileOffscreen = YES;

    return self;
}

- (void)dealloc
{    
    ASSERT(applicationIsTerminating || !page);
    ASSERT(applicationIsTerminating || !preferences);
    ASSERT(!insertionPasteboard);

    [applicationNameForUserAgent release];
    [backgroundColor release];
    
    [inspector release];
    [currentNodeHighlight release];

    [hostWindow release];

    [policyDelegateForwarder release];
    [UIDelegateForwarder release];
    [frameLoadDelegateForwarder release];
    [editingDelegateForwarder release];
    
    [mediaStyle release];

    [super dealloc];
}

- (void)finalize
{
    ASSERT_MAIN_THREAD();

    ASSERT(!insertionPasteboard);

    [super finalize];
}

#if USE(ACCELERATED_COMPOSITING)
- (void)_clearViewUpdateRunLoopObserver
{
    if (viewUpdateRunLoopObserver) {
        CFRunLoopObserverInvalidate(viewUpdateRunLoopObserver);
        CFRelease(viewUpdateRunLoopObserver);
        viewUpdateRunLoopObserver = 0;
    }
}
#endif

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

static inline int callGestalt(OSType selector)
{
    SInt32 value = 0;
    Gestalt(selector, &value);
    return value;
}

// Uses underscores instead of dots because if "4." ever appears in a user agent string, old DHTML libraries treat it as Netscape 4.
static NSString *createMacOSXVersionString()
{
    // Can't use -[NSProcessInfo operatingSystemVersionString] because it has too much stuff we don't want.
    int major = callGestalt(gestaltSystemVersionMajor);
    ASSERT(major);

    int minor = callGestalt(gestaltSystemVersionMinor);
    int bugFix = callGestalt(gestaltSystemVersionBugFix);
    if (bugFix)
        return [[NSString alloc] initWithFormat:@"%d_%d_%d", major, minor, bugFix];
    if (minor)
        return [[NSString alloc] initWithFormat:@"%d_%d", major, minor];
    return [[NSString alloc] initWithFormat:@"%d", major];
}

static NSString *createUserVisibleWebKitVersionString()
{
    // If the version is 4 digits long or longer, then the first digit represents
    // the version of the OS. Our user agent string should not include this first digit,
    // so strip it off and report the rest as the version. <rdar://problem/4997547>
    NSString *fullVersion = [[NSBundle bundleForClass:[WebView class]] objectForInfoDictionaryKey:(NSString *)kCFBundleVersionKey];
    NSRange nonDigitRange = [fullVersion rangeOfCharacterFromSet:[[NSCharacterSet decimalDigitCharacterSet] invertedSet]];
    if (nonDigitRange.location == NSNotFound && [fullVersion length] >= 4)
        return [[fullVersion substringFromIndex:1] copy];
    if (nonDigitRange.location != NSNotFound && nonDigitRange.location >= 4)
        return [[fullVersion substringFromIndex:1] copy];
    return [fullVersion copy];
}

+ (NSString *)_standardUserAgentWithApplicationName:(NSString *)applicationName
{
    // Note: Do *not* move the initialization of osVersion nor webKitVersion into the declaration.
    // Garbage collection won't correctly mark the global variable in that case <rdar://problem/5733674>.
    static NSString *osVersion;
    static NSString *webKitVersion;
    if (!osVersion)
        osVersion = createMacOSXVersionString();
    if (!webKitVersion)
        webKitVersion = createUserVisibleWebKitVersionString();
    NSString *language = [NSUserDefaults _webkit_preferredLanguageCode];
    if ([applicationName length])
        return [NSString stringWithFormat:@"Mozilla/5.0 (Macintosh; U; " PROCESSOR " Mac OS X %@; %@) AppleWebKit/%@ (KHTML, like Gecko) %@", osVersion, language, webKitVersion, applicationName];
    return [NSString stringWithFormat:@"Mozilla/5.0 (Macintosh; U; " PROCESSOR " Mac OS X %@; %@) AppleWebKit/%@ (KHTML, like Gecko)", osVersion, language, webKitVersion];
}

static void WebKitInitializeApplicationCachePathIfNecessary()
{
    static BOOL initialized = NO;
    if (initialized)
        return;

    NSString *appName = [[NSBundle mainBundle] bundleIdentifier];
    if (!appName)
        appName = [[NSProcessInfo processInfo] processName];
    
    ASSERT(appName);

    NSString* cacheDir = [NSString _webkit_localCacheDirectoryWithBundleIdentifier:appName];

    cacheStorage().setCacheDirectory(cacheDir);
    initialized = YES;
}

static bool runningLeopardMail()
{
#ifdef BUILDING_ON_LEOPARD
    return [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.mail"];
#endif
    return NO;
}

static bool runningTigerMail()
{
#ifdef BUILDING_ON_TIGER
    return [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.mail"];
#endif
    return NO;    
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

- (BOOL)_usesDocumentViews
{
    return _private->useDocumentViews;
}

- (void)_commonInitializationWithFrameName:(NSString *)frameName groupName:(NSString *)groupName usesDocumentViews:(BOOL)usesDocumentViews
{
    WebCoreThreadViolationCheckRoundTwo();

#ifndef NDEBUG
    WTF::RefCountedLeakCounter::suppressMessages(webViewIsOpen);
#endif
    
    WebPreferences *standardPreferences = [WebPreferences standardPreferences];
    [standardPreferences willAddToWebView];

    _private->preferences = [standardPreferences retain];
    _private->catchesDelegateExceptions = YES;
    _private->mainFrameDocumentReady = NO;
    _private->drawsBackground = YES;
    _private->smartInsertDeleteEnabled = YES;
    _private->backgroundColor = [[NSColor colorWithDeviceWhite:1 alpha:1] retain];
    _private->useDocumentViews = usesDocumentViews;

    WebFrameView *frameView = nil;
    if (_private->useDocumentViews) {
        NSRect f = [self frame];
        frameView = [[WebFrameView alloc] initWithFrame: NSMakeRect(0,0,f.size.width,f.size.height)];
        [frameView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [self addSubview:frameView];
        [frameView release];
    }

    static bool didOneTimeInitialization = false;
    if (!didOneTimeInitialization) {
        WebKitInitializeLoggingChannelsIfNecessary();
        WebCore::InitializeLoggingChannelsIfNecessary();
        [WebHistoryItem initWindowWatcherIfNecessary];
        WebKitInitializeDatabasesIfNecessary();
        WebKitInitializeApplicationCachePathIfNecessary();
        patchMailRemoveAttributesMethod();
        didOneTimeInitialization = true;
    }

    _private->page = new Page(new WebChromeClient(self), new WebContextMenuClient(self), new WebEditorClient(self), new WebDragClient(self), new WebInspectorClient(self));

    _private->page->settings()->setLocalStorageDatabasePath([[self preferences] _localStorageDatabasePath]);

    [WebFrame _createMainFrameWithPage:_private->page frameName:frameName frameView:frameView];

#ifndef BUILDING_ON_TIGER
    NSRunLoop *runLoop = [NSRunLoop mainRunLoop];
#else
    NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
#endif

    if (WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_LOADING_DURING_COMMON_RUNLOOP_MODES))
        [self scheduleInRunLoop:runLoop forMode:(NSString *)kCFRunLoopCommonModes];
    else
        [self scheduleInRunLoop:runLoop forMode:NSDefaultRunLoopMode];

    [self _addToAllWebViewsSet];
    [self setGroupName:groupName];
    
    // If there's already a next key view (e.g., from a nib), wire it up to our
    // contained frame view. In any case, wire our next key view up to the our
    // contained frame view. This works together with our becomeFirstResponder 
    // and setNextKeyView overrides.
    NSView *nextKeyView = [self nextKeyView];
    if (nextKeyView && nextKeyView != frameView)
        [frameView setNextKeyView:nextKeyView];
    [super setNextKeyView:frameView];

    ++WebViewCount;

    [self _registerDraggedTypes];

    WebPreferences *prefs = [self preferences];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesChangedNotification:)
                                                 name:WebPreferencesChangedNotification object:prefs];

    // Post a notification so the WebCore settings update.
    [[self preferences] _postPreferencesChangesNotification];

    if (!WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_LOCAL_RESOURCE_SECURITY_RESTRICTION)) {
        // Originally, we allowed all local loads.
        FrameLoader::setLocalLoadPolicy(FrameLoader::AllowLocalLoadsForAll);
    } else if (!WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_MORE_STRICT_LOCAL_RESOURCE_SECURITY_RESTRICTION)) {
        // Later, we allowed local loads for local URLs and documents loaded
        // with substitute data.
        FrameLoader::setLocalLoadPolicy(FrameLoader::AllowLocalLoadsForLocalAndSubstituteData);
    }
}

- (id)_initWithFrame:(NSRect)f frameName:(NSString *)frameName groupName:(NSString *)groupName usesDocumentViews:(BOOL)usesDocumentViews
{
    self = [super initWithFrame:f];
    if (!self)
        return nil;

#ifdef ENABLE_WEBKIT_UNSET_DYLD_FRAMEWORK_PATH
    // DYLD_FRAMEWORK_PATH is used so Safari will load the development version of WebKit, which
    // may not work with other WebKit applications.  Unsetting DYLD_FRAMEWORK_PATH removes the
    // need for Safari to unset it to prevent it from being passed to applications it launches.
    // Unsetting it when a WebView is first created is as good a place as any.
    // See <http://bugs.webkit.org/show_bug.cgi?id=4286> for more details.
    if (getenv("WEBKIT_UNSET_DYLD_FRAMEWORK_PATH")) {
        unsetenv("DYLD_FRAMEWORK_PATH");
        unsetenv("WEBKIT_UNSET_DYLD_FRAMEWORK_PATH");
    }
#endif

    _private = [[WebViewPrivate alloc] init];
    [self _commonInitializationWithFrameName:frameName groupName:groupName usesDocumentViews:usesDocumentViews];
    [self setMaintainsBackForwardList: YES];
    return self;
}

- (void)_boundsChanged
{
    Frame* frame = core([self mainFrame]);
    IntSize oldSize = frame->view()->frameRect().size();
    frame->view()->resize([self bounds].size.width, [self bounds].size.height);
    if (oldSize != frame->view()->frameRect().size())
        [self setNeedsDisplay: YES];
}

- (BOOL)_mustDrawUnionedRect:(NSRect)rect singleRects:(const NSRect *)rects count:(NSInteger)count
{
    // If count == 0 here, use the rect passed in for drawing. This is a workaround for:
    // <rdar://problem/3908282> REGRESSION (Mail): No drag image dragging selected text in Blot and Mail
    // The reason for the workaround is that this method is called explicitly from the code
    // to generate a drag image, and at that time, getRectsBeingDrawn:count: will return a zero count.
    const int cRectThreshold = 10;
    const float cWastedSpaceThreshold = 0.75f;
    BOOL useUnionedRect = (count <= 1) || (count > cRectThreshold);
    if (!useUnionedRect) {
        // Attempt to guess whether or not we should use the unioned rect or the individual rects.
        // We do this by computing the percentage of "wasted space" in the union.  If that wasted space
        // is too large, then we will do individual rect painting instead.
        float unionPixels = (rect.size.width * rect.size.height);
        float singlePixels = 0;
        for (int i = 0; i < count; ++i)
            singlePixels += rects[i].size.width * rects[i].size.height;
        float wastedSpace = 1 - (singlePixels / unionPixels);
        if (wastedSpace <= cWastedSpaceThreshold)
            useUnionedRect = YES;
    }
    return useUnionedRect;
}

- (void)drawSingleRect:(NSRect)rect
{
    ASSERT(!_private->useDocumentViews);
    
    [NSGraphicsContext saveGraphicsState];
    NSRectClip(rect);

    @try {
        [[self mainFrame] _drawRect:rect contentsOnly:NO];

        WebView *webView = [self _webView];
        [[webView _UIDelegateForwarder] webView:webView didDrawRect:rect];

        if (WebNodeHighlight *currentHighlight = [webView currentNodeHighlight])
            [currentHighlight setNeedsUpdateInTargetViewRect:rect];

        [NSGraphicsContext restoreGraphicsState];
    } @catch (NSException *localException) {
        [NSGraphicsContext restoreGraphicsState];
        LOG_ERROR("Exception caught while drawing: %@", localException);
        [localException raise];
    }
}

- (BOOL)isFlipped 
{
    return _private && !_private->useDocumentViews;
}

#if USE(ACCELERATED_COMPOSITING) || !defined(BUILDING_ON_TIGER)
- (void)_viewWillDrawInternal
{
    Frame* frame = core([self mainFrame]);
    if (frame && frame->view())
        frame->view()->layoutIfNeededRecursive();
}
#endif

#ifndef BUILDING_ON_TIGER

- (void)viewWillDraw
{
    if (!_private->useDocumentViews)
        [self _viewWillDrawInternal];
    [super viewWillDraw];
}

#endif


- (void)drawRect:(NSRect)rect
{
    if (_private->useDocumentViews)
        return [super drawRect:rect];
    
    ASSERT_MAIN_THREAD();

    const NSRect *rects;
    NSInteger count;
    [self getRectsBeingDrawn:&rects count:&count];

    
    if ([self _mustDrawUnionedRect:rect singleRects:rects count:count])
        [self drawSingleRect:rect];
    else
        for (int i = 0; i < count; ++i)
            [self drawSingleRect:rects[i]];
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

+ (BOOL)_viewClass:(Class *)vClass andRepresentationClass:(Class *)rClass forMIMEType:(NSString *)MIMEType
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

- (BOOL)_viewClass:(Class *)vClass andRepresentationClass:(Class *)rClass forMIMEType:(NSString *)MIMEType
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
    [self _setAlwaysUsesComplexTextCodePath:f];
}

+ (void)_setAlwaysUsesComplexTextCodePath:(BOOL)f
{
    WebCoreSetAlwaysUsesComplexTextCodePath(f);
}

+ (BOOL)canShowFile:(NSString *)path
{
    return [[self class] canShowMIMEType:[WebView _MIMETypeForFile:path]];
}

+ (NSString *)suggestedFileExtensionForMIMEType:(NSString *)type
{
    return WKGetPreferredExtensionForMIMEType(type);
}

- (BOOL)_isClosed
{
    return !_private || _private->closed;
}

- (void)_closePluginDatabases
{
    pluginDatabaseClientCount--;

    // Close both sets of plug-in databases because plug-ins need an opportunity to clean up files, etc.

    // Unload the WebView local plug-in database. 
    if (_private->pluginDatabase) {
        [_private->pluginDatabase destroyAllPluginInstanceViews];
        [_private->pluginDatabase close];
        [_private->pluginDatabase release];
        _private->pluginDatabase = nil;
    }
    
    // Keep the global plug-in database active until the app terminates to avoid having to reload plug-in bundles.
    if (!pluginDatabaseClientCount && applicationIsTerminating)
        [WebPluginDatabase closeSharedDatabase];
}

- (void)_closeWithFastTeardown 
{
#ifndef NDEBUG
    WTF::RefCountedLeakCounter::suppressMessages("At least one WebView was closed with fast teardown.");
#endif

    // Dispatch unload events.
    // FIXME: Shouldn't have to use a RefPtr here -- keeping the frame alive while stopping it
    // should be WebCore's responsibility -- but we do as of the time this comment was written.
    RefPtr<Frame> mainFrame = core([self mainFrame]);
    if (mainFrame && mainFrame->page() && mainFrame->page()->pendingUnloadEventCount())
        mainFrame->loader()->stopLoading(true);

    _private->closed = YES;

    [self _closePluginDatabases];
}

// _close is here only for backward compatibility; clients and subclasses should use
// public method -close instead.
- (void)_close
{
    if (!_private || _private->closed)
        return;

#ifndef NDEBUG
    WTF::RefCountedLeakCounter::cancelMessageSuppression(webViewIsOpen);
#endif
    
    WebPreferences *preferences = _private->preferences;
    BOOL fullDocumentTeardown = [preferences fullDocumentTeardownEnabled];
     
    // To quit the apps fast we skip document teardown.  Two exceptions: 
    //    1) plugins need to be destroyed and unloaded
    //    2) unload events need to be called
    if (applicationIsTerminating && !fullDocumentTeardown) {
        [self _closeWithFastTeardown];
        return;
    }

    if (Frame* mainFrame = core([self mainFrame]))
        mainFrame->loader()->detachFromParent();

    [self _removeFromAllWebViewsSet];
    [self setHostWindow:nil];

    [self setDownloadDelegate:nil];
    [self setEditingDelegate:nil];
    [self setFrameLoadDelegate:nil];
    [self setPolicyDelegate:nil];
    [self setResourceLoadDelegate:nil];
    [self setScriptDebugDelegate:nil];
    [self setUIDelegate:nil];

    [_private->inspector webViewClosed];

    // setHostWindow:nil must be called before this value is set (see 5408186)
    _private->closed = YES;

    // To avoid leaks, call removeDragCaret in case it wasn't called after moveDragCaretToPoint.
    [self removeDragCaret];

    // Deleteing the WebCore::Page will clear the page cache so we call destroy on 
    // all the plug-ins in the page cache to break any retain cycles.
    // See comment in HistoryItem::releaseAllPendingPageCaches() for more information.
    delete _private->page;
    _private->page = 0;

    if (_private->hasSpellCheckerDocumentTag) {
        [[NSSpellChecker sharedSpellChecker] closeSpellDocumentWithTag:_private->spellCheckerDocumentTag];
        _private->hasSpellCheckerDocumentTag = NO;
    }

#if USE(ACCELERATED_COMPOSITING)
    [_private _clearViewUpdateRunLoopObserver];
#endif
    
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [WebPreferences _removeReferenceForIdentifier:[self preferencesIdentifier]];

    _private->preferences = nil;
    [preferences didRemoveFromWebView];
    [preferences release];

    [self _closePluginDatabases];

#ifndef NDEBUG
    // Need this to make leak messages accurate.
    if (applicationIsTerminating) {
        gcController().garbageCollectNow();
        [WebCache setDisabled:YES];
    }
#endif
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

- (WebDownload *)_downloadURL:(NSURL *)URL
{
    ASSERT(URL);
    
    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:URL];
    WebDownload *download = [WebDownload _downloadWithRequest:request
                                                     delegate:_private->downloadDelegate
                                                    directory:nil];
    [request release];
    
    return download;
}

- (WebView *)_openNewWindowWithRequest:(NSURLRequest *)request
{
    NSDictionary *features = [[NSDictionary alloc] init];
    WebView *newWindowWebView = [[self _UIDelegateForwarder] webView:self
                                            createWebViewWithRequest:nil
                                                      windowFeatures:features];
    [features release];
    if (!newWindowWebView)
        return nil;

    CallUIDelegate(newWindowWebView, @selector(webViewShow:));
    return newWindowWebView;
}

- (WebInspector *)inspector
{
    if (!_private->inspector)
        _private->inspector = [[WebInspector alloc] initWithWebView:self];
    return _private->inspector;
}

- (WebCore::Page*)page
{
    return _private->page;
}

- (NSMenu *)_menuForElement:(NSDictionary *)element defaultItems:(NSArray *)items
{
    NSArray *defaultMenuItems = [[WebDefaultUIDelegate sharedUIDelegate] webView:self contextMenuItemsForElement:element defaultMenuItems:items];

    NSArray *menuItems = CallUIDelegate(self, @selector(webView:contextMenuItemsForElement:defaultMenuItems:), element, defaultMenuItems);
    if (!menuItems)
        return nil;

    unsigned count = [menuItems count];
    if (!count)
        return nil;

    NSMenu *menu = [[NSMenu alloc] init];
    for (unsigned i = 0; i < count; i++)
        [menu addItem:[menuItems objectAtIndex:i]];

    return [menu autorelease];
}

- (void)_mouseDidMoveOverElement:(NSDictionary *)dictionary modifierFlags:(NSUInteger)modifierFlags
{
    // We originally intended to call this delegate method sometimes with a nil dictionary, but due to
    // a bug dating back to WebKit 1.0 this delegate was never called with nil! Unfortunately we can't
    // start calling this with nil since it will break Adobe Help Viewer, and possibly other clients.
    if (!dictionary)
        return;
    CallUIDelegate(self, @selector(webView:mouseDidMoveOverElement:modifierFlags:), dictionary, modifierFlags);
}

- (void)_loadBackForwardListFromOtherView:(WebView *)otherView
{
    if (!_private->page)
        return;
    
    if (!otherView->_private->page)
        return;
    
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

- (BOOL)_needsAdobeFrameReloadingQuirk
{
    static BOOL checked = NO;
    static BOOL needsQuirk = NO;

    if (checked)
        return needsQuirk;

    needsQuirk = WKAppVersionCheckLessThan(@"com.adobe.Acrobat", -1, 9.0)
        || WKAppVersionCheckLessThan(@"com.adobe.Acrobat.Pro", -1, 9.0)
        || WKAppVersionCheckLessThan(@"com.adobe.Reader", -1, 9.0)
        || WKAppVersionCheckLessThan(@"com.adobe.distiller", -1, 9.0)
        || WKAppVersionCheckLessThan(@"com.adobe.Contribute", -1, 4.2)
        || WKAppVersionCheckLessThan(@"com.adobe.dreamweaver-9.0", -1, 9.1)
        || WKAppVersionCheckLessThan(@"com.macromedia.fireworks", -1, 9.1)
        || WKAppVersionCheckLessThan(@"com.adobe.InCopy", -1, 5.1)
        || WKAppVersionCheckLessThan(@"com.adobe.InDesign", -1, 5.1)
        || WKAppVersionCheckLessThan(@"com.adobe.Soundbooth", -1, 2);
    checked = YES;

    return needsQuirk;
}

- (BOOL)_needsKeyboardEventDisambiguationQuirks
{
    static BOOL checked = NO;
    static BOOL needsQuirks = NO;

    if (checked)
        return needsQuirks;

    needsQuirks = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_IE_COMPATIBLE_KEYBOARD_EVENT_DISPATCH)
        && ![[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.Safari"];
    checked = YES;

    return needsQuirks;
}

- (void)_preferencesChangedNotification:(NSNotification *)notification
{
    WebPreferences *preferences = (WebPreferences *)[notification object];
    ASSERT(preferences == [self preferences]);
    
    if (!_private->userAgentOverridden)
        _private->userAgent = String();

    // Cache this value so we don't have to read NSUserDefaults on each page load
    _private->useSiteSpecificSpoofing = [preferences _useSiteSpecificSpoofing];

    // Update corresponding WebCore Settings object.
    if (!_private->page)
        return;
    
    Settings* settings = _private->page->settings();
    
    settings->setCursiveFontFamily([preferences cursiveFontFamily]);
    settings->setDefaultFixedFontSize([preferences defaultFixedFontSize]);
    settings->setDefaultFontSize([preferences defaultFontSize]);
    settings->setDefaultTextEncodingName([preferences defaultTextEncodingName]);
    settings->setUsesEncodingDetector([preferences usesEncodingDetector]);
    settings->setFantasyFontFamily([preferences fantasyFontFamily]);
    settings->setFixedFontFamily([preferences fixedFontFamily]);
    settings->setForceFTPDirectoryListings([preferences _forceFTPDirectoryListings]);
    settings->setFTPDirectoryTemplatePath([preferences _ftpDirectoryTemplatePath]);
    settings->setLocalStorageDatabasePath([preferences _localStorageDatabasePath]);
    settings->setJavaEnabled([preferences isJavaEnabled]);
    settings->setJavaScriptEnabled([preferences isJavaScriptEnabled]);
    settings->setWebSecurityEnabled([preferences isWebSecurityEnabled]);
    settings->setAllowUniversalAccessFromFileURLs([preferences allowUniversalAccessFromFileURLs]);
    settings->setJavaScriptCanOpenWindowsAutomatically([preferences javaScriptCanOpenWindowsAutomatically]);
    settings->setMinimumFontSize([preferences minimumFontSize]);
    settings->setMinimumLogicalFontSize([preferences minimumLogicalFontSize]);
    settings->setPluginsEnabled([preferences arePlugInsEnabled]);
    settings->setDatabasesEnabled([preferences databasesEnabled]);
    settings->setLocalStorageEnabled([preferences localStorageEnabled]);
    settings->setPrivateBrowsingEnabled([preferences privateBrowsingEnabled]);
    settings->setSansSerifFontFamily([preferences sansSerifFontFamily]);
    settings->setSerifFontFamily([preferences serifFontFamily]);
    settings->setStandardFontFamily([preferences standardFontFamily]);
    settings->setLoadsImagesAutomatically([preferences loadsImagesAutomatically]);
    settings->setShouldPrintBackgrounds([preferences shouldPrintBackgrounds]);
    settings->setTextAreasAreResizable([preferences textAreasAreResizable]);
    settings->setShrinksStandaloneImagesToFit([preferences shrinksStandaloneImagesToFit]);
    settings->setEditableLinkBehavior(core([preferences editableLinkBehavior]));
    settings->setTextDirectionSubmenuInclusionBehavior(core([preferences textDirectionSubmenuInclusionBehavior]));
    settings->setDOMPasteAllowed([preferences isDOMPasteAllowed]);
    settings->setUsesPageCache([self usesPageCache]);
    settings->setShowsURLsInToolTips([preferences showsURLsInToolTips]);
    settings->setDeveloperExtrasEnabled([preferences developerExtrasEnabled]);
    settings->setAuthorAndUserStylesEnabled([preferences authorAndUserStylesEnabled]);
    settings->setApplicationChromeMode([preferences applicationChromeModeEnabled]);
    if ([preferences userStyleSheetEnabled]) {
        NSString* location = [[preferences userStyleSheetLocation] _web_originalDataAsString];
        settings->setUserStyleSheetLocation([NSURL URLWithString:(location ? location : @"")]);
    } else
        settings->setUserStyleSheetLocation([NSURL URLWithString:@""]);
    settings->setNeedsAdobeFrameReloadingQuirk([self _needsAdobeFrameReloadingQuirk]);
    settings->setNeedsKeyboardEventDisambiguationQuirks([self _needsKeyboardEventDisambiguationQuirks]);
    settings->setNeedsLeopardMailQuirks(runningLeopardMail());
    settings->setNeedsTigerMailQuirks(runningTigerMail());
    settings->setNeedsSiteSpecificQuirks(_private->useSiteSpecificSpoofing);
    settings->setWebArchiveDebugModeEnabled([preferences webArchiveDebugModeEnabled]);
    settings->setOfflineWebApplicationCacheEnabled([preferences offlineWebApplicationCacheEnabled]);
    settings->setZoomsTextOnly([preferences zoomsTextOnly]);
    settings->setEnforceCSSMIMETypeInStrictMode(!WKAppVersionCheckLessThan(@"com.apple.iWeb", -1, 2.1));
}

static inline IMP getMethod(id o, SEL s)
{
    return [o respondsToSelector:s] ? [o methodForSelector:s] : 0;
}

- (void)_cacheResourceLoadDelegateImplementations
{
    WebResourceDelegateImplementationCache *cache = &_private->resourceLoadDelegateImplementations;
    id delegate = _private->resourceProgressDelegate;

    if (!delegate) {
        bzero(cache, sizeof(WebResourceDelegateImplementationCache));
        return;
    }

    cache->didCancelAuthenticationChallengeFunc = getMethod(delegate, @selector(webView:resource:didReceiveAuthenticationChallenge:fromDataSource:));
    cache->didFailLoadingWithErrorFromDataSourceFunc = getMethod(delegate, @selector(webView:resource:didFailLoadingWithError:fromDataSource:));
    cache->didFinishLoadingFromDataSourceFunc = getMethod(delegate, @selector(webView:resource:didFinishLoadingFromDataSource:));
    cache->didLoadResourceFromMemoryCacheFunc = getMethod(delegate, @selector(webView:didLoadResourceFromMemoryCache:response:length:fromDataSource:));
    cache->didReceiveAuthenticationChallengeFunc = getMethod(delegate, @selector(webView:resource:didReceiveAuthenticationChallenge:fromDataSource:));
    cache->didReceiveContentLengthFunc = getMethod(delegate, @selector(webView:resource:didReceiveContentLength:fromDataSource:));
    cache->didReceiveResponseFunc = getMethod(delegate, @selector(webView:resource:didReceiveResponse:fromDataSource:));
    cache->identifierForRequestFunc = getMethod(delegate, @selector(webView:identifierForInitialRequest:fromDataSource:));
    cache->plugInFailedWithErrorFunc = getMethod(delegate, @selector(webView:plugInFailedWithError:dataSource:));
    cache->willCacheResponseFunc = getMethod(delegate, @selector(webView:resource:willCacheResponse:fromDataSource:));
    cache->willSendRequestFunc = getMethod(delegate, @selector(webView:resource:willSendRequest:redirectResponse:fromDataSource:));
    cache->shouldUseCredentialStorageFunc = getMethod(delegate, @selector(webView:resource:shouldUseCredentialStorageForDataSource:));
}

WebResourceDelegateImplementationCache* WebViewGetResourceLoadDelegateImplementations(WebView *webView)
{
    static WebResourceDelegateImplementationCache empty;
    if (!webView)
        return &empty;
    return &webView->_private->resourceLoadDelegateImplementations;
}

- (void)_cacheFrameLoadDelegateImplementations
{
    WebFrameLoadDelegateImplementationCache *cache = &_private->frameLoadDelegateImplementations;
    id delegate = _private->frameLoadDelegate;

    if (!delegate) {
        bzero(cache, sizeof(WebFrameLoadDelegateImplementationCache));
        return;
    }

    cache->didCancelClientRedirectForFrameFunc = getMethod(delegate, @selector(webView:didCancelClientRedirectForFrame:));
    cache->didChangeLocationWithinPageForFrameFunc = getMethod(delegate, @selector(webView:didChangeLocationWithinPageForFrame:));
    cache->didClearWindowObjectForFrameFunc = getMethod(delegate, @selector(webView:didClearWindowObject:forFrame:));
    cache->didCommitLoadForFrameFunc = getMethod(delegate, @selector(webView:didCommitLoadForFrame:));
    cache->didFailLoadWithErrorForFrameFunc = getMethod(delegate, @selector(webView:didFailLoadWithError:forFrame:));
    cache->didFailProvisionalLoadWithErrorForFrameFunc = getMethod(delegate, @selector(webView:didFailProvisionalLoadWithError:forFrame:));
    cache->didFinishDocumentLoadForFrameFunc = getMethod(delegate, @selector(webView:didFinishDocumentLoadForFrame:));
    cache->didFinishLoadForFrameFunc = getMethod(delegate, @selector(webView:didFinishLoadForFrame:));
    cache->didFirstLayoutInFrameFunc = getMethod(delegate, @selector(webView:didFirstLayoutInFrame:));
    cache->didFirstVisuallyNonEmptyLayoutInFrameFunc = getMethod(delegate, @selector(webView:didFirstVisuallyNonEmptyLayoutInFrame:));
    cache->didHandleOnloadEventsForFrameFunc = getMethod(delegate, @selector(webView:didHandleOnloadEventsForFrame:));
    cache->didReceiveIconForFrameFunc = getMethod(delegate, @selector(webView:didReceiveIcon:forFrame:));
    cache->didReceiveServerRedirectForProvisionalLoadForFrameFunc = getMethod(delegate, @selector(webView:didReceiveServerRedirectForProvisionalLoadForFrame:));
    cache->didReceiveTitleForFrameFunc = getMethod(delegate, @selector(webView:didReceiveTitle:forFrame:));
    cache->didStartProvisionalLoadForFrameFunc = getMethod(delegate, @selector(webView:didStartProvisionalLoadForFrame:));
    cache->willCloseFrameFunc = getMethod(delegate, @selector(webView:willCloseFrame:));
    cache->willPerformClientRedirectToURLDelayFireDateForFrameFunc = getMethod(delegate, @selector(webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:));
    cache->windowScriptObjectAvailableFunc = getMethod(delegate, @selector(webView:windowScriptObjectAvailable:));
}

WebFrameLoadDelegateImplementationCache* WebViewGetFrameLoadDelegateImplementations(WebView *webView)
{
    static WebFrameLoadDelegateImplementationCache empty;
    if (!webView)
        return &empty;
    return &webView->_private->frameLoadDelegateImplementations;
}

- (void)_cacheScriptDebugDelegateImplementations
{
    WebScriptDebugDelegateImplementationCache *cache = &_private->scriptDebugDelegateImplementations;
    id delegate = _private->scriptDebugDelegate;

    if (!delegate) {
        bzero(cache, sizeof(WebScriptDebugDelegateImplementationCache));
        return;
    }

    cache->didParseSourceFunc = getMethod(delegate, @selector(webView:didParseSource:baseLineNumber:fromURL:sourceId:forWebFrame:));
    if (cache->didParseSourceFunc)
        cache->didParseSourceExpectsBaseLineNumber = YES;
    else
        cache->didParseSourceFunc = getMethod(delegate, @selector(webView:didParseSource:fromURL:sourceId:forWebFrame:));

    cache->failedToParseSourceFunc = getMethod(delegate, @selector(webView:failedToParseSource:baseLineNumber:fromURL:withError:forWebFrame:));
    cache->didEnterCallFrameFunc = getMethod(delegate, @selector(webView:didEnterCallFrame:sourceId:line:forWebFrame:));
    cache->willExecuteStatementFunc = getMethod(delegate, @selector(webView:willExecuteStatement:sourceId:line:forWebFrame:));
    cache->willLeaveCallFrameFunc = getMethod(delegate, @selector(webView:willLeaveCallFrame:sourceId:line:forWebFrame:));
    cache->exceptionWasRaisedFunc = getMethod(delegate, @selector(webView:exceptionWasRaised:sourceId:line:forWebFrame:));
}

WebScriptDebugDelegateImplementationCache* WebViewGetScriptDebugDelegateImplementations(WebView *webView)
{
    static WebScriptDebugDelegateImplementationCache empty;
    if (!webView)
        return &empty;
    return &webView->_private->scriptDebugDelegateImplementations;
}

- (id)_policyDelegateForwarder
{
    if (!_private->policyDelegateForwarder)
        _private->policyDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget:_private->policyDelegate defaultTarget:[WebDefaultPolicyDelegate sharedPolicyDelegate] catchExceptions:_private->catchesDelegateExceptions];
    return _private->policyDelegateForwarder;
}

- (id)_UIDelegateForwarder
{
    if (!_private->UIDelegateForwarder)
        _private->UIDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget:_private->UIDelegate defaultTarget:[WebDefaultUIDelegate sharedUIDelegate] catchExceptions:_private->catchesDelegateExceptions];
    return _private->UIDelegateForwarder;
}

- (id)_editingDelegateForwarder
{
    // This can be called during window deallocation by QTMovieView in the QuickTime Cocoa Plug-in.
    // Not sure if that is a bug or not.
    if (!_private)
        return nil;

    if (!_private->editingDelegateForwarder)
        _private->editingDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget:_private->editingDelegate defaultTarget:[WebDefaultEditingDelegate sharedEditingDelegate] catchExceptions:_private->catchesDelegateExceptions];
    return _private->editingDelegateForwarder;
}

- (void)_closeWindow
{
    [[self _UIDelegateForwarder] webViewClose:self];
}

+ (void)_unregisterViewClassAndRepresentationClassForMIMEType:(NSString *)MIMEType
{
    [[WebFrameView _viewTypesAllowImageTypeOmission:NO] removeObjectForKey:MIMEType];
    [[WebDataSource _repTypesAllowImageTypeOmission:NO] removeObjectForKey:MIMEType];
    
    // FIXME: We also need to maintain MIMEType registrations (which can be dynamically changed)
    // in the WebCore MIMEType registry.  For now we're doing this in a safe, limited manner
    // to fix <rdar://problem/5372989> - a future revamping of the entire system is neccesary for future robustness
    MIMETypeRegistry::getSupportedNonImageMIMETypes().remove(MIMEType);
}

+ (void)_registerViewClass:(Class)viewClass representationClass:(Class)representationClass forURLScheme:(NSString *)URLScheme
{
    NSString *MIMEType = [self _generatedMIMETypeForURLScheme:URLScheme];
    [self registerViewClass:viewClass representationClass:representationClass forMIMEType:MIMEType];

    // FIXME: We also need to maintain MIMEType registrations (which can be dynamically changed)
    // in the WebCore MIMEType registry.  For now we're doing this in a safe, limited manner
    // to fix <rdar://problem/5372989> - a future revamping of the entire system is neccesary for future robustness
    if ([viewClass class] == [WebHTMLView class])
        MIMETypeRegistry::getSupportedNonImageMIMETypes().add(MIMEType);
    
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

+ (BOOL)_canHandleRequest:(NSURLRequest *)request forMainFrame:(BOOL)forMainFrame
{
    // FIXME: If <rdar://problem/5217309> gets fixed, this check can be removed.
    if (!request)
        return NO;

    if ([NSURLConnection canHandleRequest:request])
        return YES;

    NSString *scheme = [[request URL] scheme];

    // Representations for URL schemes work at the top level.
    if (forMainFrame && [self _representationExistsForURLScheme:scheme])
        return YES;
        
    return [scheme _webkit_isCaseInsensitiveEqualToString:@"applewebdata"];
}

+ (BOOL)_canHandleRequest:(NSURLRequest *)request
{
    return [self _canHandleRequest:request forMainFrame:YES];
}

+ (NSString *)_decodeData:(NSData *)data
{
    HTMLNames::init(); // this method is used for importing bookmarks at startup, so HTMLNames are likely to be uninitialized yet
    RefPtr<TextResourceDecoder> decoder = TextResourceDecoder::create("text/html"); // bookmark files are HTML
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

+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key {
    static NSSet *manualNotifyKeys = nil;
    if (!manualNotifyKeys)
        manualNotifyKeys = [[NSSet alloc] initWithObjects:_WebMainFrameURLKey, _WebIsLoadingKey, _WebEstimatedProgressKey,
            _WebCanGoBackKey, _WebCanGoForwardKey, _WebMainFrameTitleKey, _WebMainFrameIconKey, _WebMainFrameDocumentKey,
#if USE(ACCELERATED_COMPOSITING)
            UsingAcceleratedCompositingProperty, // used by DRT
#endif            
            nil];
    if ([manualNotifyKeys containsObject:key])
        return NO;
    return YES;
}

- (NSArray *)_declaredKeys {
    static NSArray *declaredKeys = nil;
    if (!declaredKeys)
        declaredKeys = [[NSArray alloc] initWithObjects:_WebMainFrameURLKey, _WebIsLoadingKey, _WebEstimatedProgressKey,
            _WebCanGoBackKey, _WebCanGoForwardKey, _WebMainFrameTitleKey, _WebMainFrameIconKey, _WebMainFrameDocumentKey, nil];
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
    if (frame == [self mainFrame])
        [self _didChangeValueForKey: _WebMainFrameURLKey];
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

- (NSCachedURLResponse *)_cachedResponseForURL:(NSURL *)URL
{
    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:URL];
    [request _web_setHTTPUserAgent:[self userAgentForURL:URL]];
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
                          types:types
                         source:nil];
}

- (void)_writeLinkElement:(NSDictionary *)element withPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    [pasteboard _web_writeURL:[element objectForKey:WebElementLinkURLKey]
                     andTitle:[element objectForKey:WebElementLinkLabelKey]
                        types:types];
}

- (void)_setInitiatedDrag:(BOOL)initiatedDrag
{
    if (!_private->page)
        return;
    _private->page->dragController()->setDidInitiateDrag(initiatedDrag);
}

#if ENABLE(DASHBOARD_SUPPORT)

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
    Frame* mainFrame = core([self mainFrame]);
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
    if (behavior == WebDashboardBehaviorAllowWheelScrolling && flag == NO && _private->page)
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
            if (_private->page)
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
            return _private->page && _private->page->settings()->usesDashboardBackwardCompatibilityMode();
        }
    }
    return NO;
}

#endif /* ENABLE(DASHBOARD_SUPPORT) */

+ (void)_setShouldUseFontSmoothing:(BOOL)f
{
    WebCoreSetShouldUseFontSmoothing(f);
}

+ (BOOL)_shouldUseFontSmoothing
{
    return WebCoreShouldUseFontSmoothing();
}

+ (void)_setUsesTestModeFocusRingColor:(BOOL)f
{
    setUsesTestModeFocusRingColor(f);
}

+ (BOOL)_usesTestModeFocusRingColor
{
    return usesTestModeFocusRingColor();
}

// This is only used by versions of Safari up to and including 3.0 and should be removed in a future release. 
+ (NSString *)_minimumRequiredSafariBuildNumber
{
    return @"420+";
}

- (void)setAlwaysShowVerticalScroller:(BOOL)flag
{
    WebDynamicScrollBarsView *scrollview = [[[self mainFrame] frameView] _scrollView];
    if (flag) {
        [scrollview setVerticalScrollingMode:ScrollbarAlwaysOn andLock:YES];
    } else {
        [scrollview setVerticalScrollingModeLocked:NO];
        [scrollview setVerticalScrollingMode:ScrollbarAuto andLock:NO];
    }
}

- (BOOL)alwaysShowVerticalScroller
{
    WebDynamicScrollBarsView *scrollview = [[[self mainFrame] frameView] _scrollView];
    return [scrollview verticalScrollingModeLocked] && [scrollview verticalScrollingMode] == ScrollbarAlwaysOn;
}

- (void)setAlwaysShowHorizontalScroller:(BOOL)flag
{
    WebDynamicScrollBarsView *scrollview = [[[self mainFrame] frameView] _scrollView];
    if (flag) {
        [scrollview setHorizontalScrollingMode:ScrollbarAlwaysOn andLock:YES];
    } else {
        [scrollview setHorizontalScrollingModeLocked:NO];
        [scrollview setHorizontalScrollingMode:ScrollbarAuto andLock:NO];
    }
}

- (void)setProhibitsMainFrameScrolling:(BOOL)prohibits
{
    Frame* mainFrame = core([self mainFrame]);
    if (mainFrame)
        mainFrame->view()->setProhibitsScrolling(prohibits);
}

- (BOOL)alwaysShowHorizontalScroller
{
    WebDynamicScrollBarsView *scrollview = [[[self mainFrame] frameView] _scrollView];
    return [scrollview horizontalScrollingModeLocked] && [scrollview horizontalScrollingMode] == ScrollbarAlwaysOn;
}

- (void)_setInViewSourceMode:(BOOL)flag
{
    Frame* mainFrame = core([self mainFrame]);
    if (mainFrame)
        mainFrame->setInViewSourceMode(flag);
}

- (BOOL)_inViewSourceMode
{
    Frame* mainFrame = core([self mainFrame]);
    return mainFrame && mainFrame->inViewSourceMode();
}

- (void)_setUseFastImageScalingMode:(BOOL)flag
{
    if (_private->page && _private->page->inLowQualityImageInterpolationMode() != flag) {
        _private->page->setInLowQualityImageInterpolationMode(flag);
        [self setNeedsDisplay:YES];
    }
}

- (BOOL)_inFastImageScalingMode
{
    if (_private->page)
        return _private->page->inLowQualityImageInterpolationMode();
    return NO;
}

- (BOOL)_cookieEnabled
{
    if (_private->page)
        return _private->page->cookieEnabled();
    return YES;
}

- (void)_setCookieEnabled:(BOOL)enable
{
    if (_private->page)
        _private->page->setCookieEnabled(enable);
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

    [[self mainFrame] _updateBackgroundAndUpdatesWhileOffscreen];
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

// For backwards compatibility with the WebBackForwardList API, we honor both
// a per-WebView and a per-preferences setting for whether to use the page cache.

- (BOOL)usesPageCache
{
    return _private->usesPageCache && [[self preferences] usesPageCache];
}

- (void)setUsesPageCache:(BOOL)usesPageCache
{
    _private->usesPageCache = usesPageCache;

    // Post a notification so the WebCore settings update.
    [[self preferences] _postPreferencesChangesNotification];
}

- (WebHistoryItem *)_globalHistoryItem
{
    if (!_private->page)
        return nil;
    return kit(_private->page->globalHistoryItem());
}

- (WebTextIterator *)textIteratorForRect:(NSRect)rect
{
    IntPoint rectStart(rect.origin.x, rect.origin.y);
    IntPoint rectEnd(rect.origin.x + rect.size.width, rect.origin.y + rect.size.height);
    
    Frame* coreFrame = core([self mainFrame]);
    if (!coreFrame)
        return nil;
    
    VisibleSelection selectionInsideRect(coreFrame->visiblePositionForPoint(rectStart), coreFrame->visiblePositionForPoint(rectEnd));
    
    return [[[WebTextIterator alloc] initWithRange:[DOMRange _wrapRange:selectionInsideRect.toNormalizedRange().get()]] autorelease];
}

- (void)handleAuthenticationForResource:(id)identifier challenge:(NSURLAuthenticationChallenge *)challenge fromDataSource:(WebDataSource *)dataSource 
{
    NSWindow *window = [self hostWindow] ? [self hostWindow] : [self window]; 
    [[WebPanelAuthenticationHandler sharedHandler] startAuthentication:challenge window:window]; 
} 

- (void)_clearUndoRedoOperations
{
    if (!_private->page)
        return;
    _private->page->clearUndoRedoOperations();
}

- (void)_setCatchesDelegateExceptions:(BOOL)f
{
    _private->catchesDelegateExceptions = f;
}

- (BOOL)_catchesDelegateExceptions
{
    return _private->catchesDelegateExceptions;
}

- (void)_executeCoreCommandByName:(NSString *)name value:(NSString *)value
{
    Frame* coreFrame = core([self mainFrame]);
    if (!coreFrame)
        return;
    coreFrame->editor()->command(name).execute(value);
}

- (void)_setCustomHTMLTokenizerTimeDelay:(double)timeDelay
{
    if (!_private->page)
        return;
    return _private->page->setCustomHTMLTokenizerTimeDelay(timeDelay);
}

- (void)_setCustomHTMLTokenizerChunkSize:(int)chunkSize
{
    if (!_private->page)
        return;
    return _private->page->setCustomHTMLTokenizerChunkSize(chunkSize);
}

- (void)_clearMainFrameName
{
    _private->page->mainFrame()->tree()->clearName();
}

- (void)setSelectTrailingWhitespaceEnabled:(BOOL)flag
{
    _private->selectTrailingWhitespaceEnabled = flag;
    if (flag)
        [self setSmartInsertDeleteEnabled:false];
}

- (BOOL)isSelectTrailingWhitespaceEnabled
{
    return _private->selectTrailingWhitespaceEnabled;
}

- (void)setMemoryCacheDelegateCallsEnabled:(BOOL)enabled
{
    _private->page->setMemoryCacheClientCallsEnabled(enabled);
}

- (BOOL)areMemoryCacheDelegateCallsEnabled
{
    return _private->page->areMemoryCacheClientCallsEnabled();
}

- (void)_setJavaScriptURLsAreAllowed:(BOOL)areAllowed
{
    _private->page->setJavaScriptURLsAreAllowed(areAllowed);
}

+ (NSCursor *)_pointingHandCursor
{
    return handCursor().impl();
}

#if USE(ACCELERATED_COMPOSITING)

- (BOOL)_needsOneShotDrawingSynchronization
{
    return _private->needsOneShotDrawingSynchronization;
}

- (void)_setNeedsOneShotDrawingSynchronization:(BOOL)needsSynchronization
{
    _private->needsOneShotDrawingSynchronization = needsSynchronization;
}

- (void)_startedAcceleratedCompositingForFrame:(WebFrame*)webFrame
{
    BOOL entering = _private->acceleratedFramesCount == 0;
    if (entering)
        [self willChangeValueForKey:UsingAcceleratedCompositingProperty];
    ++_private->acceleratedFramesCount;
    if (entering)
        [self didChangeValueForKey:UsingAcceleratedCompositingProperty];
}

- (void)_stoppedAcceleratedCompositingForFrame:(WebFrame*)webFrame
{
    BOOL leaving = _private->acceleratedFramesCount == 1;
    ASSERT(_private->acceleratedFramesCount > 0);
    
    if (leaving)
        [self willChangeValueForKey:UsingAcceleratedCompositingProperty];
    --_private->acceleratedFramesCount;
    if (leaving)
        [self didChangeValueForKey:UsingAcceleratedCompositingProperty];
}

#endif    

- (BOOL)_isUsingAcceleratedCompositing
{
#if USE(ACCELERATED_COMPOSITING)
    return _private->acceleratedFramesCount > 0;
#else
    return NO;
#endif
}

- (NSPasteboard *)_insertionPasteboard
{
    return _private ? _private->insertionPasteboard : nil;
}

@end

@implementation _WebSafeForwarder

// Used to send messages to delegates that implement informal protocols.

- (id)initWithTarget:(id)t defaultTarget:(id)dt catchExceptions:(BOOL)c
{
    self = [super init];
    if (!self)
        return nil;
    target = t; // Non retained.
    defaultTarget = dt;
    catchExceptions = c;
    return self;
}

- (void)forwardInvocation:(NSInvocation *)invocation
{
    if ([target respondsToSelector:[invocation selector]]) {
        if (catchExceptions) {
            @try {
                [invocation invokeWithTarget:target];
            } @catch(id exception) {
                ReportDiscardedDelegateException([invocation selector], exception);
            }
        } else
            [invocation invokeWithTarget:target];
        return;
    }

    if ([defaultTarget respondsToSelector:[invocation selector]])
        [invocation invokeWithTarget:defaultTarget];

    // Do nothing quietly if method not implemented.
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    return [defaultTarget methodSignatureForSelector:aSelector];
}

@end

@implementation WebView

+ (void)initialize
{
    static BOOL initialized = NO;
    if (initialized)
        return;
    initialized = YES;

    InitWebCoreSystemInterface();

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationWillTerminate) name:NSApplicationWillTerminateNotification object:NSApp];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesChangedNotification:) name:WebPreferencesChangedNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesRemovedNotification:) name:WebPreferencesRemovedNotification object:nil];    
}

+ (void)_applicationWillTerminate
{   
    applicationIsTerminating = YES;
    if (!pluginDatabaseClientCount)
        [WebPluginDatabase closeSharedDatabase];

    PageGroup::closeLocalStorage();
}

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

- (void)addPluginInstanceView:(NSView *)view
{
    if (!_private->pluginDatabase)
        _private->pluginDatabase = [[WebPluginDatabase alloc] init];
    [_private->pluginDatabase addPluginInstanceView:view];
}

- (void)removePluginInstanceView:(NSView *)view
{
    if (_private->pluginDatabase)
        [_private->pluginDatabase removePluginInstanceView:view];    
}

- (void)removePluginInstanceViewsFor:(WebFrame*)webFrame 
{
    if (_private->pluginDatabase)
        [_private->pluginDatabase removePluginInstanceViewsFor:webFrame];    
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
    NSDictionary *viewTypes = [[WebFrameView _viewTypesAllowImageTypeOmission:YES] copy];
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
    [viewTypes release];
}

+ (NSURL *)URLFromPasteboard:(NSPasteboard *)pasteboard
{
    return [pasteboard _web_bestURL];
}

+ (NSString *)URLTitleFromPasteboard:(NSPasteboard *)pasteboard
{
    return [pasteboard stringForType:WebURLNamePboardType];
}

+ (void)registerURLSchemeAsLocal:(NSString *)protocol
{
    FrameLoader::registerURLSchemeAsLocal(protocol);
}

- (id)_initWithArguments:(NSDictionary *) arguments
{
    NSCoder *decoder = [arguments objectForKey:@"decoder"];
    if (decoder) {
        self = [self initWithCoder:decoder];
    } else {
        ASSERT([arguments objectForKey:@"frame"]);
        NSValue *frameValue = [arguments objectForKey:@"frame"];
        NSRect frame = (frameValue ? [frameValue rectValue] : NSZeroRect);
        NSString *frameName = [arguments objectForKey:@"frameName"];
        NSString *groupName = [arguments objectForKey:@"groupName"];
        self = [self initWithFrame:frame frameName:frameName groupName:groupName];
    }

    return self;
}

static bool needsWebViewInitThreadWorkaround()
{
    static BOOL isOldClient = !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITHOUT_WEBVIEW_INIT_THREAD_WORKAROUND)
        && ([[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.installer"] ||
            [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.Automator"]);
    return isOldClient && !pthread_main_np();
}

- (id)initWithFrame:(NSRect)f
{
    return [self initWithFrame:f frameName:nil groupName:nil];
}

- (id)initWithFrame:(NSRect)f frameName:(NSString *)frameName groupName:(NSString *)groupName
{
    if (needsWebViewInitThreadWorkaround())
        return [[self _webkit_invokeOnMainThread] initWithFrame:f frameName:frameName groupName:groupName];

    WebCoreThreadViolationCheckRoundTwo();
    return [self _initWithFrame:f frameName:frameName groupName:groupName usesDocumentViews:YES];
}

- (id)initWithCoder:(NSCoder *)decoder
{
    if (needsWebViewInitThreadWorkaround())
        return [[self _webkit_invokeOnMainThread] initWithCoder:decoder];

    WebCoreThreadViolationCheckRoundTwo();
    WebView *result = nil;

    @try {
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
            // The allowsUndo field is no longer written out in encodeWithCoder, but since there are
            // version 3 NIBs that have this field encoded, we still need to read it in.
            if (version == 3)
                [decoder decodeValuesOfObjCTypes:"c", &allowsUndo];
        }

        if (![frameName isKindOfClass:[NSString class]])
            frameName = nil;
        if (![groupName isKindOfClass:[NSString class]])
            groupName = nil;
        if (![preferences isKindOfClass:[WebPreferences class]])
            preferences = nil;

        LOG(Encoding, "FrameName = %@, GroupName = %@, useBackForwardList = %d\n", frameName, groupName, (int)useBackForwardList);
        [result _commonInitializationWithFrameName:frameName groupName:groupName usesDocumentViews:YES];
        [result page]->backForwardList()->setEnabled(useBackForwardList);
        result->_private->allowsUndo = allowsUndo;
        if (preferences)
            [result setPreferences:preferences];
    } @catch (NSException *localException) {
        result = nil;
        [self release];
    }

    return result;
}

- (void)encodeWithCoder:(NSCoder *)encoder
{
    // Set asside the subviews before we archive. We don't want to archive any subviews.
    // The subviews will always be created in _commonInitializationFrameName:groupName:.
    id originalSubviews = _subviews;
    _subviews = nil;

    [super encodeWithCoder:encoder];

    // Restore the subviews we set aside.
    _subviews = originalSubviews;

    BOOL useBackForwardList = _private->page && _private->page->backForwardList()->enabled();
    if ([encoder allowsKeyedCoding]) {
        [encoder encodeObject:[[self mainFrame] name] forKey:@"FrameName"];
        [encoder encodeObject:[self groupName] forKey:@"GroupName"];
        [encoder encodeObject:[self preferences] forKey:@"Preferences"];
        [encoder encodeBool:useBackForwardList forKey:@"UseBackForwardList"];
        [encoder encodeBool:_private->allowsUndo forKey:@"AllowsUndo"];
    } else {
        int version = WebViewVersion;
        [encoder encodeValueOfObjCType:@encode(int) at:&version];
        [encoder encodeObject:[[self mainFrame] name]];
        [encoder encodeObject:[self groupName]];
        [encoder encodeObject:[self preferences]];
        [encoder encodeValuesOfObjCTypes:"c", &useBackForwardList];
        // DO NOT encode any new fields here, doing so will break older WebKit releases.
    }

    LOG(Encoding, "FrameName = %@, GroupName = %@, useBackForwardList = %d\n", [[self mainFrame] name], [self groupName], (int)useBackForwardList);
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebView class], self))
        return;

    // call close to ensure we tear-down completely
    // this maintains our old behavior for existing applications
    [self close];

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
    // _close existed first, and some clients might be calling or overriding it, so call through.
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

- (void)removeSizeObservers
{
    // -removeSizeObservers can be called from -viewWillMoveToSuperview: below -[NSView initWithCoder:], before
    // we've had a chance to initialize _private
    if (_private && !_private->useDocumentViews && [self window]) {
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSViewFrameDidChangeNotification object:self];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSViewBoundsDidChangeNotification object:self];
    }
}

- (void)addSizeObservers
{
    // -addSizeObservers can be called from -viewDidMoveToSuperview: below -[NSView initWithCoder:], before
    // we've had a chance to initialize _private
    if (_private && !_private->useDocumentViews && [self window]) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_boundsChanged) 
            name:NSViewFrameDidChangeNotification object:self];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_boundsChanged) 
            name:NSViewBoundsDidChangeNotification object:self];
        [self _boundsChanged];
    }
}

- (void)addWindowObservers
{
    NSWindow *window = [self window];
    if (!_private->useDocumentViews && window) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowDidBecomeKey:)
            name:NSWindowDidBecomeKeyNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowDidResignKey:)
            name:NSWindowDidResignKeyNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowWillOrderOnScreen:)
            name:WKWindowWillOrderOnScreenNotification() object:window];
    }
}

- (void)removeWindowObservers
{
    NSWindow *window = [self window];
    if (!_private->useDocumentViews && window) {
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidBecomeKeyNotification object:nil];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidResignKeyNotification object:nil];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:WKWindowWillOrderOnScreenNotification() object:window];
    }
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    // Don't do anything if the WebView isn't initialized.
    // This happens when decoding a WebView in a nib.
    // FIXME: What sets up the observer of NSWindowWillCloseNotification in this case?
    if (!_private)
        return;

    if (_private->closed)
        return;
    
    if ([self window] && [self window] != [self hostWindow])
        [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowWillCloseNotification object:[self window]];

    if (window) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowWillClose:) name:NSWindowWillCloseNotification object:window];
         
        // Ensure that we will receive the events that WebHTMLView (at least) needs.
        // The following are expensive enough that we don't want to call them over
        // and over, so do them when we move into a window.
        [window setAcceptsMouseMovedEvents:YES];
        WKSetNSWindowShouldPostEventNotifications(window, YES);
        
        [self removeWindowObservers];
        [self removeSizeObservers];
    } else
        _private->page->willMoveOffscreen();
}

- (void)viewDidMoveToWindow
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (!_private || _private->closed)
        return;
        
    if ([self window]) {
        [self addWindowObservers];
        [self addSizeObservers];
        _private->page->didMoveOnscreen();
    }
}

- (void)_updateFocusedAndActiveState
{
    ASSERT(!_private->useDocumentViews);
    [self _updateFocusedAndActiveStateForFrame:[self mainFrame]];
}

- (void)_updateFocusedAndActiveStateForFrame:(WebFrame *)webFrame
{
    Frame* frame = core(webFrame);
    if (!frame)
        return;
    
    Page* page = frame->page();
    if (!page)
        return;

    NSWindow *window = [self window];
    BOOL windowIsKey = [window isKeyWindow];
    BOOL windowOrSheetIsKey = windowIsKey || [[window attachedSheet] isKeyWindow];

    WebFrameView *mainFrameView = [[self mainFrame] frameView];
    id <WebDocumentView> documentView = [mainFrameView documentView];
    BOOL documentViewIsResigningFirstResponder = [documentView isKindOfClass:[WebHTMLView class]] && [(WebHTMLView *)documentView _isResigningFirstResponder];
        
    NSResponder *firstResponder = [window firstResponder];
    if ([firstResponder isKindOfClass:[NSView class]] 
        && [(NSView *)firstResponder isDescendantOf:mainFrameView])
        page->focusController()->setActive(windowIsKey && !documentViewIsResigningFirstResponder);

    Frame* focusedFrame = page->focusController()->focusedOrMainFrame();
    frame->selection()->setFocused(frame == focusedFrame && windowOrSheetIsKey);
}

- (void)_windowDidBecomeKey:(NSNotification *)notification
{
    ASSERT(!_private->useDocumentViews);
    NSWindow *keyWindow = [notification object];
    if (keyWindow == [self window] || keyWindow == [[self window] attachedSheet])
        [self _updateFocusedAndActiveState];
}

- (void)_windowDidResignKey:(NSNotification *)notification
{
    ASSERT(!_private->useDocumentViews);
    NSWindow *formerKeyWindow = [notification object];
    if (formerKeyWindow == [self window] || formerKeyWindow == [[self window] attachedSheet])
        [self _updateFocusedAndActiveState];
}

- (void)_windowWillOrderOnScreen:(NSNotification *)notification
{
    ASSERT(!_private->useDocumentViews);
    if (![self shouldUpdateWhileOffscreen])
        [self setNeedsDisplay:YES];
}

- (void)_windowWillClose:(NSNotification *)notification
{
    if ([self shouldCloseWithWindow] && ([self window] == [self hostWindow] || ([self window] && ![self hostWindow]) || (![self window] && [self hostWindow])))
        [self close];
}

- (void)setPreferences:(WebPreferences *)prefs
{
    if (!prefs)
        prefs = [WebPreferences standardPreferences];

    if (_private->preferences == prefs)
        return;

    [prefs willAddToWebView];

    WebPreferences *oldPrefs = _private->preferences;

    [[NSNotificationCenter defaultCenter] removeObserver:self name:WebPreferencesChangedNotification object:[self preferences]];
    [WebPreferences _removeReferenceForIdentifier:[oldPrefs identifier]];

    _private->preferences = [prefs retain];

    // After registering for the notification, post it so the WebCore settings update.
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesChangedNotification:)
        name:WebPreferencesChangedNotification object:[self preferences]];
    [[self preferences] _postPreferencesChangesNotification];

    [oldPrefs didRemoveFromWebView];
    [oldPrefs release];
}

- (WebPreferences *)preferences
{
    return _private->preferences;
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
    [self _cacheFrameLoadDelegateImplementations];

    // If this delegate wants callbacks for icons, fire up the icon database.
    if (_private->frameLoadDelegateImplementations.didReceiveIconForFrameFunc)
        [WebIconDatabase sharedIconDatabase];
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
    if (!_private->page)
        return nil;
    if (!_private->page->backForwardList()->enabled())
        return nil;
    return kit(_private->page->backForwardList());
}

- (void)setMaintainsBackForwardList: (BOOL)flag
{
    if (!_private->page)
        return;
    _private->page->backForwardList()->setEnabled(flag);
}

- (BOOL)goBack
{
    if (!_private->page)
        return NO;
    
    return _private->page->goBack();
}

- (BOOL)goForward
{
    if (!_private->page)
        return NO;

    return _private->page->goForward();
}

- (BOOL)goToBackForwardItem:(WebHistoryItem *)item
{
    if (!_private->page)
        return NO;

    _private->page->goToItem(core(item), FrameLoadTypeIndexedBackForward);
    return YES;
}

- (void)setTextSizeMultiplier:(float)m
{
    [self _setZoomMultiplier:m isTextOnly:![[NSUserDefaults standardUserDefaults] boolForKey:WebKitDebugFullPageZoomPreferenceKey]];
}

- (float)textSizeMultiplier
{
    return [self _realZoomMultiplierIsTextOnly] ? _private->zoomMultiplier : 1.0f;
}

- (void)_setZoomMultiplier:(float)m isTextOnly:(BOOL)isTextOnly
{
    // NOTE: This has no visible effect when viewing a PDF (see <rdar://problem/4737380>)
    _private->zoomMultiplier = m;
    ASSERT(_private->page);
    if (_private->page)
        _private->page->settings()->setZoomsTextOnly(isTextOnly);
    
    // FIXME: it would be nice to rework this code so that _private->zoomMultiplier doesn't exist and callers
    // all access _private->page->settings().
    Frame* coreFrame = core([self mainFrame]);
    if (coreFrame)
        coreFrame->setZoomFactor(m, isTextOnly);
}

- (float)_zoomMultiplier:(BOOL)isTextOnly
{
    if (isTextOnly != [self _realZoomMultiplierIsTextOnly])
        return 1.0f;
    return _private->zoomMultiplier;
}

- (float)_realZoomMultiplier
{
    return _private->zoomMultiplier;
}

- (BOOL)_realZoomMultiplierIsTextOnly
{
    if (!_private->page)
        return NO;
    
    return _private->page->settings()->zoomsTextOnly();
}

#define MinimumZoomMultiplier       0.5f
#define MaximumZoomMultiplier       3.0f
#define ZoomMultiplierRatio         1.2f

- (BOOL)_canZoomOut:(BOOL)isTextOnly
{
    id docView = [[[self mainFrame] frameView] documentView];
    if ([docView conformsToProtocol:@protocol(_WebDocumentZooming)]) {
        id <_WebDocumentZooming> zoomingDocView = (id <_WebDocumentZooming>)docView;
        return [zoomingDocView _canZoomOut];
    }
    return [self _zoomMultiplier:isTextOnly] / ZoomMultiplierRatio > MinimumZoomMultiplier;
}


- (BOOL)_canZoomIn:(BOOL)isTextOnly
{
    id docView = [[[self mainFrame] frameView] documentView];
    if ([docView conformsToProtocol:@protocol(_WebDocumentZooming)]) {
        id <_WebDocumentZooming> zoomingDocView = (id <_WebDocumentZooming>)docView;
        return [zoomingDocView _canZoomIn];
    }
    return [self _zoomMultiplier:isTextOnly] * ZoomMultiplierRatio < MaximumZoomMultiplier;
}

- (IBAction)_zoomOut:(id)sender isTextOnly:(BOOL)isTextOnly
{
    id docView = [[[self mainFrame] frameView] documentView];
    if ([docView conformsToProtocol:@protocol(_WebDocumentZooming)]) {
        id <_WebDocumentZooming> zoomingDocView = (id <_WebDocumentZooming>)docView;
        return [zoomingDocView _zoomOut:sender];
    }
    float newScale = [self _zoomMultiplier:isTextOnly] / ZoomMultiplierRatio;
    if (newScale > MinimumZoomMultiplier)
        [self _setZoomMultiplier:newScale isTextOnly:isTextOnly];
}

- (IBAction)_zoomIn:(id)sender isTextOnly:(BOOL)isTextOnly
{
    id docView = [[[self mainFrame] frameView] documentView];
    if ([docView conformsToProtocol:@protocol(_WebDocumentZooming)]) {
        id <_WebDocumentZooming> zoomingDocView = (id <_WebDocumentZooming>)docView;
        return [zoomingDocView _zoomIn:sender];
    }
    float newScale = [self _zoomMultiplier:isTextOnly] * ZoomMultiplierRatio;
    if (newScale < MaximumZoomMultiplier)
        [self _setZoomMultiplier:newScale isTextOnly:isTextOnly];
}

- (BOOL)_canResetZoom:(BOOL)isTextOnly
{
    id docView = [[[self mainFrame] frameView] documentView];
    if ([docView conformsToProtocol:@protocol(_WebDocumentZooming)]) {
        id <_WebDocumentZooming> zoomingDocView = (id <_WebDocumentZooming>)docView;
        return [zoomingDocView _canResetZoom];
    }
    return [self _zoomMultiplier:isTextOnly] != 1.0f;
}

- (IBAction)_resetZoom:(id)sender isTextOnly:(BOOL)isTextOnly
{
    id docView = [[[self mainFrame] frameView] documentView];
    if ([docView conformsToProtocol:@protocol(_WebDocumentZooming)]) {
        id <_WebDocumentZooming> zoomingDocView = (id <_WebDocumentZooming>)docView;
        return [zoomingDocView _resetZoom:sender];
    }
    if ([self _zoomMultiplier:isTextOnly] != 1.0f)
        [self _setZoomMultiplier:1.0f isTextOnly:isTextOnly];
}

- (void)viewWillMoveToSuperview:(NSView *)newSuperview
{
    [self removeSizeObservers];
}

- (void)viewDidMoveToSuperview
{
    if ([self superview] != nil)
        [self addSizeObservers];
}

- (void)setApplicationNameForUserAgent:(NSString *)applicationName
{
    NSString *name = [applicationName copy];
    [_private->applicationNameForUserAgent release];
    _private->applicationNameForUserAgent = name;
    if (!_private->userAgentOverridden)
        _private->userAgent = String();
}

- (NSString *)applicationNameForUserAgent
{
    return [[_private->applicationNameForUserAgent retain] autorelease];
}

- (void)setCustomUserAgent:(NSString *)userAgentString
{
    _private->userAgent = userAgentString;
    _private->userAgentOverridden = userAgentString != nil;
}

- (NSString *)customUserAgent
{
    if (!_private->userAgentOverridden)
        return nil;
    return _private->userAgent;
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
    if (Frame* mainFrame = core([self mainFrame]))
        mainFrame->loader()->reloadWithOverrideEncoding(encoding);
}

- (NSString *)_mainFrameOverrideEncoding
{
    WebDataSource *dataSource = [[self mainFrame] provisionalDataSource];
    if (dataSource == nil)
        dataSource = [[self mainFrame] _dataSource];
    if (dataSource == nil)
        return nil;
    return nsStringNilIfEmpty([dataSource _documentLoader]->overrideEncoding());
}

- (NSString *)customTextEncodingName
{
    return [self _mainFrameOverrideEncoding];
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)script
{
    // Return statements are only valid in a function but some applications pass in scripts
    // prefixed with return (<rdar://problems/5103720&4616860>) since older WebKit versions
    // silently ignored the return. If the application is linked against an earlier version
    // of WebKit we will strip the return so the script wont fail.
    if (!WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITHOUT_JAVASCRIPT_RETURN_QUIRK)) {
        NSRange returnStringRange = [script rangeOfString:@"return "];
        if (returnStringRange.length && !returnStringRange.location)
            script = [script substringFromIndex:returnStringRange.location + returnStringRange.length];
    }

    NSString *result = [[self mainFrame] _stringByEvaluatingJavaScriptFromString:script];
    // The only way stringByEvaluatingJavaScriptFromString can return nil is if the frame was removed by the script
    // Since there's no way to get rid of the main frame, result will never ever be nil here.
    ASSERT(result);

    return result;
}

- (WebScriptObject *)windowScriptObject
{
    Frame* coreFrame = core([self mainFrame]);
    if (!coreFrame)
        return nil;
    return coreFrame->script()->windowScriptObject();
}

// Get the appropriate user-agent string for a particular URL.
- (NSString *)userAgentForURL:(NSURL *)url
{
    return [self _userAgentForURL:KURL([url absoluteURL])];
}

- (void)setHostWindow:(NSWindow *)hostWindow
{
    if (_private->closed)
        return;
    if (hostWindow == _private->hostWindow)
        return;

    Frame* coreFrame = core([self mainFrame]);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame))
        [[[kit(frame) frameView] documentView] viewWillMoveToHostWindow:hostWindow];
    if (_private->hostWindow && [self window] != _private->hostWindow)
        [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowWillCloseNotification object:_private->hostWindow];
    if (hostWindow)
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowWillClose:) name:NSWindowWillCloseNotification object:hostWindow];
    [_private->hostWindow release];
    _private->hostWindow = [hostWindow retain];
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame))
        [[[kit(frame) frameView] documentView] viewDidMoveToHostWindow];
}

- (NSWindow *)hostWindow
{
    // -[WebView hostWindow] can sometimes be called from the WebView's [super dealloc] method
    // so we check here to make sure it's not null.
    if (!_private)
        return nil;
    
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
    Page* page = core(self);
    if (!page)
        return NSDragOperationNone;

    NSView <WebDocumentView>* view = [self documentViewAtWindowPoint:[draggingInfo draggingLocation]];
    WebPasteboardHelper helper([view isKindOfClass:[WebHTMLView class]] ? (WebHTMLView*)view : nil);
    IntPoint client([draggingInfo draggingLocation]);
    IntPoint global(globalPoint([draggingInfo draggingLocation], [self window]));
    DragData dragData(draggingInfo, client, global, (DragOperation)[draggingInfo draggingSourceOperationMask], &helper);
    return page->dragController()->dragUpdated(&dragData);
}

- (void)draggingExited:(id <NSDraggingInfo>)draggingInfo
{
    Page* page = core(self);
    if (!page)
        return;

    NSView <WebDocumentView>* view = [self documentViewAtWindowPoint:[draggingInfo draggingLocation]];
    WebPasteboardHelper helper([view isKindOfClass:[WebHTMLView class]] ? (WebHTMLView*)view : nil);
    IntPoint client([draggingInfo draggingLocation]);
    IntPoint global(globalPoint([draggingInfo draggingLocation], [self window]));
    DragData dragData(draggingInfo, client, global, (DragOperation)[draggingInfo draggingSourceOperationMask], &helper);
    page->dragController()->dragExited(&dragData);
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

    NSResponder *previousFirstResponder = [[self window] _oldFirstResponderBeforeBecoming];
    BOOL fromOutside = ![previousFirstResponder isKindOfClass:[NSView class]] || (![(NSView *)previousFirstResponder isDescendantOf:self] && previousFirstResponder != self);
    
    if ([window keyViewSelectionDirection] == NSSelectingPrevious) {
        NSView *previousValidKeyView = [self previousValidKeyView];
        if ((previousValidKeyView != self) && (previousValidKeyView != mainFrameView)) {
            _private->becomingFirstResponder = YES;
            _private->becomingFirstResponderFromOutside = fromOutside;
            [window makeFirstResponder:previousValidKeyView];
            _private->becomingFirstResponderFromOutside = NO;
            _private->becomingFirstResponder = NO;
            return YES;
        } else {
            return NO;
        }
    }
    
    if ([mainFrameView acceptsFirstResponder]) {
        _private->becomingFirstResponder = YES;
        _private->becomingFirstResponderFromOutside = fromOutside;
        [window makeFirstResponder:mainFrameView];
        _private->becomingFirstResponderFromOutside = NO;
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
    
    // FIXME: We also need to maintain MIMEType registrations (which can be dynamically changed)
    // in the WebCore MIMEType registry.  For now we're doing this in a safe, limited manner
    // to fix <rdar://problem/5372989> - a future revamping of the entire system is neccesary for future robustness
    if ([viewClass class] == [WebHTMLView class])
        MIMETypeRegistry::getSupportedNonImageMIMETypes().add(MIMEType);
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
    if (Page* page = core(self))
        page->dragController()->placeDragCaret(IntPoint([self convertPoint:point toView:nil]));
}

- (void)removeDragCaret
{
    if (Page* page = core(self))
        page->dragController()->dragEnded();
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
        ds = [[self mainFrame] _dataSource];
    return [[[ds request] URL] _web_originalDataAsString];
}

- (BOOL)isLoading
{
    LOG (Bindings, "isLoading = %d", (int)[self _isLoading]);
    return [self _isLoading];
}

- (NSString *)mainFrameTitle
{
    NSString *mainFrameTitle = [[[self mainFrame] _dataSource] pageTitle];
    return (mainFrameTitle != nil) ? mainFrameTitle : (NSString *)@"";
}

- (NSImage *)mainFrameIcon
{
    return [[WebIconDatabase sharedIconDatabase] iconForURL:[[[[self mainFrame] _dataSource] _URL] _web_originalDataAsString] withSize:WebIconSmallSize];
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
    [[self mainFrame] _updateBackgroundAndUpdatesWhileOffscreen];
}

- (BOOL)drawsBackground
{
    // This method can be called beneath -[NSView dealloc] after we have cleared _private,
    // indirectly via -[WebFrameView viewDidMoveToWindow].
    return !_private || _private->drawsBackground;
}

- (void)setShouldUpdateWhileOffscreen:(BOOL)updateWhileOffscreen
{
    if (_private->shouldUpdateWhileOffscreen == updateWhileOffscreen)
        return;
    _private->shouldUpdateWhileOffscreen = updateWhileOffscreen;
    [[self mainFrame] _updateBackgroundAndUpdatesWhileOffscreen];
}

- (BOOL)shouldUpdateWhileOffscreen
{
    return _private->shouldUpdateWhileOffscreen;
}

- (void)setCurrentNodeHighlight:(WebNodeHighlight *)nodeHighlight
{
    id old = _private->currentNodeHighlight;
    _private->currentNodeHighlight = [nodeHighlight retain];
    [old release];
}

- (WebNodeHighlight *)currentNodeHighlight
{
    return _private->currentNodeHighlight;
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
    if (!_private->page)
        return NO;

    return !!_private->page->backForwardList()->backItem();
}

- (BOOL)canGoForward
{
    if (!_private->page)
        return NO;

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

- (IBAction)reloadFromOrigin:(id)sender
{
    [[self mainFrame] reloadFromOrigin];
}

// FIXME: This code should move into WebCore so that it is not duplicated in each WebKit.
// (This includes canMakeTextSmaller/Larger, makeTextSmaller/Larger, and canMakeTextStandardSize/makeTextStandardSize)
- (BOOL)canMakeTextSmaller
{
    return [self _canZoomOut:![[NSUserDefaults standardUserDefaults] boolForKey:WebKitDebugFullPageZoomPreferenceKey]];
}

- (IBAction)makeTextSmaller:(id)sender
{
    return [self _zoomOut:sender isTextOnly:![[NSUserDefaults standardUserDefaults] boolForKey:WebKitDebugFullPageZoomPreferenceKey]];
}

- (BOOL)canMakeTextLarger
{
    return [self _canZoomIn:![[NSUserDefaults standardUserDefaults] boolForKey:WebKitDebugFullPageZoomPreferenceKey]];
}

- (IBAction)makeTextLarger:(id)sender
{
    return [self _zoomIn:sender isTextOnly:![[NSUserDefaults standardUserDefaults] boolForKey:WebKitDebugFullPageZoomPreferenceKey]];
}

- (BOOL)canMakeTextStandardSize
{
    return [self _canResetZoom:![[NSUserDefaults standardUserDefaults] boolForKey:WebKitDebugFullPageZoomPreferenceKey]];
}

- (IBAction)makeTextStandardSize:(id)sender
{
   return [self _resetZoom:sender isTextOnly:![[NSUserDefaults standardUserDefaults] boolForKey:WebKitDebugFullPageZoomPreferenceKey]];
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
        if ([responder respondsToSelector:@selector(validateUserInterfaceItemWithoutDelegate:)])
            return [responder validateUserInterfaceItemWithoutDelegate:item];
        if ([responder respondsToSelector:@selector(validateUserInterfaceItem:)])
            return [responder validateUserInterfaceItem:item];
        return YES;
    }
    return NO;
}

#define VALIDATE(name) \
    else if (action == @selector(name:)) { return [self _responderValidateUserInterfaceItem:item]; }

- (BOOL)validateUserInterfaceItemWithoutDelegate:(id <NSValidatedUserInterfaceItem>)item
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
        return [[self mainFrame] _dataSource] != nil;
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

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    BOOL result = [self validateUserInterfaceItemWithoutDelegate:item];
    return CallUIDelegateReturningBoolean(result, self, @selector(webView:validateUserInterfaceItem:defaultValidation:), item, result);
}

@end

@implementation WebView (WebPendingPublic)

- (void)scheduleInRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode
{
    if (runLoop && mode)
        core(self)->addSchedulePair(SchedulePair::create(runLoop, (CFStringRef)mode));
}

- (void)unscheduleFromRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode
{
    if (runLoop && mode)
        core(self)->removeSchedulePair(SchedulePair::create(runLoop, (CFStringRef)mode));
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag startInSelection:(BOOL)startInSelection
{
    if (_private->closed)
        return NO;
    
    // Get the frame holding the selection, or start with the main frame
    WebFrame *startFrame = [self _selectedOrMainFrame];
    
    // Search the first frame, then all the other frames, in order
    NSView <WebDocumentSearching> *startSearchView = nil;
    WebFrame *frame = startFrame;
    do {
        WebFrame *nextFrame = incrementFrame(frame, forward, wrapFlag);
        
        BOOL onlyOneFrame = (frame == nextFrame);
        ASSERT(!onlyOneFrame || frame == startFrame);
        
        id <WebDocumentView> view = [[frame frameView] documentView];
        if ([view conformsToProtocol:@protocol(WebDocumentSearching)]) {
            NSView <WebDocumentSearching> *searchView = (NSView <WebDocumentSearching> *)view;
            
            if (frame == startFrame)
                startSearchView = searchView;
            
            BOOL foundString;
            // In some cases we have to search some content twice; see comment later in this method.
            // We can avoid ever doing this in the common one-frame case by passing YES for wrapFlag 
            // here, and then bailing out before we get to the code that would search again in the
            // same content.
            BOOL wrapOnThisPass = wrapFlag && onlyOneFrame;
            if ([searchView conformsToProtocol:@protocol(WebDocumentIncrementalSearching)])
                foundString = [(NSView <WebDocumentIncrementalSearching> *)searchView searchFor:string direction:forward caseSensitive:caseFlag wrap:wrapOnThisPass startInSelection:startInSelection];
            else
                foundString = [searchView searchFor:string direction:forward caseSensitive:caseFlag wrap:wrapOnThisPass];
            
            if (foundString) {
                if (frame != startFrame)
                    [startFrame _clearSelection];
                [[self window] makeFirstResponder:searchView];
                return YES;
            }
            
            if (onlyOneFrame)
                return NO;
        }
        frame = nextFrame;
    } while (frame && frame != startFrame);
    
    // If there are multiple frames and wrapFlag is true and we've visited each one without finding a result, we still need to search in the 
    // first-searched frame up to the selection. However, the API doesn't provide a way to search only up to a particular point. The only 
    // way to make sure the entire frame is searched is to pass YES for the wrapFlag. When there are no matches, this will search again
    // some content that we already searched on the first pass. In the worst case, we could search the entire contents of this frame twice.
    // To fix this, we'd need to add a mechanism to specify a range in which to search.
    if (wrapFlag && startSearchView) {
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
    if (_private->page)
        _private->page->setTabKeyCyclesThroughElements(cyclesElements);
}

- (BOOL)tabKeyCyclesThroughElements
{
    return _private->page && _private->page->tabKeyCyclesThroughElements();
}

- (void)setScriptDebugDelegate:(id)delegate
{
    _private->scriptDebugDelegate = delegate;
    [self _cacheScriptDebugDelegateImplementations];

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
    Frame* coreFrame = core([self mainFrame]);
    if (!coreFrame)
        return YES;
    return coreFrame->shouldClose();
}

static NSAppleEventDescriptor* aeDescFromJSValue(ExecState* exec, JSValuePtr jsValue)
{
    NSAppleEventDescriptor* aeDesc = 0;
    if (jsValue.isBoolean())
        return [NSAppleEventDescriptor descriptorWithBoolean:jsValue.getBoolean()];
    if (jsValue.isString())
        return [NSAppleEventDescriptor descriptorWithString:String(jsValue.getString())];
    if (jsValue.isNumber()) {
        double value = jsValue.uncheckedGetNumber();
        int intValue = value;
        if (value == intValue)
            return [NSAppleEventDescriptor descriptorWithDescriptorType:typeSInt32 bytes:&intValue length:sizeof(intValue)];
        return [NSAppleEventDescriptor descriptorWithDescriptorType:typeIEEE64BitFloatingPoint bytes:&value length:sizeof(value)];
    }
    if (jsValue.isObject()) {
        JSObject* object = jsValue.getObject();
        if (object->inherits(&DateInstance::info)) {
            DateInstance* date = static_cast<DateInstance*>(object);
            double ms = 0;
            int tzOffset = 0;
            if (date->getTime(ms, tzOffset)) {
                CFAbsoluteTime utcSeconds = ms / 1000 - kCFAbsoluteTimeIntervalSince1970;
                LongDateTime ldt;
                if (noErr == UCConvertCFAbsoluteTimeToLongDateTime(utcSeconds, &ldt))
                    return [NSAppleEventDescriptor descriptorWithDescriptorType:typeLongDateTime bytes:&ldt length:sizeof(ldt)];
            }
        }
        else if (object->inherits(&JSArray::info)) {
            DEFINE_STATIC_LOCAL(HashSet<JSObject*>, visitedElems, ());
            if (!visitedElems.contains(object)) {
                visitedElems.add(object);
                
                JSArray* array = static_cast<JSArray*>(object);
                aeDesc = [NSAppleEventDescriptor listDescriptor];
                unsigned numItems = array->length();
                for (unsigned i = 0; i < numItems; ++i)
                    [aeDesc insertDescriptor:aeDescFromJSValue(exec, array->get(exec, i)) atIndex:0];
                
                visitedElems.remove(object);
                return aeDesc;
            }
        }
        JSValuePtr primitive = object->toPrimitive(exec);
        if (exec->hadException()) {
            exec->clearException();
            return [NSAppleEventDescriptor nullDescriptor];
        }
        return aeDescFromJSValue(exec, primitive);
    }
    if (jsValue.isUndefined())
        return [NSAppleEventDescriptor descriptorWithTypeCode:cMissingValue];
    ASSERT(jsValue.isNull());
    return [NSAppleEventDescriptor nullDescriptor];
}

- (NSAppleEventDescriptor *)aeDescByEvaluatingJavaScriptFromString:(NSString *)script
{
    Frame* coreFrame = core([self mainFrame]);
    if (!coreFrame)
        return nil;
    if (!coreFrame->document())
        return nil;
    JSValuePtr result = coreFrame->loader()->executeScript(script, true).jsValue();
    if (!result) // FIXME: pass errors
        return 0;
    JSLock lock(false);
    return aeDescFromJSValue(coreFrame->script()->globalObject()->globalExec(), result);
}

- (BOOL)canMarkAllTextMatches
{
    WebFrame *frame = [self mainFrame];
    do {
        id <WebDocumentView> view = [[frame frameView] documentView];
        if (view && ![view conformsToProtocol:@protocol(WebMultipleTextMatches)])
            return NO;
        
        frame = incrementFrame(frame, YES, NO);
    } while (frame);
    
    return YES;
}

- (NSUInteger)markAllMatchesForText:(NSString *)string caseSensitive:(BOOL)caseFlag highlight:(BOOL)highlight limit:(NSUInteger)limit
{
    WebFrame *frame = [self mainFrame];
    unsigned matchCount = 0;
    do {
        id <WebDocumentView> view = [[frame frameView] documentView];
        if ([view conformsToProtocol:@protocol(WebMultipleTextMatches)]) {
            [(NSView <WebMultipleTextMatches>*)view  setMarkedTextMatchesAreHighlighted:highlight];
        
            ASSERT(limit == 0 || matchCount < limit);
            matchCount += [(NSView <WebMultipleTextMatches>*)view markAllMatchesForText:string caseSensitive:caseFlag limit:limit == 0 ? 0 : limit - matchCount];

            // Stop looking if we've reached the limit. A limit of 0 means no limit.
            if (limit > 0 && matchCount >= limit)
                break;
        }
        
        frame = incrementFrame(frame, YES, NO);
    } while (frame);
    
    return matchCount;
}

- (void)unmarkAllTextMatches
{
    WebFrame *frame = [self mainFrame];
    do {
        id <WebDocumentView> view = [[frame frameView] documentView];
        if ([view conformsToProtocol:@protocol(WebMultipleTextMatches)])
            [(NSView <WebMultipleTextMatches>*)view unmarkAllTextMatches];
        
        frame = incrementFrame(frame, YES, NO);
    } while (frame);
}

- (NSArray *)rectsForTextMatches
{
    NSMutableArray *result = [NSMutableArray array];
    WebFrame *frame = [self mainFrame];
    do {
        id <WebDocumentView> view = [[frame frameView] documentView];
        if ([view conformsToProtocol:@protocol(WebMultipleTextMatches)]) {
            NSView <WebMultipleTextMatches> *documentView = (NSView <WebMultipleTextMatches> *)view;
            NSRect documentViewVisibleRect = [documentView visibleRect];
            NSArray *originalRects = [documentView rectsForTextMatches];
            unsigned rectCount = [originalRects count];
            unsigned rectIndex;
            NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
            for (rectIndex = 0; rectIndex < rectCount; ++rectIndex) {
                NSRect r = [[originalRects objectAtIndex:rectIndex] rectValue];
                // Clip rect to document view's visible rect so rect is confined to subframe
                r = NSIntersectionRect(r, documentViewVisibleRect);
                if (NSIsEmptyRect(r))
                    continue;
                
                // Convert rect to our coordinate system
                r = [documentView convertRect:r toView:self];
                [result addObject:[NSValue valueWithRect:r]];
                if (rectIndex % 10 == 0) {
                    [pool drain];
                    pool = [[NSAutoreleasePool alloc] init];
                }
            }
            [pool drain];
        }
        
        frame = incrementFrame(frame, YES, NO);
    } while (frame);
    
    return result;
}

- (void)scrollDOMRangeToVisible:(DOMRange *)range
{
    [[[[range startContainer] ownerDocument] webFrame] _scrollDOMRangeToVisible:range];
}

- (BOOL)allowsUndo
{
    return _private->allowsUndo;
}

- (void)setAllowsUndo:(BOOL)flag
{
    _private->allowsUndo = flag;
}

- (void)setPageSizeMultiplier:(float)m
{
    [self _setZoomMultiplier:m isTextOnly:NO];
}

- (float)pageSizeMultiplier
{
    return ![self _realZoomMultiplierIsTextOnly] ? _private->zoomMultiplier : 1.0f;
}

- (BOOL)canZoomPageIn
{
    return [self _canZoomIn:NO];
}

- (IBAction)zoomPageIn:(id)sender
{
    return [self _zoomIn:sender isTextOnly:NO];
}

- (BOOL)canZoomPageOut
{
    return [self _canZoomOut:NO];
}

- (IBAction)zoomPageOut:(id)sender
{
    return [self _zoomOut:sender isTextOnly:NO];
}

- (BOOL)canResetPageZoom
{
    return [self _canResetZoom:NO];
}

- (IBAction)resetPageZoom:(id)sender
{
    return [self _resetZoom:sender isTextOnly:NO];
}

- (void)setMediaVolume:(float)volume
{
    if (_private->page)
        _private->page->setMediaVolume(volume);
}

- (float)mediaVolume
{
    if (!_private->page)
        return 0;

    return _private->page->mediaVolume();
}

@end

@implementation WebView (WebViewPrintingPrivate)

- (float)_headerHeight
{
    return CallUIDelegateReturningFloat(self, @selector(webViewHeaderHeight:));
}

- (float)_footerHeight
{
    return CallUIDelegateReturningFloat(self, @selector(webViewFooterHeight:));
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

    SEL selector = @selector(webView:drawHeaderInRect:);
    if (![_private->UIDelegate respondsToSelector:selector])
        return;

    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    [currentContext saveGraphicsState];

    NSRectClip(rect);
    CallUIDelegate(self, selector, rect);

    [currentContext restoreGraphicsState];
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
    
    SEL selector = @selector(webView:drawFooterInRect:);
    if (![_private->UIDelegate respondsToSelector:selector])
        return;

    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    [currentContext saveGraphicsState];

    NSRectClip(rect);
    CallUIDelegate(self, selector, rect);

    [currentContext restoreGraphicsState];
}

- (void)_adjustPrintingMarginsForHeaderAndFooter
{
    NSPrintOperation *op = [NSPrintOperation currentOperation];
    NSPrintInfo *info = [op printInfo];
    NSMutableDictionary *infoDictionary = [info dictionary];
    
    // We need to modify the top and bottom margins in the NSPrintInfo to account for the space needed by the
    // header and footer. Because this method can be called more than once on the same NSPrintInfo (see 5038087),
    // we stash away the unmodified top and bottom margins the first time this method is called, and we read from
    // those stashed-away values on subsequent calls.
    float originalTopMargin;
    float originalBottomMargin;
    NSNumber *originalTopMarginNumber = [infoDictionary objectForKey:WebKitOriginalTopPrintingMarginKey];
    if (!originalTopMarginNumber) {
        ASSERT(![infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey]);
        originalTopMargin = [info topMargin];
        originalBottomMargin = [info bottomMargin];
        [infoDictionary setObject:[NSNumber numberWithFloat:originalTopMargin] forKey:WebKitOriginalTopPrintingMarginKey];
        [infoDictionary setObject:[NSNumber numberWithFloat:originalBottomMargin] forKey:WebKitOriginalBottomPrintingMarginKey];
    } else {
        ASSERT([originalTopMarginNumber isKindOfClass:[NSNumber class]]);
        ASSERT([[infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey] isKindOfClass:[NSNumber class]]);
        originalTopMargin = [originalTopMarginNumber floatValue];
        originalBottomMargin = [[infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey] floatValue];
    }
    
    float scale = [op _web_pageSetupScaleFactor];
    [info setTopMargin:originalTopMargin + [self _headerHeight] * scale];
    [info setBottomMargin:originalBottomMargin + [self _footerHeight] * scale];
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
    if (pseudoElement == nil)
        pseudoElement = @"";

    return [[element ownerDocument] getComputedStyle:element pseudoElement:pseudoElement];
}

@end

@implementation WebView (WebViewEditing)

- (DOMRange *)editableDOMRangeForPoint:(NSPoint)point
{
    Page* page = core(self);
    if (!page)
        return nil;
    return kit(page->mainFrame()->editor()->rangeForPoint(IntPoint([self convertPoint:point toView:nil])).get());
}

- (BOOL)_shouldChangeSelectedDOMRange:(DOMRange *)currentRange toDOMRange:(DOMRange *)proposedRange affinity:(NSSelectionAffinity)selectionAffinity stillSelecting:(BOOL)flag
{
    // FIXME: This quirk is needed due to <rdar://problem/4985321> - We can phase it out once Aperture can adopt the new behavior on their end
    if (!WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITHOUT_APERTURE_QUIRK) && [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.Aperture"])
        return YES;
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
        coreFrame->selection()->clear();
    else {
        // Derive the frame to use from the range passed in.
        // Using _selectedOrMainFrame could give us a different document than
        // the one the range uses.
        coreFrame = core([range startContainer])->document()->frame();
        if (!coreFrame)
            return;

        coreFrame->selection()->setSelectedRange([range _range], core(selectionAffinity), true);
    }
}

- (DOMRange *)selectedDOMRange
{
    Frame* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return nil;
    return kit(coreFrame->selection()->toNormalizedRange().get());
}

- (NSSelectionAffinity)selectionAffinity
{
    Frame* coreFrame = core([self _selectedOrMainFrame]);
    if (!coreFrame)
        return NSSelectionAffinityDownstream;
    return kit(coreFrame->selection()->affinity());
}

- (void)setEditable:(BOOL)flag
{
    if (_private->editable != flag) {
        _private->editable = flag;
        if (!_private->tabKeyCyclesThroughElementsChanged && _private->page)
            _private->page->setTabKeyCyclesThroughElements(!flag);
        Frame* mainFrame = core([self mainFrame]);
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
    [[self _selectedOrMainFrame] _setTypingStyle:style withUndoAction:EditActionUnspecified];
}

- (DOMCSSStyleDeclaration *)typingStyle
{
    return [[self _selectedOrMainFrame] _typingStyle];
}

- (void)setSmartInsertDeleteEnabled:(BOOL)flag
{
    _private->smartInsertDeleteEnabled = flag;
    if (flag)
        [self setSelectTrailingWhitespaceEnabled:false];
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

- (NSInteger)spellCheckerDocumentTag
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
#ifdef BUILDING_ON_TIGER
    return NO;
#else
    return grammarCheckingEnabled;
#endif
}

#ifndef BUILDING_ON_TIGER
// FIXME: This method should be merged into WebViewEditing when we're not in API freeze
- (void)setGrammarCheckingEnabled:(BOOL)flag
{
    if (grammarCheckingEnabled == flag)
        return;
    
    grammarCheckingEnabled = flag;
    [[NSUserDefaults standardUserDefaults] setBool:grammarCheckingEnabled forKey:WebGrammarCheckingEnabled];    
    
    // FIXME 4811447: workaround for lack of API
    NSSpellChecker *spellChecker = [NSSpellChecker sharedSpellChecker];
    if ([spellChecker respondsToSelector:@selector(_updateGrammar)])
        [spellChecker performSelector:@selector(_updateGrammar)];
    
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
    [[self _selectedOrMainFrame] _replaceSelectionWithNode:node selectReplacement:YES smartReplace:NO matchStyle:NO];
}    

- (void)replaceSelectionWithText:(NSString *)text
{
    [[self _selectedOrMainFrame] _replaceSelectionWithText:text selectReplacement:YES smartReplace:NO];
}

- (void)replaceSelectionWithMarkupString:(NSString *)markupString
{
    [[self _selectedOrMainFrame] _replaceSelectionWithMarkupString:markupString baseURLString:nil selectReplacement:YES smartReplace:NO];
}

- (void)replaceSelectionWithArchive:(WebArchive *)archive
{
    [[[self _selectedOrMainFrame] _dataSource] _replaceSelectionWithArchive:archive selectReplacement:YES];
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

- (void)_insertNewlineInQuotedContent
{
    [[self _selectedOrMainFrame] _insertParagraphSeparatorInQuotedContent];
}

- (void)_replaceSelectionWithNode:(DOMNode *)node matchStyle:(BOOL)matchStyle
{
    [[self _selectedOrMainFrame] _replaceSelectionWithNode:node selectReplacement:YES smartReplace:NO matchStyle:matchStyle];
}

@end

static WebFrameView *containingFrameView(NSView *view)
{
    while (view && ![view isKindOfClass:[WebFrameView class]])
        view = [view superview];
    return (WebFrameView *)view;    
}

@implementation WebView (WebFileInternal)

+ (void)_setCacheModel:(WebCacheModel)cacheModel
{
    if (s_didSetCacheModel && cacheModel == s_cacheModel)
        return;

    NSString *nsurlCacheDirectory = (NSString *)WebCFAutorelease(WKCopyFoundationCacheDirectory());
    if (!nsurlCacheDirectory)
        nsurlCacheDirectory = NSHomeDirectory();

    // As a fudge factor, use 1000 instead of 1024, in case the reported byte 
    // count doesn't align exactly to a megabyte boundary.
    uint64_t memSize = WebMemorySize() / 1024 / 1000;
    unsigned long long diskFreeSize = WebVolumeFreeSize(nsurlCacheDirectory) / 1024 / 1000;
    NSURLCache *nsurlCache = [NSURLCache sharedURLCache];

    unsigned cacheTotalCapacity = 0;
    unsigned cacheMinDeadCapacity = 0;
    unsigned cacheMaxDeadCapacity = 0;
    double deadDecodedDataDeletionInterval = 0;

    unsigned pageCacheCapacity = 0;

    NSUInteger nsurlCacheMemoryCapacity = 0;
    NSUInteger nsurlCacheDiskCapacity = 0;

    switch (cacheModel) {
    case WebCacheModelDocumentViewer: {
        // Page cache capacity (in pages)
        pageCacheCapacity = 0;

        // Object cache capacities (in bytes)
        if (memSize >= 2048)
            cacheTotalCapacity = 96 * 1024 * 1024;
        else if (memSize >= 1536)
            cacheTotalCapacity = 64 * 1024 * 1024;
        else if (memSize >= 1024)
            cacheTotalCapacity = 32 * 1024 * 1024;
        else if (memSize >= 512)
            cacheTotalCapacity = 16 * 1024 * 1024;

        cacheMinDeadCapacity = 0;
        cacheMaxDeadCapacity = 0;

        // Foundation memory cache capacity (in bytes)
        nsurlCacheMemoryCapacity = 0;

        // Foundation disk cache capacity (in bytes)
        nsurlCacheDiskCapacity = [nsurlCache diskCapacity];

        break;
    }
    case WebCacheModelDocumentBrowser: {
        // Page cache capacity (in pages)
        if (memSize >= 1024)
            pageCacheCapacity = 3;
        else if (memSize >= 512)
            pageCacheCapacity = 2;
        else if (memSize >= 256)
            pageCacheCapacity = 1;
        else
            pageCacheCapacity = 0;

        // Object cache capacities (in bytes)
        if (memSize >= 2048)
            cacheTotalCapacity = 96 * 1024 * 1024;
        else if (memSize >= 1536)
            cacheTotalCapacity = 64 * 1024 * 1024;
        else if (memSize >= 1024)
            cacheTotalCapacity = 32 * 1024 * 1024;
        else if (memSize >= 512)
            cacheTotalCapacity = 16 * 1024 * 1024;

        cacheMinDeadCapacity = cacheTotalCapacity / 8;
        cacheMaxDeadCapacity = cacheTotalCapacity / 4;

        // Foundation memory cache capacity (in bytes)
        if (memSize >= 2048)
            nsurlCacheMemoryCapacity = 4 * 1024 * 1024;
        else if (memSize >= 1024)
            nsurlCacheMemoryCapacity = 2 * 1024 * 1024;
        else if (memSize >= 512)
            nsurlCacheMemoryCapacity = 1 * 1024 * 1024;
        else
            nsurlCacheMemoryCapacity =      512 * 1024; 

        // Foundation disk cache capacity (in bytes)
        if (diskFreeSize >= 16384)
            nsurlCacheDiskCapacity = 50 * 1024 * 1024;
        else if (diskFreeSize >= 8192)
            nsurlCacheDiskCapacity = 40 * 1024 * 1024;
        else if (diskFreeSize >= 4096)
            nsurlCacheDiskCapacity = 30 * 1024 * 1024;
        else
            nsurlCacheDiskCapacity = 20 * 1024 * 1024;

        break;
    }
    case WebCacheModelPrimaryWebBrowser: {
        // Page cache capacity (in pages)
        // (Research indicates that value / page drops substantially after 3 pages.)
        if (memSize >= 2048)
            pageCacheCapacity = 5;
        else if (memSize >= 1024)
            pageCacheCapacity = 4;
        else if (memSize >= 512)
            pageCacheCapacity = 3;
        else if (memSize >= 256)
            pageCacheCapacity = 2;
        else
            pageCacheCapacity = 1;

        // Object cache capacities (in bytes)
        // (Testing indicates that value / MB depends heavily on content and
        // browsing pattern. Even growth above 128MB can have substantial 
        // value / MB for some content / browsing patterns.)
        if (memSize >= 2048)
            cacheTotalCapacity = 128 * 1024 * 1024;
        else if (memSize >= 1536)
            cacheTotalCapacity = 96 * 1024 * 1024;
        else if (memSize >= 1024)
            cacheTotalCapacity = 64 * 1024 * 1024;
        else if (memSize >= 512)
            cacheTotalCapacity = 32 * 1024 * 1024;

        cacheMinDeadCapacity = cacheTotalCapacity / 4;
        cacheMaxDeadCapacity = cacheTotalCapacity / 2;

        // This code is here to avoid a PLT regression. We can remove it if we
        // can prove that the overall system gain would justify the regression.
        cacheMaxDeadCapacity = max(24u, cacheMaxDeadCapacity);

        deadDecodedDataDeletionInterval = 60;

        // Foundation memory cache capacity (in bytes)
        // (These values are small because WebCore does most caching itself.)
        if (memSize >= 1024)
            nsurlCacheMemoryCapacity = 4 * 1024 * 1024;
        else if (memSize >= 512)
            nsurlCacheMemoryCapacity = 2 * 1024 * 1024;
        else if (memSize >= 256)
            nsurlCacheMemoryCapacity = 1 * 1024 * 1024;
        else
            nsurlCacheMemoryCapacity =      512 * 1024; 

        // Foundation disk cache capacity (in bytes)
        if (diskFreeSize >= 16384)
            nsurlCacheDiskCapacity = 175 * 1024 * 1024;
        else if (diskFreeSize >= 8192)
            nsurlCacheDiskCapacity = 150 * 1024 * 1024;
        else if (diskFreeSize >= 4096)
            nsurlCacheDiskCapacity = 125 * 1024 * 1024;
        else if (diskFreeSize >= 2048)
            nsurlCacheDiskCapacity = 100 * 1024 * 1024;
        else if (diskFreeSize >= 1024)
            nsurlCacheDiskCapacity = 75 * 1024 * 1024;
        else
            nsurlCacheDiskCapacity = 50 * 1024 * 1024;

        break;
    }
    default:
        ASSERT_NOT_REACHED();
    };

#ifdef BUILDING_ON_TIGER
    // Don't use a big Foundation disk cache on Tiger because, according to the 
    // PLT, the Foundation disk cache on Tiger is slower than the network. 
    nsurlCacheDiskCapacity = [nsurlCache diskCapacity];
#endif

    // Don't shrink a big disk cache, since that would cause churn.
    nsurlCacheDiskCapacity = max(nsurlCacheDiskCapacity, [nsurlCache diskCapacity]);

    cache()->setCapacities(cacheMinDeadCapacity, cacheMaxDeadCapacity, cacheTotalCapacity);
    cache()->setDeadDecodedDataDeletionInterval(deadDecodedDataDeletionInterval);
    pageCache()->setCapacity(pageCacheCapacity);
    [nsurlCache setMemoryCapacity:nsurlCacheMemoryCapacity];
    [nsurlCache setDiskCapacity:nsurlCacheDiskCapacity];
    
    s_cacheModel = cacheModel;
    s_didSetCacheModel = YES;
}

+ (WebCacheModel)_cacheModel
{
    return s_cacheModel;
}

+ (WebCacheModel)_didSetCacheModel
{
    return s_didSetCacheModel;
}

+ (WebCacheModel)_maxCacheModelInAnyInstance
{
    WebCacheModel cacheModel = WebCacheModelDocumentViewer;
    NSEnumerator *enumerator = [(NSMutableSet *)allWebViewsSet objectEnumerator];
    while (WebPreferences *preferences = [[enumerator nextObject] preferences])
        cacheModel = max(cacheModel, [preferences cacheModel]);
    return cacheModel;
}

+ (void)_preferencesChangedNotification:(NSNotification *)notification
{
    WebPreferences *preferences = (WebPreferences *)[notification object];
    ASSERT([preferences isKindOfClass:[WebPreferences class]]);

    WebCacheModel cacheModel = [preferences cacheModel];
    if (![self _didSetCacheModel] || cacheModel > [self _cacheModel])
        [self _setCacheModel:cacheModel];
    else if (cacheModel < [self _cacheModel])
        [self _setCacheModel:max([[WebPreferences standardPreferences] cacheModel], [self _maxCacheModelInAnyInstance])];
}

+ (void)_preferencesRemovedNotification:(NSNotification *)notification
{
    WebPreferences *preferences = (WebPreferences *)[notification object];
    ASSERT([preferences isKindOfClass:[WebPreferences class]]);

    if ([preferences cacheModel] == [self _cacheModel])
        [self _setCacheModel:max([[WebPreferences standardPreferences] cacheModel], [self _maxCacheModelInAnyInstance])];
}

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

- (BOOL)_isLoading
{
    WebFrame *mainFrame = [self mainFrame];
    return [[mainFrame _dataSource] isLoading]
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

- (void)_openFrameInNewWindowFromMenu:(NSMenuItem *)sender
{
    ASSERT_ARG(sender, [sender isKindOfClass:[NSMenuItem class]]);

    NSDictionary *element = [sender representedObject];
    ASSERT([element isKindOfClass:[NSDictionary class]]);

    WebDataSource *dataSource = [[element objectForKey:WebElementFrameKey] dataSource];
    NSURLRequest *request = [[dataSource request] copy];
    ASSERT(request);
    
    [self _openNewWindowWithRequest:request];
    [request release];
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
    if (![documentView conformsToProtocol:@protocol(WebDocumentText)])
        return;
    
    NSString *selectedString = [(id <WebDocumentText>)documentView selectedString];
    if ([selectedString length] == 0) {
        return;
    }

    (void)HISearchWindowShow((CFStringRef)selectedString, kNilOptions);
}
@end

@implementation WebView (WebViewInternal)

- (BOOL)_becomingFirstResponderFromOutside
{
    return _private->becomingFirstResponderFromOutside;
}

- (void)_receivedIconChangedNotification:(NSNotification *)notification
{
    // Get the URL for this notification
    NSDictionary *userInfo = [notification userInfo];
    ASSERT([userInfo isKindOfClass:[NSDictionary class]]);
    NSString *urlString = [userInfo objectForKey:WebIconNotificationUserInfoURLKey];
    ASSERT([urlString isKindOfClass:[NSString class]]);
    
    // If that URL matches the current main frame, dispatch the delegate call, which will also unregister
    // us for this notification
    if ([[self mainFrameURL] isEqualTo:urlString])
        [self _dispatchDidReceiveIconFromWebFrame:[self mainFrame]];
}

- (void)_registerForIconNotification:(BOOL)listen
{
    if (listen)
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_receivedIconChangedNotification:) name:WebIconDatabaseDidAddIconNotification object:nil];        
    else
        [[NSNotificationCenter defaultCenter] removeObserver:self name:WebIconDatabaseDidAddIconNotification object:nil];
}

- (void)_dispatchDidReceiveIconFromWebFrame:(WebFrame *)webFrame
{
    // FIXME: This willChangeValueForKey call is too late, because the icon has already changed by now.
    [self _willChangeValueForKey:_WebMainFrameIconKey];
    
    // Since we definitely have an icon and are about to send out the delegate call for that, this WebView doesn't need to listen for the general
    // notification any longer
    [self _registerForIconNotification:NO];

    WebFrameLoadDelegateImplementationCache* cache = &_private->frameLoadDelegateImplementations;
    if (cache->didReceiveIconForFrameFunc) {
        Image* image = iconDatabase()->iconForPageURL(core(webFrame)->loader()->url().string(), IntSize(16, 16));
        if (NSImage *icon = webGetNSImage(image, NSMakeSize(16, 16)))
            CallFrameLoadDelegate(cache->didReceiveIconForFrameFunc, self, @selector(webView:didReceiveIcon:forFrame:), icon, webFrame);
    }

    [self _didChangeValueForKey:_WebMainFrameIconKey];
}

// Get the appropriate user-agent string for a particular URL.
- (WebCore::String)_userAgentForURL:(const WebCore::KURL&)url
{
    if (_private->useSiteSpecificSpoofing) {
        // No current site-specific spoofs.
    }

    if (_private->userAgent.isNull())
        _private->userAgent = [[self class] _standardUserAgentWithApplicationName:_private->applicationNameForUserAgent];

    return _private->userAgent;
}

- (void)_addObject:(id)object forIdentifier:(unsigned long)identifier
{
    ASSERT(!_private->identifierMap.contains(identifier));

    // If the identifier map is initially empty it means we're starting a load
    // of something. The semantic is that the web view should be around as long 
    // as something is loading. Because of that we retain the web view.
    if (_private->identifierMap.isEmpty())
        CFRetain(self);
    
    _private->identifierMap.set(identifier, object);
}

- (id)_objectForIdentifier:(unsigned long)identifier
{
    return _private->identifierMap.get(identifier).get();
}

- (void)_removeObjectForIdentifier:(unsigned long)identifier
{
    ASSERT(_private->identifierMap.contains(identifier));
    _private->identifierMap.remove(identifier);
    
    // If the identifier map is now empty it means we're no longer loading anything
    // and we should release the web view.
    if (_private->identifierMap.isEmpty())
        CFRelease(self);
}

- (void)_retrieveKeyboardUIModeFromPreferences:(NSNotification *)notification
{
    CFPreferencesAppSynchronize(UniversalAccessDomain);

    Boolean keyExistsAndHasValidFormat;
    int mode = CFPreferencesGetAppIntegerValue(AppleKeyboardUIMode, UniversalAccessDomain, &keyExistsAndHasValidFormat);
    
    // The keyboard access mode is reported by two bits:
    // Bit 0 is set if feature is on
    // Bit 1 is set if full keyboard access works for any control, not just text boxes and lists
    // We require both bits to be on.
    // I do not know that we would ever get one bit on and the other off since
    // checking the checkbox in system preferences which is marked as "Turn on full keyboard access"
    // turns on both bits.
    _private->_keyboardUIMode = (mode & 0x2) ? KeyboardAccessFull : KeyboardAccessDefault;
    
    // check for tabbing to links
    if ([_private->preferences tabsToLinks])
        _private->_keyboardUIMode = (KeyboardUIMode)(_private->_keyboardUIMode | KeyboardAccessTabsToLinks);
}

- (KeyboardUIMode)_keyboardUIMode
{
    if (!_private->_keyboardUIModeAccessed) {
        _private->_keyboardUIModeAccessed = YES;

        [self _retrieveKeyboardUIModeFromPreferences:nil];
        
        [[NSDistributedNotificationCenter defaultCenter] 
            addObserver:self selector:@selector(_retrieveKeyboardUIModeFromPreferences:) 
            name:KeyboardUIModeDidChangeNotification object:nil];

        [[NSNotificationCenter defaultCenter] 
            addObserver:self selector:@selector(_retrieveKeyboardUIModeFromPreferences:) 
            name:WebPreferencesChangedNotification object:nil];
    }
    return _private->_keyboardUIMode;
}

- (void)_setInsertionPasteboard:(NSPasteboard *)pasteboard
{
    _private->insertionPasteboard = pasteboard;
}

@end

// We use these functions to call the delegates and block exceptions. These functions are
// declared inside a WebView category to get direct access to the delegate data memebers,
// preventing more ObjC message dispatch and compensating for the expense of the @try/@catch.

@implementation WebView (WebCallDelegateFunctions)

typedef float (*ObjCMsgSendFPRet)(id, SEL, ...);
#if defined(__i386__)
static const ObjCMsgSendFPRet objc_msgSend_float_return = reinterpret_cast<ObjCMsgSendFPRet>(objc_msgSend_fpret);
#else
static const ObjCMsgSendFPRet objc_msgSend_float_return = reinterpret_cast<ObjCMsgSendFPRet>(objc_msgSend);
#endif

static inline id CallDelegate(WebView *self, id delegate, SEL selector)
{
    if (!delegate || ![delegate respondsToSelector:selector])
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return objc_msgSend(delegate, selector, self);
    @try {
        return objc_msgSend(delegate, selector, self);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(WebView *self, id delegate, SEL selector, id object)
{
    if (!delegate || ![delegate respondsToSelector:selector])
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return objc_msgSend(delegate, selector, self, object);
    @try {
        return objc_msgSend(delegate, selector, self, object);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(WebView *self, id delegate, SEL selector, NSRect rect)
{
    if (!delegate || ![delegate respondsToSelector:selector])
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return reinterpret_cast<id (*)(id, SEL, WebView *, NSRect)>(objc_msgSend)(delegate, selector, self, rect);
    @try {
        return reinterpret_cast<id (*)(id, SEL, WebView *, NSRect)>(objc_msgSend)(delegate, selector, self, rect);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(WebView *self, id delegate, SEL selector, id object1, id object2)
{
    if (!delegate || ![delegate respondsToSelector:selector])
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return objc_msgSend(delegate, selector, self, object1, object2);
    @try {
        return objc_msgSend(delegate, selector, self, object1, object2);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(WebView *self, id delegate, SEL selector, id object, BOOL boolean)
{
    if (!delegate || ![delegate respondsToSelector:selector])
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return objc_msgSend(delegate, selector, self, object, boolean);
    @try {
        return objc_msgSend(delegate, selector, self, object, boolean);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(WebView *self, id delegate, SEL selector, id object1, id object2, id object3)
{
    if (!delegate || ![delegate respondsToSelector:selector])
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return objc_msgSend(delegate, selector, self, object1, object2, object3);
    @try {
        return objc_msgSend(delegate, selector, self, object1, object2, object3);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(WebView *self, id delegate, SEL selector, id object, NSUInteger integer)
{
    if (!delegate || ![delegate respondsToSelector:selector])
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return objc_msgSend(delegate, selector, self, object, integer);
    @try {
        return objc_msgSend(delegate, selector, self, object, integer);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline float CallDelegateReturningFloat(WebView *self, id delegate, SEL selector)
{
    if (!delegate || ![delegate respondsToSelector:selector])
        return 0.0f;
    if (!self->_private->catchesDelegateExceptions)
        return objc_msgSend_float_return(delegate, selector, self);
    @try {
        return objc_msgSend_float_return(delegate, selector, self);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return 0.0f;
}

static inline BOOL CallDelegateReturningBoolean(BOOL result, WebView *self, id delegate, SEL selector)
{
    if (!delegate || ![delegate respondsToSelector:selector])
        return result;
    if (!self->_private->catchesDelegateExceptions)
        return reinterpret_cast<BOOL (*)(id, SEL, WebView *)>(objc_msgSend)(delegate, selector, self);
    @try {
        return reinterpret_cast<BOOL (*)(id, SEL, WebView *)>(objc_msgSend)(delegate, selector, self);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return result;
}

static inline BOOL CallDelegateReturningBoolean(BOOL result, WebView *self, id delegate, SEL selector, id object)
{
    if (!delegate || ![delegate respondsToSelector:selector])
        return result;
    if (!self->_private->catchesDelegateExceptions)
        return reinterpret_cast<BOOL (*)(id, SEL, WebView *, id)>(objc_msgSend)(delegate, selector, self, object);
    @try {
        return reinterpret_cast<BOOL (*)(id, SEL, WebView *, id)>(objc_msgSend)(delegate, selector, self, object);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return result;
}

static inline BOOL CallDelegateReturningBoolean(BOOL result, WebView *self, id delegate, SEL selector, id object, BOOL boolean)
{
    if (!delegate || ![delegate respondsToSelector:selector])
        return result;
    if (!self->_private->catchesDelegateExceptions)
        return reinterpret_cast<BOOL (*)(id, SEL, WebView *, id, BOOL)>(objc_msgSend)(delegate, selector, self, object, boolean);
    @try {
        return reinterpret_cast<BOOL (*)(id, SEL, WebView *, id, BOOL)>(objc_msgSend)(delegate, selector, self, object, boolean);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return result;
}

static inline BOOL CallDelegateReturningBoolean(BOOL result, WebView *self, id delegate, SEL selector, id object1, id object2)
{
    if (!delegate || ![delegate respondsToSelector:selector])
        return result;
    if (!self->_private->catchesDelegateExceptions)
        return reinterpret_cast<BOOL (*)(id, SEL, WebView *, id, id)>(objc_msgSend)(delegate, selector, self, object1, object2);
    @try {
        return reinterpret_cast<BOOL (*)(id, SEL, WebView *, id, id)>(objc_msgSend)(delegate, selector, self, object1, object2);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return result;
}

static inline id CallDelegate(IMP implementation, WebView *self, id delegate, SEL selector)
{
    if (!delegate)
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return implementation(delegate, selector, self);
    @try {
        return implementation(delegate, selector, self);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(IMP implementation, WebView *self, id delegate, SEL selector, id object)
{
    if (!delegate)
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return implementation(delegate, selector, self, object);
    @try {
        return implementation(delegate, selector, self, object);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(IMP implementation, WebView *self, id delegate, SEL selector, id object1, id object2)
{
    if (!delegate)
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return implementation(delegate, selector, self, object1, object2);
    @try {
        return implementation(delegate, selector, self, object1, object2);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(IMP implementation, WebView *self, id delegate, SEL selector, id object1, id object2, id object3)
{
    if (!delegate)
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return implementation(delegate, selector, self, object1, object2, object3);
    @try {
        return implementation(delegate, selector, self, object1, object2, object3);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(IMP implementation, WebView *self, id delegate, SEL selector, id object1, id object2, id object3, id object4)
{
    if (!delegate)
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return implementation(delegate, selector, self, object1, object2, object3, object4);
    @try {
        return implementation(delegate, selector, self, object1, object2, object3, object4);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(IMP implementation, WebView *self, id delegate, SEL selector, id object1, NSInteger integer, id object2)
{
    if (!delegate)
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return implementation(delegate, selector, self, object1, integer, object2);
    @try {
        return implementation(delegate, selector, self, object1, integer, object2);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(IMP implementation, WebView *self, id delegate, SEL selector, id object1, NSInteger integer1, NSInteger integer2, id object2)
{
    if (!delegate)
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return implementation(delegate, selector, self, object1, integer1, integer2, object2);
    @try {
        return implementation(delegate, selector, self, object1, integer1, integer2, object2);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(IMP implementation, WebView *self, id delegate, SEL selector, id object1, id object2, NSInteger integer, id object3)
{
    if (!delegate)
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return implementation(delegate, selector, self, object1, object2, integer, object3);
    @try {
        return implementation(delegate, selector, self, object1, object2, integer, object3);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(IMP implementation, WebView *self, id delegate, SEL selector, id object1, NSInteger integer1, id object2, NSInteger integer2, id object3)
{
    if (!delegate)
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return implementation(delegate, selector, self, object1, integer1, object2, integer2, object3);
    @try {
        return implementation(delegate, selector, self, object1, integer1, object2, integer2, object3);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(IMP implementation, WebView *self, id delegate, SEL selector, id object1, NSInteger integer, id object2, id object3, id object4)
{
    if (!delegate)
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return implementation(delegate, selector, self, object1, integer, object2, object3, object4);
    @try {
        return implementation(delegate, selector, self, object1, integer, object2, object3, object4);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

static inline id CallDelegate(IMP implementation, WebView *self, id delegate, SEL selector, id object1, NSTimeInterval interval, id object2, id object3)
{
    if (!delegate)
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return implementation(delegate, selector, self, object1, interval, object2, object3);
    @try {
        return implementation(delegate, selector, self, object1, interval, object2, object3);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

id CallUIDelegate(WebView *self, SEL selector)
{
    return CallDelegate(self, self->_private->UIDelegate, selector);
}

id CallUIDelegate(WebView *self, SEL selector, id object)
{
    return CallDelegate(self, self->_private->UIDelegate, selector, object);
}

id CallUIDelegate(WebView *self, SEL selector, id object, BOOL boolean)
{
    return CallDelegate(self, self->_private->UIDelegate, selector, object, boolean);
}

id CallUIDelegate(WebView *self, SEL selector, NSRect rect)
{
    return CallDelegate(self, self->_private->UIDelegate, selector, rect);
}

id CallUIDelegate(WebView *self, SEL selector, id object1, id object2)
{
    return CallDelegate(self, self->_private->UIDelegate, selector, object1, object2);
}

id CallUIDelegate(WebView *self, SEL selector, id object1, id object2, id object3)
{
    return CallDelegate(self, self->_private->UIDelegate, selector, object1, object2, object3);
}

id CallUIDelegate(WebView *self, SEL selector, id object, NSUInteger integer)
{
    return CallDelegate(self, self->_private->UIDelegate, selector, object, integer);
}

float CallUIDelegateReturningFloat(WebView *self, SEL selector)
{
    return CallDelegateReturningFloat(self, self->_private->UIDelegate, selector);
}

BOOL CallUIDelegateReturningBoolean(BOOL result, WebView *self, SEL selector)
{
    return CallDelegateReturningBoolean(result, self, self->_private->UIDelegate, selector);
}

BOOL CallUIDelegateReturningBoolean(BOOL result, WebView *self, SEL selector, id object)
{
    return CallDelegateReturningBoolean(result, self, self->_private->UIDelegate, selector, object);
}

BOOL CallUIDelegateReturningBoolean(BOOL result, WebView *self, SEL selector, id object, BOOL boolean)
{
    return CallDelegateReturningBoolean(result, self, self->_private->UIDelegate, selector, object, boolean);
}

BOOL CallUIDelegateReturningBoolean(BOOL result, WebView *self, SEL selector, id object1, id object2)
{
    return CallDelegateReturningBoolean(result, self, self->_private->UIDelegate, selector, object1, object2);
}

id CallFrameLoadDelegate(IMP implementation, WebView *self, SEL selector)
{
    return CallDelegate(implementation, self, self->_private->frameLoadDelegate, selector);
}

id CallFrameLoadDelegate(IMP implementation, WebView *self, SEL selector, id object)
{
    return CallDelegate(implementation, self, self->_private->frameLoadDelegate, selector, object);
}

id CallFrameLoadDelegate(IMP implementation, WebView *self, SEL selector, id object1, id object2)
{
    return CallDelegate(implementation, self, self->_private->frameLoadDelegate, selector, object1, object2);
}

id CallFrameLoadDelegate(IMP implementation, WebView *self, SEL selector, id object1, id object2, id object3)
{
    return CallDelegate(implementation, self, self->_private->frameLoadDelegate, selector, object1, object2, object3);
}

id CallFrameLoadDelegate(IMP implementation, WebView *self, SEL selector, id object1, id object2, id object3, id object4)
{
    return CallDelegate(implementation, self, self->_private->frameLoadDelegate, selector, object1, object2, object3, object4);
}

id CallFrameLoadDelegate(IMP implementation, WebView *self, SEL selector, id object1, NSTimeInterval interval, id object2, id object3)
{
    return CallDelegate(implementation, self, self->_private->frameLoadDelegate, selector, object1, interval, object2, object3);
}

id CallResourceLoadDelegate(IMP implementation, WebView *self, SEL selector, id object1, id object2)
{
    return CallDelegate(implementation, self, self->_private->resourceProgressDelegate, selector, object1, object2);
}

id CallResourceLoadDelegate(IMP implementation, WebView *self, SEL selector, id object1, id object2, id object3)
{
    return CallDelegate(implementation, self, self->_private->resourceProgressDelegate, selector, object1, object2, object3);
}

id CallResourceLoadDelegate(IMP implementation, WebView *self, SEL selector, id object1, id object2, id object3, id object4)
{
    return CallDelegate(implementation, self, self->_private->resourceProgressDelegate, selector, object1, object2, object3, object4);
}

id CallResourceLoadDelegate(IMP implementation, WebView *self, SEL selector, id object1, NSInteger integer, id object2)
{
    return CallDelegate(implementation, self, self->_private->resourceProgressDelegate, selector, object1, integer, object2);
}

id CallResourceLoadDelegate(IMP implementation, WebView *self, SEL selector, id object1, id object2, NSInteger integer, id object3)
{
    return CallDelegate(implementation, self, self->_private->resourceProgressDelegate, selector, object1, object2, integer, object3);
}

BOOL CallResourceLoadDelegateReturningBoolean(BOOL result, IMP implementation, WebView *self, SEL selector, id object1, id object2)
{
    if (!self->_private->catchesDelegateExceptions)
        return reinterpret_cast<BOOL (*)(id, SEL, WebView *, id, id)>(objc_msgSend)(self->_private->resourceProgressDelegate, selector, self, object1, object2);
    @try {
        return reinterpret_cast<BOOL (*)(id, SEL, WebView *, id, id)>(objc_msgSend)(self->_private->resourceProgressDelegate, selector, self, object1, object2);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return result;
}

id CallScriptDebugDelegate(IMP implementation, WebView *self, SEL selector, id object1, id object2, NSInteger integer, id object3)
{
    return CallDelegate(implementation, self, self->_private->scriptDebugDelegate, selector, object1, object2, integer, object3);
}

id CallScriptDebugDelegate(IMP implementation, WebView *self, SEL selector, id object1, NSInteger integer1, id object2, NSInteger integer2, id object3)
{
    return CallDelegate(implementation, self, self->_private->scriptDebugDelegate, selector, object1, integer1, object2, integer2, object3);
}

id CallScriptDebugDelegate(IMP implementation, WebView *self, SEL selector, id object1, NSInteger integer, id object2, id object3, id object4)
{
    return CallDelegate(implementation, self, self->_private->scriptDebugDelegate, selector, object1, integer, object2, object3, object4);
}

id CallScriptDebugDelegate(IMP implementation, WebView *self, SEL selector, id object1, NSInteger integer1, NSInteger integer2, id object2)
{
    return CallDelegate(implementation, self, self->_private->scriptDebugDelegate, selector, object1, integer1, integer2, object2);
}

// The form delegate needs to have it's own implementation, because the first argument is never the WebView

id CallFormDelegate(WebView *self, SEL selector, id object1, id object2)
{
    id delegate = self->_private->formDelegate;
    if (!delegate || ![delegate respondsToSelector:selector])
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return objc_msgSend(delegate, selector, object1, object2);
    @try {
        return objc_msgSend(delegate, selector, object1, object2);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

id CallFormDelegate(WebView *self, SEL selector, id object1, id object2, id object3, id object4, id object5)
{
    id delegate = self->_private->formDelegate;
    if (!delegate || ![delegate respondsToSelector:selector])
        return nil;
    if (!self->_private->catchesDelegateExceptions)
        return objc_msgSend(delegate, selector, object1, object2, object3, object4, object5);
    @try {
        return objc_msgSend(delegate, selector, object1, object2, object3, object4, object5);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return nil;
}

BOOL CallFormDelegateReturningBoolean(BOOL result, WebView *self, SEL selector, id object1, SEL selectorArg, id object2)
{
    id delegate = self->_private->formDelegate;
    if (!delegate || ![delegate respondsToSelector:selector])
        return result;
    if (!self->_private->catchesDelegateExceptions)
        return reinterpret_cast<BOOL (*)(id, SEL, id, SEL, id)>(objc_msgSend)(delegate, selector, object1, selectorArg, object2);
    @try {
        return reinterpret_cast<BOOL (*)(id, SEL, id, SEL, id)>(objc_msgSend)(delegate, selector, object1, selectorArg, object2);
    } @catch(id exception) {
        ReportDiscardedDelegateException(selector, exception);
    }
    return result;
}

#if USE(ACCELERATED_COMPOSITING)
static void viewUpdateRunLoopObserverCallBack(CFRunLoopObserverRef, CFRunLoopActivity, void* info)
{
    WebView* webView = reinterpret_cast<WebView*>(info);
    [webView _viewWillDrawInternal];
    [webView->_private _clearViewUpdateRunLoopObserver];
}

- (void)_scheduleViewUpdate
{
    // This is the compositing equivalent of -viewWillDraw. When we know that compositing layers
    // have been set as needing display, we have to make an eager layout happen before the
    // layers get committed by CA.
    if (!_private->viewUpdateRunLoopObserver) {
        const CFIndex runLoopOrder = NSDisplayWindowRunLoopOrdering-1; // Run before AppKit does its window update
        CFRunLoopObserverContext context = {
            0,
            reinterpret_cast<void*>(self),
            NULL,   // The WebView always outlives the observer, so no need to retain/release.
            NULL,
            NULL
        };
        _private->viewUpdateRunLoopObserver = CFRunLoopObserverCreate(NULL, kCFRunLoopBeforeWaiting | kCFRunLoopExit, false /* one shot */,
                                                        runLoopOrder, viewUpdateRunLoopObserverCallBack, &context);

        CFRunLoopAddObserver(CFRunLoopGetCurrent(), _private->viewUpdateRunLoopObserver, kCFRunLoopCommonModes);
    }
    
}
#endif

@end

#ifdef BUILDING_ON_LEOPARD

static IMP originalRecursivelyRemoveMailAttributesImp;

static id objectElementDataAttribute(DOMHTMLObjectElement *self, SEL)
{
    return [self getAttribute:@"data"];
}

static void recursivelyRemoveMailAttributes(DOMNode *self, SEL selector, BOOL a, BOOL b, BOOL c)
{
    // While inside this Mail function, change the behavior of -[DOMHTMLObjectElement data] back to what it used to be
    // before we fixed a bug in it (see http://trac.webkit.org/changeset/30044 for that change).

    // It's a little bit strange to patch a method defined by WebKit, but it helps keep this workaround self-contained.

    Method methodToPatch = class_getInstanceMethod(objc_getRequiredClass("DOMHTMLObjectElement"), @selector(data));
    IMP originalDataImp = method_setImplementation(methodToPatch, reinterpret_cast<IMP>(objectElementDataAttribute));
    originalRecursivelyRemoveMailAttributesImp(self, selector, a, b, c);
    method_setImplementation(methodToPatch, originalDataImp);
}

#endif

static void patchMailRemoveAttributesMethod()
{
#ifdef BUILDING_ON_LEOPARD
    if (!WKAppVersionCheckLessThan(@"com.apple.mail", -1, 4.0))
        return;
    Method methodToPatch = class_getInstanceMethod(objc_getRequiredClass("DOMNode"), @selector(recursivelyRemoveMailAttributes:convertObjectsToImages:convertEditableElements:));
    if (!methodToPatch)
        return;
    originalRecursivelyRemoveMailAttributesImp = method_setImplementation(methodToPatch, reinterpret_cast<IMP>(recursivelyRemoveMailAttributes));
#endif
}
