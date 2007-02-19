/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "DOMDocumentInternal.h"
#import "DOMElementInternal.h"
#import "DOMHTMLElementInternal.h"
#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "WebBackForwardList.h"
#import "WebChromeClient.h"
#import "WebDataSourceInternal.h"
#import "WebDocumentInternal.h"
#import "WebDocumentLoaderMac.h"
#import "WebFrameBridge.h"
#import "WebFrameLoadDelegate.h"
#import "WebFrameLoaderClient.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLViewInternal.h"
#import "WebHistoryItem.h"
#import "WebHistoryItemInternal.h"
#import "WebHistoryItemPrivate.h"
#import "WebKitLogging.h"
#import "WebKitStatisticsPrivate.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebNetscapePluginEmbeddedView.h"
#import "WebNullPluginView.h"
#import "WebPlugin.h"
#import "WebPluginController.h"
#import "WebPreferencesPrivate.h"
#import "WebScriptDebugDelegatePrivate.h"
#import "WebViewInternal.h"
#import <WebCore/Chrome.h>
#import <WebCore/Document.h>
#import <WebCore/Event.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameTree.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLFrameOwnerElement.h>
#import <WebCore/Page.h>
#import <WebCore/SelectionController.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/FormState.h>
#import <WebCore/ResourceRequest.h>
#import <WebKit/DOMDocument.h>
#import <WebKit/DOMElement.h>
#import <WebKit/DOMHTMLElement.h>
#import <WebKit/DOMNode.h>
#import <WebKit/DOMRange.h>

using namespace WebCore;

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

using namespace WebCore;

NSString *WebPageCacheEntryDateKey = @"WebPageCacheEntryDateKey";
NSString *WebPageCacheDataSourceKey = @"WebPageCacheDataSourceKey";
NSString *WebPageCacheDocumentViewKey = @"WebPageCacheDocumentViewKey";

@interface WebFrame (ForwardDecls)
- (void)_loadHTMLString:(NSString *)string baseURL:(NSURL *)URL unreachableURL:(NSURL *)unreachableURL;
- (WebHistoryItem *)_createItem:(BOOL)useOriginal;
- (WebHistoryItem *)_createItemTreeWithTargetFrame:(WebFrame *)targetFrame clippedAtTarget:(BOOL)doClip;
@end

@interface NSView (WebFramePluginHosting)
- (void)setWebFrame:(WebFrame *)webFrame;
@end

@implementation WebFramePrivate

- (void)dealloc
{
    [webFrameView release];
    
    [scriptDebugger release];
    
    [inspectors release];

    [super dealloc];
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
    return [DOMCSSStyleDeclaration _CSSStyleDeclarationWith:declaration];
}

Element* core(DOMElement *element)
{
    return [element _element];
}

DOMElement *kit(Element* element)
{
    return [DOMElement _elementWith:element];
}

Node* core(DOMNode *node)
{
    return [node _node];
}

DOMNode *kit(Node* node)
{
    return [DOMNode _nodeWith:node];
}

Document* core(DOMDocument *document)
{
    return [document _document];
}

DOMDocument *kit(Document* document)
{
    return [DOMDocument _documentWith:document];
}

HTMLElement* core(DOMHTMLElement *element)
{
    return [element _HTMLElement];
}

DOMHTMLElement *kit(HTMLElement *element)
{
    return [DOMHTMLElement _HTMLElementWith:element];
}

Range* core(DOMRange *range)
{
    return [range _range];
}

DOMRange *kit(Range* range)
{
    return [DOMRange _rangeWith:range];
}

WebCore::EditableLinkBehavior core(WebKitEditableLinkBehavior editableLinkBehavior)
{
    return static_cast<WebCore::EditableLinkBehavior>(editableLinkBehavior);
}

WebKitEditableLinkBehavior kit(WebCore::EditableLinkBehavior editableLinkBehavior)
{
    return static_cast<WebKitEditableLinkBehavior>(editableLinkBehavior);
}

@implementation WebFrame (WebInternal)


static inline WebFrame *frame(WebCoreFrameBridge *bridge)
{
    return ((WebFrameBridge *)bridge)->_frame;
}

Frame* core(WebFrame *frame)
{
    if (!frame)
        return 0;
    
    if (!frame->_private->bridge)
        return 0;

    return frame->_private->bridge->m_frame;
}

WebFrame *kit(Frame* frame)
{
    return frame ? ((WebFrameBridge *)frame->bridge())->_frame : nil;
}

Page* core(WebView *webView)
{
    return [webView page];
}

WebView *kit(Page* page)
{
    WebChromeClient* chromeClient = static_cast<WebChromeClient*>(page->chrome()->client());
    return chromeClient->webView();
}

WebView *getWebView(WebFrame *webFrame)
{
    Frame* coreFrame = core(webFrame);
    if (!coreFrame)
        return nil;
    return kit(coreFrame->page());
}

/*
    In the case of saving state about a page with frames, we store a tree of items that mirrors the frame tree.  
    The item that was the target of the user's navigation is designated as the "targetItem".  
    When this method is called with doClip=YES we're able to create the whole tree except for the target's children, 
    which will be loaded in the future.  That part of the tree will be filled out as the child loads are committed.
*/
- (BOOL)_canCachePage
{
    return core(self)->page()->backForwardList()->usesPageCache() && core(self)->loader()->canCachePage();
}

+ (CFAbsoluteTime)_timeOfLastCompletedLoad
{
    return FrameLoader::timeOfLastCompletedLoad() - kCFAbsoluteTimeIntervalSince1970;
}

- (WebFrameBridge *)_bridge
{
    return _private->bridge;
}

- (void)_loadURL:(NSURL *)URL referrer:(NSString *)referrer intoChild:(WebFrame *)childFrame
{
    ASSERT(childFrame);
    HistoryItem* parentItem = core(self)->loader()->currentHistoryItem();
    FrameLoadType loadType = [self _frameLoader]->loadType();
    FrameLoadType childLoadType = FrameLoadTypeInternal;

    // If we're moving in the backforward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    // Reload will maintain the frame contents, LoadSame will not.
    if (parentItem && parentItem->children().size() &&
        (isBackForwardLoadType(loadType)
         || loadType == FrameLoadTypeReload
         || loadType == FrameLoadTypeReloadAllowingStaleData))
    {
        HistoryItem* childItem = parentItem->childItemWithName([childFrame name]);
        if (childItem) {
            // Use the original URL to ensure we get all the side-effects, such as
            // onLoad handlers, of any redirects that happened. An example of where
            // this is needed is Radar 3213556.
            URL = [NSURL _web_URLWithDataAsString:childItem->originalURLString()];
            // These behaviors implied by these loadTypes should apply to the child frames
            childLoadType = loadType;

            if (isBackForwardLoadType(loadType))
                // For back/forward, remember this item so we can traverse any child items as child frames load
                core(childFrame)->loader()->setProvisionalHistoryItem(childItem);
            else
                // For reload, just reinstall the current item, since a new child frame was created but we won't be creating a new BF item
                core(childFrame)->loader()->setCurrentHistoryItem(childItem);
        }
    }

    WebArchive *archive = [[self dataSource] _popSubframeArchiveWithName:[childFrame name]];
    if (archive)
        [childFrame loadArchive:archive];
    else
        [childFrame _frameLoader]->load(URL, referrer, childLoadType,
            String(), nil, 0, HashMap<String, String>());
}


- (void)_viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame))
        [[[kit(frame) frameView] documentView] viewWillMoveToHostWindow:hostWindow];
}

- (void)_viewDidMoveToHostWindow
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame))
        [[[kit(frame) frameView] documentView] viewDidMoveToHostWindow];
}

- (void)_addChild:(WebFrame *)child
{
    core(self)->tree()->appendChild(adoptRef(core(child)));
    if ([child dataSource])
        [[child dataSource] _documentLoader]->setOverrideEncoding([[self dataSource] _documentLoader]->overrideEncoding());  
}

- (int)_numPendingOrLoadingRequests:(BOOL)recurse
{
    return core(self)->loader()->numPendingOrLoadingRequests(recurse);
}

- (void)_reloadForPluginChanges
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        NSView <WebDocumentView> *documentView = [[kit(frame) frameView] documentView];
        if (([documentView isKindOfClass:[WebHTMLView class]] && coreFrame->loader()->containsPlugins()))
            [kit(frame) reload];
    }
}

- (void)_attachScriptDebugger
{
    if (!_private->scriptDebugger)
        _private->scriptDebugger = [[WebScriptDebugger alloc] initWithWebFrame:self];
}

- (void)_detachScriptDebugger
{
    if (_private->scriptDebugger) {
        id old = _private->scriptDebugger;
        _private->scriptDebugger = nil;
        [old release];
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

- (id)_initWithWebFrameView:(WebFrameView *)fv webView:(WebView *)v bridge:(WebFrameBridge *)bridge
{
    self = [super init];
    if (!self)
        return nil;

    _private = [[WebFramePrivate alloc] init];
    _private->bridge = bridge;

    if (fv) {
        [_private setWebFrameView:fv];
        [fv _setWebFrame:self];
    }

    ++WebFrameCount;

    return self;
}

- (NSArray *)_documentViews
{
    NSMutableArray *result = [NSMutableArray array];
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        id docView = [[kit(frame) frameView] documentView];
        if (docView)
            [result addObject:docView];
    }
    return result;
}

- (void)_updateBackground
{
    BOOL drawsBackground = [getWebView(self) drawsBackground];
    NSColor *backgroundColor = [getWebView(self) backgroundColor];

    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        WebFrameBridge *bridge = (WebFrameBridge *)frame->bridge();
        WebFrame *webFrame = [bridge webFrame];
        // Never call setDrawsBackground:YES here on the scroll view or the background color will
        // flash between pages loads. setDrawsBackground:YES will be called in _frameLoadCompleted.
        if (!drawsBackground)
            [[[webFrame frameView] _scrollView] setDrawsBackground:NO];
        [[[webFrame frameView] _scrollView] setBackgroundColor:backgroundColor];
        id documentView = [[webFrame frameView] documentView];
        if ([documentView respondsToSelector:@selector(setDrawsBackground:)])
            [documentView setDrawsBackground:drawsBackground];
        if ([documentView respondsToSelector:@selector(setBackgroundColor:)])
            [documentView setBackgroundColor:backgroundColor];
        [bridge setDrawsBackground:drawsBackground];
        [bridge setBaseBackgroundColor:backgroundColor];
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
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        Document *doc = frame->document();
        if (!doc)
            return;

        doc->removeMarkers(DocumentMarker::Grammar);
    }
}
#endif

- (void)_unmarkAllMisspellings
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        Document *doc = frame->document();
        if (!doc)
            return;

        doc->removeMarkers(DocumentMarker::Spelling);
    }
}

- (BOOL)_hasSelection
{
    id documentView = [_private->webFrameView documentView];    

    // optimization for common case to avoid creating potentially large selection string
    if ([documentView isKindOfClass:[WebHTMLView class]])
        if (Frame* coreFrame = core(self))
            return coreFrame->selectionController()->isRange();

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
    Frame* coreFrame = core(self);
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
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame))
        if ([kit(frame) _hasSelection])
            return kit(frame);
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

- (BOOL)_isMainFrame
{
   Frame* coreFrame = core(self);
   if (!coreFrame)
       return NO;
   return coreFrame == coreFrame->page()->mainFrame() ;
}

- (void)_addInspector:(WebInspector *)inspector
{
    if (!_private->inspectors)
        _private->inspectors = [[NSMutableSet alloc] init];
    ASSERT(![_private->inspectors containsObject:inspector]);
    [_private->inspectors addObject:inspector];
}

- (void)_removeInspector:(WebInspector *)inspector
{
    ASSERT([_private->inspectors containsObject:inspector]);
    [_private->inspectors removeObject:inspector];
}

- (FrameLoader*)_frameLoader
{
    Frame* frame = core(self);
    return frame ? frame->loader() : 0;
}

static inline WebDataSource *dataSource(DocumentLoader* loader)
{
    return loader ? static_cast<WebDocumentLoaderMac*>(loader)->dataSource() : nil;
}

- (WebDataSource *)_dataSourceForDocumentLoader:(DocumentLoader*)loader
{
    return dataSource(loader);
}

- (void)_addDocumentLoader:(DocumentLoader*)loader toUnarchiveState:(WebArchive *)archive
{
    [dataSource(loader) _addToUnarchiveState:archive];
}

@end

@implementation WebFrame (WebPrivate)

// FIXME: Yhis exists only as a convenience for Safari, consider moving there.
- (BOOL)_isDescendantOfFrame:(WebFrame *)ancestor
{
    Frame* coreFrame = core(self);
    return coreFrame && coreFrame->tree()->isDescendantOf(core(ancestor));
}

- (void)_setShouldCreateRenderers:(BOOL)frame
{
    [_private->bridge setShouldCreateRenderers:frame];
}

- (NSColor *)_bodyBackgroundColor
{
    Document* document = core(self)->document();
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
    return core(self)->isFrameSet();
}

- (BOOL)_firstLayoutDone
{
    return [self _frameLoader]->firstLayoutDone();
}

- (WebFrameLoadType)_loadType
{
    return (WebFrameLoadType)[self _frameLoader]->loadType();
}

- (void)_recursive_resumeNullEventsForAllNetscapePlugins
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        NSView <WebDocumentView> *documentView = [[kit(frame) frameView] documentView];
        if ([documentView isKindOfClass:[WebHTMLView class]])
            [(WebHTMLView *)documentView _resumeNullEventsForAllNetscapePlugins];
    }
}

- (NSRange)_selectedNSRange
{
    return [_private->bridge selectedNSRange];
}

- (void)_selectNSRange:(NSRange)range
{
    [_private->bridge selectNSRange:range];
}

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
    ASSERT(_private->bridge == nil);
    [_private release];
    --WebFrameCount;
    [super dealloc];
}

- (void)finalize
{
    ASSERT(_private->bridge == nil);
    --WebFrameCount;
    [super finalize];
}

- (NSString *)name
{
    Frame* coreFrame = core(self);
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
    Frame* coreFrame = core(self);
    if (!coreFrame)
        return nil;
    // FIXME: Why do we need this check?
    if (![[self dataSource] _isDocumentHTML])
        return nil;
    return kit(coreFrame->document());
}

- (DOMHTMLElement *)frameElement
{
    Frame* coreFrame = core(self);
    if (!coreFrame)
        return nil;
    return kit(coreFrame->ownerElement());
}

- (WebDataSource *)provisionalDataSource
{
    FrameLoader* frameLoader = [self _frameLoader];
    return frameLoader ? dataSource(frameLoader->provisionalDocumentLoader()) : nil;
}

- (WebDataSource *)dataSource
{
    FrameLoader* frameLoader = [self _frameLoader];
    return frameLoader ? dataSource(frameLoader->documentLoader()) : nil;
}

- (void)loadRequest:(NSURLRequest *)request
{
    [self _frameLoader]->load(request);
}

- (void)_loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)URL unreachableURL:(NSURL *)unreachableURL
{
    if (!URL)
        URL = [NSURL URLWithString:@""];
    ResourceRequest request(URL);

    // hack because Mail checks for this property to detect data / archive loads
    [NSURLProtocol setProperty:@"" forKey:@"WebDataRequest" inRequest:(NSMutableURLRequest *)request.nsURLRequest()];

    SubstituteData substituteData(WebCore::SharedBuffer::wrapNSData(data), MIMEType, encodingName, unreachableURL);

    [self _frameLoader]->load(request, substituteData);
}


- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)URL
{
    if (!MIMEType)
        MIMEType = @"text/html";
    [self _loadData:data MIMEType:MIMEType textEncodingName:encodingName baseURL:URL unreachableURL:nil];
}

- (void)_loadHTMLString:(NSString *)string baseURL:(NSURL *)URL unreachableURL:(NSURL *)unreachableURL
{
    NSData *data = [string dataUsingEncoding:NSUTF8StringEncoding];
    [self _loadData:data MIMEType:@"text/html" textEncodingName:@"UTF-8" baseURL:URL unreachableURL:unreachableURL];
}

- (void)loadHTMLString:(NSString *)string baseURL:(NSURL *)URL
{
    [self _loadHTMLString:string baseURL:URL unreachableURL:nil];
}

- (void)loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)URL forUnreachableURL:(NSURL *)unreachableURL
{
    [self _loadHTMLString:string baseURL:URL unreachableURL:unreachableURL];
}

- (void)loadArchive:(WebArchive *)archive
{
    WebResource *mainResource = [archive mainResource];
    if (mainResource) {
        SubstituteData substituteData(WebCore::SharedBuffer::wrapNSData([mainResource data]), [mainResource MIMEType], [mainResource textEncodingName], KURL());
        ResourceRequest request([mainResource URL]);

        // hack because Mail checks for this property to detect data / archive loads
        [NSURLProtocol setProperty:@"" forKey:@"WebDataRequest" inRequest:(NSMutableURLRequest *)request.nsURLRequest()];

        RefPtr<DocumentLoader> documentLoader = core(self)->loader()->client()->createDocumentLoader(request, substituteData);

        [dataSource(documentLoader.get()) _addToUnarchiveState:archive];

        [self _frameLoader]->load(documentLoader.get());
    }
}

- (void)stopLoading
{
    [self _frameLoader]->stopAllLoaders();
}

- (void)reload
{
    [self _frameLoader]->reload();
}

- (WebFrame *)findFrameNamed:(NSString *)name
{
    Frame* coreFrame = core(self);
    if (!coreFrame)
        return nil;
    return kit(coreFrame->tree()->find(name));
}

- (WebFrame *)parentFrame
{
    Frame* coreFrame = core(self);
    if (!coreFrame)
        return nil;
    return [[kit(coreFrame->tree()->parent()) retain] autorelease];
}

- (NSArray *)childFrames
{
    Frame* coreFrame = core(self);
    if (!coreFrame)
        return [NSArray array];
    NSMutableArray *children = [NSMutableArray arrayWithCapacity:coreFrame->tree()->childCount()];
    for (Frame* child = coreFrame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        [children addObject:kit(child)];
    return children;
}

@end
