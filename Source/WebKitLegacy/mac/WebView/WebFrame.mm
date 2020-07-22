/*
 * Copyright (C) 2005-2020 Apple Inc. All rights reserved.
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
#import "DOMPrivate.h"
#import "DOMRangeInternal.h"
#import "WebArchiveInternal.h"
#import "WebChromeClient.h"
#import "WebDataSourceInternal.h"
#import "WebDocumentLoaderMac.h"
#import "WebDynamicScrollBarsView.h"
#import "WebEditorClient.h"
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
#import <JavaScriptCore/JSGlobalObjectInlines.h>
#import <JavaScriptCore/JSLock.h>
#import <JavaScriptCore/JSObject.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/AccessibilityObject.h>
#import <WebCore/CSSStyleDeclaration.h>
#import <WebCore/CachedResourceLoader.h>
#import <WebCore/Chrome.h>
#import <WebCore/ColorMac.h>
#import <WebCore/CompositionHighlight.h>
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
#import <WebCore/FrameSelection.h>
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
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <wtf/cocoa/VectorCocoa.h>

#if PLATFORM(IOS_FAMILY)
#import "WebMailDelegate.h"
#import "WebResource.h"
#import "WebUIKitDelegate.h"
#import <WebCore/Document.h>
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

WebCore::EditableLinkBehavior core(WebKitEditableLinkBehavior editableLinkBehavior)
{
    using namespace WebCore;
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

WebCore::TextDirectionSubmenuInclusionBehavior core(WebTextDirectionSubmenuInclusionBehavior behavior)
{
    using namespace WebCore;
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

WebCore::Frame* core(WebFrame *frame)
{
    return frame ? frame->_private->coreFrame : 0;
}

WebFrame *kit(WebCore::Frame* frame)
{
    if (!frame)
        return nil;

    WebCore::FrameLoaderClient& frameLoaderClient = frame->loader().client();
    if (frameLoaderClient.isEmptyFrameLoaderClient())
        return nil;

    return static_cast<WebFrameLoaderClient&>(frameLoaderClient).webFrame();
}

WebCore::Page* core(WebView *webView)
{
    return [webView page];
}

WebView *kit(WebCore::Page* page)
{
    if (!page)
        return nil;

    if (page->chrome().client().isEmptyChromeClient())
        return nil;

    return static_cast<WebChromeClient&>(page->chrome().client()).webView();
}

WebView *getWebView(WebFrame *webFrame)
{
    auto coreFrame = core(webFrame);
    if (!coreFrame)
        return nil;
    return kit(coreFrame->page());
}

+ (Ref<WebCore::Frame>)_createFrameWithPage:(WebCore::Page*)page frameName:(const String&)name frameView:(WebFrameView *)frameView ownerElement:(WebCore::HTMLFrameOwnerElement*)ownerElement
{
    WebView *webView = kit(page);

    WebFrame *frame = [[self alloc] _initWithWebFrameView:frameView webView:webView];
    auto coreFrame = WebCore::Frame::create(page, ownerElement, makeUniqueRef<WebFrameLoaderClient>(frame));
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

+ (void)_createMainFrameWithPage:(WebCore::Page*)page frameName:(const String&)name frameView:(WebFrameView *)frameView
{
    WebView *webView = kit(page);

    WebFrame *frame = [[self alloc] _initWithWebFrameView:frameView webView:webView];
    frame->_private->coreFrame = &page->mainFrame();
    static_cast<WebFrameLoaderClient&>(page->mainFrame().loader().client()).setWebFrame(*frame);
    [frame release];

    page->mainFrame().tree().setName(name);
    page->mainFrame().init();

    [webView _setZoomMultiplier:[webView _realZoomMultiplier] isTextOnly:[webView _realZoomMultiplierIsTextOnly]];
}

+ (Ref<WebCore::Frame>)_createSubframeWithOwnerElement:(WebCore::HTMLFrameOwnerElement*)ownerElement frameName:(const String&)name frameView:(WebFrameView *)frameView
{
    return [self _createFrameWithPage:ownerElement->document().frame()->page() frameName:name frameView:frameView ownerElement:ownerElement];
}

- (BOOL)_isIncludedInWebKitStatistics
{
    return _private && _private->includedInWebKitStatistics;
}

#if PLATFORM(IOS_FAMILY)
static NSURL *createUniqueWebDataURL();

+ (void)_createMainFrameWithSimpleHTMLDocumentWithPage:(WebCore::Page*)page frameView:(WebFrameView *)frameView style:(NSString *)style
{
    WebView *webView = kit(page);
    
    WebFrame *frame = [[self alloc] _initWithWebFrameView:frameView webView:webView];
    frame->_private->coreFrame = &page->mainFrame();
    static_cast<WebFrameLoaderClient&>(page->mainFrame().loader().client()).setWebFrame(*frame);
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
    if (!windowProxy.existingJSWindowProxy(WebCore::debuggerWorld()))
        return;

    auto* globalObject = windowProxy.globalObject(WebCore::debuggerWorld());
    if (!globalObject)
        return;

    if (_private->scriptDebugger) {
        ASSERT(_private->scriptDebugger.get() == globalObject->debugger());
        return;
    }

    _private->scriptDebugger = makeUnique<WebScriptDebugger>(globalObject);
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

    auto coreFrame = _private->coreFrame;
    for (auto frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        // Don't call setDrawsBackground:YES here because it may be NO because of a load
        // in progress; WebFrameLoaderClient keeps it set to NO during the load process.
        WebFrame *webFrame = kit(frame);
        if (!drawsBackground)
            [[[webFrame frameView] _scrollView] setDrawsBackground:NO];
#if !PLATFORM(IOS_FAMILY)
        [[[webFrame frameView] _scrollView] setBackgroundColor:backgroundColor];
#endif

        if (auto* view = frame->view()) {
            view->setTransparent(!drawsBackground);
#if !PLATFORM(IOS_FAMILY)
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            WebCore::Color color = WebCore::colorFromNSColor([backgroundColor colorUsingColorSpaceName:NSDeviceRGBColorSpace]);
            ALLOW_DEPRECATED_DECLARATIONS_END
#else
            WebCore::Color color = WebCore::Color(backgroundColor);
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
    auto coreFrame = _private->coreFrame;
    for (auto frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        if (auto* document = frame->document())
            document->markers().removeMarkers(WebCore::DocumentMarker::Grammar);
    }
}

- (void)_unmarkAllMisspellings
{
#if !PLATFORM(IOS_FAMILY)
    auto coreFrame = _private->coreFrame;
    for (auto frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        if (auto* document = frame->document())
            document->markers().removeMarkers(WebCore::DocumentMarker::Spelling);
    }
#endif
}

- (BOOL)_hasSelection
{
    id documentView = [_private->webFrameView documentView];    

    // optimization for common case to avoid creating potentially large selection string
    if ([documentView isKindOfClass:[WebHTMLView class]])
        if (auto coreFrame = _private->coreFrame)
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

#if ASSERT_ENABLED
- (BOOL)_atMostOneFrameHasSelection
{
    // FIXME: 4186050 is one known case that makes this debug check fail.
    BOOL found = NO;
    auto coreFrame = _private->coreFrame;
    for (auto frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        if ([kit(frame) _hasSelection]) {
            if (found)
                return NO;
            found = YES;
        }
    }
    return YES;
}
#endif // ASSERT_ENABLED

- (WebFrame *)_findFrameWithSelection
{
    auto coreFrame = _private->coreFrame;
    for (auto frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
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

- (NSString *)_selectedString
{
    return _private->coreFrame->displayStringModifiedByEncoding(_private->coreFrame->editor().selectedText());
}

- (NSString *)_stringForRange:(DOMRange *)range
{
    if (!range)
        return @"";
    return plainText(*core(range), WebCore::TextIteratorDefaultBehavior, true);
}

- (OptionSet<WebCore::PaintBehavior>)_paintBehaviorForDestinationContext:(CGContextRef)context
{
#if PLATFORM(MAC)
    // -currentContextDrawingToScreen returns YES for bitmap contexts.
    BOOL isPrinting = ![NSGraphicsContext currentContextDrawingToScreen];
    if (isPrinting)
        return OptionSet<WebCore::PaintBehavior>(WebCore::PaintBehavior::FlattenCompositingLayers) | WebCore::PaintBehavior::Snapshotting;
#endif

    if (CGContextGetType(context) != kCGContextTypeBitmap)
        return WebCore::PaintBehavior::Normal;

    // If we're drawing into a bitmap, we could be snapshotting or drawing into a layer-backed view.
    if (WebHTMLView *documentView = [self _webHTMLDocumentView]) {
#if PLATFORM(IOS_FAMILY)
        return [[documentView window] isInSnapshottingPaint] ? WebCore::PaintBehavior::Snapshotting : WebCore::PaintBehavior::Normal;
#endif
#if PLATFORM(MAC)
        if ([documentView _web_isDrawingIntoLayer])
            return WebCore::PaintBehavior::Normal;
#endif
    }
    
    return OptionSet<WebCore::PaintBehavior>(WebCore::PaintBehavior::FlattenCompositingLayers) | WebCore::PaintBehavior::Snapshotting;
}

- (void)_drawRect:(NSRect)rect contentsOnly:(BOOL)contentsOnly
{
#if !PLATFORM(IOS_FAMILY)
    ASSERT([[NSGraphicsContext currentContext] isFlipped]);

    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
#else
    CGContextRef ctx = WKGetCurrentGraphicsContext();
#endif
    WebCore::GraphicsContext context(ctx);

#if PLATFORM(IOS_FAMILY)
    WebCore::Frame *frame = core(self);
    if (WebCore::Page* page = frame->page())
        context.setIsAcceleratedContext(page->settings().acceleratedDrawingEnabled());
#elif PLATFORM(MAC)
    if (WebHTMLView *htmlDocumentView = [self _webHTMLDocumentView])
        context.setIsAcceleratedContext([htmlDocumentView _web_isDrawingIntoAcceleratedLayer]);
#endif

    auto* view = _private->coreFrame->view();
    
    OptionSet<WebCore::PaintBehavior> oldBehavior = view->paintBehavior();
    OptionSet<WebCore::PaintBehavior> paintBehavior = oldBehavior;
    
    if (auto* parentFrame = _private->coreFrame->tree().parent()) {
        // For subframes, we need to inherit the paint behavior from our parent
        if (auto* parentView = parentFrame ? parentFrame->view() : nullptr) {
            if (parentView->paintBehavior().contains(WebCore::PaintBehavior::FlattenCompositingLayers))
                paintBehavior.add(WebCore::PaintBehavior::FlattenCompositingLayers);
            
            if (parentView->paintBehavior().contains(WebCore::PaintBehavior::Snapshotting))
                paintBehavior.add(WebCore::PaintBehavior::Snapshotting);
            
            if (parentView->paintBehavior().contains(WebCore::PaintBehavior::TileFirstPaint))
                paintBehavior.add(WebCore::PaintBehavior::TileFirstPaint);
        }
    } else
        paintBehavior.add([self _paintBehaviorForDestinationContext:ctx]);
        
    view->setPaintBehavior(paintBehavior);

    if (contentsOnly)
        view->paintContents(context, WebCore::enclosingIntRect(rect));
    else
        view->paint(context, WebCore::enclosingIntRect(rect));

    view->setPaintBehavior(oldBehavior);
}

- (BOOL)_getVisibleRect:(NSRect*)rect
{
    ASSERT_ARG(rect, rect);
    if (auto* ownerRenderer = _private->coreFrame->ownerRenderer()) {
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
    JSC::JSGlobalObject* lexicalGlobalObject = _private->coreFrame->script().globalObject(WebCore::mainThreadNormalWorld());
    JSC::JSLockHolder jscLock(lexicalGlobalObject);
#endif

    JSC::JSValue result = _private->coreFrame->script().executeScriptIgnoringException(string, forceUserGesture);

    if (!_private->coreFrame) // In case the script removed our frame from the page.
        return @"";

    // This bizarre set of rules matches behavior from WebKit for Safari 2.0.
    // If you don't like it, use -[WebScriptObject evaluateWebScript:] or 
    // JSEvaluateScript instead, since they have less surprising semantics.
    if (!result || (!result.isBoolean() && !result.isString() && !result.isNumber()))
        return @"";

#if !PLATFORM(IOS_FAMILY)
    JSC::JSGlobalObject* lexicalGlobalObject = _private->coreFrame->script().globalObject(WebCore::mainThreadNormalWorld());
    JSC::JSLockHolder lock(lexicalGlobalObject);
#endif
    return result.toWTFString(lexicalGlobalObject);
}

- (NSRect)_caretRectAtPosition:(const WebCore::Position&)pos affinity:(NSSelectionAffinity)affinity
{
    WebCore::VisiblePosition visiblePosition(pos, static_cast<WebCore::EAffinity>(affinity));
    return visiblePosition.absoluteCaretBounds();
}

- (NSRect)_firstRectForDOMRange:(DOMRange *)range
{
    if (!range)
        return NSZeroRect;
    return _private->coreFrame->editor().firstRectForRange(*makeSimpleRange(core(range)));
}

- (void)_scrollDOMRangeToVisible:(DOMRange *)range
{
    bool insideFixed = false; // FIXME: get via firstRectForRange().
    NSRect rangeRect = [self _firstRectForDOMRange:range];
    auto* startNode = core([range startContainer]);
        
    if (startNode && startNode->renderer()) {
#if !PLATFORM(IOS_FAMILY)
        startNode->renderer()->scrollRectToVisible(WebCore::enclosingIntRect(rangeRect), insideFixed, { WebCore::SelectionRevealMode::Reveal, WebCore::ScrollAlignment::alignToEdgeIfNeeded, WebCore::ScrollAlignment::alignToEdgeIfNeeded, WebCore::ShouldAllowCrossOriginScrolling::Yes });
#else
        auto* layer = startNode->renderer()->enclosingLayer();
        if (layer) {
            layer->setAdjustForIOSCaretWhenScrolling(true);
            startNode->renderer()->scrollRectToVisible(WebCore::enclosingIntRect(rangeRect), insideFixed, { WebCore::SelectionRevealMode::Reveal, WebCore::ScrollAlignment::alignToEdgeIfNeeded, WebCore::ScrollAlignment::alignToEdgeIfNeeded, WebCore::ShouldAllowCrossOriginScrolling::Yes });
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
    auto* startNode = core([range startContainer]);

    if (startNode && startNode->renderer()) {
        auto* layer = startNode->renderer()->enclosingLayer();
        if (layer) {
            layer->setAdjustForIOSCaretWhenScrolling(true);
            startNode->renderer()->scrollRectToVisible(WebCore::enclosingIntRect(rangeRect), insideFixed, { WebCore::SelectionRevealMode::Reveal, WebCore::ScrollAlignment::alignToEdgeIfNeeded, WebCore::ScrollAlignment::alignToEdgeIfNeeded, WebCore::ShouldAllowCrossOriginScrolling::Yes});
            layer->setAdjustForIOSCaretWhenScrolling(false);

            auto coreFrame = core(self);
            if (coreFrame) {
                auto& frameSelection = coreFrame->selection();
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
- (DOMRange *)_rangeByAlteringCurrentSelection:(WebCore::FrameSelection::EAlteration)alteration direction:(WebCore::SelectionDirection)direction granularity:(WebCore::TextGranularity)granularity
{
    if (_private->coreFrame->selection().isNone())
        return nil;

    WebCore::FrameSelection selection;
    selection.setSelection(_private->coreFrame->selection().selection());
    selection.modify(alteration, direction, granularity);
    return kit(selection.selection().toNormalizedRange());
}
#endif

- (WebCore::TextGranularity)_selectionGranularity
{
    return _private->coreFrame->selection().granularity();
}

- (NSRange)_convertToNSRange:(const WebCore::SimpleRange&)range
{
    auto frame = _private->coreFrame;
    if (!frame)
        return NSMakeRange(NSNotFound, 0);

    auto* element = frame->selection().rootEditableElementOrDocumentElement();
    if (!element)
        return NSMakeRange(NSNotFound, 0);

    return characterRange(makeBoundaryPointBeforeNodeContents(*element), range);
}

- (RefPtr<WebCore::Range>)_convertToDOMRange:(NSRange)nsrange
{
    return [self _convertToDOMRange:nsrange rangeIsRelativeTo:WebRangeIsRelativeTo::EditableRoot];
}

- (RefPtr<WebCore::Range>)_convertToDOMRange:(NSRange)range rangeIsRelativeTo:(WebRangeIsRelativeTo)rangeIsRelativeTo
{
    if (range.location == NSNotFound)
        return nullptr;

    if (rangeIsRelativeTo == WebRangeIsRelativeTo::EditableRoot) {
        // Our critical assumption is that this code path is only called by input methods that
        // concentrate on a given area containing the selection
        // We have to do this because of text fields and textareas. The DOM for those is not
        // directly in the document DOM, so serialization is problematic. Our solution is
        // to use the root editable element of the selection start as the positional base.
        // That fits with AppKit's idea of an input context.
        auto* element = _private->coreFrame->selection().rootEditableElementOrDocumentElement();
        if (!element)
            return nullptr;
        return createLiveRange(resolveCharacterRange(makeRangeSelectingNodeContents(*element), range));
    }

    ASSERT(rangeIsRelativeTo == WebRangeIsRelativeTo::Paragraph);

    auto paragraphStart = makeBoundaryPoint(startOfParagraph(_private->coreFrame->selection().selection().visibleStart()));
    if (!paragraphStart)
        return nullptr;

    auto scopeEnd = makeRangeSelectingNodeContents(paragraphStart->container->treeScope().rootNode()).end;
    return createLiveRange(WebCore::resolveCharacterRange({ WTFMove(*paragraphStart), WTFMove(scopeEnd) }, range));
}

- (DOMRange *)_convertNSRangeToDOMRange:(NSRange)nsrange
{
    return kit([self _convertToDOMRange:nsrange].get());
}

- (NSRange)_convertDOMRangeToNSRange:(DOMRange *)range
{
    if (!range)
        return NSMakeRange(NSNotFound, 0);
    return [self _convertToNSRange:*core(range)];
}

- (DOMRange *)_markDOMRange
{
    return kit(_private->coreFrame->editor().mark().toNormalizedRange());
}

- (DOMDocumentFragment *)_documentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString
{
    auto frame = _private->coreFrame;
    if (!frame)
        return nil;

    auto* document = frame->document();
    if (!document)
        return nil;

    return kit(createFragmentFromMarkup(*document, markupString, baseURLString, WebCore::DisallowScriptingContent).ptr());
}

- (DOMDocumentFragment *)_documentFragmentWithNodesAsParagraphs:(NSArray *)nodes
{
    auto frame = _private->coreFrame;
    if (!frame)
        return nil;

    auto* document = frame->document();
    if (!document)
        return nil;

    NSEnumerator *nodeEnum = [nodes objectEnumerator];
    Vector<WebCore::Node*> nodesVector;
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

- (WebCore::VisiblePosition)_visiblePositionForPoint:(NSPoint)point
{
    // FIXME: Someone with access to Apple's sources could remove this needless wrapper call.
    return _private->coreFrame->visiblePositionForPoint(WebCore::IntPoint(point));
}

- (DOMRange *)_characterRangeAtPoint:(NSPoint)point
{
    return kit(_private->coreFrame->rangeForPoint(WebCore::IntPoint(point)).get());
}

- (DOMCSSStyleDeclaration *)_typingStyle
{
    if (!_private->coreFrame)
        return nil;
    RefPtr<WebCore::MutableStyleProperties> typingStyle = _private->coreFrame->selection().copyTypingStyle();
    if (!typingStyle)
        return nil;
    return kit(&typingStyle->ensureCSSStyleDeclaration());
}

- (void)_setTypingStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebCore::EditAction)undoAction
{
    if (!_private->coreFrame || !style)
        return;
    // FIXME: We shouldn't have to create a copy here.
    Ref<WebCore::MutableStyleProperties> properties(core(style)->copyProperties());
    _private->coreFrame->editor().computeAndSetTypingStyle(properties.get(), undoAction);
}

#if ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)
- (void)_dragSourceEndedAt:(NSPoint)windowLoc operation:(NSDragOperation)dragOperationMask
{
    if (!_private->coreFrame)
        return;
    auto* view = _private->coreFrame->view();
    if (!view)
        return;
    // FIXME: These are fake modifier keys here, but they should be real ones instead.
    WebCore::PlatformMouseEvent event(WebCore::IntPoint(windowLoc), WebCore::IntPoint(WebCore::globalPoint(windowLoc, [view->platformWidget() window])),
        WebCore::LeftButton, WebCore::PlatformEvent::MouseMoved, 0, false, false, false, false, WallTime::now(), WebCore::ForceAtClick, WebCore::NoTap);
    _private->coreFrame->eventHandler().dragSourceEndedAt(event, coreDragOperationMask(dragOperationMask));
}
#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)

- (BOOL)_canProvideDocumentSource
{
    auto frame = _private->coreFrame;
    String mimeType = frame->document()->loader()->writer().mimeType();
    auto* pluginData = frame->page() ? &frame->page()->pluginData() : 0;

    if (WebCore::MIMETypeRegistry::isTextMIMEType(mimeType)
        || WebCore::Image::supportsType(mimeType)
        || (pluginData && pluginData->supportsWebVisibleMimeType(mimeType, WebCore::PluginData::AllPlugins) && frame->loader().arePluginsEnabled())
        || (pluginData && pluginData->supportsWebVisibleMimeType(mimeType, WebCore::PluginData::OnlyApplicationPlugins)))
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
    auto* document = _private->coreFrame->document();
    document->setShouldCreateRenderers(_private->shouldCreateRenderers);

    _private->coreFrame->loader().documentLoader()->commitData((const char *)[data bytes], [data length]);
}

@end

@implementation WebFrame (WebPrivate)

// FIXME: This exists only as a convenience for Safari, consider moving there.
- (BOOL)_isDescendantOfFrame:(WebFrame *)ancestor
{
    auto coreFrame = _private->coreFrame;
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
    auto* document = _private->coreFrame->document();
    if (!document)
        return nil;
    auto* body = document->bodyOrFrameset();
    if (!body)
        return nil;
    auto* bodyRenderer = body->renderer();
    if (!bodyRenderer)
        return nil;
    WebCore::Color color = bodyRenderer->style().visitedDependentColorWithColorFilter(WebCore::CSSPropertyBackgroundColor);
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
    auto* document = _private->coreFrame->document();
    return document && document->isFrameSet();
}

- (BOOL)_firstLayoutDone
{
    return _private->coreFrame->loader().stateMachine().firstLayoutDone();
}

- (BOOL)_isVisuallyNonEmpty
{
    if (auto* view = _private->coreFrame->view())
        return view->isVisuallyNonEmpty();
    return NO;
}

static WebFrameLoadType toWebFrameLoadType(WebCore::FrameLoadType frameLoadType)
{
    using namespace WebCore;
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
    case FrameLoadType::ReloadExpiredOnly:
        // NOTE: reloading via remote inspection may trigger ReloadExpiredOnly, but otherwise
        // it is not a supported load type as it was added after WebKit1 became WebKitLegacy.
        return WebFrameLoadTypeReloadFromOrigin;
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

- (NSArray *)_rectsForRange:(DOMRange *)range
{
    return range ? range.textRects : @[];
}

- (DOMRange *)_selectionRangeForFirstPoint:(CGPoint)first secondPoint:(CGPoint)second
{
    auto firstPosition = [self _visiblePositionForPoint:first];
    auto secondPosition = [self _visiblePositionForPoint:second];
    return kit(WebCore::VisibleSelection(firstPosition, secondPosition).toNormalizedRange());
}

- (DOMRange *)_selectionRangeForPoint:(CGPoint)point
{
    return kit(WebCore::VisibleSelection([self _visiblePositionForPoint:point]).toNormalizedRange());
}

#endif // PLATFORM(IOS_FAMILY)

- (NSRange)_selectedNSRange
{
    auto range = _private->coreFrame->selection().selection().toNormalizedRange();
    if (!range)
        return NSMakeRange(NSNotFound, 0);
    return [self _convertToNSRange:*range];
}

- (void)_selectNSRange:(NSRange)range
{
    RefPtr<WebCore::Range> domRange = [self _convertToDOMRange:range];
    if (domRange)
        _private->coreFrame->selection().setSelection(WebCore::VisibleSelection(*domRange, WebCore::SEL_DEFAULT_AFFINITY));
}

- (BOOL)_isDisplayingStandaloneImage
{
    auto* document = _private->coreFrame->document();
    return document && document->isImageDocument();
}

- (unsigned)_pendingFrameUnloadEventCount
{
    return _private->coreFrame->document()->domWindow()->pendingUnloadEventListeners();
}

#if ENABLE(NETSCAPE_PLUGIN_API)
- (void)_recursive_resumeNullEventsForAllNetscapePlugins
{
    auto coreFrame = core(self);
    for (auto* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        NSView <WebDocumentView> *documentView = [[kit(frame) frameView] documentView];
        if ([documentView isKindOfClass:[WebHTMLView class]])
            [(WebHTMLView *)documentView _resumeNullEventsForAllNetscapePlugins];
    }
}

- (void)_recursive_pauseNullEventsForAllNetscapePlugins
{
    auto coreFrame = core(self);
    for (auto* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
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
        if (auto coreFrame = _private->coreFrame)
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
        if (auto coreFrame = _private->coreFrame)
            coreFrame->dispatchPageHideEventBeforePause();
    }
}

- (void)resumeFromPause
{
    if ([self _webHTMLDocumentView]) {
        if (auto coreFrame = _private->coreFrame)
            coreFrame->dispatchPageShowEventBeforeResume();
    }
}

- (void)selectNSRange:(NSRange)range
{
    [self _selectNSRange:range];
}

- (void)selectWithoutClosingTypingNSRange:(NSRange)range
{
    RefPtr<WebCore::Range> domRange = [self _convertToDOMRange:range];
    if (domRange) {
        const auto& newSelection = WebCore::VisibleSelection(*domRange, WebCore::SEL_DEFAULT_AFFINITY);
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
    auto& frameLoader = _private->coreFrame->loader();
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
        if (auto* frame = core(self))
            frame->orientationChanged();
    });
}

- (void)setNeedsLayout
{
    WebCore::Frame *frame = core(self);
    if (frame->view())
        frame->view()->setNeedsLayoutAfterViewConfigurationChange();
}

- (CGSize)renderedSizeOfNode:(DOMNode *)node constrainedToWidth:(float)width
{
    WebCore::Node* n = core(node);
    auto* renderer = n ? n->renderer() : nullptr;
    float w = std::min((float)renderer->maxPreferredLogicalWidth(), width);
    return is<WebCore::RenderBox>(renderer) ? CGSizeMake(w, downcast<WebCore::RenderBox>(*renderer).height()) : CGSizeMake(0, 0);
}

- (DOMNode *)deepestNodeAtViewportLocation:(CGPoint)aViewportLocation
{
    WebCore::Frame *frame = core(self);
    return kit(frame->deepestNodeAtLocation(WebCore::FloatPoint(aViewportLocation)));
}

- (DOMNode *)scrollableNodeAtViewportLocation:(CGPoint)aViewportLocation
{
    WebCore::Frame *frame = core(self);
    WebCore::Node *node = frame->nodeRespondingToScrollWheelEvents(WebCore::FloatPoint(aViewportLocation));
    return kit(node);
}

- (DOMNode *)approximateNodeAtViewportLocation:(CGPoint *)aViewportLocation
{
    WebCore::Frame *frame = core(self);
    WebCore::FloatPoint viewportLocation(*aViewportLocation);
    WebCore::FloatPoint adjustedLocation;
    WebCore::Node *node = frame->approximateNodeAtViewportLocationLegacy(viewportLocation, adjustedLocation);
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
    WebCore::RevealExtentOption revealExtentOption = revealExtent ? WebCore::RevealExtent : WebCore::DoNotRevealExtent;
    frame->selection().revealSelection(WebCore::SelectionRevealMode::Reveal, WebCore::ScrollAlignment::alignToEdgeIfNeeded, revealExtentOption);
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
    WebCore::Color qColor = color ? WebCore::Color(color) : WebCore::Color::black;
    WebCore::Frame *frame = core(self);
    frame->selection().setCaretColor(qColor);
}

- (CGColorRef)caretColor
{
    auto* frame = core(self);
    if (!frame)
        return nil;
    auto* document = frame->document();
    if (!document)
        return nil;
    auto* focusedElement = document->focusedElement();
    if (!focusedElement)
        return nil;
    auto* renderer = focusedElement->renderer();
    if (!renderer)
        return nil;
    auto color = WebCore::CaretBase::computeCaretColor(renderer->style(), renderer->element());
    return color.isValid() ? cachedCGColor(color) : nil;
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
    WebCore::Frame *frame = core(self);
    if (!frame || !frame->document())
        return false;
    return frame->document()->isTelephoneNumberParsingAllowed();
}

- (BOOL)isTelephoneNumberParsingEnabled
{
    WebCore::Frame *frame = core(self);
    if (!frame || !frame->document())
        return false;
    return frame->document()->isTelephoneNumberParsingEnabled();
}

- (DOMRange *)selectedDOMRange
{
    return kit(core(self)->selection().selection().toNormalizedRange());
}

- (void)setSelectedDOMRange:(DOMRange *)range affinity:(NSSelectionAffinity)affinity closeTyping:(BOOL)closeTyping
{
    [self setSelectedDOMRange:range affinity:affinity closeTyping:closeTyping userTriggered:NO];
}

- (void)setSelectedDOMRange:(DOMRange *)range affinity:(NSSelectionAffinity)affinity closeTyping:(BOOL)closeTyping userTriggered:(BOOL)userTriggered
{
    using namespace WebCore;

    auto& frame = *core(self);
    if (!frame.page())
        return;

    // Ensure the view becomes first responder. This does not happen automatically on iOS because
    // we don't forward all the click events to WebKit.
    if (NSView *documentView = frame.view()->documentView())
        frame.page()->chrome().focusNSView(documentView);

    auto coreCloseTyping = closeTyping ? FrameSelection::ShouldCloseTyping::Yes : FrameSelection::ShouldCloseTyping::No;
    auto coreUserTriggered = userTriggered ? UserTriggered : NotUserTriggered;
    frame.selection().setSelectedRange(core(range), core(affinity), coreCloseTyping, coreUserTriggered);
    if (!closeTyping)
        frame.editor().ensureLastEditCommandHasCurrentSelectionIfOpenForMoreTyping();
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
    return kit(core(self)->selection().elementRangeContainingCaretSelection());
}

- (void)expandSelectionToWordContainingCaretSelection
{
    core(self)->selection().expandSelectionToWordContainingCaretSelection();
}

- (void)expandSelectionToStartOfWordContainingCaretSelection
{
    core(self)->selection().expandSelectionToStartOfWordContainingCaretSelection();
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
    return kit(core(self)->selection().wordRangeContainingCaretSelection());
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
    return kit(core(self)->selection().rangeByMovingCurrentSelection(amount));
}

- (DOMRange *)rangeByExtendingCurrentSelection:(int)amount
{
    return kit(core(self)->selection().rangeByExtendingCurrentSelection(amount));
}

- (void)selectNSRange:(NSRange)range onElement:(DOMElement *)element
{
    WebCore::Frame *frame = core(self);

    WebCore::Document* doc = frame->document();
    if (!doc)
        return;

    auto* node = core(element);
    if (!node->isConnected())
        return;

    frame->selection().selectRangeOnElement(range.location, range.length, *node);
}

- (DOMRange *)markedTextDOMRange
{
    WebCore::Frame *frame = core(self);
    if (!frame)
        return nil;

    return kit(frame->editor().compositionRange());
}

- (void)setMarkedText:(NSString *)text selectedRange:(NSRange)newSelRange
{
    WebCore::Frame *frame = core(self);
    if (!frame)
        return;
    
    Vector<WebCore::CompositionUnderline> underlines;
    frame->page()->chrome().client().suppressFormNotifications();
    frame->editor().setComposition(text, underlines, { }, newSelRange.location, NSMaxRange(newSelRange));
    frame->page()->chrome().client().restoreFormNotifications();
}

- (void)setMarkedText:(NSString *)text forCandidates:(BOOL)forCandidates
{
    WebCore::Frame *frame = core(self);
    if (!frame)
        return;
        
    Vector<WebCore::CompositionUnderline> underlines;
    frame->editor().setComposition(text, underlines, { }, 0, [text length]);
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
    
    auto* frame = core(self);
    auto* document = frame->document();

    const auto& selection = frame->selection().selection();
    WebCore::Element* root = selection.selectionType() == WebCore::VisibleSelection::NoSelection ? frame->document()->bodyOrFrameset() : selection.rootEditableElement();
    
    DOMRange *previousDOMRange = nil;
    id previousMetadata = nil;
    
    for (WebCore::Node* node = root; node; node = WebCore::NodeTraversal::next(*node)) {
        auto markers = document->markers().markersFor(*node);
        for (auto* marker : markers) {
            if (marker->type() != WebCore::DocumentMarker::DictationResult)
                continue;

            id metadata = WTF::get<RetainPtr<id>>(marker->data()).get();

            // All result markers should have metadata.
            ASSERT(metadata);
            if (!metadata)
                continue;
            
            auto range = WebCore::Range::create(*document, node, marker->startOffset(), node, marker->endOffset());
            DOMRange *domRange = kit(range.ptr());
            
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

    auto markers = core(self)->document()->markers().markersInRange(*core(range), WebCore::DocumentMarker::DictationResult);

    // UIKit should only ever give us a DOMRange for a phrase with alternatives, which should not be part of more than one result.
    ASSERT(markers.size() <= 1);
    if (markers.size() == 0)
        return nil;

    return WTF::get<RetainPtr<id>>(markers[0]->data()).get();
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
        encoding = WebCore::WindowsLatin1Encoding();
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
        const WebCore::Font* fd = _private->coreFrame->editor().fontForSelection(multipleFonts);
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
    if (auto* view = _private->coreFrame->view())
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
    auto range = _private->coreFrame->selection().selection().toNormalizedRange();
    DOMDocumentFragment* fragment = range ? kit(createFragmentFromText(createLiveRange(*range), text).ptr()) : nil;
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
    
    auto coreFrame = core(self);
    for (auto* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        WebCore::Document* doc = frame->document();
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
    auto* frame = core(self);
    auto* page = frame->page();
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
    _private->coreFrame->editor().replaceSelectionWithFragment(*core(fragment), selectReplacement ? WebCore::Editor::SelectReplacement::Yes : WebCore::Editor::SelectReplacement::No, smartReplace ? WebCore::Editor::SmartReplace::Yes : WebCore::Editor::SmartReplace::No, matchStyle ? WebCore::Editor::MatchStyle::Yes : WebCore::Editor::MatchStyle::No);
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
    auto range = _private->coreFrame->selection().selection().toNormalizedRange();
    DOMDocumentFragment* fragment = range ? kit(createFragmentFromText(createLiveRange(*range), text).ptr()) : nil;
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
    WebCore::Node *startContainer = core([rangeToReplace startContainer]);
    WebCore::Node *endContainer = core([rangeToReplace endContainer]);

    WebCore::Position startPos(startContainer, [rangeToReplace startOffset], WebCore::Position::PositionIsOffsetInAnchor);
    WebCore::Position endPos(endContainer, [rangeToReplace endOffset], WebCore::Position::PositionIsOffsetInAnchor);

    WebCore::VisiblePosition startVisiblePos = WebCore::VisiblePosition(startPos, WebCore::VP_DEFAULT_AFFINITY);
    WebCore::VisiblePosition endVisiblePos = WebCore::VisiblePosition(endPos, WebCore::VP_DEFAULT_AFFINITY);
    
    // this check also ensures startContainer, startPos, endContainer, and endPos are non-null
    if (startVisiblePos.isNull() || endVisiblePos.isNull())
        return;

    bool addLeadingSpace = startPos.leadingWhitespacePosition(WebCore::VP_DEFAULT_AFFINITY, true).isNull() && !isStartOfParagraph(startVisiblePos);
    if (addLeadingSpace)
        if (UChar previousChar = startVisiblePos.previous().characterAfter())
            addLeadingSpace = !WebCore::isCharacterSmartReplaceExempt(previousChar, true);
    
    bool addTrailingSpace = endPos.trailingWhitespacePosition(WebCore::VP_DEFAULT_AFFINITY, true).isNull() && !isEndOfParagraph(endVisiblePos);
    if (addTrailingSpace)
        if (UChar thisChar = endVisiblePos.characterAfter())
            addTrailingSpace = !WebCore::isCharacterSmartReplaceExempt(thisChar, false);
    
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
    
    auto& frameLoader = _private->coreFrame->loader();
    auto* documentLoader = frameLoader.documentLoader();
    if (documentLoader && !documentLoader->mainDocumentError().isNull())
        [result setObject:(NSError *)documentLoader->mainDocumentError() forKey:WebFrameMainDocumentError];
        
    if (frameLoader.subframeLoader().containsPlugins())
        [result setObject:@YES forKey:WebFrameHasPlugins];
    
    if (WebCore::DOMWindow* domWindow = _private->coreFrame->document()->domWindow()) {
        if (domWindow->hasEventListeners(WebCore::eventNames().unloadEvent))
            [result setObject:@YES forKey:WebFrameHasUnloadListener];
        if (domWindow->optionalApplicationCache())
            [result setObject:@YES forKey:WebFrameUsesApplicationCache];
    }
    
    if (auto* document = _private->coreFrame->document()) {
        if (WebCore::DatabaseManager::singleton().hasOpenDatabases(*document))
            [result setObject:@YES forKey:WebFrameUsesDatabases];
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
    auto* anyWorldGlobalObject = _private->coreFrame->script().globalObject(WebCore::mainThreadNormalWorld());

    // The global object is probably a proxy object? - if so, we know how to use this!
    JSC::JSObject* globalObjectObj = toJS(globalObjectRef);
    JSC::VM& vm = globalObjectObj->vm();
    if (!strcmp(globalObjectObj->classInfo(vm)->className, "JSWindowProxy"))
        anyWorldGlobalObject = JSC::jsDynamicCast<WebCore::JSDOMWindow*>(vm, static_cast<WebCore::JSWindowProxy*>(globalObjectObj)->window());

    if (!anyWorldGlobalObject)
        return @"";

    // Get the frame frome the global object we've settled on.
    auto* frame = anyWorldGlobalObject->wrapped().frame();
    ASSERT(frame->document());
    RetainPtr<WebFrame> webFrame(kit(frame)); // Running arbitrary JavaScript can destroy the frame.

    JSC::JSValue result = frame->script().executeUserAgentScriptInWorldIgnoringException(*core(world), string, true);

    if (!webFrame->_private->coreFrame) // In case the script removed our frame from the page.
        return @"";

    // This bizarre set of rules matches behavior from WebKit for Safari 2.0.
    // If you don't like it, use -[WebScriptObject evaluateWebScript:] or 
    // JSEvaluateScript instead, since they have less surprising semantics.
    if (!result || (!result.isBoolean() && !result.isString() && !result.isNumber()))
        return @"";

    JSC::JSGlobalObject* lexicalGlobalObject = anyWorldGlobalObject;
    JSC::JSLockHolder lock(lexicalGlobalObject);
    return result.toWTFString(lexicalGlobalObject);
}

- (JSGlobalContextRef)_globalContextForScriptWorld:(WebScriptWorld *)world
{
    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return 0;
    auto* coreWorld = core(world);
    if (!coreWorld)
        return 0;
    return toGlobalRef(coreFrame->script().globalObject(*coreWorld));
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
#if ENABLE(ACCESSIBILITY)
    if (!WebCore::AXObjectCache::accessibilityEnabled())
        return;
    
    if (!_private->coreFrame || !_private->coreFrame->document())
        return;
    
    auto* rootObject = _private->coreFrame->document()->axObjectCache()->rootObject();
    if (rootObject) {
        String strName(name);
        rootObject->setAccessibleName(strName);
    }
#endif
}

- (BOOL)enhancedAccessibilityEnabled
{
#if ENABLE(ACCESSIBILITY)
    return WebCore::AXObjectCache::accessibilityEnhancedUserInterfaceEnabled();
#else
    return NO;
#endif
}

- (void)setEnhancedAccessibility:(BOOL)enable
{
#if ENABLE(ACCESSIBILITY)
    WebCore::AXObjectCache::setEnhancedUserInterfaceAccessibility(enable);
#endif
}

- (NSString*)_layerTreeAsText
{
    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return @"";

    return coreFrame->layerTreeAsText();
}

- (id)accessibilityRoot
{
#if ENABLE(ACCESSIBILITY)
    if (!WebCore::AXObjectCache::accessibilityEnabled()) {
        WebCore::AXObjectCache::enableAccessibility();
#if !PLATFORM(IOS_FAMILY)
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        WebCore::AXObjectCache::setEnhancedUserInterfaceAccessibility([[NSApp accessibilityAttributeValue:NSAccessibilityEnhancedUserInterfaceAttribute] boolValue]);
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    }
    
    if (!_private->coreFrame)
        return nil;
    
    auto* document = _private->coreFrame->document();
    if (!document || !document->axObjectCache())
        return nil;
    
    auto* rootObject = document->axObjectCache()->rootObjectForFrame(_private->coreFrame);
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
    auto coreFrame = _private->coreFrame;
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
        return @[];
    }

    if (!_private->coreFrame)
        return @[];
    if (!_private->coreFrame->document())
        return @[];
    if (!_private->coreFrame->view())
        return @[];
    if (!_private->coreFrame->view()->documentView())
        return @[];

    auto* root = _private->coreFrame->document()->renderView();
    if (!root)
        return @[];

    const auto& documentRect = root->documentRect();
    float printWidth = root->style().isHorizontalWritingMode() ? static_cast<float>(documentRect.width()) / printScaleFactor : pageSize.width;
    float printHeight = root->style().isHorizontalWritingMode() ? pageSize.height : static_cast<float>(documentRect.height()) / printScaleFactor;

    WebCore::PrintContext printContext(_private->coreFrame);
    printContext.computePageRectsWithPageSize(WebCore::FloatSize(printWidth, printHeight), true);
    return createNSArray(printContext.pageRects()).autorelease();
}

#if PLATFORM(IOS_FAMILY)

- (DOMDocumentFragment *)_documentFragmentForText:(NSString *)text
{
    return kit(createFragmentFromText(*createLiveRange(_private->coreFrame->selection().selection().toNormalizedRange()), text).ptr());
}

- (DOMDocumentFragment *)_documentFragmentForWebArchive:(WebArchive *)webArchive
{
    return [[self dataSource] _documentFragmentWithArchive:webArchive];
}

- (DOMDocumentFragment *)_documentFragmentForImageData:(NSData *)data withRelativeURLPart:(NSString *)relativeURLPart andMIMEType:(NSString *)mimeType
{
    auto resource = adoptNS([[WebResource alloc] initWithData:data
        URL:URL::fakeURLWithRelativePart(String { relativeURLPart })
        MIMEType:mimeType textEncodingName:nil frameName:nil]);
    return [[self _dataSource] _documentFragmentWithImageResource:resource.get()];
}

- (BOOL)focusedNodeHasContent
{
    auto coreFrame = _private->coreFrame;
   
    WebCore::Element* root;
    const auto& selection = coreFrame->selection().selection();
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

    WebCore::VisiblePosition first(createLegacyEditingPosition(root, 0));
    WebCore::VisiblePosition last(createLegacyEditingPosition(root, root->countChildNodes()));
    return first != last;
}

- (void)_dispatchDidReceiveTitle:(NSString *)title
{
    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return;
    coreFrame->loader().client().dispatchDidReceiveTitle({ title, WebCore::TextDirection::LTR });
}

#endif // PLATFORM(IOS_FAMILY)

- (JSValueRef)jsWrapperForNode:(DOMNode *)node inScriptWorld:(WebScriptWorld *)world
{
    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return 0;

    if (!world)
        return 0;

    WebCore::JSDOMWindow* globalObject = coreFrame->script().globalObject(*core(world));
    JSC::JSGlobalObject* lexicalGlobalObject = globalObject;

    JSC::JSLockHolder lock(lexicalGlobalObject);
    return toRef(lexicalGlobalObject, toJS(lexicalGlobalObject, globalObject, core(node)));
}

- (NSDictionary *)elementAtPoint:(NSPoint)point
{
    using namespace WebCore;

    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return nil;
    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::ReadOnly, HitTestRequest::Active, HitTestRequest::IgnoreClipping, HitTestRequest::DisallowUserAgentShadowContent, HitTestRequest::AllowChildFrameContent };
    return [[[WebElementDictionary alloc] initWithHitTestResult:coreFrame->eventHandler().hitTestResultAtPoint(WebCore::IntPoint(point), hitType)] autorelease];
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
    auto coreFrame = _private->coreFrame;
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
    static bool needsWorkaround = WebCore::MacApplication::isMicrosoftMessenger() && [[[NSBundle mainBundle] objectForInfoDictionaryKey:(NSString *)kCFBundleVersionKey] compare:@"7.1" options:NSNumericSearch] == NSOrderedAscending;
    return needsWorkaround;
#endif
}

- (DOMDocument *)DOMDocument
{
    if (needsMicrosoftMessengerDOMDocumentWorkaround() && !pthread_main_np())
        return nil;

    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return nil;
    
    // FIXME: <rdar://problem/5145841> When loading a custom view/representation 
    // into a web frame, the old document can still be around. This makes sure that
    // we'll return nil in those cases.
    if (![[self _dataSource] _isDocumentHTML]) 
        return nil; 

    auto* document = coreFrame->document();
    
    // According to the documentation, we should return nil if the frame doesn't have a document.
    // While full-frame images and plugins do have an underlying HTML document, we return nil here to be
    // backwards compatible.
    if (document && (document->isPluginDocument() || document->isImageDocument()))
        return nil;
    
    return kit(coreFrame->document());
}

- (DOMHTMLElement *)frameElement
{
    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return nil;
    return kit(coreFrame->ownerElement());
}

- (WebDataSource *)provisionalDataSource
{
    auto coreFrame = _private->coreFrame;
    return coreFrame ? dataSource(coreFrame->loader().provisionalDocumentLoader()) : nil;
}

- (WebDataSource *)dataSource
{
    auto coreFrame = _private->coreFrame;
    return coreFrame && coreFrame->loader().frameHasLoaded() ? [self _dataSource] : nil;
}

- (void)loadRequest:(NSURLRequest *)request
{
    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return;

    WebCore::ResourceRequest resourceRequest(request);
    
    // Some users of WebKit API incorrectly use "file path as URL" style requests which are invalid.
    // By re-writing those URLs here we technically break the -[WebDataSource initialRequest] API
    // but that is necessary to implement this quirk only at the API boundary.
    // Note that other users of WebKit API use nil requests or requests with nil URLs or empty URLs, so we
    // only implement this workaround when the request had a non-nil or non-empty URL.
    if (!resourceRequest.url().isValid() && !resourceRequest.url().isEmpty())
        resourceRequest.setURL([NSURL URLWithString:[@"file:" stringByAppendingString:[[request URL] absoluteString]]]);

    coreFrame->loader().load(WebCore::FrameLoadRequest(*coreFrame, resourceRequest));
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
        baseURL = aboutBlankURL();
        responseURL = createUniqueWebDataURL();
    }
    
#if USE(QUICK_LOOK)
    if (WebCore::shouldUseQuickLookForMIMEType(MIMEType)) {
        NSURL *quickLookURL = responseURL ? responseURL : baseURL;
        if (auto request = WebCore::registerQLPreviewConverterIfNeeded(quickLookURL, MIMEType, data)) {
            _private->coreFrame->loader().load(WebCore::FrameLoadRequest(*_private->coreFrame, request.get()));
            return;
        }
    }
#endif

    WebCore::ResourceRequest request(baseURL);

    WebCore::ResourceResponse response(responseURL, MIMEType, [data length], encodingName);
    WebCore::SubstituteData substituteData(WebCore::SharedBuffer::create(data), [unreachableURL absoluteURL], response, WebCore::SubstituteData::SessionHistoryVisibility::Hidden);

    _private->coreFrame->loader().load(WebCore::FrameLoadRequest(*_private->coreFrame, request, substituteData));
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
    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return nil;
    return kit(coreFrame->tree().find(name, *coreFrame));
}

- (WebFrame *)parentFrame
{
    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return nil;
    return [[kit(coreFrame->tree().parent()) retain] autorelease];
}

- (NSArray *)childFrames
{
    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return @[];
    NSMutableArray *children = [NSMutableArray arrayWithCapacity:coreFrame->tree().childCount()];
    for (WebCore::Frame* child = coreFrame->tree().firstChild(); child; child = child->tree().nextSibling())
        [children addObject:kit(child)];
    return children;
}

- (WebScriptObject *)windowObject
{
    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return 0;
    return coreFrame->script().windowScriptObject();
}

- (JSGlobalContextRef)globalContext
{
    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return 0;
    return toGlobalRef(coreFrame->script().globalObject(WebCore::mainThreadNormalWorld()));
}

#if JSC_OBJC_API_ENABLED
- (JSContext *)javaScriptContext
{
    auto coreFrame = _private->coreFrame;
    if (!coreFrame)
        return 0;
    return coreFrame->script().javaScriptContext();
}
#endif

@end
