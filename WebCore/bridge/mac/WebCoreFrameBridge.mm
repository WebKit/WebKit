/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2005, 2006 Alexey Proskuryakov (ap@nypop.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "WebCoreFrameBridge.h"

#import "Cache.h"
#import "DOMInternal.h"
#import "DocumentTypeImpl.h"
#import "FrameView.h"
#import "HTMLFormElementImpl.h"
#import "HTMLInputElementImpl.h"
#import "KWQAccObjectCache.h"
#import "KWQCharsets.h"
#import "KWQClipboard.h"
#import "KWQEditCommand.h"
#import "KWQFont.h"
#import "KWQFoundationExtras.h"
#import "KWQFrame.h"
#import "KWQLoader.h"
#import "KWQPageState.h"
#import "KWQPrinter.h"
#import "KWQRenderTreeDebug.h"
#import "KWQTextCodec.h"
#import "KWQView.h"
#import "MacFrame.h"
#import "FrameTreeNode.h"
#import "NodeImpl.h"
#import "SelectionController.h"
#import "WebCoreFrameNamespaces.h"
#import "WebCoreImageRenderer.h"
#import "WebCoreSettings.h"
#import "WebCoreTextRendererFactory.h"
#import "WebCoreViewFactory.h"
#import "csshelper.h"
#import "delete_selection_command.h"
#import "dom2_eventsimpl.h"
#import "dom2_range.h"
#import "dom2_rangeimpl.h"
#import "dom2_viewsimpl.h"
#import "dom_node.h"
#import "dom_position.h"
#import "html_documentimpl.h"
#import "html_imageimpl.h"
#import "htmlediting.h"
#import "kjs_proxy.h"
#import "kjs_window.h"
#import "loader.h"
#import "markup.h"
#import "move_selection_command.h"
#import "render_canvas.h"
#import "render_frames.h"
#import "render_image.h"
#import "render_replaced.h"
#import "render_style.h"
#import "replace_selection_command.h"
#import "typing_command.h"
#import "visible_position.h"
#import "visible_text.h"
#import "visible_units.h"
#import "xml_tokenizer.h"
#import "htmlnames.h"
#import <JavaScriptCore/interpreter.h>
#import <JavaScriptCore/jni_jsobject.h>
#import <JavaScriptCore/npruntime.h>
#import <JavaScriptCore/object.h>
#import <JavaScriptCore/property_map.h>
#import <JavaScriptCore/runtime_root.h>
#import <kxmlcore/Assertions.h>

@class NSView;

using namespace WebCore;
using namespace HTMLNames;

using KJS::ExecState;
using KJS::Interpreter;
using KJS::JSLock;
using KJS::JSObject;
using KJS::JSValue;
using KJS::SavedProperties;
using KJS::SavedBuiltins;
using KJS::Window;

using KJS::Bindings::RootObject;

NSString *WebCoreElementDOMNodeKey =            @"WebElementDOMNode";
NSString *WebCoreElementFrameKey =              @"WebElementFrame";
NSString *WebCoreElementImageAltStringKey =     @"WebElementImageAltString";
NSString *WebCoreElementImageRendererKey =      @"WebCoreElementImageRenderer";
NSString *WebCoreElementImageRectKey =          @"WebElementImageRect";
NSString *WebCoreElementImageURLKey =           @"WebElementImageURL";
NSString *WebCoreElementIsSelectedKey =         @"WebElementIsSelected";
NSString *WebCoreElementLinkURLKey =            @"WebElementLinkURL";
NSString *WebCoreElementLinkTargetFrameKey =    @"WebElementTargetFrame";
NSString *WebCoreElementLinkLabelKey =          @"WebElementLinkLabel";
NSString *WebCoreElementLinkTitleKey =          @"WebElementLinkTitle";
NSString *WebCoreElementNameKey =               @"WebElementName";
NSString *WebCoreElementTitleKey =              @"WebCoreElementTitle"; // not in WebKit API for now, could be in API some day

NSString *WebCorePageCacheStateKey =            @"WebCorePageCacheState";

@interface WebCoreFrameBridge (WebCoreBridgeInternal)
- (RootObject *)executionContextForView:(NSView *)aView;
@end

static RootObject *rootForView(void *v)
{
    NSView *aView = (NSView *)v;
    WebCoreFrameBridge *aBridge = [[WebCoreViewFactory sharedFactory] bridgeForView:aView];
    RootObject *root = 0;

    if (aBridge)
        root = [aBridge executionContextForView:aView];

    return root;
}

static pthread_t mainThread = 0;

static void updateRenderingForBindings (ExecState *exec, JSObject *rootObject)
{
    if (pthread_self() != mainThread)
        return;
        
    if (!rootObject)
        return;
        
    Window *window = static_cast<Window*>(rootObject);
    if (!window)
        return;
        
    DocumentImpl *doc = static_cast<DocumentImpl*>(window->frame()->document());
    if (doc)
        doc->updateRendering();
}

static BOOL frameHasSelection(WebCoreFrameBridge *bridge)
{
    if (!bridge)
        return NO;
    
    Frame *frame = [bridge part];
    if (!frame)
        return NO;
        
    if (frame->selection().isNone())
        return NO;

    // If a part has a selection, it should also have a document.        
    ASSERT(frame->document());

    return YES;
}

static BOOL hasCaseInsensitivePrefix(NSString *string, NSString *prefix)
{
    return [string rangeOfString:prefix options:(NSCaseInsensitiveSearch | NSAnchoredSearch)].location !=
        NSNotFound;
}

static BOOL isCaseSensitiveEqual(NSString *a, NSString *b)
{
    return [a caseInsensitiveCompare:b] == NSOrderedSame;
}

@implementation WebCoreFrameBridge

static bool initializedObjectCacheSize = false;
static bool initializedKJS = false;

static inline WebCoreFrameBridge *bridge(Frame *frame)
{
    if (!frame)
        return nil;
    return Mac(frame)->bridge();
}

- (WebCoreFrameBridge *)firstChild
{
    return bridge(m_frame->treeNode()->firstChild());
}

- (WebCoreFrameBridge *)lastChild
{
    return bridge(m_frame->treeNode()->lastChild());
}

- (unsigned)childCount
{
    return m_frame->treeNode()->childCount();
}

- (WebCoreFrameBridge *)previousSibling;
{
    return bridge(m_frame->treeNode()->previousSibling());
}

- (WebCoreFrameBridge *)nextSibling;
{
    return bridge(m_frame->treeNode()->nextSibling());
}

- (BOOL)isDescendantOfFrame:(WebCoreFrameBridge *)ancestor
{
    for (WebCoreFrameBridge *frame = self; frame; frame = [frame parent])
        if (frame == ancestor)
            return YES;

    return NO;
}

- (WebCoreFrameBridge *)traverseNextFrameStayWithin:(WebCoreFrameBridge *)stayWithin
{
    WebCoreFrameBridge *firstChild = [self firstChild];
    if (firstChild) {
        ASSERT(!stayWithin || [firstChild isDescendantOfFrame:stayWithin]);
        return firstChild;
    }

    if (self == stayWithin)
        return 0;

    WebCoreFrameBridge *nextSibling = [self nextSibling];
    if (nextSibling) {
        assert(!stayWithin || [nextSibling isDescendantOfFrame:stayWithin]);
        return nextSibling;
    }

    WebCoreFrameBridge *frame = self;
    while (frame && !nextSibling && (!stayWithin || [frame parent] != stayWithin)) {
        frame = (WebCoreFrameBridge *)[frame parent];
        nextSibling = [frame nextSibling];
    }

    if (frame) {
        ASSERT(!stayWithin || !nextSibling || [nextSibling isDescendantOfFrame:stayWithin]);
        return nextSibling;
    }

    return nil;
}

- (void)appendChild:(WebCoreFrameBridge *)child
{
    m_frame->treeNode()->appendChild(adoptRef([child part]));
}

- (void)_clearRenderPart
{
    if (_renderPart)
        _renderPart->deref(_renderPartArena);
    _renderPart = 0;
}

- (void)removeChild:(WebCoreFrameBridge *)child
{
    [child _clearRenderPart];
    m_frame->treeNode()->removeChild([child part]);
}

- (WebCoreFrameBridge *)childFrameNamed:(NSString *)name
{
    // FIXME: with a better data structure this could be O(1) instead of O(n) in number 
    // of child frames
    for (WebCoreFrameBridge *child = [self firstChild]; child; child = [child nextSibling])
        if ([[child name] isEqualToString:name])
            return child;

    return nil;
}

// Returns the last child of us and any children, or self
- (WebCoreFrameBridge *)_deepLastChildFrame
{
    WebCoreFrameBridge *result = self;
    for (WebCoreFrameBridge *lastChild = [self lastChild]; lastChild; lastChild = [lastChild lastChild])
        result = lastChild;

    return result;
}

// Return next frame to be traversed, visiting children after parent
- (WebCoreFrameBridge *)nextFrameWithWrap:(BOOL)wrapFlag
{
    WebCoreFrameBridge *result = [self traverseNextFrameStayWithin:nil];

    if (!result && wrapFlag)
        return [self mainFrame];

    return result;
}

// Return previous frame to be traversed, exact reverse order of _nextFrame
- (WebCoreFrameBridge *)previousFrameWithWrap:(BOOL)wrapFlag
{
    // FIXME: besides the wrap feature, this is just the traversePreviousNode algorithm

    WebCoreFrameBridge *prevSibling = [self previousSibling];
    if (prevSibling)
        return [prevSibling _deepLastChildFrame];
    if ([self parent])
        return [self parent];
    
    // no siblings, no parent, self==top
    if (wrapFlag)
        return [self _deepLastChildFrame];

    // top view is always the last one in this ordering, so prev is nil without wrap
    return nil;
}

- (void)setFrameNamespace:(NSString *)ns
{
    ASSERT(self == [self mainFrame]);

    if (ns != _frameNamespace){
        [WebCoreFrameNamespaces removeFrame:self fromNamespace:_frameNamespace];
        ns = [ns copy];
        [_frameNamespace release];
        _frameNamespace = ns;
        [WebCoreFrameNamespaces addFrame:self toNamespace:_frameNamespace];
    }
}

- (NSString *)frameNamespace
{
    ASSERT(self == [self mainFrame]);
    return _frameNamespace;
}

- (BOOL)_shouldAllowAccessFrom:(WebCoreFrameBridge *)source
{
    // if no source frame, allow access
    if (source == nil)
        return YES;

    //   - allow access if the two frames are in the same window
    if ([self mainFrame] == [source mainFrame])
        return YES;

    //   - allow if the request is made from a local file.
    NSString *sourceDomain = [self domain];
    if ([sourceDomain length] == 0)
        return YES;

    //   - allow access if this frame or one of its ancestors
    //     has the same origin as source
    for (WebCoreFrameBridge *ancestor = self; ancestor; ancestor = [ancestor parent]) {
        NSString *ancestorDomain = [ancestor domain];
        if (ancestorDomain != nil && 
            isCaseSensitiveEqual(sourceDomain, ancestorDomain))
            return YES;
        
        ancestor = [ancestor parent];
    }

    //   - allow access if this frame is a toplevel window and the source
    //     can access its opener. Note that we only allow one level of
    //     recursion here.
    if ([self parent] == nil) {
        NSString *openerDomain = [[self opener] domain];
        if (openerDomain != nil && isCaseSensitiveEqual(sourceDomain, openerDomain))
            return YES;
    }
    
    // otherwise deny access
    return NO;
}

- (WebCoreFrameBridge *)_descendantFrameNamed:(NSString *)name sourceFrame:(WebCoreFrameBridge *)source
{
    for (WebCoreFrameBridge *frame = self; frame; frame = [frame traverseNextFrameStayWithin:self])
        // for security reasons, we do not want to even make frames visible to frames that
        // can't access them 
        if ([[frame name] isEqualToString:name] && [frame _shouldAllowAccessFrom:source])
            return frame;

    return nil;
}

- (WebCoreFrameBridge *)_frameInAnyWindowNamed:(NSString *)name sourceFrame:(WebCoreFrameBridge *)source
{
    ASSERT(self == [self mainFrame]);

    // Try this WebView first.
    WebCoreFrameBridge *frame = [self _descendantFrameNamed:name sourceFrame:source];

    if (frame != nil)
        return frame;

    // Try other WebViews in the same set

    if ([self frameNamespace] != nil) {
        NSEnumerator *enumerator = [WebCoreFrameNamespaces framesInNamespace:[self frameNamespace]];
        WebCoreFrameBridge *searchFrame;
        while ((searchFrame = [enumerator nextObject]))
            frame = [searchFrame _descendantFrameNamed:name sourceFrame:source];
    }

    return frame;
}

- (WebCoreFrameBridge *)findFrameNamed:(NSString *)name
{
    // First, deal with 'special' names.
    if ([name isEqualToString:@"_self"] || [name isEqualToString:@"_current"])
        return self;
    
    if ([name isEqualToString:@"_top"])
        return [self mainFrame];
    
    if ([name isEqualToString:@"_parent"]) {
        WebCoreFrameBridge *parent = [self parent];
        return parent ? parent : self;
    }
    
    if ([name isEqualToString:@"_blank"])
        return nil;

    // Search from this frame down.
    WebCoreFrameBridge *frame = [self _descendantFrameNamed:name sourceFrame:self];

    // Search in the main frame for this window then in others.
    if (!frame)
        frame = [[self mainFrame] _frameInAnyWindowNamed:name sourceFrame:self];

    return frame;
}

+ (NSArray *)supportedMIMETypes
{
    return [NSArray arrayWithObjects:
        @"text/html",
        @"text/xml",
        @"text/xsl",
        @"application/xml",
        @"application/xhtml+xml",
        @"application/rss+xml",
        @"application/atom+xml",
        @"application/x-webarchive",
        @"multipart/x-mixed-replace",
#if SVG_SUPPORT
        @"image/svg+xml",
#endif
        nil];
}

+ (WebCoreFrameBridge *)bridgeForDOMDocument:(DOMDocument *)document
{
    Frame *frame = [document _documentImpl]->frame();
    return frame ? Mac(frame)->bridge() : nil;
}

- (id)init
{
    if (!(self = [super init]))
        return nil;
    
    m_frame = new MacFrame;
    m_frame->setBridge(self);

    if (!initializedObjectCacheSize){
        Cache::setSize([self getObjectCacheSize]);
        initializedObjectCacheSize = true;
    }
    
    if (!initializedKJS) {
        mainThread = pthread_self();
        
        RootObject::setFindRootObjectForNativeHandleFunction (rootForView);

        KJS::Bindings::Instance::setDidExecuteFunction(updateRenderingForBindings);
        
        initializedKJS = true;
    }
    
    _shouldCreateRenderers = YES;
    
    return self;
}

- (void)initializeSettings: (WebCoreSettings *)settings
{
    m_frame->setSettings ([settings settings]);
}

- (void)dealloc
{
    [self removeFromFrame];
    
    if (_renderPart)
        _renderPart->deref(_renderPartArena);
        
    [super dealloc];
}

- (void)finalize
{
    // FIXME: This work really should not be done at deallocation time.
    // We need to do it at some well-defined time instead.

    [self removeFromFrame];
    
    if (_renderPart) {
        _renderPart->deref(_renderPartArena);
    }
    m_frame->setBridge(nil);
        
    [super finalize];
}

- (MacFrame *)part
{
    return m_frame;
}

- (void)setRenderPart:(RenderPart *)newPart;
{
    RenderArena *arena = newPart->ref();
    if (_renderPart) {
        _renderPart->deref(_renderPartArena);
    }
    _renderPart = newPart;
    _renderPartArena = arena;
}

- (RenderPart *)renderPart
{
    return _renderPart;
}

- (void)setParent:(WebCoreFrameBridge *)parent
{
    // FIXME: frames should be created with the right parent in the first place
    m_frame->treeNode()->setParent([parent part]);
}

- (WebCoreFrameBridge *)parent
{
    MacFrame *parentFrame = Mac(m_frame->treeNode()->parent());
    if (!parentFrame)
        return nil;
    return parentFrame->bridge();
}

- (void)provisionalLoadStarted
{
    m_frame->provisionalLoadStarted();
}

- (void)openURL:(NSURL *)URL reload:(BOOL)reload contentType:(NSString *)contentType refresh:(NSString *)refresh lastModified:(NSDate *)lastModified pageCache:(NSDictionary *)pageCache
{
    if (pageCache) {
        KWQPageState *state = [pageCache objectForKey:WebCorePageCacheStateKey];
        m_frame->openURLFromPageCache(state);
        [state invalidate];
        return;
    }
        
    // arguments
    URLArgs args(m_frame->browserExtension()->urlArgs());
    args.reload = reload;
    if (contentType) {
        args.serviceType = QString::fromNSString(contentType);
    }
    m_frame->browserExtension()->setURLArgs(args);

    // opening the URL
    if (m_frame->didOpenURL(URL)) {
        // things we have to set up after calling didOpenURL
        if (refresh) {
            m_frame->addMetaData("http-refresh", QString::fromNSString(refresh));
        }
        if (lastModified) {
            NSString *modifiedString = [lastModified descriptionWithCalendarFormat:@"%a %b %d %Y %H:%M:%S" timeZone:nil locale:nil];
            m_frame->addMetaData("modified", QString::fromNSString(modifiedString));
        }
    }
}

- (void)setEncoding:(NSString *)encoding userChosen:(BOOL)userChosen
{
    m_frame->setEncoding(QString::fromNSString(encoding), userChosen);
}

- (void)addData:(NSData *)data
{
    DocumentImpl *doc = m_frame->document();
    
    // Document may be nil if the part is about to redirect
    // as a result of JS executing during load, i.e. one frame
    // changing another's location before the frame's document
    // has been created. 
    if (doc){
        doc->setShouldCreateRenderers([self shouldCreateRenderers]);
        m_frame->addData((const char *)[data bytes], [data length]);
    }
}

- (void)closeURL
{
    m_frame->closeURL();
}

- (void)stopLoading
{
    m_frame->stopLoading();
}

- (void)didNotOpenURL:(NSURL *)URL pageCache:(NSDictionary *)pageCache
{
    m_frame->didNotOpenURL(KURL(URL).url());

    // We might have made a page cache item, but now we're bailing out due to an error before we ever
    // transitioned to the new page (before WebFrameState==commit).  The goal here is to restore any state
    // so that the existing view (that wenever got far enough to replace) can continue being used.
    DocumentImpl *doc = m_frame->document();
    if (doc) {
        doc->setInPageCache(NO);
    }
    KWQPageState *state = [pageCache objectForKey:WebCorePageCacheStateKey];
    [state invalidate];
}

- (BOOL)canLoadURL:(NSURL *)URL fromReferrer:(NSString *)referrer hideReferrer:(BOOL *)hideReferrer
{
    BOOL referrerIsWebURL = hasCaseInsensitivePrefix(referrer, @"http:") || hasCaseInsensitivePrefix(referrer, @"https:");
    BOOL referrerIsLocalURL = hasCaseInsensitivePrefix(referrer, @"file:") || hasCaseInsensitivePrefix(referrer, @"applewebdata:");
    BOOL URLIsFileURL = [URL scheme] != NULL && [[URL scheme] compare:@"file" options:(NSCaseInsensitiveSearch|NSLiteralSearch)] == NSOrderedSame;
    
    *hideReferrer = !referrerIsWebURL;
    return !URLIsFileURL || referrerIsLocalURL;
}

- (void)saveDocumentState
{
    DocumentImpl *doc = m_frame->document();
    if (doc != 0){
        QStringList list = doc->docState();
        NSMutableArray *documentState = [[[NSMutableArray alloc] init] autorelease];
        
        for (uint i = 0; i < list.count(); i++){
            QString s = list[i];
            [documentState addObject: [NSString stringWithCharacters: (const unichar *)s.unicode() length: s.length()]];
        }
        [self saveDocumentState: documentState];
    }
}

- (void)restoreDocumentState
{
    DocumentImpl *doc = m_frame->document();
    
    if (doc != 0){
        NSArray *documentState = [self documentState];
        
        QStringList s;
        for (uint i = 0; i < [documentState count]; i++){
            NSString *string = [documentState objectAtIndex: i];
            s.append(QString::fromNSString(string));
        }
            
        doc->setRestoreState(s);
    }
}

- (void)scrollToAnchorWithURL:(NSURL *)URL
{
    m_frame->scrollToAnchor(KURL(URL).url().latin1());
}

- (BOOL)scrollOverflowInDirection:(WebScrollDirection)direction granularity:(WebScrollGranularity)granularity
{
    if (m_frame == NULL) {
        return NO;
    }
    return m_frame->scrollOverflow((KWQScrollDirection)direction, (KWQScrollGranularity)granularity);
}

- (BOOL)sendScrollWheelEvent:(NSEvent *)event
{
    return m_frame ? m_frame->wheelEvent(event) : NO;
}

- (BOOL)saveDocumentToPageCache
{
    DocumentImpl *doc = m_frame->document();
    if (!doc)
        return NO;
    if (!doc->view())
        return NO;

    m_frame->clearTimers();

    JSLock lock;

    SavedProperties *windowProperties = new SavedProperties;
    m_frame->saveWindowProperties(windowProperties);

    SavedProperties *locationProperties = new SavedProperties;
    m_frame->saveLocationProperties(locationProperties);
    
    SavedBuiltins *interpreterBuiltins = new SavedBuiltins;
    m_frame->saveInterpreterBuiltins(*interpreterBuiltins);

    KWQPageState *pageState = [[KWQPageState alloc] initWithDocument:doc
                                                                 URL:m_frame->url()
                                                    windowProperties:windowProperties
                                                  locationProperties:locationProperties
                                                 interpreterBuiltins:interpreterBuiltins
                                                      pausedTimeouts:m_frame->pauseTimeouts()];

    BOOL result = [self saveDocumentToPageCache:pageState];

    [pageState release];

    return result;
}

- (BOOL)canCachePage
{
    return m_frame->canCachePage();
}

- (void)clear
{
    m_frame->clear();
}

- (void)end
{
    m_frame->end();
}

- (void)stop
{
    m_frame->stop();
}

- (void)clearFrame
{
    m_frame = 0;
}

- (void)handleFallbackContent
{
    // this needs to be callable even after teardown of the frame
    if (!m_frame)
        return;
    m_frame->handleFallbackContent();
}

- (void)createFrameViewWithNSView:(NSView *)view marginWidth:(int)mw marginHeight:(int)mh
{
    // If we own the view, delete the old one - otherwise the render m_frame will take care of deleting the view.
    [self removeFromFrame];

    FrameView *kview = new FrameView(m_frame, 0);
    m_frame->setView(kview);
    kview->deref();

    kview->setView(view);
    if (mw >= 0)
        kview->setMarginWidth(mw);
    if (mh >= 0)
        kview->setMarginHeight(mh);
}

- (void)scrollToAnchor:(NSString *)a
{
    m_frame->gotoAnchor(QString::fromNSString(a));
}

- (BOOL)isSelectionEditable
{
    // EDIT FIXME: This needs to consider the entire selected range
    NodeImpl *startNode = m_frame->selection().start().node();
    return startNode ? startNode->isContentEditable() : NO;
}

- (WebSelectionState)selectionState
{
    switch (m_frame->selection().state()) {
        case WebCore::Selection::NONE:
            return WebSelectionStateNone;
        case WebCore::Selection::CARET:
            return WebSelectionStateCaret;
        case WebCore::Selection::RANGE:
            return WebSelectionStateRange;
    }
    
    ASSERT_NOT_REACHED();
    return WebSelectionStateNone;
}

- (NSString *)_documentTypeString
{
    NSString *documentTypeString = nil;
    DocumentImpl *doc = m_frame->document();
    if (doc) {
        DocumentTypeImpl *doctype = doc->realDocType();
        if (doctype) {
            documentTypeString = doctype->toString().qstring().getNSString();
        }
    }
    return documentTypeString;
}

- (NSString *)_stringWithDocumentTypeStringAndMarkupString:(NSString *)markupString
{
    NSString *documentTypeString = [self _documentTypeString];
    if (documentTypeString && markupString) {
        return [NSString stringWithFormat:@"%@%@", documentTypeString, markupString];
    } else if (documentTypeString) {
        return documentTypeString;
    } else if (markupString) {
        return markupString;
    } else {
        return @"";
    }
}

- (NSArray *)nodesFromList:(QPtrList<NodeImpl> *)nodeList
{
    NSMutableArray *nodes = [NSMutableArray arrayWithCapacity:nodeList->count()];
    for (QPtrListIterator<NodeImpl> i(*nodeList); i.current(); ++i) {
        [nodes addObject:[DOMNode _nodeWithImpl:i.current()]];
    }
    return nodes;
}

- (NSString *)markupStringFromNode:(DOMNode *)node nodes:(NSArray **)nodes
{
    // FIXME: This is never "for interchange". Is that right? See the next method.
    QPtrList<NodeImpl> nodeList;
    NSString *markupString = createMarkup([node _nodeImpl], IncludeNode, nodes ? &nodeList : 0).getNSString();
    if (nodes) {
        *nodes = [self nodesFromList:&nodeList];
    }
    return [self _stringWithDocumentTypeStringAndMarkupString:markupString];
}

- (NSString *)markupStringFromRange:(DOMRange *)range nodes:(NSArray **)nodes
{
    // FIXME: This is always "for interchange". Is that right? See the previous method.
    QPtrList<NodeImpl> nodeList;
    NSString *markupString = createMarkup([range _rangeImpl], nodes ? &nodeList : 0, AnnotateForInterchange).getNSString();
    if (nodes) {
        *nodes = [self nodesFromList:&nodeList];
    }
    return [self _stringWithDocumentTypeStringAndMarkupString:markupString];
}

- (NSString *)selectedString
{
    QString text = m_frame->selectedText();
    text.replace(QChar('\\'), m_frame->backslashAsCurrencySymbol());
    return [[text.getNSString() copy] autorelease];
}

- (NSString *)stringForRange:(DOMRange *)range
{
    QString text = plainText([range _rangeImpl]);
    text.replace(QChar('\\'), m_frame->backslashAsCurrencySymbol());
    return [[text.getNSString() copy] autorelease];
}

- (void)selectAll
{
    m_frame->selectAll();
}

- (void)deselectAll
{
    [self deselectText];
    DocumentImpl *doc = m_frame->document();
    if (doc) {
        doc->setFocusNode(0);
    }
}

- (void)deselectText
{
    // FIXME: 6498 Should just be able to call m_frame->selection().clear()
    m_frame->setSelection(SelectionController());
}

- (BOOL)isFrameSet
{
    return m_frame->isFrameSet();
}

- (void)reapplyStylesForDeviceType:(WebCoreDeviceType)deviceType
{
    m_frame->setMediaType(deviceType == WebCoreDeviceScreen ? "screen" : "print");
    DocumentImpl *doc = m_frame->document();
    if (doc)
        doc->setPrinting(deviceType == WebCoreDevicePrinter);
    return m_frame->reparseConfiguration();
}

static BOOL nowPrinting(WebCoreFrameBridge *self)
{
    DocumentImpl *doc = self->m_frame->document();
    return doc && doc->printing();
}

// Set or unset the printing mode in the view.  We only toy with this if we're printing.
- (void)_setupRootForPrinting:(BOOL)onOrOff
{
    if (nowPrinting(self)) {
        RenderCanvas *root = static_cast<RenderCanvas *>(m_frame->document()->renderer());
        if (root) {
            root->setPrintingMode(onOrOff);
        }
    }
}

- (void)forceLayoutAdjustingViewSize:(BOOL)flag
{
    [self _setupRootForPrinting:YES];
    m_frame->forceLayout();
    if (flag) {
        [self adjustViewSize];
    }
    [self _setupRootForPrinting:NO];
}

- (void)forceLayoutWithMinimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustingViewSize:(BOOL)flag
{
    [self _setupRootForPrinting:YES];
    m_frame->forceLayoutWithPageWidthRange(minPageWidth, maxPageWidth);
    if (flag) {
        [self adjustViewSize];
    }
    [self _setupRootForPrinting:NO];
}

- (void)sendResizeEvent
{
    m_frame->sendResizeEvent();
}

- (void)sendScrollEvent
{
    m_frame->sendScrollEvent();
}

- (void)drawRect:(NSRect)rect withPainter:(QPainter *)p
{
    [self _setupRootForPrinting:YES];
    m_frame->paint(p, enclosingIntRect(rect));
    [self _setupRootForPrinting:NO];
}

- (void)drawRect:(NSRect)rect
{
    QPainter painter(nowPrinting(self));
    bool displaysWithFocusAttributes = m_frame->displaysWithFocusAttributes();
    painter.setUsesInactiveTextBackgroundColor(!displaysWithFocusAttributes);
    [self drawRect:rect withPainter:&painter];
}

// Used by pagination code called from AppKit when a standalone web page is printed.
- (NSArray*)computePageRectsWithPrintWidthScaleFactor:(float)printWidthScaleFactor printHeight:(float)printHeight
{
    [self _setupRootForPrinting:YES];
    NSMutableArray* pages = [NSMutableArray arrayWithCapacity:5];
    if (printWidthScaleFactor <= 0) {
        ERROR("printWidthScaleFactor has bad value %.2f", printWidthScaleFactor);
        return pages;
    }
    
    if (printHeight <= 0) {
        ERROR("printHeight has bad value %.2f", printHeight);
        return pages;
    }

    if (!m_frame || !m_frame->document() || !m_frame->view()) return pages;
    RenderCanvas* root = static_cast<RenderCanvas *>(m_frame->document()->renderer());
    if (!root) return pages;
    
    FrameView* view = m_frame->view();
    NSView* documentView = view->getDocumentView();
    if (!documentView)
        return pages;

    float currPageHeight = printHeight;
    float docHeight = root->layer()->height();
    float docWidth = root->layer()->width();
    float printWidth = docWidth/printWidthScaleFactor;
    
    // We need to give the part the opportunity to adjust the page height at each step.
    for (float i = 0; i < docHeight; i += currPageHeight) {
        float proposedBottom = kMin(docHeight, i + printHeight);
        m_frame->adjustPageHeight(&proposedBottom, i, proposedBottom, i);
        currPageHeight = kMax(1.0f, proposedBottom - i);
        for (float j = 0; j < docWidth; j += printWidth) {
            NSValue* val = [NSValue valueWithRect: NSMakeRect(j, i, printWidth, currPageHeight)];
            [pages addObject: val];
        }
    }
    [self _setupRootForPrinting:NO];
    
    return pages;
}

// This is to support the case where a webview is embedded in the view that's being printed
- (void)adjustPageHeightNew:(float *)newBottom top:(float)oldTop bottom:(float)oldBottom limit:(float)bottomLimit
{
    [self _setupRootForPrinting:YES];
    m_frame->adjustPageHeight(newBottom, oldTop, oldBottom, bottomLimit);
    [self _setupRootForPrinting:NO];
}

- (NSObject *)copyRenderNode:(RenderObject *)node copier:(id <WebCoreRenderTreeCopier>)copier
{
    NSMutableArray *children = [[NSMutableArray alloc] init];
    for (RenderObject *child = node->firstChild(); child; child = child->nextSibling()) {
        [children addObject:[self copyRenderNode:child copier:copier]];
    }
          
    NSString *name = [[NSString alloc] initWithUTF8String:node->renderName()];
    
    RenderWidget *renderWidget = node->isWidget() ? static_cast<RenderWidget *>(node) : 0;
    QWidget *widget = renderWidget ? renderWidget->widget() : 0;
    NSView *view = widget ? widget->getView() : nil;
    
    int nx, ny;
    node->absolutePosition(nx,ny);
    NSObject *copiedNode = [copier nodeWithName:name
                                       position:NSMakePoint(nx,ny)
                                           rect:NSMakeRect(node->xPos(), node->yPos(), node->width(), node->height())
                                           view:view
                                       children:children];
    
    [name release];
    [children release];
    
    return copiedNode;
}

- (NSObject *)copyRenderTree:(id <WebCoreRenderTreeCopier>)copier
{
    RenderObject *renderer = m_frame->renderer();
    if (!renderer) {
        return nil;
    }
    return [self copyRenderNode:renderer copier:copier];
}

- (void)removeFromFrame
{
    if (m_frame)
        m_frame->setView(0);
}

- (void)installInFrame:(NSView *)view
{
    // If this isn't the main frame, it must have a render m_frame set, or it
    // won't ever get installed in the view hierarchy.
    ASSERT(self == [self mainFrame] || _renderPart != nil);

    m_frame->view()->setView(view);
    if (_renderPart) {
        _renderPart->setWidget(m_frame->view());
        // Now the render part owns the view, so we don't any more.
    }

    m_frame->view()->initScrollBars();
}

- (void)setActivationEventNumber:(int)num
{
    m_frame->setActivationEventNumber(num);
}

- (void)mouseDown:(NSEvent *)event
{
    m_frame->mouseDown(event);
}

- (void)mouseDragged:(NSEvent *)event
{
    m_frame->mouseDragged(event);
}

- (void)mouseUp:(NSEvent *)event
{
    m_frame->mouseUp(event);
}

- (void)mouseMoved:(NSEvent *)event
{
    m_frame->mouseMoved(event);
}

- (BOOL)sendContextMenuEvent:(NSEvent *)event
{
    return m_frame->sendContextMenuEvent(event);
}

- (DOMElement *)elementForView:(NSView *)view
{
    // FIXME: implemented currently for only a subset of the KWQ widgets
    if ([view conformsToProtocol:@protocol(KWQWidgetHolder)]) {
        NSView <KWQWidgetHolder> *widgetHolder = view;
        QWidget *widget = [widgetHolder widget];
        if (widget != nil && widget->eventFilterObject() != nil) {
            NodeImpl *node = static_cast<const RenderWidget *>(widget->eventFilterObject())->element();
            return [DOMElement _elementWithImpl:static_cast<ElementImpl *>(node)];
        }
    }
    return nil;
}

static HTMLInputElementImpl *inputElementFromDOMElement(DOMElement *element)
{
    NodeImpl *node = [element _nodeImpl];
    if (node->hasTagName(inputTag)) {
        return static_cast<HTMLInputElementImpl *>(node);
    }
    return nil;
}

static HTMLFormElementImpl *formElementFromDOMElement(DOMElement *element)
{
    NodeImpl *node = [element _nodeImpl];
    // This should not be necessary, but an XSL file on
    // maps.google.com crashes otherwise because it is an xslt file
    // that contains <form> elements that aren't in any namespace, so
    // they come out as generic CML elements
    if (node && node->hasTagName(formTag)) {
        return static_cast<HTMLFormElementImpl *>(node);
    }
    return nil;
}

- (DOMElement *)elementWithName:(NSString *)name inForm:(DOMElement *)form
{
    HTMLFormElementImpl *formElement = formElementFromDOMElement(form);
    if (formElement) {
        Vector<HTMLGenericFormElementImpl*>& elements = formElement->formElements;
        AtomicString targetName = name;
        for (unsigned int i = 0; i < elements.size(); i++) {
            HTMLGenericFormElementImpl *elt = elements[i];
            // Skip option elements, other duds
            if (elt->name() == targetName)
                return [DOMElement _elementWithImpl:elt];
        }
    }
    return nil;
}

- (BOOL)elementDoesAutoComplete:(DOMElement *)element
{
    HTMLInputElementImpl *inputElement = inputElementFromDOMElement(element);
    return inputElement != nil
        && inputElement->inputType() == HTMLInputElementImpl::TEXT
        && inputElement->autoComplete();
}

- (BOOL)elementIsPassword:(DOMElement *)element
{
    HTMLInputElementImpl *inputElement = inputElementFromDOMElement(element);
    return inputElement != nil
        && inputElement->inputType() == HTMLInputElementImpl::PASSWORD;
}

- (DOMElement *)formForElement:(DOMElement *)element;
{
    HTMLInputElementImpl *inputElement = inputElementFromDOMElement(element);
    if (inputElement) {
        HTMLFormElementImpl *formElement = inputElement->form();
        if (formElement) {
            return [DOMElement _elementWithImpl:formElement];
        }
    }
    return nil;
}

- (DOMElement *)currentForm
{
    HTMLFormElementImpl *formElement = m_frame->currentForm();
    return formElement ? [DOMElement _elementWithImpl:formElement] : nil;
}

- (NSArray *)controlsInForm:(DOMElement *)form
{
    NSMutableArray *results = nil;
    HTMLFormElementImpl *formElement = formElementFromDOMElement(form);
    if (formElement) {
        Vector<HTMLGenericFormElementImpl*>& elements = formElement->formElements;
        for (unsigned int i = 0; i < elements.size(); i++) {
            if (elements.at(i)->isEnumeratable()) { // Skip option elements, other duds
                DOMElement *de = [DOMElement _elementWithImpl:elements.at(i)];
                if (!results) {
                    results = [NSMutableArray arrayWithObject:de];
                } else {
                    [results addObject:de];
                }
            }
        }
    }
    return results;
}

- (NSString *)searchForLabels:(NSArray *)labels beforeElement:(DOMElement *)element
{
    return m_frame->searchForLabelsBeforeElement(labels, [element _elementImpl]);
}

- (NSString *)matchLabels:(NSArray *)labels againstElement:(DOMElement *)element
{
    return m_frame->matchLabelsAgainstElement(labels, [element _elementImpl]);
}

- (NSDictionary *)elementAtPoint:(NSPoint)point
{
    RenderObject *renderer = m_frame->renderer();
    if (!renderer) 
        return nil;
    
    RenderObject::NodeInfo nodeInfo(true, true);
    renderer->layer()->hitTest(nodeInfo, (int)point.x, (int)point.y);

    NodeImpl *n;
    QWidget *widget = 0;
    IntPoint widgetPoint(point);
    
    while (true) {
        n = nodeInfo.innerNode();
        if (!n || !n->renderer() || !n->renderer()->isWidget())
            break;
        widget = static_cast<RenderWidget *>(n->renderer())->widget();
        if (!widget || !widget->inherits("FrameView"))
            break;
        Frame *kpart = static_cast<HTMLFrameElementImpl *>(n)->contentPart();
        if (!kpart || !static_cast<MacFrame *>(kpart)->renderer())
            break;
        int absX, absY;
        n->renderer()->absolutePosition(absX, absY, true);
        FrameView *view = static_cast<FrameView *>(widget);
        widgetPoint.setX(widgetPoint.x() - absX + view->contentsX());
        widgetPoint.setY(widgetPoint.y() - absY + view->contentsY());

        RenderObject::NodeInfo widgetNodeInfo(true, true);
        static_cast<MacFrame *>(kpart)->renderer()->layer()->hitTest(widgetNodeInfo, widgetPoint.x(), widgetPoint.y());
        nodeInfo = widgetNodeInfo;
    }
    
    NSMutableDictionary *element = [NSMutableDictionary dictionary];
    [element setObject:[NSNumber numberWithBool:m_frame->isPointInsideSelection((int)point.x, (int)point.y)]
                forKey:WebCoreElementIsSelectedKey];

    // Find the title in the nearest enclosing DOM node.
    // For <area> tags in image maps, walk the tree for the <area>, not the <img> using it.
    for (NodeImpl *titleNode = nodeInfo.innerNode(); titleNode; titleNode = titleNode->parentNode()) {
        if (titleNode->isElementNode()) {
            const AtomicString& title = static_cast<ElementImpl *>(titleNode)->getAttribute(titleAttr);
            if (!title.isNull()) {
                // We found a node with a title.
                QString titleText = title.qstring();
                titleText.replace(QChar('\\'), m_frame->backslashAsCurrencySymbol());
                [element setObject:titleText.getNSString() forKey:WebCoreElementTitleKey];
                break;
            }
        }
    }

    NodeImpl *URLNode = nodeInfo.URLElement();
    if (URLNode) {
        ElementImpl *e = static_cast<ElementImpl *>(URLNode);
        DocumentImpl *doc = e->getDocument();
        ASSERT(doc);
        
        const AtomicString& title = e->getAttribute(titleAttr);
        if (!title.isEmpty()) {
            QString titleText = title.qstring();
            titleText.replace(QChar('\\'), m_frame->backslashAsCurrencySymbol());
            [element setObject:titleText.getNSString() forKey:WebCoreElementLinkTitleKey];
        }
        
        const AtomicString& link = e->getAttribute(hrefAttr);
        if (!link.isNull()) {
            QString t = plainText(rangeOfContents(e).get());
            if (!t.isEmpty()) {
                [element setObject:t.getNSString() forKey:WebCoreElementLinkLabelKey];
            }
            QString URLString = parseURL(link).qstring();
            [element setObject:doc->completeURL(URLString).getNSString() forKey:WebCoreElementLinkURLKey];
        }
        
        DOMString target = e->getAttribute(targetAttr);
        if (target.isEmpty() && doc) { // FIXME: Take out this doc check when we're not just before a release.
            target = doc->baseTarget();
        }
        if (!target.isEmpty()) {
            [element setObject:target.qstring().getNSString() forKey:WebCoreElementLinkTargetFrameKey];
        }
    }

    NodeImpl *node = nodeInfo.innerNonSharedNode();
    if (node) {
        [element setObject:[DOMNode _nodeWithImpl:node] forKey:WebCoreElementDOMNodeKey];
    
        // Only return image information if there is an image.
        if (node->renderer() && node->renderer()->isImage()) {
            RenderImage *r = static_cast<RenderImage *>(node->renderer());
            if (!r->errorOccurred()) {
                const Image& p = r->image();
                if (p.imageRenderer())
                    [element setObject:p.imageRenderer() forKey:WebCoreElementImageRendererKey];
            }

            int x, y;
            if (r->absolutePosition(x, y)) {
                NSValue *rect = [NSValue valueWithRect:NSMakeRect(x, y, r->contentWidth(), r->contentHeight())];
                [element setObject:rect forKey:WebCoreElementImageRectKey];
            }

            ElementImpl *i = static_cast<ElementImpl*>(node);
    
            // FIXME: Code copied from RenderImage::updateFromElement; should share.
            DOMString attr;
            if (i->hasTagName(objectTag)) {
                attr = i->getAttribute(dataAttr);
            } else {
                attr = i->getAttribute(srcAttr);
            }
            if (!attr.isEmpty()) {
                QString URLString = parseURL(attr).qstring();
                [element setObject:i->getDocument()->completeURL(URLString).getNSString() forKey:WebCoreElementImageURLKey];
            }
            
            // FIXME: Code copied from RenderImage::updateFromElement; should share.
            DOMString alt;
            if (i->hasTagName(inputTag))
                alt = static_cast<HTMLInputElementImpl *>(i)->altText();
            else if (i->hasTagName(imgTag))
                alt = static_cast<HTMLImageElementImpl *>(i)->altText();
            if (!alt.isNull()) {
                QString altText = alt.qstring();
                altText.replace(QChar('\\'), m_frame->backslashAsCurrencySymbol());
                [element setObject:altText.getNSString() forKey:WebCoreElementImageAltStringKey];
            }
        }
    }
    
    return element;
}

- (NSURL *)URLWithAttributeString:(NSString *)string
{
    DocumentImpl *doc = m_frame->document();
    if (!doc) {
        return nil;
    }
    QString rel = parseURL(QString::fromNSString(string)).qstring();
    return KURL(doc->baseURL(), rel, doc->decoder() ? doc->decoder()->codec() : 0).getNSURL();
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag
{
    return m_frame->findString(string, forward, caseFlag, wrapFlag);
}

- (NSString *)advanceToNextMisspelling
{
    return m_frame->advanceToNextMisspelling().getNSString();
}

- (NSString *)advanceToNextMisspellingStartingJustBeforeSelection
{
    return m_frame->advanceToNextMisspelling(true).getNSString();
}

- (void)unmarkAllMisspellings
{
    DocumentImpl *doc = m_frame->document();
    if (!doc) {
        return;
    }
    doc->removeMarkers(DocumentMarker::Spelling);
}

- (void)setTextSizeMultiplier:(float)multiplier
{
    int newZoomFactor = (int)rint(multiplier * 100);
    if (m_frame->zoomFactor() == newZoomFactor) {
        return;
    }
    m_frame->setZoomFactor(newZoomFactor);
}

- (CFStringEncoding)textEncoding
{
    return KWQCFStringEncodingFromIANACharsetName(m_frame->encoding().latin1());
}

- (NSView *)nextKeyView
{
    DocumentImpl *doc = m_frame->document();
    if (!doc) {
        return nil;
    }
    return m_frame->nextKeyView(doc->focusNode(), KWQSelectingNext);
}

- (NSView *)previousKeyView
{
    DocumentImpl *doc = m_frame->document();
    if (!doc) {
        return nil;
    }
    return m_frame->nextKeyView(doc->focusNode(), KWQSelectingPrevious);
}

- (NSView *)nextKeyViewInsideWebFrameViews
{
    DocumentImpl *doc = m_frame->document();
    if (!doc) {
        return nil;
    }
    
    return m_frame->nextKeyViewInFrameHierarchy(doc->focusNode(), KWQSelectingNext);
}

- (NSView *)previousKeyViewInsideWebFrameViews
{
    DocumentImpl *doc = m_frame->document();
    if (!doc) {
        return nil;
    }

    return m_frame->nextKeyViewInFrameHierarchy(doc->focusNode(), KWQSelectingPrevious);
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string
{
    return [self stringByEvaluatingJavaScriptFromString:string forceUserGesture:true];
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string forceUserGesture:(BOOL)forceUserGesture
{
    m_frame->createEmptyDocument();
    JSValue* result = m_frame->executeScript(0, QString::fromNSString(string), forceUserGesture);
    if (!result || !result->isString())
        return 0;
    JSLock lock;
    return result->getString().domString();
}

- (WebScriptObject *)windowScriptObject
{
    return m_frame->windowScriptObject();
}

- (NPObject *)windowScriptNPObject
{
    return m_frame->windowScriptNPObject();
}

- (DOMDocument *)DOMDocument
{
    return [DOMDocument _documentWithImpl:m_frame->document()];
}

- (DOMHTMLElement *)frameElement
{
    return (DOMHTMLElement *)[[self DOMDocument] _ownerElement];
}

- (NSAttributedString *)selectedAttributedString
{
    // FIXME: should be a no-arg version of attributedString() that does this
    return m_frame->attributedString(m_frame->selection().start().node(), m_frame->selection().start().offset(), m_frame->selection().end().node(), m_frame->selection().end().offset());
}

- (NSAttributedString *)attributedStringFrom:(DOMNode *)start startOffset:(int)startOffset to:(DOMNode *)end endOffset:(int)endOffset
{
    return m_frame->attributedString([start _nodeImpl], startOffset, [end _nodeImpl], endOffset);
}

- (NSRect)selectionRect
{
    return m_frame->selectionRect(); 
}

- (NSRect)visibleSelectionRect
{
    return m_frame->visibleSelectionRect(); 
}

- (void)centerSelectionInVisibleArea
{
    m_frame->centerSelectionInVisibleArea(); 
}

- (NSRect)caretRectAtNode:(DOMNode *)node offset:(int)offset affinity:(NSSelectionAffinity)affinity
{
    return [node _nodeImpl]->renderer()->caretRect(offset, static_cast<EAffinity>(affinity));
}
- (NSRect)firstRectForDOMRange:(DOMRange *)range
{
    int extraWidthToEndOfLine = 0;
    IntRect startCaretRect = [[range startContainer] _nodeImpl]->renderer()->caretRect([range startOffset], DOWNSTREAM, &extraWidthToEndOfLine);
    IntRect endCaretRect = [[range endContainer] _nodeImpl]->renderer()->caretRect([range endOffset], UPSTREAM);

    if (startCaretRect.y() == endCaretRect.y()) {
        // start and end are on the same line
        return IntRect(MIN(startCaretRect.x(), endCaretRect.x()), 
                     startCaretRect.y(), 
                     abs(endCaretRect.x() - startCaretRect.x()),
                     MAX(startCaretRect.height(), endCaretRect.height()));
    }

    // start and end aren't on the same line, so go from start to the end of its line
    return IntRect(startCaretRect.x(), 
                 startCaretRect.y(),
                 startCaretRect.width() + extraWidthToEndOfLine,
                 startCaretRect.height());
}

- (NSImage *)selectionImage
{
    return m_frame->selectionImage();
}

- (void)setName:(NSString *)name
{
    m_frame->treeNode()->setName(name);
}

- (NSString *)name
{
    return m_frame->treeNode()->name();
}

- (void)_addFramePathToString:(NSMutableString *)path
{
    NSString *name = [self name];
    if ([name hasPrefix:@"<!--framePath "]) {
        // we have a generated name - take the path from our name
        NSRange ourPathRange = {14, [name length] - 14 - 3};
        [path appendString:[name substringWithRange:ourPathRange]];
    } else {
        // we don't have a generated name - just add our simple name to the end
        [[self parent] _addFramePathToString:path];
        [path appendString:@"/"];
        if (name)
            [path appendString:name];
    }
}

// Generate a repeatable name for a child about to be added to us.  The name must be
// unique within the frame tree.  The string we generate includes a "path" of names
// from the root frame down to us.  For this path to be unique, each set of siblings must
// contribute a unique name to the path, which can't collide with any HTML-assigned names.
// We generate this path component by index in the child list along with an unlikely frame name.
- (NSString *)generateFrameName
{
    NSMutableString *path = [NSMutableString stringWithCapacity:256];
    [path insertString:@"<!--framePath " atIndex:0];
    [self _addFramePathToString:path];
    // The new child's path component is all but the 1st char and the last 3 chars
    // FIXME: Shouldn't this number be the index of this frame in its parent rather than the child count?
    [path appendFormat:@"/<!--frame%d-->-->", [self childCount]];
    return path;
}

- (NSURL *)URL
{
    return m_frame->url().getNSURL();
}

- (NSURL *)baseURL
{
    return m_frame->completeURL(m_frame->document()->baseURL()).getNSURL();
}

- (NSString *)referrer
{
    return m_frame->referrer().getNSString();
}

- (NSString *)domain
{
    DocumentImpl *doc = m_frame->document();
    if (doc && doc->isHTMLDocument()) {
        return doc->domain().qstring().getNSString();
    }
    return nil;
}

- (WebCoreFrameBridge *)opener
{
    Frame *openerPart = m_frame->opener();

    if (openerPart)
        return Mac(openerPart)->bridge();

    return nil;
}

- (void)setOpener:(WebCoreFrameBridge *)bridge;
{
    Frame *p = [bridge part];
    
    if (p)
        p->setOpener(m_frame);
}

+ (NSString *)stringWithData:(NSData *)data textEncoding:(CFStringEncoding)textEncoding
{
    if (textEncoding == kCFStringEncodingInvalidId || textEncoding == kCFStringEncodingISOLatin1) {
        textEncoding = kCFStringEncodingWindowsLatin1;
    }
    return QTextCodec(textEncoding).toUnicode((const char*)[data bytes], [data length]).getNSString();
}

+ (NSString *)stringWithData:(NSData *)data textEncodingName:(NSString *)textEncodingName
{
    CFStringEncoding textEncoding = KWQCFStringEncodingFromIANACharsetName([textEncodingName lossyCString]);
    return [WebCoreFrameBridge stringWithData:data textEncoding:textEncoding];
}

- (BOOL)needsLayout
{
    RenderObject *renderer = m_frame->renderer();
    return renderer ? renderer->needsLayout() : false;
}

- (void)setNeedsLayout
{
    RenderObject *renderer = m_frame->renderer();
    if (renderer)
        renderer->setNeedsLayout(true);
}

- (BOOL)interceptKeyEvent:(NSEvent *)event toView:(NSView *)view
{
    return m_frame->keyEvent(event);
}

- (NSString *)renderTreeAsExternalRepresentation
{
    return externalRepresentation(m_frame->renderer()).getNSString();
}

- (void)setSelectionFromNone
{
    m_frame->setSelectionFromNone();
}

- (void)setDisplaysWithFocusAttributes:(BOOL)flag
{
    m_frame->setDisplaysWithFocusAttributes(flag);
}

- (void)setWindowHasFocus:(BOOL)flag
{
    m_frame->setWindowHasFocus(flag);
}

- (void)setShouldCreateRenderers:(BOOL)f
{
    _shouldCreateRenderers = f;
}

- (BOOL)shouldCreateRenderers
{
    return _shouldCreateRenderers;
}

- (int)numPendingOrLoadingRequests
{
    DocumentImpl *doc = m_frame->document();
    
    if (doc)
        return KWQNumberOfPendingOrLoadingRequests (doc->docLoader());
    return 0;
}

- (BOOL)doneProcessingData
{
    DocumentImpl *doc = m_frame->document();
    if (doc) {
        Tokenizer* tok = doc->tokenizer();
        if (tok)
            return !tok->processingData();
    }
    return YES;
}

- (BOOL)shouldClose
{
    return m_frame->shouldClose();
}

- (NSColor *)bodyBackgroundColor
{
    return m_frame->bodyBackgroundColor();
}

- (NSColor *)selectionColor
{
    RenderCanvas* root = static_cast<RenderCanvas *>(m_frame->document()->renderer());
    if (root) {
        RenderStyle *pseudoStyle = root->getPseudoStyle(RenderStyle::SELECTION);
        if (pseudoStyle && pseudoStyle->backgroundColor().isValid()) {
            return nsColor(pseudoStyle->backgroundColor());
        }
    }
    return m_frame->displaysWithFocusAttributes() ? [NSColor selectedTextBackgroundColor] : [NSColor secondarySelectedControlColor];
}

- (void)adjustViewSize
{
    FrameView *view = m_frame->view();
    if (view)
        view->adjustViewSize();
}

-(id)accessibilityTree
{
    KWQAccObjectCache::enableAccessibility();
    if (!m_frame || !m_frame->document()) return nil;
    RenderCanvas* root = static_cast<RenderCanvas *>(m_frame->document()->renderer());
    if (!root) return nil;
    return m_frame->document()->getAccObjectCache()->accObject(root);
}

- (void)setDrawsBackground:(BOOL)drawsBackground
{
    if (m_frame && m_frame->view())
        m_frame->view()->setTransparent(!drawsBackground);
}

- (void)undoEditing:(id)arg
{
    ASSERT([arg isKindOfClass:[KWQEditCommand class]]);
    [arg command]->unapply();
}

- (void)redoEditing:(id)arg
{
    ASSERT([arg isKindOfClass:[KWQEditCommand class]]);
    [arg command]->reapply();
}

- (DOMRange *)rangeByExpandingSelectionWithGranularity:(WebBridgeSelectionGranularity)granularity
{
    if (!frameHasSelection(self))
        return nil;
        
    // NOTE: The enums *must* match the very similar ones declared in SelectionController.h
    SelectionController selection(m_frame->selection());
    selection.expandUsingGranularity(static_cast<ETextGranularity>(granularity));
    return [DOMRange _rangeWithImpl:selection.toRange().get()];
}

- (DOMRange *)rangeByAlteringCurrentSelection:(WebSelectionAlteration)alteration direction:(WebBridgeSelectionDirection)direction granularity:(WebBridgeSelectionGranularity)granularity
{
    if (!frameHasSelection(self))
        return nil;
        
    // NOTE: The enums *must* match the very similar ones declared in SelectionController.h
    SelectionController selection(m_frame->selection());
    selection.modify(static_cast<SelectionController::EAlter>(alteration), 
                     static_cast<SelectionController::EDirection>(direction), 
                     static_cast<ETextGranularity>(granularity));
    return [DOMRange _rangeWithImpl:selection.toRange().get()];
}

- (void)alterCurrentSelection:(WebSelectionAlteration)alteration direction:(WebBridgeSelectionDirection)direction granularity:(WebBridgeSelectionGranularity)granularity
{
    if (!frameHasSelection(self))
        return;
        
    // NOTE: The enums *must* match the very similar ones declared in SelectionController.h
    SelectionController selection(m_frame->selection());
    selection.modify(static_cast<SelectionController::EAlter>(alteration), 
                     static_cast<SelectionController::EDirection>(direction), 
                     static_cast<ETextGranularity>(granularity));

    // save vertical navigation x position if necessary; many types of motion blow it away
    int xPos = Frame::NoXPosForVerticalArrowNavigation;
    switch (granularity) {
        case WebBridgeSelectByLine:
        case WebBridgeSelectByParagraph:
            xPos = m_frame->xPosForVerticalArrowNavigation();
            break;
        case WebBridgeSelectByCharacter:
        case WebBridgeSelectByWord:
        case WebBridgeSelectToLineBoundary:
        case WebBridgeSelectToParagraphBoundary:
        case WebBridgeSelectToDocumentBoundary:
            break;
    }

    
    // setting the selection always clears saved vertical navigation x position
    m_frame->setSelection(selection);
    
    // altering the selection also sets the granularity back to character
    // NOTE: The one exception is that we need to keep word granularity
    // to preserve smart delete behavior when extending by word.  e.g. double-click,
    // then shift-option-rightarrow, then delete needs to smart delete, per TextEdit.
    if (!((alteration == WebSelectByExtending) &&
          (granularity == WebBridgeSelectByWord) && (m_frame->selectionGranularity() == WORD)))
        m_frame->setSelectionGranularity(static_cast<ETextGranularity>(WebBridgeSelectByCharacter));
    
    // restore vertical navigation x position if necessary
    if (xPos != Frame::NoXPosForVerticalArrowNavigation)
        m_frame->setXPosForVerticalArrowNavigation(xPos);

    m_frame->selectFrameElementInParentIfFullySelected();

    [self ensureSelectionVisible];
}

- (DOMRange *)rangeByAlteringCurrentSelection:(WebSelectionAlteration)alteration verticalDistance:(float)verticalDistance
{
    if (!frameHasSelection(self))
        return nil;
        
    SelectionController selection(m_frame->selection());
    selection.modify(static_cast<SelectionController::EAlter>(alteration), static_cast<int>(verticalDistance));
    return [DOMRange _rangeWithImpl:selection.toRange().get()];
}

- (void)alterCurrentSelection:(WebSelectionAlteration)alteration verticalDistance:(float)verticalDistance
{
    if (!frameHasSelection(self))
        return;
        
    SelectionController selection(m_frame->selection());
    selection.modify(static_cast<SelectionController::EAlter>(alteration), static_cast<int>(verticalDistance));

    // setting the selection always clears saved vertical navigation x position, so preserve it
    int xPos = m_frame->xPosForVerticalArrowNavigation();
    m_frame->setSelection(selection);
    m_frame->setSelectionGranularity(static_cast<ETextGranularity>(WebBridgeSelectByCharacter));
    m_frame->setXPosForVerticalArrowNavigation(xPos);

    m_frame->selectFrameElementInParentIfFullySelected();

    [self ensureSelectionVisible];
}

- (WebBridgeSelectionGranularity)selectionGranularity
{
    // NOTE: The enums *must* match the very similar ones declared in SelectionController.h
    return static_cast<WebBridgeSelectionGranularity>(m_frame->selectionGranularity());
}

- (void)setSelectedDOMRange:(DOMRange *)range affinity:(NSSelectionAffinity)selectionAffinity closeTyping:(BOOL)closeTyping
{
    NodeImpl *startContainer = [[range startContainer] _nodeImpl];
    NodeImpl *endContainer = [[range endContainer] _nodeImpl];
    ASSERT(startContainer);
    ASSERT(endContainer);
    ASSERT(startContainer->getDocument());
    ASSERT(startContainer->getDocument() == endContainer->getDocument());
    
    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    EAffinity affinity = static_cast<EAffinity>(selectionAffinity);
    
    // Non-collapsed ranges are not allowed to start at the end of a line that is wrapped,
    // they start at the beginning of the next line instead
    if (![range collapsed])
        affinity = DOWNSTREAM;
    
    // FIXME: Can we provide extentAffinity?
    VisiblePosition visibleStart(startContainer, [range startOffset], affinity);
    VisiblePosition visibleEnd(endContainer, [range endOffset], SEL_DEFAULT_AFFINITY);
    SelectionController selection(visibleStart, visibleEnd);
    m_frame->setSelection(selection, closeTyping);
}

- (DOMRange *)selectedDOMRange
{
    return [DOMRange _rangeWithImpl:m_frame->selection().toRange().get()];
}

- (NSRange)convertToNSRange:(RangeImpl *)range
{
    if (!range || range->isDetached()) {
        return NSMakeRange(NSNotFound, 0);
    }

    RefPtr<RangeImpl> fromStartRange(m_frame->document()->createRange());
    int exception = 0;

    fromStartRange->setEnd(range->startContainer(exception), range->startOffset(exception), exception);
    int startPosition = TextIterator::rangeLength(fromStartRange.get());

    fromStartRange->setEnd(range->endContainer(exception), range->endOffset(exception), exception);
    int endPosition = TextIterator::rangeLength(fromStartRange.get());

    return NSMakeRange(startPosition, endPosition - startPosition);
}

- (RangeImpl *)convertToDOMRange:(NSRange)nsrange
{
    if (nsrange.location > INT_MAX)
        return 0;
    if (nsrange.length > INT_MAX || nsrange.location + nsrange.length > INT_MAX)
        nsrange.length = INT_MAX - nsrange.location;

    return TextIterator::rangeFromLocationAndLength(m_frame->document(), nsrange.location, nsrange.length);
}

- (DOMRange *)convertNSRangeToDOMRange:(NSRange)nsrange
{
    return [DOMRange _rangeWithImpl:[self convertToDOMRange:nsrange]];
}

- (NSRange)convertDOMRangeToNSRange:(DOMRange *)range
{
    return [self convertToNSRange:[range _rangeImpl]];
}

- (void)selectNSRange:(NSRange)range
{
    m_frame->setSelection(SelectionController([self convertToDOMRange:range], SEL_DEFAULT_AFFINITY));
}

- (NSRange)selectedNSRange
{
    return [self convertToNSRange:m_frame->selection().toRange().get()];
}

- (NSSelectionAffinity)selectionAffinity
{
    return static_cast<NSSelectionAffinity>(m_frame->selection().affinity());
}

- (void)setMarkDOMRange:(DOMRange *)range
{
    RangeImpl* r = [range _rangeImpl];
    m_frame->setMark(Selection(startPosition(r), endPosition(r), SEL_DEFAULT_AFFINITY));
}

- (DOMRange *)markDOMRange
{
    return [DOMRange _rangeWithImpl:m_frame->mark().toRange().get()];
}

- (void)setMarkedTextDOMRange:(DOMRange *)range customAttributes:(NSArray *)attributes ranges:(NSArray *)ranges
{
    m_frame->setMarkedTextRange([range _rangeImpl], attributes, ranges);
}

- (DOMRange *)markedTextDOMRange
{
    return [DOMRange _rangeWithImpl:m_frame->markedTextRange()];
}

- (NSRange)markedTextNSRange
{
    return [self convertToNSRange:m_frame->markedTextRange()];
}

- (void)replaceMarkedTextWithText:(NSString *)text
{
    if (!frameHasSelection(self))
        return;
    
    int exception = 0;

    RangeImpl *markedTextRange = m_frame->markedTextRange();
    if (markedTextRange && !markedTextRange->collapsed(exception))
        TypingCommand::deleteKeyPressed(m_frame->document(), NO);
    
    if ([text length] > 0)
        TypingCommand::insertText(m_frame->document(), text, YES);
    
    [self ensureSelectionVisible];
}

- (BOOL)canDeleteRange:(DOMRange *)range
{
    NodeImpl *startContainer = [[range startContainer] _nodeImpl];
    NodeImpl *endContainer = [[range endContainer] _nodeImpl];
    if (startContainer == nil || endContainer == nil)
        return NO;
    
    if (!startContainer->isContentEditable() || !endContainer->isContentEditable())
        return NO;
    
    if ([range collapsed]) {
        VisiblePosition start(startContainer, [range startOffset], DOWNSTREAM);
        if (isStartOfEditableContent(start))
            return NO;
    }
    
    return YES;
}

// Given proposedRange, returns an extended range that includes adjacent whitespace that should
// be deleted along with the proposed range in order to preserve proper spacing and punctuation of
// the text surrounding the deletion.
- (DOMRange *)smartDeleteRangeForProposedRange:(DOMRange *)proposedRange
{
    NodeImpl *startContainer = [[proposedRange startContainer] _nodeImpl];
    NodeImpl *endContainer = [[proposedRange endContainer] _nodeImpl];
    if (startContainer == nil || endContainer == nil)
        return nil;

    ASSERT(startContainer->getDocument());
    ASSERT(startContainer->getDocument() == endContainer->getDocument());
    
    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    Position start(startContainer, [proposedRange startOffset]);
    Position end(endContainer, [proposedRange endOffset]);
    Position newStart = start.upstream().leadingWhitespacePosition(DOWNSTREAM, true);
    if (newStart.isNull())
        newStart = start;
    Position newEnd = end.downstream().trailingWhitespacePosition(DOWNSTREAM, true);
    if (newEnd.isNull())
        newEnd = end;

    RangeImpl *range = m_frame->document()->createRange();
    int exception = 0;
    range->setStart(newStart.node(), newStart.offset(), exception);
    range->setEnd(newStart.node(), newStart.offset(), exception);
    return [DOMRange _rangeWithImpl:range];
}

// Determines whether whitespace needs to be added around aString to preserve proper spacing and
// punctuation when it’s inserted into the receiver’s text over charRange. Returns by reference
// in beforeString and afterString any whitespace that should be added, unless either or both are
// nil. Both are returned as nil if aString is nil or if smart insertion and deletion are disabled.
- (void)smartInsertForString:(NSString *)pasteString replacingRange:(DOMRange *)rangeToReplace beforeString:(NSString **)beforeString afterString:(NSString **)afterString
{
    // give back nil pointers in case of early returns
    if (beforeString)
        *beforeString = nil;
    if (afterString)
        *afterString = nil;
        
    // inspect destination
    NodeImpl *startContainer = [[rangeToReplace startContainer] _nodeImpl];
    NodeImpl *endContainer = [[rangeToReplace endContainer] _nodeImpl];

    Position startPos(startContainer, [rangeToReplace startOffset]);
    Position endPos(endContainer, [rangeToReplace endOffset]);

    VisiblePosition startVisiblePos = VisiblePosition(startPos, VP_DEFAULT_AFFINITY);
    VisiblePosition endVisiblePos = VisiblePosition(endPos, VP_DEFAULT_AFFINITY);
    
    // this check also ensures startContainer, startPos, endContainer, and endPos are non-null
    if (startVisiblePos.isNull() || endVisiblePos.isNull())
        return;

    bool addLeadingSpace = startPos.leadingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull() && !isStartOfParagraph(startVisiblePos);
    if (addLeadingSpace) {
        QChar previousChar = startVisiblePos.previous().character();
        if (!previousChar.isNull()) {
            addLeadingSpace = !m_frame->isCharacterSmartReplaceExempt(previousChar, true);
        }
    }
    
    bool addTrailingSpace = endPos.trailingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull() && !isEndOfParagraph(endVisiblePos);
    if (addTrailingSpace) {
        QChar thisChar = endVisiblePos.character();
        if (!thisChar.isNull()) {
            addTrailingSpace = !m_frame->isCharacterSmartReplaceExempt(thisChar, false);
        }
    }
    
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

- (DOMDocumentFragment *)documentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString 
{
    if (!m_frame || !m_frame->document())
        return 0;

    PassRefPtr<DocumentFragmentImpl> fragment = createFragmentFromMarkup(m_frame->document(), QString::fromNSString(markupString), QString::fromNSString(baseURLString));
    return [DOMDocumentFragment _documentFragmentWithImpl:fragment.get()];
}

- (DOMDocumentFragment *)documentFragmentWithText:(NSString *)text
{
    if (!frameHasSelection(self) || !text)
        return 0;
    
    return [DOMDocumentFragment _documentFragmentWithImpl:createFragmentFromText(m_frame->document(), QString::fromNSString(text)).get()];
}

- (DOMDocumentFragment *)documentFragmentWithNodesAsParagraphs:(NSArray *)nodes
{
    NSEnumerator *nodeEnum = [nodes objectEnumerator];
    DOMNode *node;
    QPtrList<NodeImpl> nodeList;
    
    if (!m_frame || !m_frame->document())
        return 0;
    
    while ((node = [nodeEnum nextObject])) {
        nodeList.append([node _nodeImpl]);
    }
    
    return [DOMDocumentFragment _documentFragmentWithImpl:createFragmentFromNodeList(m_frame->document(), nodeList).get()];
}

- (void)replaceSelectionWithFragment:(DOMDocumentFragment *)fragment selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle
{
    if (!frameHasSelection(self) || !fragment)
        return;
    
    EditCommandPtr(new ReplaceSelectionCommand(m_frame->document(), [fragment _fragmentImpl], selectReplacement, smartReplace, matchStyle)).apply();
    [self ensureSelectionVisible];
}

- (void)replaceSelectionWithNode:(DOMNode *)node selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace
{
    DOMDocumentFragment *fragment = [[self DOMDocument] createDocumentFragment];
    [fragment appendChild:node];
    [self replaceSelectionWithFragment:fragment selectReplacement:selectReplacement smartReplace:smartReplace matchStyle:NO];
}

- (void)replaceSelectionWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace
{
    DOMDocumentFragment *fragment = [self documentFragmentWithMarkupString:markupString baseURLString:baseURLString];
    [self replaceSelectionWithFragment:fragment selectReplacement:selectReplacement smartReplace:smartReplace matchStyle:NO];
}

- (void)replaceSelectionWithText:(NSString *)text selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace
{
    [self replaceSelectionWithFragment:[self documentFragmentWithText:text] selectReplacement:selectReplacement smartReplace:smartReplace matchStyle:YES];
}

- (void)insertLineBreak
{
    if (!frameHasSelection(self))
        return;
    
    TypingCommand::insertLineBreak(m_frame->document());
    [self ensureSelectionVisible];
}

- (void)insertParagraphSeparator
{
    if (!frameHasSelection(self))
        return;
    
    TypingCommand::insertParagraphSeparator(m_frame->document());
    [self ensureSelectionVisible];
}

- (void)insertParagraphSeparatorInQuotedContent
{
    if (!frameHasSelection(self))
        return;
    
    TypingCommand::insertParagraphSeparatorInQuotedContent(m_frame->document());
    [self ensureSelectionVisible];
}

- (void)insertText:(NSString *)text selectInsertedText:(BOOL)selectInsertedText
{
    if (!frameHasSelection(self))
        return;
    
    TypingCommand::insertText(m_frame->document(), text, selectInsertedText);
    [self ensureSelectionVisible];
}

- (void)setSelectionToDragCaret
{
    m_frame->setSelection(m_frame->dragCaret());
}

- (void)moveSelectionToDragCaret:(DOMDocumentFragment *)selectionFragment smartMove:(BOOL)smartMove
{
    Position base = m_frame->dragCaret().base();
    EditCommandPtr(new MoveSelectionCommand(m_frame->document(), [selectionFragment _fragmentImpl], base, smartMove)).apply();
}

- (VisiblePosition)_visiblePositionForPoint:(NSPoint)point
{
    RenderObject *renderer = m_frame->renderer();
    if (!renderer) {
        return VisiblePosition();
    }
    
    RenderObject::NodeInfo nodeInfo(true, true);
    renderer->layer()->hitTest(nodeInfo, (int)point.x, (int)point.y);
    NodeImpl *node = nodeInfo.innerNode();
    if (!node || !node->renderer())
        return VisiblePosition();
    
    return node->renderer()->positionForCoordinates((int)point.x, (int)point.y);
}

- (void)moveDragCaretToPoint:(NSPoint)point
{   
    SelectionController dragCaret([self _visiblePositionForPoint:point]);
    m_frame->setDragCaret(dragCaret);
}

- (void)removeDragCaret
{
    m_frame->setDragCaret(SelectionController());
}

- (DOMRange *)dragCaretDOMRange
{
    return [DOMRange _rangeWithImpl:m_frame->dragCaret().toRange().get()];
}

- (DOMRange *)editableDOMRangeForPoint:(NSPoint)point
{
    VisiblePosition position = [self _visiblePositionForPoint:point];
    return position.isNull() ? nil : [DOMRange _rangeWithImpl:SelectionController(position).toRange().get()];
}

- (DOMRange *)characterRangeAtPoint:(NSPoint)point
{
    VisiblePosition position = [self _visiblePositionForPoint:point];
    if (position.isNull())
        return nil;
    
    VisiblePosition previous = position.previous();
    if (previous.isNotNull()) {
        DOMRange *previousCharacterRange = [DOMRange _rangeWithImpl:makeRange(previous, position).get()];
        NSRect rect = [self firstRectForDOMRange:previousCharacterRange];
        if (NSPointInRect(point, rect))
            return previousCharacterRange;
    }

    VisiblePosition next = position.next();
    if (next.isNotNull()) {
        DOMRange *nextCharacterRange = [DOMRange _rangeWithImpl:makeRange(position, next).get()];
        NSRect rect = [self firstRectForDOMRange:nextCharacterRange];
        if (NSPointInRect(point, rect))
            return nextCharacterRange;
    }
    
    return nil;
}

- (void)deleteSelectionWithSmartDelete:(BOOL)smartDelete
{
    if (!frameHasSelection(self))
        return;
    
    EditCommandPtr(new DeleteSelectionCommand(m_frame->document(), smartDelete)).apply();
}

- (void)deleteKeyPressedWithSmartDelete:(BOOL)smartDelete
{
    if (!m_frame || !m_frame->document())
        return;
    
    TypingCommand::deleteKeyPressed(m_frame->document(), smartDelete);
    [self ensureSelectionVisible];
}

- (void)forwardDeleteKeyPressedWithSmartDelete:(BOOL)smartDelete
{
    if (!m_frame || !m_frame->document())
        return;
    
    TypingCommand::forwardDeleteKeyPressed(m_frame->document(), smartDelete);
    [self ensureSelectionVisible];
}

- (DOMCSSStyleDeclaration *)typingStyle
{
    if (!m_frame || !m_frame->typingStyle())
        return nil;
    return [DOMCSSStyleDeclaration _styleDeclarationWithImpl:m_frame->typingStyle()->copy()];
}

- (void)setTypingStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction
{
    if (!m_frame)
        return;
    m_frame->computeAndSetTypingStyle([style _styleDeclarationImpl], static_cast<EditAction>(undoAction));
}

- (void)applyStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction
{
    if (!m_frame)
        return;
    m_frame->applyStyle([style _styleDeclarationImpl], static_cast<EditAction>(undoAction));
}

- (void)applyParagraphStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction
{
    if (!m_frame)
        return;
    m_frame->applyParagraphStyle([style _styleDeclarationImpl], static_cast<EditAction>(undoAction));
}

- (BOOL)selectionStartHasStyle:(DOMCSSStyleDeclaration *)style
{
    if (!m_frame)
        return NO;
    return m_frame->selectionStartHasStyle([style _styleDeclarationImpl]);
}

- (NSCellStateValue)selectionHasStyle:(DOMCSSStyleDeclaration *)style
{
    if (!m_frame)
        return NSOffState;
    switch (m_frame->selectionHasStyle([style _styleDeclarationImpl])) {
        case Frame::falseTriState:
            return NSOffState;
        case Frame::trueTriState:
            return NSOnState;
        case Frame::mixedTriState:
            return NSMixedState;
    }
    return NSOffState;
}

- (void)applyEditingStyleToBodyElement
{
    if (!m_frame)
        return;
    m_frame->applyEditingStyleToBodyElement();
}

- (void)removeEditingStyleFromBodyElement
{
    if (!m_frame)
        return;
    m_frame->removeEditingStyleFromBodyElement();
}

- (void)applyEditingStyleToElement:(DOMElement *)element
{
    if (!m_frame)
        return;
    m_frame->applyEditingStyleToElement([element _elementImpl]);
}

- (void)removeEditingStyleFromElement:(DOMElement *)element
{
    if (!m_frame)
        return;
    m_frame->removeEditingStyleFromElement([element _elementImpl]);
}

- (NSFont *)fontForSelection:(BOOL *)hasMultipleFonts
{
    bool multipleFonts = false;
    NSFont *font = nil;
    if (m_frame)
        font = m_frame->fontForSelection(hasMultipleFonts ? &multipleFonts : 0);
    if (hasMultipleFonts)
        *hasMultipleFonts = multipleFonts;
    return font;
}

- (NSDictionary *)fontAttributesForSelectionStart
{
    return m_frame ? m_frame->fontAttributesForSelectionStart() : nil;
}

- (NSWritingDirection)baseWritingDirectionForSelectionStart
{
    return m_frame ? m_frame->baseWritingDirectionForSelectionStart() : NSWritingDirectionLeftToRight;
}

- (void)ensureSelectionVisible
{
    if (!frameHasSelection(self))
        return;
    
    FrameView *v = m_frame->view();
    if (!v)
        return;

    Position extent = m_frame->selection().extent();
    if (extent.isNull())
        return;
    
    RenderObject *renderer = extent.node()->renderer();
    if (!renderer)
        return;
    
    NSView *documentView = v->getDocumentView();
    if (!documentView)
        return;
    
    IntRect extentRect = renderer->caretRect(extent.offset(), m_frame->selection().affinity());
    RenderLayer *layer = renderer->enclosingLayer();
    if (layer)
        layer->scrollRectToVisible(extentRect);
}

// [info draggingLocation] is in window coords

- (BOOL)eventMayStartDrag:(NSEvent *)event
{
    return m_frame ? m_frame->eventMayStartDrag(event) : NO;
}

- (NSDragOperation)dragOperationForDraggingInfo:(id <NSDraggingInfo>)info
{
    NSDragOperation op = NSDragOperationNone;
    if (m_frame) {
        FrameView *v = m_frame->view();
        if (v) {
            // Sending an event can result in the destruction of the view and part.
            v->ref();
            
            KWQClipboard::AccessPolicy policy = m_frame->baseURL().isLocalFile() ? KWQClipboard::Readable : KWQClipboard::TypesReadable;
            KWQClipboard *clipboard = new KWQClipboard(true, [info draggingPasteboard], policy);
            clipboard->ref();
            NSDragOperation srcOp = [info draggingSourceOperationMask];
            clipboard->setSourceOperation(srcOp);

            if (v->updateDragAndDrop(IntPoint([info draggingLocation]), clipboard)) {
                // *op unchanged if no source op was set
                if (!clipboard->destinationOperation(&op)) {
                    // The element accepted but they didn't pick an operation, so we pick one for them
                    // (as does WinIE).
                    if (srcOp & NSDragOperationCopy) {
                        op = NSDragOperationCopy;
                    } else if (srcOp & NSDragOperationMove || srcOp & NSDragOperationGeneric) {
                        op = NSDragOperationMove;
                    } else if (srcOp & NSDragOperationLink) {
                        op = NSDragOperationLink;
                    } else {
                        op = NSDragOperationGeneric;
                    }
                } else if (!(op & srcOp)) {
                    // make sure WC picked an op that was offered.  Cocoa doesn't seem to enforce this,
                    // but IE does.
                    op = NSDragOperationNone;
                }
            }
            clipboard->setAccessPolicy(KWQClipboard::Numb);    // invalidate clipboard here for security

            clipboard->deref();
            v->deref();
            return op;
        }
    }
    return op;
}

- (void)dragExitedWithDraggingInfo:(id <NSDraggingInfo>)info
{
    if (m_frame) {
        FrameView *v = m_frame->view();
        if (v) {
            // Sending an event can result in the destruction of the view and part.
            v->ref();

            KWQClipboard::AccessPolicy policy = m_frame->baseURL().isLocalFile() ? KWQClipboard::Readable : KWQClipboard::TypesReadable;
            KWQClipboard *clipboard = new KWQClipboard(true, [info draggingPasteboard], policy);
            clipboard->ref();
            clipboard->setSourceOperation([info draggingSourceOperationMask]);
            
            v->cancelDragAndDrop(IntPoint([info draggingLocation]), clipboard);
            clipboard->setAccessPolicy(KWQClipboard::Numb);    // invalidate clipboard here for security

            clipboard->deref();
            v->deref();
        }
    }
}

- (BOOL)concludeDragForDraggingInfo:(id <NSDraggingInfo>)info
{
    if (m_frame) {
        FrameView *v = m_frame->view();
        if (v) {
            // Sending an event can result in the destruction of the view and part.
            v->ref();

            KWQClipboard *clipboard = new KWQClipboard(true, [info draggingPasteboard], KWQClipboard::Readable);
            clipboard->ref();
            clipboard->setSourceOperation([info draggingSourceOperationMask]);

            BOOL result = v->performDragAndDrop(IntPoint([info draggingLocation]), clipboard);
            clipboard->setAccessPolicy(KWQClipboard::Numb);    // invalidate clipboard here for security

            clipboard->deref();
            v->deref();

            return result;
        }
    }
    return NO;
}

- (void)dragSourceMovedTo:(NSPoint)windowLoc
{
    if (m_frame) {
        m_frame->dragSourceMovedTo(IntPoint(windowLoc));
    }
}

- (void)dragSourceEndedAt:(NSPoint)windowLoc operation:(NSDragOperation)operation
{
    if (m_frame) {
        m_frame->dragSourceEndedAt(IntPoint(windowLoc), operation);
    }
}

- (BOOL)mayDHTMLCut
{
    return m_frame->mayCut();
}

- (BOOL)mayDHTMLCopy
{
    return m_frame->mayCopy();
}

- (BOOL)mayDHTMLPaste
{
    return m_frame->mayPaste();
}

- (BOOL)tryDHTMLCut
{
    return m_frame->tryCut();
}

- (BOOL)tryDHTMLCopy
{
    return m_frame->tryCopy();
}

- (BOOL)tryDHTMLPaste
{
    return m_frame->tryPaste();
}

- (DOMRange *)rangeOfCharactersAroundCaret
{
    if (!m_frame)
        return nil;
        
    SelectionController selection(m_frame->selection());
    if (!selection.isCaret())
        return nil;

    VisiblePosition caret(selection.start(), selection.affinity());
    VisiblePosition next = caret.next();
    VisiblePosition previous = caret.previous();
    if (previous.isNull() || next.isNull() || caret == next || caret == previous)
        return nil;

    return [DOMRange _rangeWithImpl:makeRange(previous, next).get()];
}

- (NSMutableDictionary *)dashboardRegions
{
    return m_frame->dashboardRegionsDictionary();
}

@end

@implementation WebCoreFrameBridge (WebCoreBridgeInternal)

- (RootObject *)executionContextForView:(NSView *)aView
{
    MacFrame *frame = [self part];
    RootObject *root = new RootObject(aView);    // The root gets deleted by JavaScriptCore.
    root->setRootObjectImp(Window::retrieveWindow(frame));
    root->setInterpreter(frame->jScript()->interpreter());
    frame->addPluginRootObject(root);
    return root;
}

@end
