/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#import "WebFrameLoaderClient.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLView.h"
#import "WebHTMLViewInternal.h"
#import "WebIconFetcherInternal.h"
#import "WebKitStatisticsPrivate.h"
#import "WebNSURLExtras.h"
#import "WebScriptDebugger.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/APICast.h>
#import <WebCore/AccessibilityObject.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/ColorMac.h>
#import <WebCore/DOMImplementation.h>
#import <WebCore/DocLoader.h>
#import <WebCore/DocumentFragment.h>
#import <WebCore/EventHandler.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameTree.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/HTMLFrameOwnerElement.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/LegacyWebArchive.h>
#import <WebCore/Page.h>
#import <WebCore/PluginData.h>
#import <WebCore/RenderPart.h>
#import <WebCore/RenderView.h>
#import <WebCore/RenderLayer.h>
#import <WebCore/ReplaceSelectionCommand.h>
#import <WebCore/SmartReplace.h>
#import <WebCore/SystemTime.h>
#import <WebCore/TextIterator.h>
#import <WebCore/TypingCommand.h>
#import <WebCore/htmlediting.h>
#import <WebCore/ScriptController.h>
#import <WebCore/markup.h>
#import <WebCore/visible_units.h>
#import <runtime/JSLock.h>

using namespace std;
using namespace WebCore;
using namespace HTMLNames;

using JSC::JSGlobalObject;
using JSC::JSLock;
using JSC::JSValue;

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

// FIXME: Remove when this key becomes publicly defined
NSString *NSAccessibilityEnhancedUserInterfaceAttribute = @"AXEnhancedUserInterface";

@implementation WebFramePrivate

- (void)dealloc
{
    [webFrameView release];

    delete scriptDebugger;

    [super dealloc];
}

- (void)finalize
{
    delete scriptDebugger;

    [super finalize];
}

- (void)setWebFrameView:(WebFrameView *)v 
{ 
    [v retain];
    [webFrameView release];
    webFrameView = v;
}

@end

CSSStyleDeclaration* core(DOMCSSStyleDeclaration *declaration)
{
    return [declaration _CSSStyleDeclaration];
}

DOMCSSStyleDeclaration *kit(WebCore::CSSStyleDeclaration* declaration)
{
    return [DOMCSSStyleDeclaration _wrapCSSStyleDeclaration:declaration];
}

Element* core(DOMElement *element)
{
    return [element _element];
}

DOMElement *kit(Element* element)
{
    return [DOMElement _wrapElement:element];
}

Node* core(DOMNode *node)
{
    return [node _node];
}

DOMNode *kit(Node* node)
{
    return [DOMNode _wrapNode:node];
}

DOMNode *kit(PassRefPtr<Node> node)
{
    return [DOMNode _wrapNode:node.get()];
}

Document* core(DOMDocument *document)
{
    return [document _document];
}

DOMDocument *kit(Document* document)
{
    return [DOMDocument _wrapDocument:document];
}

DocumentFragment* core(DOMDocumentFragment *fragment)
{
    return [fragment _documentFragment];
}

DOMDocumentFragment *kit(DocumentFragment* fragment)
{
    return [DOMDocumentFragment _wrapDocumentFragment:fragment];
}

HTMLElement* core(DOMHTMLElement *element)
{
    return [element _HTMLElement];
}

DOMHTMLElement *kit(HTMLElement *element)
{
    return [DOMHTMLElement _wrapHTMLElement:element];
}

Range* core(DOMRange *range)
{
    return [range _range];
}

DOMRange *kit(Range* range)
{
    return [DOMRange _wrapRange:range];
}

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

@implementation WebFrame (WebInternal)

Frame* core(WebFrame *frame)
{
    return frame ? frame->_private->coreFrame : 0;
}

WebFrame *kit(Frame* frame)
{
    return frame ? static_cast<WebFrameLoaderClient*>(frame->loader()->client())->webFrame() : nil;
}

Page* core(WebView *webView)
{
    return [webView page];
}

WebView *kit(Page* page)
{
    return page ? static_cast<WebChromeClient*>(page->chrome()->client())->webView() : nil;
}

WebView *getWebView(WebFrame *webFrame)
{
    Frame* coreFrame = core(webFrame);
    if (!coreFrame)
        return nil;
    return kit(coreFrame->page());
}

+ (PassRefPtr<Frame>)_createFrameWithPage:(Page*)page frameName:(const String&)name frameView:(WebFrameView *)frameView ownerElement:(HTMLFrameOwnerElement*)ownerElement
{
    WebView *webView = kit(page);

    WebFrame *frame = [[self alloc] _initWithWebFrameView:frameView webView:webView];
    RefPtr<Frame> coreFrame = Frame::create(page, ownerElement, new WebFrameLoaderClient(frame));
    [frame release];
    frame->_private->coreFrame = coreFrame.get();

    coreFrame->tree()->setName(name);
    if (ownerElement) {
        ASSERT(ownerElement->document()->frame());
        ownerElement->document()->frame()->tree()->appendChild(coreFrame.get());
    }

    coreFrame->init();

    [webView _setZoomMultiplier:[webView _realZoomMultiplier] isTextOnly:[webView _realZoomMultiplierIsTextOnly]];

    return coreFrame.release();
}

+ (void)_createMainFrameWithPage:(Page*)page frameName:(const String&)name frameView:(WebFrameView *)frameView
{
    [self _createFrameWithPage:page frameName:name frameView:frameView ownerElement:0];
}

+ (PassRefPtr<WebCore::Frame>)_createSubframeWithOwnerElement:(HTMLFrameOwnerElement*)ownerElement frameName:(const String&)name frameView:(WebFrameView *)frameView
{
    return [self _createFrameWithPage:ownerElement->document()->frame()->page() frameName:name frameView:frameView ownerElement:ownerElement];
}

- (void)_attachScriptDebugger
{
    ScriptController* scriptController = _private->coreFrame->script();

    // Calling ScriptController::globalObject() would create a window shell, and dispatch corresponding callbacks, which may be premature
    //  if the script debugger is attached before a document is created.
    if (!scriptController->haveWindowShell())
        return;

    JSGlobalObject* globalObject = scriptController->globalObject();
    if (!globalObject)
        return;

    if (_private->scriptDebugger) {
        ASSERT(_private->scriptDebugger == globalObject->debugger());
        return;
    }

    _private->scriptDebugger = new WebScriptDebugger(globalObject);
}

- (void)_detachScriptDebugger
{
    if (!_private->scriptDebugger)
        return;

    delete _private->scriptDebugger;
    _private->scriptDebugger = 0;
}

- (id)_initWithWebFrameView:(WebFrameView *)fv webView:(WebView *)v
{
    self = [super init];
    if (!self)
        return nil;

    _private = [[WebFramePrivate alloc] init];

    if (fv) {
        [_private setWebFrameView:fv];
        [fv _setWebFrame:self];
    }

    _private->shouldCreateRenderers = YES;

    ++WebFrameCount;

    return self;
}

- (void)_clearCoreFrame
{
    _private->coreFrame = 0;
}

- (void)_updateBackgroundAndUpdatesWhileOffscreen
{
    WebView *webView = getWebView(self);
    BOOL drawsBackground = [webView drawsBackground];
    NSColor *backgroundColor = [webView backgroundColor];

    Frame* coreFrame = _private->coreFrame;
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        WebFrame *webFrame = kit(frame);
        // Don't call setDrawsBackground:YES here because it may be NO because of a load
        // in progress; WebFrameLoaderClient keeps it set to NO during the load process.
        if (!drawsBackground)
            [[[webFrame frameView] _scrollView] setDrawsBackground:NO];
        [[[webFrame frameView] _scrollView] setBackgroundColor:backgroundColor];
        id documentView = [[webFrame frameView] documentView];
        if ([documentView respondsToSelector:@selector(setDrawsBackground:)])
            [documentView setDrawsBackground:drawsBackground];
        if ([documentView respondsToSelector:@selector(setBackgroundColor:)])
            [documentView setBackgroundColor:backgroundColor];
        if (frame && frame->view()) {
            frame->view()->setTransparent(!drawsBackground);
            Color color = colorFromNSColor([backgroundColor colorUsingColorSpaceName:NSDeviceRGBColorSpace]);
            frame->view()->setBaseBackgroundColor(color);
            frame->view()->setShouldUpdateWhileOffscreen([webView shouldUpdateWhileOffscreen]);
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

#ifndef BUILDING_ON_TIGER
- (void)_unmarkAllBadGrammar
{
    Frame* coreFrame = _private->coreFrame;
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        if (Document* document = frame->document())
            document->removeMarkers(DocumentMarker::Grammar);
    }
}
#endif

- (void)_unmarkAllMisspellings
{
    Frame* coreFrame = _private->coreFrame;
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        if (Document* document = frame->document())
            document->removeMarkers(DocumentMarker::Spelling);
    }
}

- (BOOL)_hasSelection
{
    id documentView = [_private->webFrameView documentView];    

    // optimization for common case to avoid creating potentially large selection string
    if ([documentView isKindOfClass:[WebHTMLView class]])
        if (Frame* coreFrame = _private->coreFrame)
            return coreFrame->selection()->isRange();

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
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame))
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
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
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
    return dataSource(_private->coreFrame->loader()->documentLoader());
}

- (void)_addData:(NSData *)data
{
    Document* document = _private->coreFrame->document();
    
    // Document may be nil if the part is about to redirect
    // as a result of JS executing during load, i.e. one frame
    // changing another's location before the frame's document
    // has been created. 
    if (!document)
        return;

    document->setShouldCreateRenderers(_private->shouldCreateRenderers);
    _private->coreFrame->loader()->addData((const char *)[data bytes], [data length]);
}

- (NSString *)_stringWithDocumentTypeStringAndMarkupString:(NSString *)markupString
{
    return _private->coreFrame->documentTypeString() + markupString;
}

- (NSArray *)_nodesFromList:(Vector<Node*> *)nodesVector
{
    size_t size = nodesVector->size();
    NSMutableArray *nodes = [NSMutableArray arrayWithCapacity:size];
    for (size_t i = 0; i < size; ++i)
        [nodes addObject:[DOMNode _wrapNode:(*nodesVector)[i]]];
    return nodes;
}

- (NSString *)_markupStringFromRange:(DOMRange *)range nodes:(NSArray **)nodes
{
    // FIXME: This is always "for interchange". Is that right? See the previous method.
    Vector<Node*> nodeList;
    NSString *markupString = createMarkup([range _range], nodes ? &nodeList : 0, AnnotateForInterchange);
    if (nodes)
        *nodes = [self _nodesFromList:&nodeList];

    return [self _stringWithDocumentTypeStringAndMarkupString:markupString];
}

- (NSString *)_selectedString
{
    String text = _private->coreFrame->selectedText();
    text.replace('\\', _private->coreFrame->backslashAsCurrencySymbol());
    return text;
}

- (NSString *)_stringForRange:(DOMRange *)range
{
    // This will give a system malloc'd buffer that can be turned directly into an NSString
    unsigned length;
    UChar* buf = plainTextToMallocAllocatedBuffer([range _range], length);
    
    if (!buf)
        return [NSString string];
    
    UChar backslashAsCurrencySymbol = _private->coreFrame->backslashAsCurrencySymbol();
    if (backslashAsCurrencySymbol != '\\')
        for (unsigned n = 0; n < length; n++) 
            if (buf[n] == '\\')
                buf[n] = backslashAsCurrencySymbol;

    // Transfer buffer ownership to NSString
    return [[[NSString alloc] initWithCharactersNoCopy:buf length:length freeWhenDone:YES] autorelease];
}

- (void)_drawRect:(NSRect)rect contentsOnly:(BOOL)contentsOnly
{
    PlatformGraphicsContext* platformContext = static_cast<PlatformGraphicsContext*>([[NSGraphicsContext currentContext] graphicsPort]);
    ASSERT([[NSGraphicsContext currentContext] isFlipped]);
    GraphicsContext context(platformContext);
    
    if (contentsOnly)
        _private->coreFrame->view()->paintContents(&context, enclosingIntRect(rect));
    else
        _private->coreFrame->view()->paint(&context, enclosingIntRect(rect));
}

// Used by pagination code called from AppKit when a standalone web page is printed.
- (NSArray*)_computePageRectsWithPrintWidthScaleFactor:(float)printWidthScaleFactor printHeight:(float)printHeight
{
    NSMutableArray* pages = [NSMutableArray arrayWithCapacity:5];
    if (printWidthScaleFactor <= 0) {
        LOG_ERROR("printWidthScaleFactor has bad value %.2f", printWidthScaleFactor);
        return pages;
    }
    
    if (printHeight <= 0) {
        LOG_ERROR("printHeight has bad value %.2f", printHeight);
        return pages;
    }

    if (!_private->coreFrame || !_private->coreFrame->document() || !_private->coreFrame->view()) return pages;
    RenderView* root = static_cast<RenderView *>(_private->coreFrame->document()->renderer());
    if (!root) return pages;
    
    FrameView* view = _private->coreFrame->view();
    if (!view)
        return pages;

    NSView* documentView = view->documentView();
    if (!documentView)
        return pages;

    float currPageHeight = printHeight;
    float docHeight = root->layer()->height();
    float docWidth = root->layer()->width();
    float printWidth = docWidth/printWidthScaleFactor;
    
    // We need to give the part the opportunity to adjust the page height at each step.
    for (float i = 0; i < docHeight; i += currPageHeight) {
        float proposedBottom = min(docHeight, i + printHeight);
        _private->coreFrame->adjustPageHeight(&proposedBottom, i, proposedBottom, i);
        currPageHeight = max(1.0f, proposedBottom - i);
        for (float j = 0; j < docWidth; j += printWidth) {
            NSValue* val = [NSValue valueWithRect: NSMakeRect(j, i, printWidth, currPageHeight)];
            [pages addObject: val];
        }
    }
    
    return pages;
}

- (BOOL)_getVisibleRect:(NSRect*)rect;
{
    ASSERT_ARG(rect, rect);
    if (RenderPart* ownerRenderer = _private->coreFrame->ownerRenderer()) {
        if (ownerRenderer->needsLayout())
            return NO;
        *rect = ownerRenderer->absoluteClippedOverflowRect();
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
    ASSERT(_private->coreFrame->document());
    
    JSValue* result = _private->coreFrame->loader()->executeScript(string, forceUserGesture);

    if (!_private->coreFrame) // In case the script removed our frame from the page.
        return @"";

    // This bizarre set of rules matches behavior from WebKit for Safari 2.0.
    // If you don't like it, use -[WebScriptObject evaluateWebScript:] or 
    // JSEvaluateScript instead, since they have less surprising semantics.
    if (!result || !result->isBoolean() && !result->isString() && !result->isNumber())
        return @"";

    JSLock lock(false);
    return String(result->toString(_private->coreFrame->script()->globalObject()->globalExec()));
}

- (NSRect)_caretRectAtNode:(DOMNode *)node offset:(int)offset affinity:(NSSelectionAffinity)affinity
{
    VisiblePosition visiblePosition([node _node], offset, static_cast<EAffinity>(affinity));
    return visiblePosition.caretRect();
}

- (NSRect)_firstRectForDOMRange:(DOMRange *)range
{
   return _private->coreFrame->firstRectForRange([range _range]);
}

- (void)_scrollDOMRangeToVisible:(DOMRange *)range
{
    NSRect rangeRect = [self _firstRectForDOMRange:range];    
    Node *startNode = [[range startContainer] _node];
        
    if (startNode && startNode->renderer()) {
        RenderLayer *layer = startNode->renderer()->enclosingLayer();
        if (layer)
            layer->scrollRectToVisible(enclosingIntRect(rangeRect), false, RenderLayer::gAlignToEdgeIfNeeded, RenderLayer::gAlignToEdgeIfNeeded);
    }
}

- (BOOL)_needsLayout
{
    return _private->coreFrame->view() ? _private->coreFrame->view()->needsLayout() : false;
}

- (id)_accessibilityTree
{
#if HAVE(ACCESSIBILITY)
    if (!AXObjectCache::accessibilityEnabled()) {
        AXObjectCache::enableAccessibility();
        if ([[NSApp accessibilityAttributeValue:NSAccessibilityEnhancedUserInterfaceAttribute] boolValue])
            AXObjectCache::enableEnhancedUserInterfaceAccessibility();
    }

    if (!_private->coreFrame || !_private->coreFrame->document())
        return nil;
    RenderView* root = static_cast<RenderView *>(_private->coreFrame->document()->renderer());
    if (!root)
        return nil;
    return _private->coreFrame->document()->axObjectCache()->get(root)->wrapper();
#else
    return nil;
#endif
}

- (DOMRange *)_rangeByAlteringCurrentSelection:(SelectionController::EAlteration)alteration direction:(SelectionController::EDirection)direction granularity:(TextGranularity)granularity
{
    if (_private->coreFrame->selection()->isNone())
        return nil;

    SelectionController selection;
    selection.setSelection(_private->coreFrame->selection()->selection());
    selection.modify(alteration, direction, granularity);
    return [DOMRange _wrapRange:selection.toRange().get()];
}

- (TextGranularity)_selectionGranularity
{
    return _private->coreFrame->selectionGranularity();
}

- (NSRange)_convertToNSRange:(Range *)range
{
    if (!range || !range->startContainer())
        return NSMakeRange(NSNotFound, 0);

    Element* selectionRoot = _private->coreFrame->selection()->rootEditableElement();
    Element* scope = selectionRoot ? selectionRoot : _private->coreFrame->document()->documentElement();
    
    // Mouse events may cause TSM to attempt to create an NSRange for a portion of the view
    // that is not inside the current editable region.  These checks ensure we don't produce
    // potentially invalid data when responding to such requests.
    if (range->startContainer() != scope && !range->startContainer()->isDescendantOf(scope))
        return NSMakeRange(NSNotFound, 0);
    if (range->endContainer() != scope && !range->endContainer()->isDescendantOf(scope))
        return NSMakeRange(NSNotFound, 0);
    
    RefPtr<Range> testRange = Range::create(scope->document(), scope, 0, range->startContainer(), range->startOffset());
    ASSERT(testRange->startContainer() == scope);
    int startPosition = TextIterator::rangeLength(testRange.get());

    ExceptionCode ec;
    testRange->setEnd(range->endContainer(), range->endOffset(), ec);
    ASSERT(testRange->startContainer() == scope);
    int endPosition = TextIterator::rangeLength(testRange.get());

    return NSMakeRange(startPosition, endPosition - startPosition);
}

- (PassRefPtr<Range>)_convertToDOMRange:(NSRange)nsrange
{
    if (nsrange.location > INT_MAX)
        return 0;
    if (nsrange.length > INT_MAX || nsrange.location + nsrange.length > INT_MAX)
        nsrange.length = INT_MAX - nsrange.location;

    // our critical assumption is that we are only called by input methods that
    // concentrate on a given area containing the selection
    // We have to do this because of text fields and textareas. The DOM for those is not
    // directly in the document DOM, so serialization is problematic. Our solution is
    // to use the root editable element of the selection start as the positional base.
    // That fits with AppKit's idea of an input context.
    Element* selectionRoot = _private->coreFrame->selection()->rootEditableElement();
    Element* scope = selectionRoot ? selectionRoot : _private->coreFrame->document()->documentElement();
    return TextIterator::rangeFromLocationAndLength(scope, nsrange.location, nsrange.length);
}

- (DOMRange *)_convertNSRangeToDOMRange:(NSRange)nsrange
{
    return [DOMRange _wrapRange:[self _convertToDOMRange:nsrange].get()];
}

- (NSRange)_convertDOMRangeToNSRange:(DOMRange *)range
{
    return [self _convertToNSRange:[range _range]];
}

- (DOMRange *)_markDOMRange
{
    return [DOMRange _wrapRange:_private->coreFrame->mark().toRange().get()];
}

// Given proposedRange, returns an extended range that includes adjacent whitespace that should
// be deleted along with the proposed range in order to preserve proper spacing and punctuation of
// the text surrounding the deletion.
- (DOMRange *)_smartDeleteRangeForProposedRange:(DOMRange *)proposedRange
{
    Node *startContainer = [[proposedRange startContainer] _node];
    Node *endContainer = [[proposedRange endContainer] _node];
    if (startContainer == nil || endContainer == nil)
        return nil;

    ASSERT(startContainer->document() == endContainer->document());
    
    _private->coreFrame->document()->updateLayoutIgnorePendingStylesheets();

    Position start(startContainer, [proposedRange startOffset]);
    Position end(endContainer, [proposedRange endOffset]);
    Position newStart = start.upstream().leadingWhitespacePosition(DOWNSTREAM, true);
    if (newStart.isNull())
        newStart = start;
    Position newEnd = end.downstream().trailingWhitespacePosition(DOWNSTREAM, true);
    if (newEnd.isNull())
        newEnd = end;

    newStart = rangeCompliantEquivalent(newStart);
    newEnd = rangeCompliantEquivalent(newEnd);

    RefPtr<Range> range = _private->coreFrame->document()->createRange();
    int exception = 0;
    range->setStart(newStart.node(), newStart.offset(), exception);
    range->setEnd(newStart.node(), newStart.offset(), exception);
    return [DOMRange _wrapRange:range.get()];
}

// Determines whether whitespace needs to be added around aString to preserve proper spacing and
// punctuation when it‚Äôs inserted into the receiver‚Äôs text over charRange. Returns by reference
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
    Node *startContainer = [[rangeToReplace startContainer] _node];
    Node *endContainer = [[rangeToReplace endContainer] _node];

    Position startPos(startContainer, [rangeToReplace startOffset]);
    Position endPos(endContainer, [rangeToReplace endOffset]);

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

- (DOMDocumentFragment *)_documentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString 
{
    if (!_private->coreFrame || !_private->coreFrame->document())
        return 0;

    return [DOMDocumentFragment _wrapDocumentFragment:createFragmentFromMarkup(_private->coreFrame->document(), markupString, baseURLString).get()];
}

- (DOMDocumentFragment *)_documentFragmentWithNodesAsParagraphs:(NSArray *)nodes
{
    if (!_private->coreFrame || !_private->coreFrame->document())
        return 0;
    
    NSEnumerator *nodeEnum = [nodes objectEnumerator];
    Vector<Node*> nodesVector;
    DOMNode *node;
    while ((node = [nodeEnum nextObject]))
        nodesVector.append([node _node]);
    
    return [DOMDocumentFragment _wrapDocumentFragment:createFragmentFromNodes(_private->coreFrame->document(), nodesVector).get()];
}

- (void)_replaceSelectionWithFragment:(DOMDocumentFragment *)fragment selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle
{
    if (_private->coreFrame->selection()->isNone() || !fragment)
        return;
    
    applyCommand(ReplaceSelectionCommand::create(_private->coreFrame->document(), [fragment _documentFragment], selectReplacement, smartReplace, matchStyle));
    _private->coreFrame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (void)_replaceSelectionWithNode:(DOMNode *)node selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle
{
    DOMDocumentFragment *fragment = [DOMDocumentFragment _wrapDocumentFragment:_private->coreFrame->document()->createDocumentFragment().get()];
    [fragment appendChild:node];
    [self _replaceSelectionWithFragment:fragment selectReplacement:selectReplacement smartReplace:smartReplace matchStyle:matchStyle];
}

- (void)_replaceSelectionWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace
{
    DOMDocumentFragment *fragment = [self _documentFragmentWithMarkupString:markupString baseURLString:baseURLString];
    [self _replaceSelectionWithFragment:fragment selectReplacement:selectReplacement smartReplace:smartReplace matchStyle:NO];
}

- (void)_replaceSelectionWithText:(NSString *)text selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace
{
    [self _replaceSelectionWithFragment:kit(createFragmentFromText(_private->coreFrame->selection()->toRange().get(), text).get())
        selectReplacement:selectReplacement smartReplace:smartReplace matchStyle:YES];
}

- (void)_insertParagraphSeparatorInQuotedContent
{
    if (_private->coreFrame->selection()->isNone())
        return;
    
    TypingCommand::insertParagraphSeparatorInQuotedContent(_private->coreFrame->document());
    _private->coreFrame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (VisiblePosition)_visiblePositionForPoint:(NSPoint)point
{
    IntPoint outerPoint(point);
    HitTestResult result = _private->coreFrame->eventHandler()->hitTestResultAtPoint(outerPoint, true);
    Node* node = result.innerNode();
    if (!node)
        return VisiblePosition();
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();
    VisiblePosition visiblePos = renderer->positionForCoordinates(result.localPoint().x(), result.localPoint().y());
    if (visiblePos.isNull())
        visiblePos = VisiblePosition(Position(node, 0));
    return visiblePos;
}

- (DOMRange *)_characterRangeAtPoint:(NSPoint)point
{
    VisiblePosition position = [self _visiblePositionForPoint:point];
    if (position.isNull())
        return nil;
    
    VisiblePosition previous = position.previous();
    if (previous.isNotNull()) {
        DOMRange *previousCharacterRange = [DOMRange _wrapRange:makeRange(previous, position).get()];
        NSRect rect = [self _firstRectForDOMRange:previousCharacterRange];
        if (NSPointInRect(point, rect))
            return previousCharacterRange;
    }

    VisiblePosition next = position.next();
    if (next.isNotNull()) {
        DOMRange *nextCharacterRange = [DOMRange _wrapRange:makeRange(position, next).get()];
        NSRect rect = [self _firstRectForDOMRange:nextCharacterRange];
        if (NSPointInRect(point, rect))
            return nextCharacterRange;
    }
    
    return nil;
}

- (DOMCSSStyleDeclaration *)_typingStyle
{
    if (!_private->coreFrame || !_private->coreFrame->typingStyle())
        return nil;
    return [DOMCSSStyleDeclaration _wrapCSSStyleDeclaration:_private->coreFrame->typingStyle()->copy().get()];
}

- (void)_setTypingStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(EditAction)undoAction
{
    if (!_private->coreFrame)
        return;
    _private->coreFrame->computeAndSetTypingStyle([style _CSSStyleDeclaration], undoAction);
}

- (void)_dragSourceMovedTo:(NSPoint)windowLoc
{
    if (!_private->coreFrame)
        return;
    FrameView* view = _private->coreFrame->view();
    if (!view)
        return;
    // FIXME: These are fake modifier keys here, but they should be real ones instead.
    PlatformMouseEvent event(IntPoint(windowLoc), globalPoint(windowLoc, [view->platformWidget() window]),
        LeftButton, MouseEventMoved, 0, false, false, false, false, currentTime());
    _private->coreFrame->eventHandler()->dragSourceMovedTo(event);
}

- (void)_dragSourceEndedAt:(NSPoint)windowLoc operation:(NSDragOperation)operation
{
    if (!_private->coreFrame)
        return;
    FrameView* view = _private->coreFrame->view();
    if (!view)
        return;
    // FIXME: These are fake modifier keys here, but they should be real ones instead.
    PlatformMouseEvent event(IntPoint(windowLoc), globalPoint(windowLoc, [view->platformWidget() window]),
        LeftButton, MouseEventMoved, 0, false, false, false, false, currentTime());
    _private->coreFrame->eventHandler()->dragSourceEndedAt(event, (DragOperation)operation);
}

- (BOOL)_canProvideDocumentSource
{
    String mimeType = _private->coreFrame->loader()->responseMIMEType();
    
    if (WebCore::DOMImplementation::isTextMIMEType(mimeType) ||
        Image::supportsType(mimeType) ||
        (_private->coreFrame->page() && _private->coreFrame->page()->pluginData()->supportsMimeType(mimeType)))
        return NO;
    
    return YES;
}

- (BOOL)_canSaveAsWebArchive
{
    // Currently, all documents that we can view source for
    // (HTML and XML documents) can also be saved as web archives
    return [self _canProvideDocumentSource];
}

- (void)_receivedData:(NSData *)data textEncodingName:(NSString *)textEncodingName
{
    // Set the encoding. This only needs to be done once, but it's harmless to do it again later.
    String encoding = _private->coreFrame->loader()->documentLoader()->overrideEncoding();
    bool userChosen = !encoding.isNull();
    if (encoding.isNull())
        encoding = textEncodingName;
    _private->coreFrame->loader()->setEncoding(encoding, userChosen);
    [self _addData:data];
}

@end

@implementation WebFrame (WebPrivate)

// FIXME: This exists only as a convenience for Safari, consider moving there.
- (BOOL)_isDescendantOfFrame:(WebFrame *)ancestor
{
    Frame* coreFrame = _private->coreFrame;
    return coreFrame && coreFrame->tree()->isDescendantOf(core(ancestor));
}

- (void)_setShouldCreateRenderers:(BOOL)shouldCreateRenderers
{
    _private->shouldCreateRenderers = shouldCreateRenderers;
}

- (NSColor *)_bodyBackgroundColor
{
    Document* document = _private->coreFrame->document();
    if (!document)
        return nil;
    HTMLElement* body = document->body();
    if (!body)
        return nil;
    RenderObject* bodyRenderer = body->renderer();
    if (!bodyRenderer)
        return nil;
    Color color = bodyRenderer->style()->backgroundColor();
    if (!color.isValid())
        return nil;
    return nsColor(color);
}

- (BOOL)_isFrameSet
{
    return _private->coreFrame->isFrameSet();
}

- (BOOL)_firstLayoutDone
{
    return _private->coreFrame->loader()->firstLayoutDone();
}

- (WebFrameLoadType)_loadType
{
    return (WebFrameLoadType)_private->coreFrame->loader()->loadType();
}

- (NSRange)_selectedNSRange
{
    return [self _convertToNSRange:_private->coreFrame->selection()->toRange().get()];
}

- (void)_selectNSRange:(NSRange)range
{
    RefPtr<Range> domRange = [self _convertToDOMRange:range];
    if (domRange)
        _private->coreFrame->selection()->setSelection(Selection(domRange.get(), SEL_DEFAULT_AFFINITY));
}

- (BOOL)_isDisplayingStandaloneImage
{
    Document* document = _private->coreFrame->document();
    return document && document->isImageDocument();
}

- (unsigned)_pendingFrameUnloadEventCount
{
    return _private->coreFrame->eventHandler()->pendingFrameUnloadEventCount();
}

- (WebIconFetcher *)fetchApplicationIcon:(id)target
                                selector:(SEL)selector
{
    return [WebIconFetcher _fetchApplicationIconForFrame:self
                                                  target:target
                                                selector:selector];
}

- (void)_setIsDisconnected:(bool)isDisconnected
{
    _private->coreFrame->setIsDisconnected(isDisconnected);
}

- (void)_setExcludeFromTextSearch:(bool)exclude
{
    _private->coreFrame->setExcludeFromTextSearch(exclude);
}

#if ENABLE(NETSCAPE_PLUGIN_API)
- (void)_recursive_resumeNullEventsForAllNetscapePlugins
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        NSView <WebDocumentView> *documentView = [[kit(frame) frameView] documentView];
        if ([documentView isKindOfClass:[WebHTMLView class]])
            [(WebHTMLView *)documentView _resumeNullEventsForAllNetscapePlugins];
    }
}

- (void)_recursive_pauseNullEventsForAllNetscapePlugins
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        NSView <WebDocumentView> *documentView = [[kit(frame) frameView] documentView];
        if ([documentView isKindOfClass:[WebHTMLView class]])
            [(WebHTMLView *)documentView _pauseNullEventsForAllNetscapePlugins];
    }
}
#endif

@end

@implementation WebFrame

- (id)init
{
    return nil;
}

// Should be deprecated.
- (id)initWithName:(NSString *)name webFrameView:(WebFrameView *)view webView:(WebView *)webView
{
    return nil;
}

- (void)dealloc
{
    [_private release];
    --WebFrameCount;
    [super dealloc];
}

- (void)finalize
{
    --WebFrameCount;
    [super finalize];
}

- (NSString *)name
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return nil;
    return coreFrame->tree()->name();
}

- (WebFrameView *)frameView
{
    return _private->webFrameView;
}

- (WebView *)webView
{
    return getWebView(self);
}

- (DOMDocument *)DOMDocument
{
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
    return coreFrame ? dataSource(coreFrame->loader()->provisionalDocumentLoader()) : nil;
}

- (WebDataSource *)dataSource
{
    Frame* coreFrame = _private->coreFrame;
    return coreFrame && coreFrame->loader()->frameHasLoaded() ? [self _dataSource] : nil;
}

- (void)loadRequest:(NSURLRequest *)request
{
    _private->coreFrame->loader()->load(request);
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
    KURL responseURL;
    if (!baseURL) {
        baseURL = blankURL();
        responseURL = createUniqueWebDataURL();
    }
    
    ResourceRequest request([baseURL absoluteURL]);

    // hack because Mail checks for this property to detect data / archive loads
    [NSURLProtocol setProperty:@"" forKey:@"WebDataRequest" inRequest:(NSMutableURLRequest *)request.nsURLRequest()];

    SubstituteData substituteData(WebCore::SharedBuffer::wrapNSData(data), MIMEType, encodingName, [unreachableURL absoluteURL], responseURL);

    _private->coreFrame->loader()->load(request, substituteData);
}


- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)baseURL
{
    if (!MIMEType)
        MIMEType = @"text/html";
    [self _loadData:data MIMEType:MIMEType textEncodingName:encodingName baseURL:baseURL unreachableURL:nil];
}

- (void)_loadHTMLString:(NSString *)string baseURL:(NSURL *)baseURL unreachableURL:(NSURL *)unreachableURL
{
    NSData *data = [string dataUsingEncoding:NSUTF8StringEncoding];
    [self _loadData:data MIMEType:@"text/html" textEncodingName:@"UTF-8" baseURL:baseURL unreachableURL:unreachableURL];
}

- (void)loadHTMLString:(NSString *)string baseURL:(NSURL *)baseURL
{
    [self _loadHTMLString:string baseURL:baseURL unreachableURL:nil];
}

- (void)loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL
{
    [self _loadHTMLString:string baseURL:baseURL unreachableURL:unreachableURL];
}

- (void)loadArchive:(WebArchive *)archive
{
    if (LegacyWebArchive* coreArchive = [archive _coreLegacyWebArchive])
        _private->coreFrame->loader()->loadArchive(coreArchive);
}

- (void)stopLoading
{
    if (!_private->coreFrame)
        return;
    _private->coreFrame->loader()->stopForUserCancel();
}

- (void)reload
{
    _private->coreFrame->loader()->reload();
}

- (WebFrame *)findFrameNamed:(NSString *)name
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return nil;
    return kit(coreFrame->tree()->find(name));
}

- (WebFrame *)parentFrame
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return nil;
    return [[kit(coreFrame->tree()->parent()) retain] autorelease];
}

- (NSArray *)childFrames
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return [NSArray array];
    NSMutableArray *children = [NSMutableArray arrayWithCapacity:coreFrame->tree()->childCount()];
    for (Frame* child = coreFrame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        [children addObject:kit(child)];
    return children;
}

- (WebScriptObject *)windowObject
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return 0;
    return coreFrame->script()->windowScriptObject();
}

- (JSGlobalContextRef)globalContext
{
    Frame* coreFrame = _private->coreFrame;
    if (!coreFrame)
        return 0;
    return toGlobalRef(coreFrame->script()->globalObject()->globalExec());
}

@end
