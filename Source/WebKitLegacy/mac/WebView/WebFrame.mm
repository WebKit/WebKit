/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import "WebFrameInternal.h"

#import "DOMCSSStyleDeclarationInternal.h"
#import "DOMDocumentFragmentInternal.h"
#import "DOMDocumentInternal.h"
#import "DOMElementInternal.h"
#import "DOMHTMLElementInternal.h"
#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "WebArchiveInternal.h"
#import "WebChromeClient.h"
#import "WebDataSourceInternal.h"
#import "WebDocumentLoaderMac.h"
#import "WebDynamicScrollBarsView.h"
#import "WebElementDictionary.h"
#import "WebFrameLoaderClient.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLView.h"
#import "WebHTMLViewInternal.h"
#import "WebKitStatisticsPrivate.h"
#import "WebKitVersionChecks.h"
#import "WebNSObjectExtras.h"
#import "WebNSURLExtras.h"
#import "WebScriptDebugger.h"
#import "WebScriptWorldInternal.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/JSCJSValue.h>
#import <JavaScriptCore/JSContextInternal.h>
#import <JavaScriptCore/JSLock.h>
#import <JavaScriptCore/JSObject.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/AccessibilityObject.h>
#import <WebCore/CSSAnimationController.h>
#import <WebCore/CSSStyleDeclaration.h>
#import <WebCore/CachedResourceLoader.h>
#import <WebCore/Chrome.h>
#import <WebCore/ColorMac.h>
#import <WebCore/DatabaseManager.h>
#import <WebCore/DocumentFragment.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/DocumentMarkerController.h>
#import <WebCore/Editing.h>
#import <WebCore/Editor.h>
#import <WebCore/EventHandler.h>
#import <WebCore/EventNames.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoadRequest.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameLoaderStateMachine.h>
#import <WebCore/FrameTree.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/HTMLFrameOwnerElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/JSNode.h>
#import <WebCore/LegacyWebArchive.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/Page.h>
#import <WebCore/PlatformEventFactoryMac.h>
#import <WebCore/PluginData.h>
#import <WebCore/PrintContext.h>
#import <WebCore/RenderLayer.h>
#import <WebCore/RenderView.h>
#import <WebCore/RenderWidget.h>
#import <WebCore/RenderedDocumentMarker.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/ScriptController.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/SmartReplace.h>
#import <WebCore/StyleProperties.h>
#import <WebCore/SubframeLoader.h>
#import <WebCore/TextIterator.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/VisibleUnits.h>
#import <WebCore/markup.h>

#if PLATFORM(IOS_FAMILY)
#import "WebMailDelegate.h"
#import "WebResource.h"
#import "WebUIKitDelegate.h"
#import <WebCore/Document.h>
#import <WebCore/EditorClient.h>
#import <WebCore/FocusController.h>
#import <WebCore/Font.h>
#import <WebCore/FrameSelection.h>
#import <WebCore/HistoryController.h>
#import <WebCore/NodeTraversal.h>
#import <WebCore/RenderLayer.h>
#import <WebCore/TextResourceDecoder.h>
#import <WebCore/WAKScrollView.h>
#import <WebCore/WAKWindow.h>
#import <WebCore/WKGraphics.h>
#import <WebCore/WebCoreThreadRun.h>
#endif

#if USE(QUICK_LOOK)
#import <WebCore/QuickLook.h>
#import <WebCore/WebCoreURLResponseIOS.h>
#endif

using namespace WebCore;
using namespace HTMLNames;

using JSC::JSGlobalObject;
using JSC::JSLock;

/*
Here is the current behavior matrix for four types of navigations:

Standard Nav:

 Restore form state:   YES
 Restore scroll and focus state:  YES
 Cache policy: NSURLRequestUseProtocolCachePolicy
 Add to back/forward list: YES
 
Back/Forward:

 Restore form state:   YES
 Restore scroll and focus state:  YES
 Cache policy: NSURLRequestReturnCacheDataElseLoad
 Add to back/forward list: NO

Reload (meaning only the reload button):

 Restore form state:   NO
 Restore scroll and focus state:  YES
 Cache policy: NSURLRequestReloadIgnoringCacheData
 Add to back/forward list: NO

Repeat load of the same URL (by any other means of navigation other than the reload button, including hitting return in the location field):

 Restore form state:   NO
 Restore scroll and focus state:  NO, reset to initial conditions
 Cache policy: NSURLRequestReloadIgnoringCacheData
 Add to back/forward list: NO
*/

NSString *WebPageCacheEntryDateKey = @"WebPageCacheEntryDateKey";
NSString *WebPageCacheDataSourceKey = @"WebPageCacheDataSourceKey";
NSString *WebPageCacheDocumentViewKey = @"WebPageCacheDocumentViewKey";

NSString *WebFrameMainDocumentError = @"WebFrameMainDocumentErrorKey";
NSString *WebFrameHasPlugins = @"WebFrameHasPluginsKey";
NSString *WebFrameHasUnloadListener = @"WebFrameHasUnloadListenerKey";
NSString *WebFrameUsesDatabases = @"WebFrameUsesDatabasesKey";
NSString *WebFrameUsesGeolocation = @"WebFrameUsesGeolocationKey";
NSString *WebFrameUsesApplicationCache = @"WebFrameUsesApplicationCacheKey";
NSString *WebFrameCanSuspendActiveDOMObjects = @"WebFrameCanSuspendActiveDOMObjectsKey";

// FIXME: Remove when this key becomes publicly defined
NSString *NSAccessibilityEnhancedUserInterfaceAttribute = @"AXEnhancedUserInterface";

@implementation WebFramePrivate

- (void)dealloc
{
    [webFrameView release];

    [super dealloc];
}

- (void)setWebFrameView:(WebFrameView *)v
{ 
    [v retain];
    [webFrameView release];
    webFrameView = v;
}

@end

EditableLinkBehavior core(WebKitEditableLinkBehavior editableLinkBehavior)
{
    switch (editableLinkBehavior) {
        case WebKitEditableLinkDefaultBehavior:
            return EditableLinkDefaultBehavior;
        case WebKitEditableLinkAlwaysLive:
            return EditableLinkAlwaysLive;
        case WebKitEditableLinkOnlyLiveWithShiftKey:
            return EditableLinkOnlyLiveWithShiftKey;
        case WebKitEditableLinkLiveWhenNotFocused:
            return EditableLinkLiveWhenNotFocused;
        case WebKitEditableLinkNeverLive:
            return EditableLinkNeverLive;
    }
    ASSERT_NOT_REACHED();
    return EditableLinkDefaultBehavior;
}

TextDirectionSubmenuInclusionBehavior core(WebTextDirectionSubmenuInclusionBehavior behavior)
{
    switch (behavior) {
        case WebTextDirectionSubmenuNeverIncluded:
            return TextDirectionSubmenuNeverIncluded;
        case WebTextDirectionSubmenuAutomaticallyIncluded:
            return TextDirectionSubmenuAutomaticallyIncluded;
        case WebTextDirectionSubmenuAlwaysIncluded:
            return TextDirectionSubmenuAlwaysIncluded;
    }
    ASSERT_NOT_REACHED();
    return TextDirectionSubmenuNeverIncluded;
}

#if PLATFORM(IOS_FAMILY)

Vector<Vector<String>> vectorForDictationPhrasesArray(NSArray *dictationPhrases)
{
    Vector<Vector<String>> result;

    for (id dictationPhrase in dictationPhrases) {
        if (![dictationPhrase isKindOfClass:[NSArray class]])
            continue;
        result.append(Vector<String>());
        for (id interpretation : (NSArray *)dictationPhrase) {
            if (![interpretation isKindOfClass:[NSString class]])
                continue;
            result.last().append((NSString *)interpretation);
        }
    }
    
    return result;
}

#endif

@implementation WebFrame (WebInternal)

Frame* core(WebFrame *frame)
{
    return frame ? frame->_private->coreFrame : 0;
}

WebFrame *kit(Frame* frame)
{
    if (!frame)
        return nil;

    FrameLoaderClient& frameLoaderClient = frame->loader().client();
    if (frameLoaderClient.isEmptyFrameLoaderClient())
        return nil;

    return static_cast<WebFrameLoaderClient&>(frameLoaderClient).webFrame();
}

Page* core(WebView *webView)
{
    return [webView page];
}

WebView *kit(Page* page)
{
    if (!page)
        return nil;

    if (page->chrome().client().isEmptyChromeClient())
        return nil;

    return static_cast<WebChromeClient&>(page->chrome().client()).webView();
}

WebView *getWebView(WebFrame *webFrame)
{
    Frame* coreFrame = core(webFrame);
    if (!coreFrame)
        return nil;
    return kit(coreFrame->page());
}

+ (Ref<WebCore::Frame>)_createFrameWithPage:(Page*)page frameName:(const String&)name frameView:(WebFrameView *)frameView ownerElement:(HTMLFrameOwnerElement*)ownerElement
{
    WebView *webView = kit(page);

    WebFrame *frame = [[self alloc] _initWithWebFrameView:frameView webView:webView];
    auto coreFrame = Frame::create(page, ownerElement, new WebFrameLoaderClient(frame));
    [frame release];
    frame->_private->coreFrame = coreFrame.ptr();

    coreFrame.get().tree().setName(name);
    if (ownerElement) {
        ASSERT(ownerElement->document().frame());
        ownerElement->document().frame()->tree().appendChild(coreFrame.get());
    }

    coreFrame.get().init();

    [webView _setZoomMultiplier:[webView _realZoomMultiplier] isTextOnly:[webView _realZoomMultiplierIsTextOnly]];

    return coreFrame;
}

+ (void)_createMainFrameWithPage:(Page*)page frameName:(const String&)name frameView:(WebFrameView *)frameView
{
    WebView *webView = kit(page);

    WebFrame *frame = [[self alloc] _initWithWebFrameView:frameView webView:webView];
    frame->_private->coreFrame = &page->mainFrame();
    static_cast<WebFrameLoaderClient&>(page->mainFrame().loader().client()).setWebFrame(frame);
    [frame release];

    page->mainFrame().tree().setName(name);
    page->mainFrame().init();

    [webView _setZoomMultiplier:[webView _realZoomMultiplier] isTextOnly:[webView _realZoomMultiplierIsTextOnly]];
}

+ (Ref<WebCore::Frame>)_createSubframeWithOwnerElement:(HTMLFrameOwnerElement*)ownerElement frameName:(const String&)name frameView:(WebFrameView *)frameView
{
    return [self _createFrameWithPage:ownerElement->document().frame()->page() frameName:name frameView:frameView ownerElement:ownerElement];
}

- (BOOL)_isIncludedInWebKitStatistics
{
    return _private && _private->includedInWebKitStatistics;
}

#if PLATFORM(IOS_FAMILY)
static NSURL *createUniqueWebDataURL();

+ (void)_createMainFrameWithSimpleHTMLDocumentWithPage:(Page*)page frameView:(WebFrameView *)frameView style:(NSString *)style
{
    WebView *webView = kit(page);
    
    WebFrame *frame = [[self alloc] _initWithWebFrameView:frameView webView:webView];
    frame->_private->coreFrame = &page->mainFrame();
    static_cast<WebFrameLoaderClient&>(page->mainFrame().loader().client()).setWebFrame(frame);
    [frame release];

    frame->_private->coreFrame->initWithSimpleHTMLDocument(style, createUniqueWebDataURL());
}
#endif

- (void)_attachScriptDebugger
{
    auto& windowProxy = _private->coreFrame->windowProxy();

    // Calling ScriptController::globalObject() would create a window proxy, and dispatch corresponding callbacks, which may be premature
    // if the script debugger is attached before a document is created.  These calls use the debuggerWorld(), we will need to pass a world
    // to be able to debug isolated worlds.
    if (!windowProxy.existingJSWindowProxy(debuggerWorld()))
        return;

    auto* globalObject = windowProxy.globalObject(debuggerWorld());
    if (!globalObject)
        return;

    if (_private->scriptDebugger) {
        ASSERT(_private->scriptDebugger.get() == globalObject->debugger());
        return;
    }

    _private->scriptDebugger = std::make_unique<WebScriptDebugger>(globalObject);
}

- (void)_detachScriptDebugger
{
    _private->scriptDebugger = nullptr;
}

- (id)_initWithWebFrameView:(WebFrameView *)fv webView:(WebView *)v
{
    self = [super init];
    if (!self)
        return nil;

    _private = [[WebFramePrivate alloc] init];

    // Set includedInWebKitStatistics before calling WebFrameView _setWebFrame, since
    // it calls WebFrame _isIncludedInWebKitStatistics.
    if ((_private->includedInWebKitStatistics = [[v class] shouldIncludeInWebKitStatistics]))
        ++WebFrameCount;

    if (fv) {
        [_private setWebFrameView:fv];
        [fv _setWebFrame:self];
    }

    _private->shouldCreateRenderers = YES;

    return self;
}

- (void)_clearCoreFrame
{
    _private->coreFrame = 0;
}

- (WebHTMLView *)_webHTMLDocumentView
{
    id documentView = [_private->webFrameView documentView];    
    return [documentView isKindOfClass:[WebHTMLView class]] ? (WebHTMLView *)documentView : nil;
}

- (void)_updateBackgroundAndUpdatesWhileOffscreen
{
    WebView *webView = getWebView(self);
    BOOL drawsBackground = [webView drawsBackground];
#if !PLATFORM(IOS_FAMILY)
    NSColor *backgroundColor = [webView backgroundColor];
#else
    CGColorRef backgroundColor = [webView backgroundColor];
#endif

    Frame* coreFrame = _private->coreFrame;
    for (Frame* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        // Don't call setDrawsBackground:YES here because it may be NO because of a load
        // in progress; WebFrameLoaderClient keeps it set to NO during the load process.
        WebFrame *webFrame = kit(frame);
        if (!drawsBackground)
            [[[webFrame frameView] _scrollView] setDrawsBackground:NO];
#if !PLATFORM(IOS_FAMILY)
        [[[webFrame frameView] _scrollView] setBackgroundColor:backgroundColor];
#endif

        if (FrameView* view = frame->view()) {
            view->setTransparent(!drawsBackground);
#if !PLATFORM(IOS_FAMILY)
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            Color color = colorFromNSColor([backgroundColor colorUsingColorSpaceName:NSDeviceRGBColorSpace]);
            ALLOW_DEPRECATED_DECLARATIONS_END
#else
            Color color = Color(backgroundColor);
#endif
            view->setBaseBackgroundColor(color);
            view->setShouldUpdateWhileOffscreen([webView shouldUpdateWhileOffscreen]);
        }
    }
}

- (void)_setInternalLoadDelegate:(id)internalLoadDelegate
{
    _private->internalLoadDelegate = internalLoadDelegate;
}

- (id)_internalLoadDelegate
{
    return _private->internalLoadDelegate;
}

- (void)_unmarkAllBadGrammar
{
    Frame* coreFrame = _private->coreFrame;
    for (Frame* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        if (Document* document = frame->document())
            document->markers().removeMarkers(DocumentMarker::Grammar);
    }
}

- (void)_unmarkAllMisspellings
{
#if !PLATFORM(IOS_FAMILY)
    Frame* coreFrame = _private->coreFrame;
    for (Frame* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        if (Document* document = frame->document())
            document->markers().removeMarkers(DocumentMarker::Spelling);
    }
#endif
}

- (BOOL)_hasSelection
{
    id documentView = [_private->webFrameView documentView];    

    // optimization for common case to avoid creating potentially large selection string
    if ([documentView isKindOfClass:[WebHTMLView class]])
        if (Frame* coreFrame = _private->coreFrame)
            return coreFrame->selection().isRange();

    if ([documentView conformsToProtocol:@protocol(WebDocumentText)])
        return [[documentView selectedString] length] > 0;
    
    return NO;
}

- (void)_clearSelection
{
    id documentView = [_private->webFrameView documentView];    
    if ([documentView conformsToProtocol:@protocol(WebDocumentText)])
        [documentView deselectAll];
}

#if !ASSERT_DISABLED
- (BOOL)_atMostOneFrameHasSelection
{
    // FIXME: 4186050 is one known case that makes this debug check fail.
    BOOL found = NO;
    Frame* coreFrame = _private->coreFrame;
    for (Frame* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame))
        if ([kit(frame) _hasSelection]) {
            if (found)
                return NO;
            found = YES;
        }
    return YES;
}
#endif

- (WebFrame *)_findFrameWithSelection
{
    Frame* coreFrame = _private->coreFrame;
    for (Frame* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        WebFrame *webFrame = kit(frame);
        if ([webFrame _hasSelection])
            return webFrame;
    }
    return nil;
}

- (void)_clearSelectionInOtherFrames
{
    // We rely on WebDocumentSelection protocol implementors to call this method when they become first 
    // responder. It would be nicer to just notice first responder changes here instead, but there's no 
    // notification sent when the first responder changes in general (Radar 2573089).
    WebFrame *frameWithSelection = [[getWebView(self) mainFrame] _findFrameWithSelection];
    if (frameWithSelection != self)
        [frameWithSelection _clearSelection];

    // While we're in the general area of selection and frames, check that there is only one now.
    ASSERT([[getWebView(self) mainFrame] _atMostOneFrameHasSelection]);
}

static inline WebDataSource *dataSource(DocumentLoader* loader)
{
    return loader ? static_cast<WebDocumentLoaderMac*>(loader)->dataSource() : nil;
}

- (WebDataSource *)_dataSource
{
    return dataSource(_private->coreFrame->loader().documentLoader());
}

#if PLATFORM(IOS_FAMILY)

- (BOOL)_isCommitting
{
    return _private->isCommitting;
}

- (void)_setIsCommitting:(BOOL)value
{
    _private->isCommitting = value;
}

#endif

- (NSArray *)_nodesFromList:(Vector<Node*> *)nodesVector
{
    size_t size = nodesVector->size();
    NSMutableArray *nodes = [NSMutableArray arrayWithCapacity:size];
    for (size_t i = 0; i < size; ++i)
        [nodes addObject:kit((*nodesVector)[i])];
    return nodes;
}

- (NSString *)_selectedString
{
    return _private->coreFrame->displayStringModifiedByEncoding(_private->coreFrame->editor().selectedText());
}

- (NSString *)_stringForRange:(DOMRange *)range
{
    return plainText(core(range), TextIteratorDefaultBehavior, true);
}

- (OptionSet<PaintBehavior>)_paintBehaviorForDestinationContext:(CGContextRef)context
{
#if PLATFORM(MAC)
    // -currentContextDrawingToScreen returns YES for bitmap contexts.
    BOOL isPrinting = ![NSGraphicsContext currentContextDrawingToScreen];
    if (isPrinting)
        return OptionSet<PaintBehavior>(PaintBehavior::FlattenCompositingLayers) | PaintBehavior::Snapshotting;
#endif

    if (CGContextGetType(context) != kCGContextTypeBitmap)
        return PaintBehavior::Normal;

    // If we're drawing into a bitmap, we could be snapshotting or drawing into a layer-backed view.
    if (WebHTMLView *documentView = [self _webHTMLDocumentView]) {
#if PLATFORM(IOS_FAMILY)
        return [[documentView window] isInSnapshottingPaint] ? PaintBehavior::Snapshotting : PaintBehavior::Normal;
#endif
#if PLATFORM(MAC)
        if ([documentView _web_isDrawingIntoLayer])
            return PaintBehavior::Normal;
#endif
    }
    
    return OptionSet<PaintBehavior>(PaintBehavior::FlattenCompositingLayers) | PaintBehavior::Snapshotting;
}

- (void)_drawRect:(NSRect)rect contentsOnly:(BOOL)contentsOnly
{
#if !PLATFORM(IOS_FAMILY)
    ASSERT([[NSGraphicsContext currentContext] isFlipped]);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CGContextRef ctx = static_cast<CGContextRef>([[NSGraphicsContext currentContext] graphicsPort]);
    ALLOW_DEPRECATED_DECLARATIONS_END
#else
    CGContextRef ctx = WKGetCurrentGraphicsContext();
#endif
    GraphicsContext context(ctx);

#if PLATFORM(IOS_FAMILY)
    WebCore::Frame *frame = core(self);
    if (WebCore::Page* page = frame->page())
        context.setIsAcceleratedContext(page->settings().acceleratedDrawingEnabled());
#elif PLATFORM(MAC)
    if (WebHTMLView *htmlDocumentView = [self _webHTMLDocumentView])
        context.setIsAcceleratedContext([htmlDocumentView _web_isDrawingIntoAcceleratedLayer]);
#endif

    FrameView* view = _private->coreFrame->view();
    
    OptionSet<PaintBehavior> oldBehavior = view->paintBehavior();
    OptionSet<PaintBehavior> paintBehavior = oldBehavior;
    
    if (Frame* parentFrame = _private->coreFrame->tree().parent()) {
        // For subframes, we need to inherit the paint behavior from our parent
        if (FrameView* parentView = parentFrame ? parentFrame->view() : nullptr) {
            if (parentView->paintBehavior().contains(PaintBehavior::FlattenCompositingLayers))
                paintBehavior.add(PaintBehavior::FlattenCompositingLayers);
            
            if (parentView->paintBehavior().contains(PaintBehavior::Snapshotting))
                paintBehavior.add(PaintBehavior::Snapshotting);
            
            if (parentView->paintBehavior().contains(PaintBehavior::TileFirstPaint))
                paintBehavior.add(PaintBehavior::TileFirstPaint);
        }
    } else
        paintBehavior.add([self _paintBehaviorForDestinationContext:ctx]);
        
    view->setPaintBehavior(paintBehavior);

    if (contentsOnly)
        view->paintContents(context, enclosingIntRect(rect));
    else
        view->paint(context, enclosingIntRect(rect));

    view->setPaintBehavior(oldBehavior);
}

- (BOOL)_getVisibleRect:(NSRect*)rect
{
    ASSERT_ARG(rect, rect);
    if (RenderWidget* ownerRenderer = _private->coreFrame->ownerRenderer()) {
        if (ownerRenderer->needsLayout())
            return NO;
        *rect = ownerRenderer->pixelSnappedAbsoluteClippedOverflowRect();
        return YES;
    }

    return NO;
}

- (NSString *)_stringByEvaluatingJavaScriptFromString:(NSString *)string
{
    return [self _stringByEvaluatingJavaScriptFromString:string forceUserGesture:true];
}

- (NSString *)_stringByEvaluatingJavaScriptFromString:(NSString *)string forceUserGesture:(BOOL)forceUserGesture
{
    if (!string)
        return @"";

    RELEASE_ASSERT(isMainThread());

    ASSERT(_private->coreFrame->document());
    RetainPtr<WebFrame> protect(self); // Executing arbitrary JavaScript can destroy the frame.
    
#if PLATFORM(IOS_FAMILY)
    ASSERT(WebThreadIsLockedOrDisabled());
    JSC::ExecState* exec = _private->coreFrame->script().globalObject(mainThreadNormalWorld())->globalExec();
    JSC::JSLockHolder jscLock(exec);
#endif

    JSC::JSValue result = _private->coreFrame->script().executeScript(string, forceUserGesture);

    if (!_private->coreFrame) // In case the script removed our frame from the page.
        return @"";

    // This bizarre set of rules matches behavior from WebKit for Safari 2.0.
    // If you don't like it, use -[WebScriptObject evaluateWebScript:] or 
    // JSEvaluateScript instead, since they have less surprising semantics.
    if (!result || (!result.isBoolean() && !result.isString() && !result.isNumber()))
        return @"";

#if !PLATFORM(IOS_FAMILY)
    JSC::ExecState* exec = _private->coreFrame->script().globalObject(mainThreadNormalWorld())->globalExec();
    JSC::JSLockHolder lock(exec);
#endif
    return result.toWTFString(exec);
}

- (NSRect)_caretRectAtPosition:(const Position&)pos affinity:(NSSelectionAffinity)affinity
{
    VisiblePosition visiblePosition(pos, static_cast<EAffinity>(affinity));
    return visiblePosition.absoluteCaretBounds();
}

- (NSRect)_firstRectForDOMRange:(DOMRange *)range
{
   return _private->coreFrame->editor().firstRectForRange(core(range));
}

- (void)_scrollDOMRangeToVisible:(DOMRange *)range
{
    bool insideFixed = false; // FIXME: get via firstRectForRange().
    NSRect rangeRect = [self _firstRectForDOMRange:range];    
    Node *startNode = core([range startContainer]);
        
    if (startNode && startNode->renderer()) {
#if !PLATFORM(IOS_FAMILY)
        startNode->renderer()->scrollRectToVisible(enclosingIntRect(rangeRect), insideFixed, { SelectionRevealMode::Reveal, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded, ShouldAllowCrossOriginScrolling::Yes });
#else
        RenderLayer* layer = startNode->renderer()->enclosingLayer();
        if (layer) {
            layer->setAdjustForIOSCaretWhenScrolling(true);
            startNode->renderer()->scrollRectToVisible(enclosingIntRect(rangeRect), insideFixed, { SelectionRevealMode::Reveal, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded, ShouldAllowCrossOriginScrolling::Yes });
            layer->setAdjustForIOSCaretWhenScrolling(false);
            _private->coreFrame->selection().setCaretRectNeedsUpdate();
            _private->coreFrame->selection().updateAppearance();
        }
#endif
    }
}

#if PLATFORM(IOS_FAMILY)
- (void)_scrollDOMRangeToVisible:(DOMRange *)range withInset:(CGFloat)inset
{
    bool insideFixed = false; // FIXME: get via firstRectForRange().
    NSRect rangeRect = NSInsetRect([self _firstRectForDOMRange:range], inset, inset);
    Node *startNode = core([range startContainer]);

    if (startNode && startNode->renderer()) {
        RenderLayer* layer = startNode->renderer()->enclosingLayer();
        if (layer) {
            layer->setAdjustForIOSCaretWhenScrolling(true);
            startNode->renderer()->scrollRectToVisible(enclosingIntRect(rangeRect), insideFixed, { SelectionRevealMode::Reveal, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded, ShouldAllowCrossOriginScrolling::Yes});
            layer->setAdjustForIOSCaretWhenScrolling(false);

            Frame *coreFrame = core(self);
            if (coreFrame) {
                FrameSelection& frameSelection = coreFrame->selection();
                frameSelection.setCaretRectNeedsUpdate();
                frameSelection.updateAppearance();
            }
        }
    }
}
#endif

- (BOOL)_needsLayout
{
    return _private->coreFrame->view() ? _private->coreFrame->view()->needsLayout() : false;
}

#if !PLATFORM(IOS_FAMILY)
- (DOMRange *)_rangeByAlteringCurrentSelection:(FrameSelection::EAlteration)alteration direction:(SelectionDirection)direction granularity:(TextGranularity)granularity
{
    if (_private->coreFrame->selection().isNone())
        return nil;

    FrameSelection selection;
    selection.setSelection(_private->coreFrame->selection().selection());
    selection.modify(alteration, direction, granularity);
    return kit(selection.toNormalizedRange().get());
}
#endif

- (TextGranularity)_selectionGranularity
{
    return _private->coreFrame->selection().granularity();
}

- (NSRange)_convertToNSRange:(Range *)range
{
    if (!range)
        return NSMakeRange(NSNotFound, 0);

    size_t location;
    size_t length;
    if (!TextIterator::getLocationAndLengthFromRange(_private->coreFrame->selection().rootEditableElementOrDocumentElement(), range, location, length))
        return NSMakeRange(NSNotFound, 0);

    return NSMakeRange(location, length);
}

- (RefPtr<Range>)_convertToDOMRange:(NSRange)nsrange
{
    return [self _convertToDOMRange:nsrange rangeIsRelativeTo:WebRangeIsRelativeTo::EditableRoot];
}

- (RefPtr<Range>)_convertToDOMRange:(NSRange)nsrange rangeIsRelativeTo:(WebRangeIsRelativeTo)rangeIsRelativeTo
{
    if (nsrange.location > INT_MAX)
        return nullptr;
    if (nsrange.length > INT_MAX || nsrange.location + nsrange.length > INT_MAX)
        nsrange.length = INT_MAX - nsrange.location;

    if (rangeIsRelativeTo == WebRangeIsRelativeTo::EditableRoot) {
        // Our critical assumption is that this code path is only called by input methods that
        // concentrate on a given area containing the selection
        // We have to do this because of text fields and textareas. The DOM for those is not
        // directly in the document DOM, so serialization is problematic. Our solution is
        // to use the root editable element of the selection start as the positional base.
        // That fits with AppKit's idea of an input context.
        Element* element = _private->coreFrame->selection().rootEditableElementOrDocumentElement();
        if (!element)
            return nil;
        return TextIterator::rangeFromLocationAndLength(element, nsrange.location, nsrange.length);
    }

    ASSERT(rangeIsRelativeTo == WebRangeIsRelativeTo::Paragraph);

    const VisibleSelection& selection = _private->coreFrame->selection().selection();
    RefPtr<Range> selectedRange = selection.toNormalizedRange();
    if (!selectedRange)
        return nullptr;

    RefPtr<Range> paragraphRange = makeRange(startOfParagraph(selection.visibleStart()), selection.visibleEnd());
    if (!paragraphRange)
        return nullptr;

    ContainerNode& rootNode = paragraphRange.get()->startContainer().treeScope().rootNode();
    int paragraphStartIndex = TextIterator::rangeLength(Range::create(rootNode.document(), &rootNode, 0, &paragraphRange->startContainer(), paragraphRange->startOffset()).ptr());
    return TextIterator::rangeFromLocationAndLength(&rootNode, paragraphStartIndex + static_cast<int>(nsrange.location), nsrange.length);
}

- (DOMRange *)_convertNSRangeToDOMRange:(NSRange)nsrange
{
    return kit([self _convertToDOMRange:nsrange].get());
}

- (NSRange)_convertDOMRangeToNSRange:(DOMRange *)range
{
    return [self _convertToNSRange:core(range)];
}

- (DOMRange *)_markDOMRange
{
    return kit(_private->coreFrame->editor().mark().toNormalizedRange().get());
}

- (DOMDocumentFragment *)_documentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString
{
    Frame* frame = _private->coreFrame;
    if (!frame)
        return nil;

    Document* document = frame->document();
    if (!document)
        return nil;

    return kit(createFragmentFromMarkup(*document, markupString, baseURLString, DisallowScriptingContent).ptr());
}

- (DOMDocumentFragment *)_documentFragmentWithNodesAsParagraphs:(NSArray *)nodes
{
    Frame* frame = _private->coreFrame;
    if (!frame)
        return nil;

    Document* document = frame->document();
    if (!document)
        return nil;

    NSEnumerator *nodeEnum = [nodes objectEnumerator];
    Vector<Node*> nodesVector;
    DOMNode *node;
    while ((node = [nodeEnum nextObject]))
        nodesVector.append(core(node));

    auto fragment = document->createDocumentFragment();

    for (auto* node : nodesVector) {
        auto element = createDefaultParagraphElement(*document);
        element->appendChild(*node);
        fragment->appendChild(element);
    }

    return kit(fragment.ptr());
}

- (void)_replaceSelectionWithNode:(DOMNode *)node selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle
{
    DOMDocumentFragment *fragment = kit(_private->coreFrame->document()->createDocumentFragment().ptr());
    [fragment appendChild:node];
    [self _replaceSelectionWithFragment:fragment selectReplacement:selectReplacement smartReplace:smartReplace matchStyle:matchStyle];
}

- (void)_insertParagraphSeparatorInQuotedContent
{
    if (_private->coreFrame->selection().isNone())
        return;

    _private->coreFrame->editor().insertParagraphSeparatorInQuotedContent();
}

- (VisiblePosition)_visiblePositionForPoint:(NSPoint)point
{
    // FIXME: Someone with access to Apple's sources could remove this needless wrapper call.
    return _private->coreFrame->visiblePositionForPoint(IntPoint(point));
}

- (DOMRange *)_characterRangeAtPoint:(NSPoint)point
{
    return kit(_private->coreFrame->rangeForPoint(IntPoint(point)).get());
}

- (DOMCSSStyleDeclaration *)_typingStyle
{
    if (!_private->coreFrame)
        return nil;
    RefPtr<MutableStyleProperties> typingStyle = _private->coreFrame->selection().copyTypingStyle();
    if (!typingStyle)
        return nil;
    return kit(&typingStyle->ensureCSSStyleDeclaration());
}

- (void)_setTypingStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(EditAction)undoAction
{
    if (!_private->coreFrame || !style)
        return;
    // FIXME: We shouldn't have to create a copy here.
    Ref<MutableStyleProperties> properties(core(style)->copyProperties());
    _private->coreFrame->editor().computeAndSetTypingStyle(properties.get(), undoAction);
}

#if ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)
- (void)_dragSourceEndedAt:(NSPoint)windowLoc operation:(NSDragOperation)operation
{
    if (!_private->coreFrame)
        return;
    FrameView* view = _private->coreFrame->view();
    if (!view)
        return;
    // FIXME: These are fake modifier keys here, but they should be real ones instead.
    PlatformMouseEvent event(IntPoint(windowLoc), IntPoint(globalPoint(windowLoc, [view->platformWidget() window])),
                             LeftButton, PlatformEvent::MouseMoved, 0, false, false, false, false, WallTime::now(), WebCore::ForceAtClick, WebCore::NoTap);
    _private->coreFrame->eventHandler().dragSourceEndedAt(event, (DragOperation)operation);
}
#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)

- (BOOL)_canProvideDocumentSource
{
    Frame* frame = _private->coreFrame;
    String mimeType = frame->document()->loader()->writer().mimeType();
    PluginData* pluginData = frame->page() ? &frame->page()->pluginData() : 0;

    if (WebCore::MIMETypeRegistry::isTextMIMEType(mimeType)
        || Image::supportsType(mimeType)
        || (pluginData && pluginData->supportsWebVisibleMimeType(mimeType, PluginData::AllPlugins) && frame->loader().subframeLoader().allowPlugins())
        || (pluginData && pluginData->supportsWebVisibleMimeType(mimeType, PluginData::OnlyApplicationPlugins)))
        return NO;

    return YES;
}

- (BOOL)_canSaveAsWebArchive
{
    // Currently, all documents that we can view source for
    // (HTML and XML documents) can also be saved as web archives
    return [self _canProvideDocumentSource];
}

- (void)_commitData:(NSData *)data
{
    // FIXME: This really should be a setting.
    Document* document = _private->coreFrame->document();
    document->setShouldCreateRenderers(_private->shouldCreateRenderers);

    _private->coreFrame->loader().documentLoader()->commitData((const char *)[data bytes], [data length]);
}

@end

@implementation WebFrame (WebPrivate)

// FIXME: This exists only as a convenience for Safari, consider moving there.
- (BOOL)_isDescendantOfFrame:(WebFrame *)ancestor
{
    Frame* coreFrame = _private->coreFrame;
    return coreFrame && coreFrame->tree().isDescendantOf(core(ancestor));
}

- (void)_setShouldCreateRenderers:(BOOL)shouldCreateRenderers
{
    _private->shouldCreateRenderers = shouldCreateRenderers;
}

#if !PLATFORM(IOS_FAMILY)
- (NSColor *)_bodyBackgroundColor
#else
- (CGColorRef)_bodyBackgroundColor
#endif
{
    Document* document = _private->coreFrame->document();
    if (!document)
        return nil;
    auto* body = document->bodyOrFrameset();
    if (!body)
        return nil;
    RenderObject* bodyRenderer = body->renderer();
    if (!bodyRenderer)
        return nil;
    Color color = bodyRenderer->style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
    if (!color.isValid())
        return nil;
#if !PLATFORM(IOS_FAMILY)
    return nsColor(color);
#else
    return cachedCGColor(color);
#endif
}

- (BOOL)_isFrameSet
{
    Document* document = _private->coreFrame->document();
    return document && document->isFrameSet();
}

- (BOOL)_firstLayoutDone
{
    return _private->coreFrame->loader().stateMachine().firstLayoutDone();
}

- (BOOL)_isVisuallyNonEmpty
{
    if (FrameView* view = _private->coreFrame->view())
        return view->isVisuallyNonEmpty();
    return NO;
}

static WebFrameLoadType toWebFrameLoadType(FrameLoadType frameLoadType)
{
    switch (frameLoadType) {
    case FrameLoadType::Standard:
        return WebFrameLoadTypeStandard;
    case FrameLoadType::Back:
        return WebFrameLoadTypeBack;
    case FrameLoadType::Forward:
        return WebFrameLoadTypeForward;
    case FrameLoadType::IndexedBackForward:
        return WebFrameLoadTypeIndexedBackForward;
    case FrameLoadType::Reload:
        return WebFrameLoadTypeReload;
    case FrameLoadType::Same:
        return WebFrameLoadTypeSame;
    case FrameLoadType::RedirectWithLockedBackForwardList:
        return WebFrameLoadTypeInternal;
    case FrameLoadType::Replace:
        return WebFrameLoadTypeReplace;
    case FrameLoadType::ReloadFromOrigin:
        return WebFrameLoadTypeReloadFromOrigin;
    case FrameLoadType::ReloadExpiredOnly:
        ASSERT_NOT_REACHED();
        return WebFrameLoadTypeReload;
    }
}

- (WebFrameLoadType)_loadType
{
    return toWebFrameLoadType(_private->coreFrame->loader().loadType());
}

#if PLATFORM(IOS_FAMILY)
- (BOOL)needsLayout
{
    // Needed for Mail <rdar://problem/6228038>
    return _private->coreFrame ? [self _needsLayout] : NO;
}

- (void)_setLoadsSynchronously:(BOOL)flag
{
    _private->coreFrame->loader().setLoadsSynchronously(flag);
}

- (BOOL)_loadsSynchronously
{
    return _private->coreFrame->loader().loadsSynchronously();
}

// FIXME: selection

- (NSArray *)_rectsForRange:(DOMRange *)domRange
{
    Range *range = core(domRange);
    
    
    Vector<IntRect> intRects;
    range->absoluteTextRects(intRects, NO);
    unsigned size = intRects.size();
    
    NSMutableArray *rectArray = [NSMutableArray arrayWithCapacity:size];
    for (unsigned i = 0; i < size; i++) {
        [rectArray addObject:[NSValue valueWithRect:(CGRect )intRects[i]]];
    }
    
    return rectArray;
}

- (DOMRange *)_selectionRangeForFirstPoint:(CGPoint)first secondPoint:(CGPoint)second
{
    VisiblePosition firstPos = [self _visiblePositionForPoint:first];
    VisiblePosition secondPos = [self _visiblePositionForPoint:second];
    VisibleSelection selection(firstPos, secondPos);
    DOMRange *range = kit(selection.toNormalizedRange().get());
    return range;    
}

- (DOMRange *)_selectionRangeForPoint:(CGPoint)point
{
    VisiblePosition pos = [self _visiblePositionForPoint:point];    
    VisibleSelection selection(pos);
    DOMRange *range = kit(selection.toNormalizedRange().get());
    return range;
}

#endif // PLATFORM(IOS_FAMILY)

- (NSRange)_selectedNSRange
{
    return [self _convertToNSRange:_private->coreFrame->selection().toNormalizedRange().get()];
}

- (void)_selectNSRange:(NSRange)range
{
    RefPtr<Range> domRange = [self _convertToDOMRange:range];
    if (domRange)
        _private->coreFrame->selection().setSelection(VisibleSelection(*domRange, SEL_DEFAULT_AFFINITY));
}

- (BOOL)_isDisplayingStandaloneImage
{
    Document* document = _private->coreFrame->document();
    return document && document->isImageDocument();
}

- (unsigned)_pendingFrameUnloadEventCount
{
    return _private->coreFrame->document()->domWindow()->pendingUnloadEventListeners();
}

#if ENABLE(NETSCAPE_PLUGIN_API)
- (void)_recursive_resumeNullEventsForAllNetscapePlugins
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        NSView <WebDocumentView> *documentView = [[kit(frame) frameView] documentView];
        if ([documentView isKindOfClass:[WebHTMLView class]])
            [(WebHTMLView *)documentView _resumeNullEventsForAllNetscapePlugins];
    }
}

- (void)_recursive_pauseNullEventsForAllNetscapePlugins
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        NSView <WebDocumentView> *documentView = [[kit(frame) frameView] documentView];
        if ([documentView isKindOfClass:[WebHTMLView class]])
            [(WebHTMLView *)documentView _pauseNullEventsForAllNetscapePlugins];
    }
}
#endif

#if PLATFORM(IOS_FAMILY)

- (unsigned)formElementsCharacterCount
{
    return core(self)->formElementsCharacterCount();
}

- (void)setTimeoutsPaused:(BOOL)flag
{
    if ([self _webHTMLDocumentView]) {
        if (Frame* coreFrame = _private->coreFrame)
            coreFrame->setTimersPaused(flag);
    }
}

- (void)setPluginsPaused:(BOOL)flag
{
    WebView *webView = getWebView(self);
    if (!webView)
        return;

    if (flag)
        [webView _stopAllPlugIns];
    else
        [webView _startAllPlugIns];
}

- (void)prepareForPause
{
    if ([self _webHTMLDocumentView]) {
        if (Frame* coreFrame = _private->coreFrame)
            coreFrame->dispatchPageHideEventBeforePause();
    }
}

- (void)resumeFromPause
{
    if ([self _webHTMLDocumentView]) {
        if (Frame* coreFrame = _private->coreFrame)
            coreFrame->dispatchPageShowEventBeforeResume();
    }
}

- (void)selectNSRange:(NSRange)range
{
    [self _selectNSRange:range];
}

- (void)selectWithoutClosingTypingNSRange:(NSRange)range
{
    RefPtr<Range> domRange = [self _convertToDOMRange:range];
    if (domRange) {
        const VisibleSelection& newSelection = VisibleSelection(*domRange, SEL_DEFAULT_AFFINITY);
        _private->coreFrame->selection().setSelection(newSelection, { });
        
        _private->coreFrame->editor().ensureLastEditCommandHasCurrentSelectionIfOpenForMoreTyping();
    }
}

- (NSRange)selectedNSRange
{
    return [self _selectedNSRange];
}

- (void)forceLayoutAdjustingViewSize:(BOOL)adjust
{
    _private->coreFrame->view()->forceLayout(!adjust);
    if (adjust)
        _private->coreFrame->view()->adjustViewSize();
}

- (void)_handleKeyEvent:(WebEvent *)event
{
    core(self)->eventHandler().keyEvent(event);
}

- (void)_selectAll
{
    core(self)->selection().selectAll();
}

- (void)_setSelectionFromNone
{
    core(self)->selection().setSelectionFromNone();
}

- (void)_restoreViewState
{
    ASSERT(!WebThreadIsEnabled() || WebThreadIsLocked());
    _private->coreFrame->loader().client().restoreViewState();
}

- (void)_saveViewState
{
    ASSERT(!WebThreadIsEnabled() || WebThreadIsLocked());
    FrameLoader& frameLoader = _private->coreFrame->loader();
    auto* item = frameLoader.history().currentItem();
    if (item)
        frameLoader.client().saveViewStateToItem(*item);
}

- (void)deviceOrientationChanged
{
    WebThreadRun(^{
#if ENABLE(ORIENTATION_EVENTS)
        WebView *webView = getWebView(self);
        [webView _setDeviceOrientation:[[webView _UIKitDelegateForwarder] deviceOrientation]];
#endif
        if (WebCore::Frame* frame = core(self))
            frame->orientationChanged();
    });
}

- (void)setNeedsLayout
{
    WebCore::Frame *frame = core(self);
    if (frame->view())
        frame->view()->setNeedsLayout();
}

- (CGSize)renderedSizeOfNode:(DOMNode *)node constrainedToWidth:(float)width
{
    Node* n = core(node);
    RenderObject* renderer = n ? n->renderer() : nullptr;
    float w = std::min((float)renderer->maxPreferredLogicalWidth(), width);
    return is<RenderBox>(renderer) ? CGSizeMake(w, downcast<RenderBox>(*renderer).height()) : CGSizeMake(0, 0);
}

- (DOMNode *)deepestNodeAtViewportLocation:(CGPoint)aViewportLocation
{
    WebCore::Frame *frame = core(self);
    return kit(frame->deepestNodeAtLocation(FloatPoint(aViewportLocation)));
}

- (DOMNode *)scrollableNodeAtViewportLocation:(CGPoint)aViewportLocation
{
    WebCore::Frame *frame = core(self);
    WebCore::Node *node = frame->nodeRespondingToScrollWheelEvents(FloatPoint(aViewportLocation));
    return kit(node);
}

- (DOMNode *)approximateNodeAtViewportLocation:(CGPoint *)aViewportLocation
{
    WebCore::Frame *frame = core(self);
    FloatPoint viewportLocation(*aViewportLocation);
    FloatPoint adjustedLocation;
    WebCore::Node *node = frame->nodeRespondingToClickEvents(viewportLocation, adjustedLocation);
    *aViewportLocation = adjustedLocation;
    return kit(node);
}

- (CGRect)renderRectForPoint:(CGPoint)point isReplaced:(BOOL *)isReplaced fontSize:(float *)fontSize
{
    WebCore::Frame *frame = core(self);
    bool replaced = false;
    CGRect rect = frame->renderRectForPoint(point, &replaced, fontSize);
    *isReplaced = replaced;
    return rect;
}

- (void)_setProhibitsScrolling:(BOOL)flag
{
    WebCore::Frame *frame = core(self);
    frame->view()->setProhibitsScrolling(flag);
}

- (void)revealSelectionAtExtent:(BOOL)revealExtent
{
    WebCore::Frame *frame = core(self);
    RevealExtentOption revealExtentOption = revealExtent ? RevealExtent : DoNotRevealExtent;
    frame->selection().revealSelection(SelectionRevealMode::Reveal, ScrollAlignment::alignToEdgeIfNeeded, revealExtentOption);
}

- (void)resetSelection
{
    WebCore::Frame *frame = core(self);
    frame->selection().setSelection(frame->selection().selection());
}

- (BOOL)hasEditableSelection
{
    return core(self)->selection().selection().isContentEditable();
}

- (int)preferredHeight
{
    return core(self)->preferredHeight();
}

- (int)innerLineHeight:(DOMNode *)node
{
    if (!node)
        return 0;

    auto& coreNode = *core(node);

    coreNode.document().updateLayout();

    auto* renderer = coreNode.renderer();
    if (!renderer)
        return 0;

    return renderer->innerLineHeight();
}

- (void)updateLayout
{
    WebCore::Frame *frame = core(self);
    frame->updateLayout();
}

- (void)setIsActive:(BOOL)flag
{
    WebCore::Frame *frame = core(self);
    frame->page()->focusController().setActive(flag);
}

- (void)setSelectionChangeCallbacksDisabled:(BOOL)flag
{
    WebCore::Frame *frame = core(self);
    frame->setSelectionChangeCallbacksDisabled(flag);
}

- (NSRect)caretRect
{
    return core(self)->caretRect();
}

- (NSRect)rectForScrollToVisible
{
    return core(self)->rectForScrollToVisible();
}

- (void)setCaretColor:(CGColorRef)color
{
    Color qColor = color ? Color(color) : Color::black;
    WebCore::Frame *frame = core(self);
    frame->selection().setCaretColor(qColor);
}

- (NSView *)documentView
{
    WebCore::Frame *frame = core(self);
    return [[kit(frame) frameView] documentView];
}

- (int)layoutCount
{
    WebCore::Frame *frame = core(self);
    if (!frame || !frame->view())
        return 0;
    return frame->view()->layoutContext().layoutCount();
}

- (BOOL)isTelephoneNumberParsingAllowed
{
    Document *document = core(self)->document();
    return document->isTelephoneNumberParsingAllowed();
}

- (BOOL)isTelephoneNumberParsingEnabled
{
    Document *document = core(self)->document();
    return document->isTelephoneNumberParsingEnabled();
}

- (DOMRange *)selectedDOMRange
{
    WebCore::Frame *frame = core(self);
    RefPtr<WebCore::Range> range = frame->selection().toNormalizedRange();
    return kit(range.get());
}

- (void)setSelectedDOMRange:(DOMRange *)range affinity:(NSSelectionAffinity)affinity closeTyping:(BOOL)closeTyping
{
    WebCore::Frame *frame = core(self);

    // Ensure the view becomes first responder.
    // This does not happen automatically on iOS because we don't forward
    // all the click events to WebKit.
    if (FrameView* frameView = frame->view()) {
        if (NSView *documentView = frameView->documentView()) {
            Page* page = frame->page();
            if (!page)
                return;
            page->chrome().focusNSView(documentView);
        }
    }

    frame->selection().setSelectedRange(core(range), (EAffinity)affinity, closeTyping ? FrameSelection::ShouldCloseTyping::Yes : FrameSelection::ShouldCloseTyping::No);
    if (!closeTyping)
        frame->editor().ensureLastEditCommandHasCurrentSelectionIfOpenForMoreTyping();
}

- (NSSelectionAffinity)selectionAffinity
{
    WebCore::Frame *frame = core(self);
    return (NSSelectionAffinity)(frame->selection().selection().affinity());
}

- (void)expandSelectionToElementContainingCaretSelection
{
    WebCore::Frame *frame = core(self);
    frame->selection().expandSelectionToElementContainingCaretSelection();
}

- (DOMRange *)elementRangeContainingCaretSelection
{
    WebCore::Frame *frame = core(self);
    RefPtr<WebCore::Range> range = frame->selection().elementRangeContainingCaretSelection();
    return kit(range.get());
}

- (void)expandSelectionToWordContainingCaretSelection
{
    WebCore::Frame *frame = core(self);
    frame->selection().expandSelectionToWordContainingCaretSelection();
}

- (void)expandSelectionToStartOfWordContainingCaretSelection
{
    WebCore::Frame *frame = core(self);
    frame->selection().expandSelectionToStartOfWordContainingCaretSelection();
}

- (unichar)characterInRelationToCaretSelection:(int)amount
{
    return core(self)->selection().characterInRelationToCaretSelection(amount);
}

- (unichar)characterBeforeCaretSelection
{
    return core(self)->selection().characterBeforeCaretSelection();
}

- (unichar)characterAfterCaretSelection
{
    return core(self)->selection().characterAfterCaretSelection();
}

- (DOMRange *)wordRangeContainingCaretSelection
{
    WebCore::Frame *frame = core(self);
    RefPtr<WebCore::Range> range = frame->selection().wordRangeContainingCaretSelection();
    return kit(range.get());
}

- (NSString *)wordInRange:(DOMRange *)range
{
    if (!range)
        return nil;
    return [self _stringForRange:range];
}

- (int)wordOffsetInRange:(DOMRange *)range
{
    return core(self)->selection().wordOffsetInRange(core(range));
}

- (BOOL)spaceFollowsWordInRange:(DOMRange *)range
{
    return core(self)->selection().spaceFollowsWordInRange(core(range));
}

- (NSArray *)wordsInCurrentParagraph
{
    return core(self)->wordsInCurrentParagraph();
}

- (BOOL)selectionAtDocumentStart
{
    WebCore::Frame *frame = core(self);
    
    if (frame->selection().selection().isNone())
        return NO;
        
    frame->document()->updateLayout();
    
    return frame->selection().selectionAtDocumentStart();
}

- (BOOL)selectionAtSentenceStart
{
    WebCore::Frame *frame = core(self);
    
    if (frame->selection().selection().isNone())
        return NO;
        
    frame->document()->updateLayout();
    
    return frame->selection().selectionAtSentenceStart();
}

- (BOOL)selectionAtWordStart
{
    WebCore::Frame *frame = core(self);
    
    if (frame->selection().selection().isNone())
        return NO;
        
    frame->document()->updateLayout();
    
    return frame->selection().selectionAtWordStart();
}

- (DOMRange *)rangeByMovingCurrentSelection:(int)amount
{
    WebCore::Frame *frame = core(self);
    RefPtr<WebCore::Range> range = frame->selection().rangeByMovingCurrentSelection(amount);
    return kit(range.get());
}

- (DOMRange *)rangeByExtendingCurrentSelection:(int)amount
{
    WebCore::Frame *frame = core(self);
    RefPtr<WebCore::Range> range = frame->selection().rangeByExtendingCurrentSelection(amount);
    return kit(range.get());
}

- (void)selectNSRange:(NSRange)range onElement:(DOMElement *)element
{
    WebCore::Frame *frame = core(self);

    Document *doc = frame->document();
    if (!doc)
        return;

    Node* node = core(element);
    if (!node->isConnected())
        return;

    frame->selection().selectRangeOnElement(range.location, range.length, *node);
}

- (DOMRange *)markedTextDOMRange
{
    WebCore::Frame *frame = core(self);
    if (!frame)
        return nil;

    return kit(frame->editor().compositionRange().get());
}

- (void)setMarkedText:(NSString *)text selectedRange:(NSRange)newSelRange
{
    WebCore::Frame *frame = core(self);
    if (!frame)
        return;
    
    Vector<CompositionUnderline> underlines;
    frame->page()->chrome().client().suppressFormNotifications();
    frame->editor().setComposition(text, underlines, newSelRange.location, NSMaxRange(newSelRange));
    frame->page()->chrome().client().restoreFormNotifications();
}

- (void)setMarkedText:(NSString *)text forCandidates:(BOOL)forCandidates
{
    WebCore::Frame *frame = core(self);
    if (!frame)
        return;
        
    Vector<CompositionUnderline> underlines;
    frame->editor().setComposition(text, underlines, 0, [text length]);
}

- (void)confirmMarkedText:(NSString *)text
{
    WebCore::Frame *frame = core(self);
    if (!frame || !frame->editor().client())
        return;
    
    frame->page()->chrome().client().suppressFormNotifications();
    if (text)
        frame->editor().confirmComposition(text);
    else
        frame->editor().confirmMarkedText();
    frame->page()->chrome().client().restoreFormNotifications();
}

- (void)setText:(NSString *)text asChildOfElement:(DOMElement *)element
{
    if (!element)
        return;
        
    WebCore::Frame *frame = core(self);
    if (!frame || !frame->document())
        return;
        
    frame->editor().setTextAsChildOfElement(text, *core(element));
}

- (void)setDictationPhrases:(NSArray *)dictationPhrases metadata:(id)metadata asChildOfElement:(DOMElement *)element
{
    if (!element)
        return;
    
    auto* frame = core(self);
    if (!frame)
        return;
    
    frame->editor().setDictationPhrasesAsChildOfElement(vectorForDictationPhrasesArray(dictationPhrases), metadata, *core(element));
}

- (NSArray *)interpretationsForCurrentRoot
{
    return core(self)->interpretationsForCurrentRoot();
}

// Collects the ranges and metadata for all of the mars voltas in the root editable element.
- (void)getDictationResultRanges:(NSArray **)outRanges andMetadatas:(NSArray **)outMetadatas
{
    ASSERT(outRanges);
    if (!outRanges)
        return;
    
    // *outRanges should not already point to an array.
    ASSERT(!(*outRanges));
    *outRanges = nil;
    
    ASSERT(outMetadatas);
    if (!outMetadatas)
        return;
    
    // *metadata should not already point to an array.
    ASSERT(!(*outMetadatas));
    *outMetadatas = nil;
    
    NSMutableArray *ranges = [NSMutableArray array];
    NSMutableArray *metadatas = [NSMutableArray array];
    
    Frame* frame = core(self);
    Document* document = frame->document();

    const VisibleSelection& selection = frame->selection().selection();
    Element* root = selection.selectionType() == VisibleSelection::NoSelection ? frame->document()->bodyOrFrameset() : selection.rootEditableElement();
    
    DOMRange *previousDOMRange = nil;
    id previousMetadata = nil;
    
    for (Node* node = root; node; node = NodeTraversal::next(*node)) {
        auto markers = document->markers().markersFor(node);
        for (auto* marker : markers) {

            if (marker->type() != DocumentMarker::DictationResult)
                continue;
            
            id metadata = marker->metadata();
            
            // All result markers should have metadata.
            ASSERT(metadata);
            if (!metadata)
                continue;
            
            RefPtr<Range> range = Range::create(*document, node, marker->startOffset(), node, marker->endOffset());
            DOMRange *domRange = kit(range.get());
            
            if (metadata != previousMetadata) {
                [metadatas addObject:metadata];
                [ranges addObject:domRange];
                previousMetadata = metadata;
                previousDOMRange = domRange;
            } else {
                // It is possible for a DocumentMarker to be split by editing. Adjacent markers with the
                // the same metadata are for the same result. So combine their ranges.
                ASSERT(previousDOMRange == [ranges lastObject]);
                [previousDOMRange retain];
                [ranges removeLastObject];
                DOMNode *startContainer = [domRange startContainer];
                int startOffset = [domRange startOffset];
                [previousDOMRange setEnd:startContainer offset:startOffset];
                [ranges addObject:previousDOMRange];
                [previousDOMRange release];
            }
        }
    }
    
    *outRanges = ranges;
    *outMetadatas = metadatas;
    
    return;
}

- (id)dictationResultMetadataForRange:(DOMRange *)range
{
    if (!range)
        return nil;
    
    auto markers = core(self)->document()->markers().markersInRange(*core(range), DocumentMarker::DictationResult);
    
    // UIKit should only ever give us a DOMRange for a phrase with alternatives, which should not be part of more than one result.
    ASSERT(markers.size() <= 1);
    if (markers.size() == 0)
        return nil;
    
    return markers[0]->metadata();
}

- (void)recursiveSetUpdateAppearanceEnabled:(BOOL)enabled
{
    WebCore::Frame *frame = core(self);
    if (frame)
        frame->recursiveSetUpdateAppearanceEnabled(enabled);
}

// WebCoreFrameBridge methods used by iOS applications and frameworks
// FIXME: WebCoreFrameBridge is long gone. Can we remove these methods?

+ (NSString *)stringWithData:(NSData *)data textEncodingName:(NSString *)textEncodingName
{
    WebCore::TextEncoding encoding(textEncodingName);
    if (!encoding.isValid())
        encoding = WindowsLatin1Encoding();
    return encoding.decode(reinterpret_cast<const char*>([data bytes]), [data length]);
}

- (NSRect)caretRectAtNode:(DOMNode *)node offset:(int)offset affinity:(NSSelectionAffinity)affinity
{
    return [self _caretRectAtPosition:createLegacyEditingPosition(core(node), offset) affinity:affinity];
}

- (DOMRange *)characterRangeAtPoint:(NSPoint)point
{
    return [self _characterRangeAtPoint:point];
}

- (NSRange)convertDOMRangeToNSRange:(DOMRange *)range
{
    return [self _convertDOMRangeToNSRange:range];
}

- (DOMRange *)convertNSRangeToDOMRange:(NSRange)nsrange
{
    return [self _convertNSRangeToDOMRange:nsrange];
}

- (NSRect)firstRectForDOMRange:(DOMRange *)range
{
    return [self _firstRectForDOMRange:range];
}

- (CTFontRef)fontForSelection:(BOOL *)hasMultipleFonts
{
    bool multipleFonts = false;
    CTFontRef font = nil;
    if (_private->coreFrame) {
        const Font
        * fd = _private->coreFrame->editor().fontForSelection(multipleFonts);
        if (fd)
            font = fd->getCTFont();
    }
    
    if (hasMultipleFonts)
        *hasMultipleFonts = multipleFonts;
    return font;
}

- (void)sendScrollEvent
{
    ASSERT(WebThreadIsLockedOrDisabled());
    _private->coreFrame->eventHandler().sendScrollEvent();
}

- (void)_userScrolled
{
    ASSERT(WebThreadIsLockedOrDisabled());
    if (FrameView* view = _private->coreFrame->view())
        view->setWasScrolledByUser(true);
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string forceUserGesture:(BOOL)forceUserGesture
{
    return [self _stringByEvaluatingJavaScriptFromString:string forceUserGesture:forceUserGesture];
}

- (NSString *)stringForRange:(DOMRange *)range
{
    return [self _stringForRange:range];
}

//
// FIXME: We needed to add this method for iOS due to the opensource version's inclusion of
// matchStyle:YES. It seems odd that we should need to explicitly match style, given that the
// fragment is being made out of plain text, which shouldn't be carrying any style of its own.
// When we paste that it will pick up its style from the surrounding content. What else would
// we expect? If we flipped that matchStyle bit to NO, we could probably just get rid
// of this method, and call the standard WebKit version.
//
// There's a second problem here, too, which is that ReplaceSelectionCommand sometimes adds
// redundant style.
// 
- (void)_replaceSelectionWithText:(NSString *)text selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle
{
    RefPtr<Range> range = _private->coreFrame->selection().toNormalizedRange();

    DOMDocumentFragment* fragment = range ? kit(createFragmentFromText(*range, text).ptr()) : nil;
    [self _replaceSelectionWithFragment:fragment selectReplacement:selectReplacement smartReplace:smartReplace matchStyle:matchStyle];
}

- (void)_replaceSelectionWithWebArchive:(WebArchive *)webArchive selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace
{
    NSArray* subresources = [webArchive subresources];
    for (WebResource* subresource in subresources) {
        if (![[self dataSource] subresourceForURL:[subresource URL]])
            [[self dataSource] addSubresource:subresource];
    }

    DOMDocumentFragment* fragment = [[self dataSource] _documentFragmentWithArchive:webArchive];
    [self _replaceSelectionWithFragment:fragment selectReplacement:selectReplacement smartReplace:smartReplace matchStyle:NO];
}

#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(TEXT_AUTOSIZING)
- (void)resetTextAutosizingBeforeLayout
{
    if (![self _webHTMLDocumentView])
        return;
    
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        Document *doc = frame->document();
        if (!doc || !doc->renderView())
            continue;
        doc->renderView()->resetTextAutosizing();
    }
}

- (void)_setVisibleSize:(CGSize)size
{
    [self _setTextAutosizingWidth:size.width];
}

- (void)_setTextAutosizingWidth:(CGFloat)width
{
    WebCore::Frame* frame = core(self);
    Page* page = frame->page();
    if (!page)
        return;

    page->setTextAutosizingWidth(width);
}
#else
- (void)resetTextAutosizingBeforeLayout
{
}

- (void)_setVisibleSize:(CGSize)size
{
}

- (void)_setTextAutosizingWidth:(CGFloat)width
{
}
#endif // ENABLE(TEXT_AUTOSIZING)

- (void)_replaceSelectionWithFragment:(DOMDocumentFragment *)fragment selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle
{
    if (_private->coreFrame->selection().isNone() || !fragment)
        return;
    _private->coreFrame->editor().replaceSelectionWithFragment(*core(fragment), selectReplacement ? Editor::SelectReplacement::Yes : Editor::SelectReplacement::No, smartReplace ? Editor::SmartReplace::Yes : Editor::SmartReplace::No, matchStyle ? Editor::MatchStyle::Yes : Editor::MatchStyle::No);
}

#if PLATFORM(IOS_FAMILY)
- (void)removeUnchangeableStyles
{
    _private->coreFrame->editor().removeUnchangeableStyles();
}

- (BOOL)hasRichlyEditableSelection
{
    return _private->coreFrame->selection().selection().isContentRichlyEditable();
}
#endif

- (void)_replaceSelectionWithText:(NSString *)text selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace
{
    RefPtr<Range> range = _private->coreFrame->selection().toNormalizedRange();
    
    DOMDocumentFragment* fragment = range ? kit(createFragmentFromText(*range, text).ptr()) : nil;
    [self _replaceSelectionWithFragment:fragment selectReplacement:selectReplacement smartReplace:smartReplace matchStyle:YES];
}

- (void)_replaceSelectionWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace
{
    DOMDocumentFragment *fragment = [self _documentFragmentWithMarkupString:markupString baseURLString:baseURLString];
    [self _replaceSelectionWithFragment:fragment selectReplacement:selectReplacement smartReplace:smartReplace matchStyle:NO];
}

#if !PLATFORM(IOS_FAMILY)
// Determines whether whitespace needs to be added around aString to preserve proper spacing and
// punctuation when it's inserted into the receiver's text over charRange. Returns by reference
// in beforeString and afterString any whitespace that should be added, unless either or both are
// nil. Both are returned as nil if aString is nil or if smart insertion and deletion are disabled.
- (void)_smartInsertForString:(NSString *)pasteString replacingRange:(DOMRange *)rangeToReplace beforeString:(NSString **)beforeString afterString:(NSString **)afterString
{
    // give back nil pointers in case of early returns
    if (beforeString)
        *beforeString = nil;
    if (afterString)
        *afterString = nil;
        
    // inspect destination
    Node *startContainer = core([rangeToReplace startContainer]);
    Node *endContainer = core([rangeToReplace endContainer]);

    Position startPos(startContainer, [rangeToReplace startOffset], Position::PositionIsOffsetInAnchor);
    Position endPos(endContainer, [rangeToReplace endOffset], Position::PositionIsOffsetInAnchor);

    VisiblePosition startVisiblePos = VisiblePosition(startPos, VP_DEFAULT_AFFINITY);
    VisiblePosition endVisiblePos = VisiblePosition(endPos, VP_DEFAULT_AFFINITY);
    
    // this check also ensures startContainer, startPos, endContainer, and endPos are non-null
    if (startVisiblePos.isNull() || endVisiblePos.isNull())
        return;

    bool addLeadingSpace = startPos.leadingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull() && !isStartOfParagraph(startVisiblePos);
    if (addLeadingSpace)
        if (UChar previousChar = startVisiblePos.previous().characterAfter())
            addLeadingSpace = !isCharacterSmartReplaceExempt(previousChar, true);
    
    bool addTrailingSpace = endPos.trailingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull() && !isEndOfParagraph(endVisiblePos);
    if (addTrailingSpace)
        if (UChar thisChar = endVisiblePos.characterAfter())
            addTrailingSpace = !isCharacterSmartReplaceExempt(thisChar, false);
    
    // inspect source
    bool hasWhitespaceAtStart = false;
    bool hasWhitespaceAtEnd = false;
    unsigned pasteLength = [pasteString length];
    if (pasteLength > 0) {
        NSCharacterSet *whiteSet = [NSCharacterSet whitespaceAndNewlineCharacterSet];
        
        if ([whiteSet characterIsMember:[pasteString characterAtIndex:0]]) {
            hasWhitespaceAtStart = YES;
        }
        if ([whiteSet characterIsMember:[pasteString characterAtIndex:(pasteLength - 1)]]) {
            hasWhitespaceAtEnd = YES;
        }
    }
    
    // issue the verdict
    if (beforeString && addLeadingSpace && !hasWhitespaceAtStart)
        *beforeString = @" ";
    if (afterString && addTrailingSpace && !hasWhitespaceAtEnd)
        *afterString = @" ";
}
#endif // !PLATFORM(IOS_FAMILY)

- (NSMutableDictionary *)_cacheabilityDictionary
{
    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    
    FrameLoader& frameLoader = _private->coreFrame->loader();
    DocumentLoader* documentLoader = frameLoader.documentLoader();
    if (documentLoader && !documentLoader->mainDocumentError().isNull())
        [result setObject:(NSError *)documentLoader->mainDocumentError() forKey:WebFrameMainDocumentError];
        
    if (frameLoader.subframeLoader().containsPlugins())
        [result setObject:[NSNumber numberWithBool:YES] forKey:WebFrameHasPlugins];
    
    if (DOMWindow* domWindow = _private->coreFrame->document()->domWindow()) {
        if (domWindow->hasEventListeners(eventNames().unloadEvent))
            [result setObject:[NSNumber numberWithBool:YES] forKey:WebFrameHasUnloadListener];
        if (domWindow->optionalApplicationCache())
            [result setObject:[NSNumber numberWithBool:YES] forKey:WebFrameUsesApplicationCache];
    }
    
    if (Document* document = _private->coreFrame->document()) {
        if (DatabaseManager::singleton().hasOpenDatabases(*document))
            [result setObject:[NSNumber numberWithBool:YES] forKey:WebFrameUsesDatabases];
        if (!document->canSuspendActiveDOMObjectsForDocumentSuspension())
            [result setObject:[NSNumber numberWithBool:YES] forKey:WebFrameCanSuspendActiveDOMObjects];
    }
    
    return result;
}

- (BOOL)_allowsFollowingLink:(NSURL *)URL
{
    if (!_private->coreFrame)
        return YES;
    return _private->coreFrame->document()->securityOrigin().canDisplay(URL);
}

- (NSString *)_stringByEvaluatingJavaScriptFromString:(NSString *)string withGlobalObject:(JSObjectRef)globalObjectRef inScriptWorld:(WebScriptWorld *)world
{
    if (!string)
        return @"";

    if (!world)
        return @"";

    // Start off with some guess at a frame and a global object, we'll try to do better...!
    JSDOMWindow* anyWorldGlobalObject = _private->coreFrame->script().globalObject(mainThreadNormalWorld());

    // The global object is probably a proxy object? - if so, we know how to use this!
    JSC::JSObject* globalObjectObj = toJS(globalObjectRef);
    JSC::VM& vm = *globalObjectObj->vm();
    if (!strcmp(globalObjectObj->classInfo(vm)->className, "JSWindowProxy"))
        anyWorldGlobalObject = JSC::jsDynamicCast<JSDOMWindow*>(vm, static_cast<JSWindowProxy*>(globalObjectObj)->window());

    if (!anyWorldGlobalObject)
        return @"";

    // Get the frame frome the global object we've settled on.
    Frame* frame = anyWorldGlobalObject->wrapped().frame();
    ASSERT(frame->document());
    RetainPtr<WebFrame> webFrame(kit(frame)); // Running arbitrary JavaScript can destroy the frame.

    JSC::JSValue result = frame->script().executeScriptInWorld(*core(world), string, true);

    if (!webFrame->_private->coreFrame) // In case the script removed our frame from the page.
        return @"";

    // This bizarre set of rules matches behavior from WebKit for Safari 2.0.
    // If you don't like it, use -[WebScriptObject evaluateWebScript:] or 
    // JSEvaluateScript instead, since they have less surprising semantics.
    if (!result || (!result.isBoolean() && !result.isString() && !result.isNumber()))
        return @"";

    JSC::ExecState* exec = anyWorldGlobalObject->globalExec();
    JSC::JSLockHolder lock(exec);
    return result.toWTFString(exec);
}

- (JSGlobalContextRef)_globalContextForScriptWorld:(WebScriptWorld *)world
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return 0;
    DOMWrapperWorld* coreWorld = core(world);
    if (!coreWorld)
        return 0;
    return toGlobalRef(coreFrame->script().globalObject(*coreWorld)->globalExec());
}

#if JSC_OBJC_API_ENABLED
- (JSContext *)_javaScriptContextForScriptWorld:(WebScriptWorld *)world
{
    JSGlobalContextRef globalContextRef = [self _globalContextForScriptWorld:world];
    if (!globalContextRef)
        return 0;
    return [JSContext contextWithJSGlobalContextRef:globalContextRef];
}
#endif

#if !PLATFORM(IOS_FAMILY)
- (void)setAllowsScrollersToOverlapContent:(BOOL)flag
{
    ASSERT([[[self frameView] _scrollView] isKindOfClass:[WebDynamicScrollBarsView class]]);
    [(WebDynamicScrollBarsView *)[[self frameView] _scrollView] setAllowsScrollersToOverlapContent:flag];
}

- (void)setAlwaysHideHorizontalScroller:(BOOL)flag
{
    ASSERT([[[self frameView] _scrollView] isKindOfClass:[WebDynamicScrollBarsView class]]);
    [(WebDynamicScrollBarsView *)[[self frameView] _scrollView] setAlwaysHideHorizontalScroller:flag];
}
- (void)setAlwaysHideVerticalScroller:(BOOL)flag
{
    ASSERT([[[self frameView] _scrollView] isKindOfClass:[WebDynamicScrollBarsView class]]);
    [(WebDynamicScrollBarsView *)[[self frameView] _scrollView] setAlwaysHideVerticalScroller:flag];
}
#endif

- (void)setAccessibleName:(NSString *)name
{
#if HAVE(ACCESSIBILITY)
    if (!AXObjectCache::accessibilityEnabled())
        return;
    
    if (!_private->coreFrame || !_private->coreFrame->document())
        return;
    
    AccessibilityObject* rootObject = _private->coreFrame->document()->axObjectCache()->rootObject();
    if (rootObject) {
        String strName(name);
        rootObject->setAccessibleName(strName);
    }
#endif
}

- (BOOL)enhancedAccessibilityEnabled
{
#if HAVE(ACCESSIBILITY)
    return AXObjectCache::accessibilityEnhancedUserInterfaceEnabled();
#else
    return NO;
#endif
}

- (void)setEnhancedAccessibility:(BOOL)enable
{
#if HAVE(ACCESSIBILITY)
    AXObjectCache::setEnhancedUserInterfaceAccessibility(enable);
#endif
}

- (NSString*)_layerTreeAsText
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return @"";

    return coreFrame->layerTreeAsText();
}

- (id)accessibilityRoot
{
#if HAVE(ACCESSIBILITY)
    if (!AXObjectCache::accessibilityEnabled()) {
        AXObjectCache::enableAccessibility();
#if !PLATFORM(IOS_FAMILY)
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        AXObjectCache::setEnhancedUserInterfaceAccessibility([[NSApp accessibilityAttributeValue:NSAccessibilityEnhancedUserInterfaceAttribute] boolValue]);
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    }
    
    if (!_private->coreFrame)
        return nil;
    
    Document* document = _private->coreFrame->document();
    if (!document || !document->axObjectCache())
        return nil;
    
    AccessibilityObject* rootObject = document->axObjectCache()->rootObjectForFrame(_private->coreFrame);
    if (!rootObject)
        return nil;
    
    // The root object will be a WebCore scroll view object. In WK1, scroll views are handled
    // by the system and the root object should be the web area (instead of the scroll view).
    if (rootObject->isAttachment() && rootObject->firstChild())
        return rootObject->firstChild()->wrapper();
    
    return rootObject->wrapper();
#else
    return nil;
#endif
}

- (void)_clearOpener
{
    Frame* coreFrame = _private->coreFrame;
    if (coreFrame)
        coreFrame->loader().setOpener(0);
}

- (BOOL)hasRichlyEditableDragCaret
{
    if (auto* page = core(self)->page())
        return page->dragCaretController().isContentRichlyEditable();
    return NO;
}

// Used by pagination code called from AppKit when a standalone web page is printed.
- (NSArray *)_computePageRectsWithPrintScaleFactor:(float)printScaleFactor pageSize:(NSSize)pageSize
{
    if (printScaleFactor <= 0) {
        LOG_ERROR("printScaleFactor has bad value %.2f", printScaleFactor);
        return [NSArray array];
    }

    if (!_private->coreFrame)
        return [NSArray array];
    if (!_private->coreFrame->document())
        return [NSArray array];
    if (!_private->coreFrame->view())
        return [NSArray array];
    if (!_private->coreFrame->view()->documentView())
        return [NSArray array];

    RenderView* root = _private->coreFrame->document()->renderView();
    if (!root)
        return [NSArray array];

    const LayoutRect& documentRect = root->documentRect();
    float printWidth = root->style().isHorizontalWritingMode() ? static_cast<float>(documentRect.width()) / printScaleFactor : pageSize.width;
    float printHeight = root->style().isHorizontalWritingMode() ? pageSize.height : static_cast<float>(documentRect.height()) / printScaleFactor;

    PrintContext printContext(_private->coreFrame);
    printContext.computePageRectsWithPageSize(FloatSize(printWidth, printHeight), true);
    const Vector<IntRect>& pageRects = printContext.pageRects();

    size_t size = pageRects.size();
    NSMutableArray *pages = [NSMutableArray arrayWithCapacity:size];
    for (size_t i = 0; i < size; ++i)
        [pages addObject:[NSValue valueWithRect:NSRect(pageRects[i])]];
    return pages;
}

#if PLATFORM(IOS_FAMILY)

- (DOMDocumentFragment *)_documentFragmentForText:(NSString *)text
{
    return kit(createFragmentFromText(*_private->coreFrame->selection().toNormalizedRange().get(), text).ptr());
}

- (DOMDocumentFragment *)_documentFragmentForWebArchive:(WebArchive *)webArchive
{
    return [[self dataSource] _documentFragmentWithArchive:webArchive];
}

- (DOMDocumentFragment *)_documentFragmentForImageData:(NSData *)data withRelativeURLPart:(NSString *)relativeURLPart andMIMEType:(NSString *)mimeType
{
    WebResource *resource = [[WebResource alloc] initWithData:data
                                                          URL:URL::fakeURLWithRelativePart(relativeURLPart)
                                                     MIMEType:mimeType
                                             textEncodingName:nil
                                                    frameName:nil];
    DOMDocumentFragment *fragment = [[self _dataSource] _documentFragmentWithImageResource:resource];
    [resource release];
    return fragment;
}

- (BOOL)focusedNodeHasContent
{
    Frame* coreFrame = _private->coreFrame;
   
    Element* root;
    const VisibleSelection& selection = coreFrame->selection().selection();
    if (selection.isNone() || !selection.isContentEditable())
        root = coreFrame->document()->bodyOrFrameset();
    else {
        // Can't use the focusedNode here because we want the root of the shadow tree for form elements.
        root = selection.rootEditableElement();
    }
    // Early return to avoid the expense of creating VisiblePositions.
    // FIXME: We fail to compute a root for SVG, we have a null check here so that we don't crash.
    if (!root || !root->hasChildNodes())
        return NO;

    VisiblePosition first(createLegacyEditingPosition(root, 0));
    VisiblePosition last(createLegacyEditingPosition(root, root->countChildNodes()));
    return first != last;
}

- (void)_dispatchDidReceiveTitle:(NSString *)title
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return;
    coreFrame->loader().client().dispatchDidReceiveTitle({ title, TextDirection::LTR });
}

#endif // PLATFORM(IOS_FAMILY)

- (JSValueRef)jsWrapperForNode:(DOMNode *)node inScriptWorld:(WebScriptWorld *)world
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return 0;

    if (!world)
        return 0;

    JSDOMWindow* globalObject = coreFrame->script().globalObject(*core(world));
    JSC::ExecState* exec = globalObject->globalExec();

    JSC::JSLockHolder lock(exec);
    return toRef(exec, toJS(exec, globalObject, core(node)));
}

- (NSDictionary *)elementAtPoint:(NSPoint)point
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return nil;
    return [[[WebElementDictionary alloc] initWithHitTestResult:coreFrame->eventHandler().hitTestResultAtPoint(IntPoint(point), HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::IgnoreClipping | HitTestRequest::DisallowUserAgentShadowContent)] autorelease];
}

- (NSURL *)_unreachableURL
{
    return [[self _dataSource] unreachableURL];
}

@end

@implementation WebFrame

- (instancetype)init
{
    return nil;
}

// Should be deprecated.
- (instancetype)initWithName:(NSString *)name webFrameView:(WebFrameView *)view webView:(WebView *)webView
{
    return nil;
}

- (void)dealloc
{
    if (_private && _private->includedInWebKitStatistics)
        --WebFrameCount;

    [_private release];

    [super dealloc];
}

- (NSString *)name
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return nil;
    return coreFrame->tree().uniqueName();
}

- (WebFrameView *)frameView
{
    return _private->webFrameView;
}

- (WebView *)webView
{
    return getWebView(self);
}

static bool needsMicrosoftMessengerDOMDocumentWorkaround()
{
#if PLATFORM(IOS_FAMILY)
    return false;
#else
    static bool needsWorkaround = MacApplication::isMicrosoftMessenger() && [[[NSBundle mainBundle] objectForInfoDictionaryKey:(NSString *)kCFBundleVersionKey] compare:@"7.1" options:NSNumericSearch] == NSOrderedAscending;
    return needsWorkaround;
#endif
}

- (DOMDocument *)DOMDocument
{
    if (needsMicrosoftMessengerDOMDocumentWorkaround() && !pthread_main_np())
        return nil;

    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return nil;
    
    // FIXME: <rdar://problem/5145841> When loading a custom view/representation 
    // into a web frame, the old document can still be around. This makes sure that
    // we'll return nil in those cases.
    if (![[self _dataSource] _isDocumentHTML]) 
        return nil; 

    Document* document = coreFrame->document();
    
    // According to the documentation, we should return nil if the frame doesn't have a document.
    // While full-frame images and plugins do have an underlying HTML document, we return nil here to be
    // backwards compatible.
    if (document && (document->isPluginDocument() || document->isImageDocument()))
        return nil;
    
    return kit(coreFrame->document());
}

- (DOMHTMLElement *)frameElement
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return nil;
    return kit(coreFrame->ownerElement());
}

- (WebDataSource *)provisionalDataSource
{
    Frame* coreFrame = _private->coreFrame;
    return coreFrame ? dataSource(coreFrame->loader().provisionalDocumentLoader()) : nil;
}

- (WebDataSource *)dataSource
{
    Frame* coreFrame = _private->coreFrame;
    return coreFrame && coreFrame->loader().frameHasLoaded() ? [self _dataSource] : nil;
}

- (void)loadRequest:(NSURLRequest *)request
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return;

    ResourceRequest resourceRequest(request);
    
    // Some users of WebKit API incorrectly use "file path as URL" style requests which are invalid.
    // By re-writing those URLs here we technically break the -[WebDataSource initialRequest] API
    // but that is necessary to implement this quirk only at the API boundary.
    // Note that other users of WebKit API use nil requests or requests with nil URLs or empty URLs, so we
    // only implement this workaround when the request had a non-nil or non-empty URL.
    if (!resourceRequest.url().isValid() && !resourceRequest.url().isEmpty())
        resourceRequest.setURL([NSURL URLWithString:[@"file:" stringByAppendingString:[[request URL] absoluteString]]]);

    coreFrame->loader().load(FrameLoadRequest(*coreFrame, resourceRequest, ShouldOpenExternalURLsPolicy::ShouldNotAllow));
}

static NSURL *createUniqueWebDataURL()
{
    CFUUIDRef UUIDRef = CFUUIDCreate(kCFAllocatorDefault);
    NSString *UUIDString = (NSString *)CFUUIDCreateString(kCFAllocatorDefault, UUIDRef);
    CFRelease(UUIDRef);
    NSURL *URL = [NSURL URLWithString:[NSString stringWithFormat:@"applewebdata://%@", UUIDString]];
    CFRelease(UUIDString);
    return URL;
}

- (void)_loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)baseURL unreachableURL:(NSURL *)unreachableURL
{
#if PLATFORM(MAC)
    if (!pthread_main_np())
        return [[self _webkit_invokeOnMainThread] _loadData:data MIMEType:MIMEType textEncodingName:encodingName baseURL:baseURL unreachableURL:unreachableURL];
#endif

    NSURL *responseURL = nil;
    if (baseURL)
        baseURL = [baseURL absoluteURL];
    else {
        baseURL = blankURL();
        responseURL = createUniqueWebDataURL();
    }
    
#if USE(QUICK_LOOK)
    if (shouldUseQuickLookForMIMEType(MIMEType)) {
        NSURL *quickLookURL = responseURL ? responseURL : baseURL;
        if (auto request = registerQLPreviewConverterIfNeeded(quickLookURL, MIMEType, data)) {
            _private->coreFrame->loader().load(FrameLoadRequest(*_private->coreFrame, request.get(), ShouldOpenExternalURLsPolicy::ShouldNotAllow));
            return;
        }
    }
#endif

    ResourceRequest request(baseURL);

    ResourceResponse response(responseURL, MIMEType, [data length], encodingName);
    SubstituteData substituteData(WebCore::SharedBuffer::create(data), [unreachableURL absoluteURL], response, SubstituteData::SessionHistoryVisibility::Hidden);

    _private->coreFrame->loader().load(FrameLoadRequest(*_private->coreFrame, request, ShouldOpenExternalURLsPolicy::ShouldNotAllow, substituteData));
}

- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)baseURL
{
    WebCoreThreadViolationCheckRoundTwo();
    
    if (!MIMEType)
        MIMEType = @"text/html";
    [self _loadData:data MIMEType:MIMEType textEncodingName:encodingName baseURL:[baseURL _webkit_URLFromURLOrSchemelessFileURL] unreachableURL:nil];
}

- (void)_loadHTMLString:(NSString *)string baseURL:(NSURL *)baseURL unreachableURL:(NSURL *)unreachableURL
{
    NSData *data = [string dataUsingEncoding:NSUTF8StringEncoding];
    [self _loadData:data MIMEType:@"text/html" textEncodingName:@"UTF-8" baseURL:baseURL unreachableURL:unreachableURL];
}

- (void)loadHTMLString:(NSString *)string baseURL:(NSURL *)baseURL
{
    WebCoreThreadViolationCheckRoundTwo();

    [self _loadHTMLString:string baseURL:[baseURL _webkit_URLFromURLOrSchemelessFileURL] unreachableURL:nil];
}

- (void)loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL
{
    WebCoreThreadViolationCheckRoundTwo();

    [self _loadHTMLString:string baseURL:[baseURL _webkit_URLFromURLOrSchemelessFileURL] unreachableURL:[unreachableURL _webkit_URLFromURLOrSchemelessFileURL]];
}

- (void)loadArchive:(WebArchive *)archive
{
    if (auto* coreArchive = [archive _coreLegacyWebArchive])
        _private->coreFrame->loader().loadArchive(*coreArchive);
}

- (void)stopLoading
{
    if (!_private->coreFrame)
        return;
    _private->coreFrame->loader().stopForUserCancel();
}

- (void)reload
{
    _private->coreFrame->loader().reload({ });
}

- (void)reloadFromOrigin
{
    _private->coreFrame->loader().reload(WebCore::ReloadOption::FromOrigin);
}

- (WebFrame *)findFrameNamed:(NSString *)name
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return nil;
    return kit(coreFrame->tree().find(name, *coreFrame));
}

- (WebFrame *)parentFrame
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return nil;
    return [[kit(coreFrame->tree().parent()) retain] autorelease];
}

- (NSArray *)childFrames
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return [NSArray array];
    NSMutableArray *children = [NSMutableArray arrayWithCapacity:coreFrame->tree().childCount()];
    for (Frame* child = coreFrame->tree().firstChild(); child; child = child->tree().nextSibling())
        [children addObject:kit(child)];
    return children;
}

- (WebScriptObject *)windowObject
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return 0;
    return coreFrame->script().windowScriptObject();
}

- (JSGlobalContextRef)globalContext
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return 0;
    return toGlobalRef(coreFrame->script().globalObject(mainThreadNormalWorld())->globalExec());
}

#if JSC_OBJC_API_ENABLED
- (JSContext *)javaScriptContext
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return 0;
    return coreFrame->script().javaScriptContext();
}
#endif

@end
