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
#import "DocumentType.h"
#import "FrameTree.h"
#import "FrameView.h"
#import "GraphicsContext.h"
#import "HTMLFormElement.h"
#import "HTMLInputElement.h"
#import "AccessibilityObjectCache.h"
#import "CharsetNames.h"
#import "ClipboardMac.h"
#import "KWQEditCommand.h"
#import "FloatRect.h"
#import "Font.h"
#import "FoundationExtras.h"
#import "KWQLoader.h"
#import "KWQPageState.h"
#import "RenderTreeAsText.h"
#import "TextEncoding.h"
#import "FrameMac.h"
#import "Node.h"
#import "Page.h"
#import "SelectionController.h"
#import "WebCorePageBridge.h"
#import "WebCoreSettings.h"
#import "WebCoreTextRendererFactory.h"
#import "WebCoreViewFactory.h"
#import "WebCoreWidgetHolder.h"
#import "csshelper.h"
#import "DeleteSelectionCommand.h"
#import "dom2_eventsimpl.h"
#import "Range.h"
#import "AbstractView.h"
#import "Position.h"
#import "HTMLDocument.h"
#import "html_imageimpl.h"
#import "htmlediting.h"
#import "HTMLNames.h"
#import "kjs_proxy.h"
#import "kjs_window.h"
#import "loader.h"
#import "markup.h"
#import "ModifySelectionListLevelCommand.h"
#import "MoveSelectionCommand.h"
#import "RenderCanvas.h"
#import "render_frames.h"
#import "RenderImage.h"
#import "render_replaced.h"
#import "render_style.h"
#import "ReplaceSelectionCommand.h"
#import "TypingCommand.h"
#import "VisiblePosition.h"
#import "TextIterator.h"
#import "visible_units.h"
#import "xml_tokenizer.h"
#import <JavaScriptCore/date_object.h>
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
using KJS::BooleanType;
using KJS::StringType;
using KJS::NumberType;
using KJS::ObjectType;
using KJS::UnspecifiedType;
using KJS::UndefinedType;
using KJS::NullType;
using KJS::GetterSetterType;
using KJS::UString;
using KJS::Identifier;
using KJS::List;
using KJS::JSType;
using KJS::DateInstance;

using KJS::Bindings::RootObject;

using WebCore::RenderObject;

NSString *WebCorePageCacheStateKey =            @"WebCorePageCacheState";

@interface WebCoreFrameBridge (WebCoreBridgeInternal)
- (RootObject *)executionContextForView:(NSView *)aView;
- (RenderObject::NodeInfo)nodeInfoAtPoint:(NSPoint)point allowShadowContent:(BOOL)allow;
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
        
    Document *doc = static_cast<Document*>(window->frame()->document());
    if (doc)
        doc->updateRendering();
}

static BOOL frameHasSelection(WebCoreFrameBridge *bridge)
{
    if (!bridge)
        return NO;
    
    Frame *frame = [bridge impl];
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

static NSAppleEventDescriptor* aeDescFromJSValue(ExecState* exec, JSValue* jsValue)
{
    NSAppleEventDescriptor* aeDesc = 0;
    switch (jsValue->type()) {
        case BooleanType:
            aeDesc = [NSAppleEventDescriptor descriptorWithBoolean:jsValue->getBoolean()];
            break;
        case StringType:
            aeDesc = [NSAppleEventDescriptor descriptorWithString:String(jsValue->getString())];
            break;
        case NumberType: {
            Float64 value = jsValue->getNumber();
            aeDesc = [NSAppleEventDescriptor descriptorWithDescriptorType:typeIEEE64BitFloatingPoint bytes:&value length:sizeof(value)];
            break;
        }
        case ObjectType: {
            JSObject* object = jsValue->getObject();
            if (object->inherits(&DateInstance::info)) {
                DateInstance* date = static_cast<DateInstance*>(object);
                double ms = 0;
                int tzOffset = 0;
                if (date->getTime(ms, tzOffset)) {
                    CFAbsoluteTime utcSeconds = ms / 1000 - kCFAbsoluteTimeIntervalSince1970;
                    LongDateTime ldt;
                    if (noErr == UCConvertCFAbsoluteTimeToLongDateTime(utcSeconds, &ldt))
                        aeDesc = [NSAppleEventDescriptor descriptorWithDescriptorType:typeLongDateTime bytes:&ldt length:sizeof(ldt)];
                }
            }
            if (!aeDesc) {
                JSValue* primitive = object->toPrimitive(exec);
                if (exec->hadException()) {
                    exec->clearException();
                    return [NSAppleEventDescriptor nullDescriptor];
                }
                return aeDescFromJSValue(exec, primitive);
            }
            break;
        }
        default:
            LOG_ERROR("Unknown JavaScript type: %d", jsValue->type());
            // no break;
        case UnspecifiedType:
        case UndefinedType:
        case NullType:
        case GetterSetterType:
            aeDesc = [NSAppleEventDescriptor nullDescriptor];
            break;
    }
    
    return aeDesc;
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
    return bridge(m_frame->tree()->firstChild());
}

- (WebCoreFrameBridge *)lastChild
{
    return bridge(m_frame->tree()->lastChild());
}

- (unsigned)childCount
{
    return m_frame->tree()->childCount();
}

- (WebCoreFrameBridge *)previousSibling;
{
    return bridge(m_frame->tree()->previousSibling());
}

- (WebCoreFrameBridge *)nextSibling;
{
    return bridge(m_frame->tree()->nextSibling());
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
    m_frame->tree()->appendChild(adoptRef(child->m_frame));
}

- (void)removeChild:(WebCoreFrameBridge *)child
{
    m_frame->tree()->removeChild(child->m_frame);
}

- (WebCoreFrameBridge *)childFrameNamed:(NSString *)name
{
    return bridge(m_frame->tree()->child(name));
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
        return [[self page] mainFrame];
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

- (BOOL)_shouldAllowAccessFrom:(WebCoreFrameBridge *)source
{
    // if no source frame, allow access
    if (source == nil)
        return YES;

    //   - allow access if the two frames are in the same window
    if ([self page] == [source page])
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

- (WebCoreFrameBridge *)findFrameNamed:(NSString *)name
{
    return bridge(m_frame->tree()->find(name));
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
    Frame *frame = [document _document]->frame();
    return frame ? Mac(frame)->bridge() : nil;
}

- (id)initMainFrameWithPage:(WebCorePageBridge *)page
{
    if (!initializedKJS) {
        mainThread = pthread_self();
        RootObject::setFindRootObjectForNativeHandleFunction(rootForView);
        KJS::Bindings::Instance::setDidExecuteFunction(updateRenderingForBindings);
        initializedKJS = true;
    }
    
    if (!(self = [super init]))
        return nil;

    m_frame = new FrameMac([page impl], 0);
    m_frame->setBridge(self);
    _shouldCreateRenderers = YES;

    // FIXME: This is one-time initialization, but it gets the value of the setting from the
    // current WebView. That's a mismatch and not good!
    if (!initializedObjectCacheSize) {
        Cache::setSize([self getObjectCacheSize]);
        initializedObjectCacheSize = true;
    }
    
    return self;
}

- (id)initSubframeWithRenderer:(RenderPart *)renderer
{
    if (!(self = [super init]))
        return nil;
    
    m_frame = new FrameMac(renderer->node()->getDocument()->frame()->page(), renderer);
    m_frame->setBridge(self);
    _shouldCreateRenderers = YES;
    return self;
}

- (WebCorePageBridge *)page
{
    return m_frame->page()->bridge();
}

- (void)initializeSettings:(WebCoreSettings *)settings
{
    m_frame->setSettings([settings settings]);
}

- (void)dealloc
{
    [self removeFromFrame];
    [super dealloc];
}

- (void)finalize
{
    // FIXME: This work really should not be done at deallocation time.
    // We need to do it at some well-defined time instead.
    [self removeFromFrame];        
    [super finalize];
}

- (FrameMac *)part
{
    return m_frame;
}

- (WebCoreFrameBridge *)parent
{
    FrameMac *parentFrame = Mac(m_frame->tree()->parent());
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
    ResourceRequest request(m_frame->resourceRequest());
    request.reload = reload;
    if (contentType)
        request.m_responseMIMEType = contentType;
    m_frame->setResourceRequest(request);

    // opening the URL
    if (m_frame->didOpenURL(URL)) {
        // things we have to set up after calling didOpenURL
        if (refresh) {
            m_frame->addMetaData("http-refresh", refresh);
        }
        if (lastModified) {
            NSString *modifiedString = [lastModified descriptionWithCalendarFormat:@"%a %b %d %Y %H:%M:%S" timeZone:nil locale:nil];
            m_frame->addMetaData("modified", modifiedString);
        }
    }
}

- (void)setEncoding:(NSString *)encoding userChosen:(BOOL)userChosen
{
    m_frame->setEncoding(DeprecatedString::fromNSString(encoding), userChosen);
}

- (void)addData:(NSData *)data
{
    Document *doc = m_frame->document();
    
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
    Document *doc = m_frame->document();
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
    BOOL referrerIsSecureURL = hasCaseInsensitivePrefix(referrer, @"https:");
    BOOL URLIsSecureURL = [URL scheme] != NULL && [[URL scheme] compare:@"https" options:(NSCaseInsensitiveSearch|NSLiteralSearch)] == NSOrderedSame;

    
    *hideReferrer = !referrerIsWebURL || (referrerIsSecureURL && !URLIsSecureURL);
    return !URLIsFileURL || referrerIsLocalURL;
}

- (void)saveDocumentState
{
    Document* doc = m_frame->document();
    if (doc) {
        DeprecatedStringList list = doc->docState();
        NSMutableArray* documentState = [[NSMutableArray alloc] init];
        DeprecatedStringList::const_iterator end = list.constEnd();
        for (DeprecatedStringList::const_iterator i = list.constBegin(); i != end; ++i) {
            const DeprecatedString& s = *i;
            [documentState addObject:[NSString stringWithCharacters:(const unichar *)s.unicode() length:s.length()]];
        }
        [self saveDocumentState:documentState];
        [documentState release];
    }
}

- (void)restoreDocumentState
{
    Document *doc = m_frame->document();
    if (doc) {
        NSArray *documentState = [self documentState];
        
        DeprecatedStringList s;
        for (unsigned i = 0; i < [documentState count]; i++){
            NSString *string = [documentState objectAtIndex: i];
            s.append(DeprecatedString::fromNSString(string));
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
    if (!m_frame)
        return NO;
    return m_frame->scrollOverflow((KWQScrollDirection)direction, (KWQScrollGranularity)granularity);
}

- (BOOL)sendScrollWheelEvent:(NSEvent *)event
{
    return m_frame ? m_frame->wheelEvent(event) : NO;
}

- (BOOL)saveDocumentToPageCache
{
    Document *doc = m_frame->document();
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

    FrameView* frameView = new FrameView(m_frame);
    m_frame->setView(frameView);
    frameView->deref();

    frameView->setView(view);
    if (mw >= 0)
        frameView->setMarginWidth(mw);
    if (mh >= 0)
        frameView->setMarginHeight(mh);
}

- (void)scrollToAnchor:(NSString *)anchor
{
    m_frame->gotoAnchor(anchor);
}

- (BOOL)isSelectionEditable
{
    // EDIT FIXME: This needs to consider the entire selected range
    Node *startNode = m_frame->selection().start().node();
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
    Document *doc = m_frame->document();
    if (doc) {
        if (DocumentType *doctype = doc->realDocType())
            documentTypeString = doctype->toString();
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

- (NSArray *)nodesFromList:(DeprecatedPtrList<Node> *)nodeList
{
    NSMutableArray *nodes = [NSMutableArray arrayWithCapacity:nodeList->count()];
    for (DeprecatedPtrListIterator<Node> i(*nodeList); i.current(); ++i) {
        [nodes addObject:[DOMNode _nodeWith:i.current()]];
    }
    return nodes;
}

- (NSString *)markupStringFromNode:(DOMNode *)node nodes:(NSArray **)nodes
{
    // FIXME: This is never "for interchange". Is that right? See the next method.
    DeprecatedPtrList<Node> nodeList;
    NSString *markupString = createMarkup([node _node], IncludeNode, nodes ? &nodeList : 0).getNSString();
    if (nodes) {
        *nodes = [self nodesFromList:&nodeList];
    }
    return [self _stringWithDocumentTypeStringAndMarkupString:markupString];
}

- (NSString *)markupStringFromRange:(DOMRange *)range nodes:(NSArray **)nodes
{
    // FIXME: This is always "for interchange". Is that right? See the previous method.
    DeprecatedPtrList<Node> nodeList;
    NSString *markupString = createMarkup([range _range], nodes ? &nodeList : 0, AnnotateForInterchange).getNSString();
    if (nodes) {
        *nodes = [self nodesFromList:&nodeList];
    }
    return [self _stringWithDocumentTypeStringAndMarkupString:markupString];
}

- (NSString *)selectedString
{
    String text = m_frame->selectedText();
    text.replace(QChar('\\'), m_frame->backslashAsCurrencySymbol());
    return [[(NSString*)text copy] autorelease];
}

- (NSString *)stringForRange:(DOMRange *)range
{
    String text = plainText([range _range]);
    text.replace(QChar('\\'), m_frame->backslashAsCurrencySymbol());
    return [[(NSString*)text copy] autorelease];
}

- (void)selectAll
{
    m_frame->selectAll();
}

- (void)deselectAll
{
    [self deselectText];
    Document *doc = m_frame->document();
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
    Document *doc = m_frame->document();
    if (doc)
        doc->setPrinting(deviceType == WebCoreDevicePrinter);
    return m_frame->reparseConfiguration();
}

static BOOL nowPrinting(WebCoreFrameBridge *self)
{
    Document *doc = self->m_frame->document();
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

- (void)drawRect:(NSRect)rect
{
    GraphicsContext context(nowPrinting(self));
    context.setUsesInactiveTextBackgroundColor(!m_frame->displaysWithFocusAttributes());
    [self _setupRootForPrinting:YES];
    m_frame->paint(&context, enclosingIntRect(rect));
    [self _setupRootForPrinting:NO];
}

// Used by pagination code called from AppKit when a standalone web page is printed.
- (NSArray*)computePageRectsWithPrintWidthScaleFactor:(float)printWidthScaleFactor printHeight:(float)printHeight
{
    [self _setupRootForPrinting:YES];
    NSMutableArray* pages = [NSMutableArray arrayWithCapacity:5];
    if (printWidthScaleFactor <= 0) {
        LOG_ERROR("printWidthScaleFactor has bad value %.2f", printWidthScaleFactor);
        return pages;
    }
    
    if (printHeight <= 0) {
        LOG_ERROR("printHeight has bad value %.2f", printHeight);
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
    Widget *widget = renderWidget ? renderWidget->widget() : 0;
    NSView *view = widget ? widget->getView() : nil;
    
    int nx, ny;
    node->absolutePosition(nx, ny);
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
    ASSERT(self == [[self page] mainFrame] || m_frame->ownerRenderer());

    m_frame->view()->setView(view);
    // FIXME: frame tries to do this too, is it needed?
    if (m_frame->ownerRenderer()) {
        m_frame->ownerRenderer()->setWidget(m_frame->view());
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

- (DOMElement*)elementForView:(NSView*)view
{
    // FIXME: implemented currently for only a subset of the KWQ widgets
    if ([view conformsToProtocol:@protocol(WebCoreWidgetHolder)]) {
        NSView <WebCoreWidgetHolder>* widgetHolder = view;
        Widget* widget = [widgetHolder widget];
        if (widget && widget->client())
            return [DOMElement _elementWith:widget->client()->element(widget)];
    }
    return nil;
}

static HTMLInputElement* inputElementFromDOMElement(DOMElement* element)
{
    Node* node = [element _node];
    if (node->hasTagName(inputTag))
        return static_cast<HTMLInputElement*>(node);
    return nil;
}

static HTMLFormElement *formElementFromDOMElement(DOMElement *element)
{
    Node *node = [element _node];
    // This should not be necessary, but an XSL file on
    // maps.google.com crashes otherwise because it is an xslt file
    // that contains <form> elements that aren't in any namespace, so
    // they come out as generic CML elements
    if (node && node->hasTagName(formTag)) {
        return static_cast<HTMLFormElement *>(node);
    }
    return nil;
}

- (DOMElement *)elementWithName:(NSString *)name inForm:(DOMElement *)form
{
    HTMLFormElement *formElement = formElementFromDOMElement(form);
    if (formElement) {
        Vector<HTMLGenericFormElement*>& elements = formElement->formElements;
        AtomicString targetName = name;
        for (unsigned int i = 0; i < elements.size(); i++) {
            HTMLGenericFormElement *elt = elements[i];
            // Skip option elements, other duds
            if (elt->name() == targetName)
                return [DOMElement _elementWith:elt];
        }
    }
    return nil;
}

- (BOOL)elementDoesAutoComplete:(DOMElement *)element
{
    HTMLInputElement *inputElement = inputElementFromDOMElement(element);
    return inputElement != nil
        && inputElement->inputType() == HTMLInputElement::TEXT
        && inputElement->autoComplete();
}

- (BOOL)elementIsPassword:(DOMElement *)element
{
    HTMLInputElement *inputElement = inputElementFromDOMElement(element);
    return inputElement != nil
        && inputElement->inputType() == HTMLInputElement::PASSWORD;
}

- (DOMElement *)formForElement:(DOMElement *)element;
{
    HTMLInputElement *inputElement = inputElementFromDOMElement(element);
    if (inputElement) {
        HTMLFormElement *formElement = inputElement->form();
        if (formElement) {
            return [DOMElement _elementWith:formElement];
        }
    }
    return nil;
}

- (DOMElement *)currentForm
{
    HTMLFormElement *formElement = m_frame->currentForm();
    return formElement ? [DOMElement _elementWith:formElement] : nil;
}

- (NSArray *)controlsInForm:(DOMElement *)form
{
    NSMutableArray *results = nil;
    HTMLFormElement *formElement = formElementFromDOMElement(form);
    if (formElement) {
        Vector<HTMLGenericFormElement*>& elements = formElement->formElements;
        for (unsigned int i = 0; i < elements.size(); i++) {
            if (elements.at(i)->isEnumeratable()) { // Skip option elements, other duds
                DOMElement *de = [DOMElement _elementWith:elements.at(i)];
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
    return m_frame->searchForLabelsBeforeElement(labels, [element _element]);
}

- (NSString *)matchLabels:(NSArray *)labels againstElement:(DOMElement *)element
{
    return m_frame->matchLabelsAgainstElement(labels, [element _element]);
}

- (void)getInnerNonSharedNode:(DOMNode **)innerNonSharedNode innerNode:(DOMNode **)innerNode URLElement:(DOMElement **)URLElement atPoint:(NSPoint)point allowShadowContent:(BOOL) allow
{
    RenderObject *renderer = m_frame->renderer();
    if (!renderer) {
        *innerNonSharedNode = nil;
        *innerNode = nil;
        *URLElement = nil;
        return;
    }

    RenderObject::NodeInfo nodeInfo = [self nodeInfoAtPoint:point allowShadowContent:allow];
    *innerNonSharedNode = [DOMNode _nodeWith:nodeInfo.innerNonSharedNode()];
    *innerNode = [DOMNode _nodeWith:nodeInfo.innerNode()];
    *URLElement = [DOMElement _elementWith:nodeInfo.URLElement()];
}

- (BOOL)isPointInsideSelection:(NSPoint)point
{
    return m_frame->isPointInsideSelection((int)point.x, (int)point.y);
}

- (NSURL *)URLWithAttributeString:(NSString *)string
{
    Document *doc = m_frame->document();
    if (!doc)
        return nil;
    // FIXME: is parseURL appropriate here?
    DeprecatedString rel = parseURL(string).deprecatedString();
    return KURL(doc->completeURL(rel)).getNSURL();
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag
{
    return m_frame->findString(string, forward, caseFlag, wrapFlag);
}

- (unsigned)highlightAllMatchesForString:(NSString *)string caseSensitive:(BOOL)caseFlag
{
    return m_frame->highlightAllMatchesForString(string, caseFlag);
}

- (void)clearHighlightedMatches
{
    Document *doc = m_frame->document();
    if (!doc) {
        return;
    }
    doc->removeMarkers(DocumentMarker::TextMatch);
}

- (NSString *)advanceToNextMisspelling
{
    return m_frame->advanceToNextMisspelling();
}

- (NSString *)advanceToNextMisspellingStartingJustBeforeSelection
{
    return m_frame->advanceToNextMisspelling(true);
}

- (void)unmarkAllMisspellings
{
    Document *doc = m_frame->document();
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
    return WebCore::TextEncoding(m_frame->encoding().latin1()).encodingID();
}

- (NSView *)nextKeyView
{
    Document *doc = m_frame->document();
    if (!doc)
        return nil;
    return m_frame->nextKeyView(doc->focusNode(), KWQSelectingNext);
}

- (NSView *)previousKeyView
{
    Document *doc = m_frame->document();
    if (!doc)
        return nil;
    return m_frame->nextKeyView(doc->focusNode(), KWQSelectingPrevious);
}

- (NSView *)nextKeyViewInsideWebFrameViews
{
    Document *doc = m_frame->document();
    if (!doc)
        return nil;
    return m_frame->nextKeyViewInFrameHierarchy(doc->focusNode(), KWQSelectingNext);
}

- (NSView *)previousKeyViewInsideWebFrameViews
{
    Document *doc = m_frame->document();
    if (!doc)
        return nil;
    return m_frame->nextKeyViewInFrameHierarchy(doc->focusNode(), KWQSelectingPrevious);
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string
{
    return [self stringByEvaluatingJavaScriptFromString:string forceUserGesture:true];
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string forceUserGesture:(BOOL)forceUserGesture
{
    m_frame->createEmptyDocument();
    JSValue* result = m_frame->executeScript(0, DeprecatedString::fromNSString(string), forceUserGesture);
    if (!result || !result->isString())
        return 0;
    JSLock lock;
    return String(result->getString());
}

- (NSAppleEventDescriptor *)aeDescByEvaluatingJavaScriptFromString:(NSString *)string
{
    m_frame->createEmptyDocument();
    JSValue* result = m_frame->executeScript(0, DeprecatedString::fromNSString(string), true);
    if (!result) // FIXME: pass errors
        return 0;
    JSLock lock;
    return aeDescFromJSValue(m_frame->jScript()->interpreter()->globalExec(), result);
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
    return [DOMDocument _documentWith:m_frame->document()];
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
    return m_frame->attributedString([start _node], startOffset, [end _node], endOffset);
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
    return [node _node]->renderer()->caretRect(offset, static_cast<EAffinity>(affinity));
}
- (NSRect)firstRectForDOMRange:(DOMRange *)range
{
    int extraWidthToEndOfLine = 0;
    IntRect startCaretRect = [[range startContainer] _node]->renderer()->caretRect([range startOffset], DOWNSTREAM, &extraWidthToEndOfLine);
    IntRect endCaretRect = [[range endContainer] _node]->renderer()->caretRect([range endOffset], UPSTREAM);

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
    m_frame->tree()->setName(name);
}

- (NSString *)name
{
    return m_frame->tree()->name();
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
    Document *doc = m_frame->document();
    if (doc && doc->isHTMLDocument())
        return doc->domain();
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
    Frame *p = [bridge impl];
    
    if (p)
        p->setOpener(m_frame);
}

+ (NSString *)stringWithData:(NSData *)data textEncoding:(CFStringEncoding)textEncoding
{
    if (textEncoding == kCFStringEncodingInvalidId)
        textEncoding = kCFStringEncodingWindowsLatin1;

    return WebCore::TextEncoding(textEncoding).toUnicode((const char*)[data bytes], [data length]).getNSString();
}

+ (NSString *)stringWithData:(NSData *)data textEncodingName:(NSString *)textEncodingName
{
    CFStringEncoding textEncoding = WebCore::TextEncoding([textEncodingName lossyCString]).encodingID();
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
    Document *doc = m_frame->document();
    
    if (doc)
        return KWQNumberOfPendingOrLoadingRequests (doc->docLoader());
    return 0;
}

- (BOOL)doneProcessingData
{
    Document *doc = m_frame->document();
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
    AccessibilityObjectCache::enableAccessibility();
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
    selection.expandUsingGranularity(static_cast<TextGranularity>(granularity));
    return [DOMRange _rangeWith:selection.toRange().get()];
}

- (DOMRange *)rangeByAlteringCurrentSelection:(WebSelectionAlteration)alteration direction:(WebBridgeSelectionDirection)direction granularity:(WebBridgeSelectionGranularity)granularity
{
    if (!frameHasSelection(self))
        return nil;
        
    // NOTE: The enums *must* match the very similar ones declared in SelectionController.h
    SelectionController selection(m_frame->selection());
    selection.modify(static_cast<SelectionController::EAlter>(alteration), 
                     static_cast<SelectionController::EDirection>(direction), 
                     static_cast<TextGranularity>(granularity));
    return [DOMRange _rangeWith:selection.toRange().get()];
}

- (void)alterCurrentSelection:(WebSelectionAlteration)alteration direction:(WebBridgeSelectionDirection)direction granularity:(WebBridgeSelectionGranularity)granularity
{
    if (!frameHasSelection(self))
        return;
        
    // NOTE: The enums *must* match the very similar ones declared in SelectionController.h
    SelectionController selection(m_frame->selection());
    selection.modify(static_cast<SelectionController::EAlter>(alteration), 
                     static_cast<SelectionController::EDirection>(direction), 
                     static_cast<TextGranularity>(granularity));

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
          (granularity == WebBridgeSelectByWord) && (m_frame->selectionGranularity() == WordGranularity)))
        m_frame->setSelectionGranularity(static_cast<TextGranularity>(WebBridgeSelectByCharacter));
    
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
    return [DOMRange _rangeWith:selection.toRange().get()];
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
    m_frame->setSelectionGranularity(static_cast<TextGranularity>(WebBridgeSelectByCharacter));
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
    Node *startContainer = [[range startContainer] _node];
    Node *endContainer = [[range endContainer] _node];
    ASSERT(startContainer);
    ASSERT(endContainer);
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
    return [DOMRange _rangeWith:m_frame->selection().toRange().get()];
}

- (NSRange)convertToNSRange:(Range *)range
{
    if (!range || range->isDetached()) {
        return NSMakeRange(NSNotFound, 0);
    }

    RefPtr<Range> fromStartRange(m_frame->document()->createRange());
    int exception = 0;

    fromStartRange->setEnd(range->startContainer(exception), range->startOffset(exception), exception);
    int startPosition = TextIterator::rangeLength(fromStartRange.get());

    fromStartRange->setEnd(range->endContainer(exception), range->endOffset(exception), exception);
    int endPosition = TextIterator::rangeLength(fromStartRange.get());

    return NSMakeRange(startPosition, endPosition - startPosition);
}

- (PassRefPtr<Range>)convertToDOMRange:(NSRange)nsrange
{
    if (nsrange.location > INT_MAX)
        return 0;
    if (nsrange.length > INT_MAX || nsrange.location + nsrange.length > INT_MAX)
        nsrange.length = INT_MAX - nsrange.location;

    return TextIterator::rangeFromLocationAndLength(m_frame->document(), nsrange.location, nsrange.length);
}

- (DOMRange *)convertNSRangeToDOMRange:(NSRange)nsrange
{
    return [DOMRange _rangeWith:[self convertToDOMRange:nsrange].get()];
}

- (NSRange)convertDOMRangeToNSRange:(DOMRange *)range
{
    return [self convertToNSRange:[range _range]];
}

- (void)selectNSRange:(NSRange)range
{
    m_frame->setSelection(SelectionController([self convertToDOMRange:range].get(), SEL_DEFAULT_AFFINITY));
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
    Range* r = [range _range];
    m_frame->setMark(Selection(startPosition(r), endPosition(r), SEL_DEFAULT_AFFINITY));
}

- (DOMRange *)markDOMRange
{
    return [DOMRange _rangeWith:m_frame->mark().toRange().get()];
}

- (void)setMarkedTextDOMRange:(DOMRange *)range customAttributes:(NSArray *)attributes ranges:(NSArray *)ranges
{
    m_frame->setMarkedTextRange([range _range], attributes, ranges);
}

- (DOMRange *)markedTextDOMRange
{
    return [DOMRange _rangeWith:m_frame->markedTextRange()];
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

    Range *markedTextRange = m_frame->markedTextRange();
    if (markedTextRange && !markedTextRange->collapsed(exception))
        TypingCommand::deleteKeyPressed(m_frame->document(), NO);
    
    if ([text length] > 0)
        TypingCommand::insertText(m_frame->document(), text, YES);
    
    [self ensureSelectionVisible];
}

- (BOOL)canDeleteRange:(DOMRange *)range
{
    Node *startContainer = [[range startContainer] _node];
    Node *endContainer = [[range endContainer] _node];
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
    Node *startContainer = [[proposedRange startContainer] _node];
    Node *endContainer = [[proposedRange endContainer] _node];
    if (startContainer == nil || endContainer == nil)
        return nil;

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

    RefPtr<Range> range = m_frame->document()->createRange();
    int exception = 0;
    range->setStart(newStart.node(), newStart.offset(), exception);
    range->setEnd(newStart.node(), newStart.offset(), exception);
    return [DOMRange _rangeWith:range.get()];
}

// Determines whether whitespace needs to be added around aString to preserve proper spacing and
// punctuation when itÕs inserted into the receiverÕs text over charRange. Returns by reference
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

    return [DOMDocumentFragment _documentFragmentWith:createFragmentFromMarkup(m_frame->document(),
        DeprecatedString::fromNSString(markupString), DeprecatedString::fromNSString(baseURLString)).get()];
}

- (DOMDocumentFragment *)documentFragmentWithText:(NSString *)text
{
    if (!frameHasSelection(self) || !text)
        return 0;
    
    return [DOMDocumentFragment _documentFragmentWith:createFragmentFromText(m_frame->document(), DeprecatedString::fromNSString(text)).get()];
}

- (DOMDocumentFragment *)documentFragmentWithNodesAsParagraphs:(NSArray *)nodes
{
    NSEnumerator *nodeEnum = [nodes objectEnumerator];
    DOMNode *node;
    DeprecatedPtrList<Node> nodeList;
    
    if (!m_frame || !m_frame->document())
        return 0;
    
    while ((node = [nodeEnum nextObject])) {
        nodeList.append([node _node]);
    }
    
    return [DOMDocumentFragment _documentFragmentWith:createFragmentFromNodeList(m_frame->document(), nodeList).get()];
}

- (void)replaceSelectionWithFragment:(DOMDocumentFragment *)fragment selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle
{
    if (!frameHasSelection(self) || !fragment)
        return;
    
    EditCommandPtr(new ReplaceSelectionCommand(m_frame->document(), [fragment _fragment], selectReplacement, smartReplace, matchStyle)).apply();
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

- (bool)canIncreaseSelectionListLevel
{
    return ModifySelectionListLevelCommand::canIncreaseSelectionListLevel(m_frame->document());
}

- (bool)canDecreaseSelectionListLevel
{
    return ModifySelectionListLevelCommand::canDecreaseSelectionListLevel(m_frame->document());
}

- (void)increaseSelectionListLevel
{
    if (!frameHasSelection(self))
        return;
    
    ModifySelectionListLevelCommand::increaseSelectionListLevel(m_frame->document());
    [self ensureSelectionVisible];
}

- (void)decreaseSelectionListLevel
{
    if (!frameHasSelection(self))
        return;
    
    ModifySelectionListLevelCommand::decreaseSelectionListLevel(m_frame->document());
    [self ensureSelectionVisible];
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
    EditCommandPtr(new MoveSelectionCommand(m_frame->document(), [selectionFragment _fragment], base, smartMove)).apply();
}

- (VisiblePosition)_visiblePositionForPoint:(NSPoint)point
{
    RenderObject *renderer = m_frame->renderer();
    if (!renderer) {
        return VisiblePosition();
    }
    
    RenderObject::NodeInfo nodeInfo = [self nodeInfoAtPoint:point allowShadowContent:YES];
    
    Node *node = nodeInfo.innerNode();
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
    return [DOMRange _rangeWith:m_frame->dragCaret().toRange().get()];
}

- (DOMRange *)editableDOMRangeForPoint:(NSPoint)point
{
    VisiblePosition position = [self _visiblePositionForPoint:point];
    return position.isNull() ? nil : [DOMRange _rangeWith:SelectionController(position).toRange().get()];
}

- (DOMRange *)characterRangeAtPoint:(NSPoint)point
{
    VisiblePosition position = [self _visiblePositionForPoint:point];
    if (position.isNull())
        return nil;
    
    VisiblePosition previous = position.previous();
    if (previous.isNotNull()) {
        DOMRange *previousCharacterRange = [DOMRange _rangeWith:makeRange(previous, position).get()];
        NSRect rect = [self firstRectForDOMRange:previousCharacterRange];
        if (NSPointInRect(point, rect))
            return previousCharacterRange;
    }

    VisiblePosition next = position.next();
    if (next.isNotNull()) {
        DOMRange *nextCharacterRange = [DOMRange _rangeWith:makeRange(position, next).get()];
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
    return [DOMCSSStyleDeclaration _styleDeclarationWith:m_frame->typingStyle()->copy().get()];
}

- (void)setTypingStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction
{
    if (!m_frame)
        return;
    m_frame->computeAndSetTypingStyle([style _styleDeclaration], static_cast<EditAction>(undoAction));
}

- (void)applyStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction
{
    if (!m_frame)
        return;
    m_frame->applyStyle([style _styleDeclaration], static_cast<EditAction>(undoAction));
}

- (void)applyParagraphStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction
{
    if (!m_frame)
        return;
    m_frame->applyParagraphStyle([style _styleDeclaration], static_cast<EditAction>(undoAction));
}

- (BOOL)selectionStartHasStyle:(DOMCSSStyleDeclaration *)style
{
    if (!m_frame)
        return NO;
    return m_frame->selectionStartHasStyle([style _styleDeclaration]);
}

- (NSCellStateValue)selectionHasStyle:(DOMCSSStyleDeclaration *)style
{
    if (!m_frame)
        return NSOffState;
    switch (m_frame->selectionHasStyle([style _styleDeclaration])) {
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
    m_frame->applyEditingStyleToElement([element _element]);
}

- (void)removeEditingStyleFromElement:(DOMElement *)element
{
    if (!m_frame)
        return;
    m_frame->removeEditingStyleFromElement([element _element]);
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
        layer->scrollRectToVisible(extentRect, RenderLayer::gAlignToEdgeIfNeeded, RenderLayer::gAlignToEdgeIfNeeded);
}

// [info draggingLocation] is in window coords

- (BOOL)eventMayStartDrag:(NSEvent *)event
{
    return m_frame ? m_frame->eventMayStartDrag(event) : NO;
}

static IntPoint globalPoint(NSWindow* window, NSPoint windowPoint)
{
    NSPoint screenPoint = [window convertBaseToScreen:windowPoint];
    return IntPoint((int)screenPoint.x, (int)(NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - screenPoint.y));
}

static PlatformMouseEvent createMouseEventFromDraggingInfo(NSWindow* window, id <NSDraggingInfo> info)
{
    // FIXME: Fake modifier keys here.
    return PlatformMouseEvent(IntPoint([info draggingLocation]), globalPoint(window, [info draggingLocation]),
        LeftButton, 0, false, false, false, false);
}

- (NSDragOperation)dragOperationForDraggingInfo:(id <NSDraggingInfo>)info
{
    NSDragOperation op = NSDragOperationNone;
    if (m_frame) {
        RefPtr<FrameView> v = m_frame->view();
        if (v) {
            ClipboardMac::AccessPolicy policy = m_frame->baseURL().isLocalFile() ? ClipboardMac::Readable : ClipboardMac::TypesReadable;
            RefPtr<ClipboardMac> clipboard = new ClipboardMac(true, [info draggingPasteboard], policy);
            NSDragOperation srcOp = [info draggingSourceOperationMask];
            clipboard->setSourceOperation(srcOp);

            PlatformMouseEvent event = createMouseEventFromDraggingInfo([self window], info);
            if (v->updateDragAndDrop(event, clipboard.get())) {
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
            clipboard->setAccessPolicy(ClipboardMac::Numb);    // invalidate clipboard here for security
            return op;
        }
    }
    return op;
}

- (void)dragExitedWithDraggingInfo:(id <NSDraggingInfo>)info
{
    if (m_frame) {
        RefPtr<FrameView> v = m_frame->view();
        if (v) {
            // Sending an event can result in the destruction of the view and part.
            ClipboardMac::AccessPolicy policy = m_frame->baseURL().isLocalFile() ? ClipboardMac::Readable : ClipboardMac::TypesReadable;
            RefPtr<ClipboardMac> clipboard = new ClipboardMac(true, [info draggingPasteboard], policy);
            clipboard->setSourceOperation([info draggingSourceOperationMask]);            
            v->cancelDragAndDrop(createMouseEventFromDraggingInfo([self window], info), clipboard.get());
            clipboard->setAccessPolicy(ClipboardMac::Numb);    // invalidate clipboard here for security
        }
    }
}

- (BOOL)concludeDragForDraggingInfo:(id <NSDraggingInfo>)info
{
    if (m_frame) {
        RefPtr<FrameView> v = m_frame->view();
        if (v) {
            // Sending an event can result in the destruction of the view and part.
            RefPtr<ClipboardMac> clipboard = new ClipboardMac(true, [info draggingPasteboard], ClipboardMac::Readable);
            clipboard->setSourceOperation([info draggingSourceOperationMask]);
            BOOL result = v->performDragAndDrop(createMouseEventFromDraggingInfo([self window], info), clipboard.get());
            clipboard->setAccessPolicy(ClipboardMac::Numb);    // invalidate clipboard here for security
            return result;
        }
    }
    return NO;
}

- (void)dragSourceMovedTo:(NSPoint)windowLoc
{
    if (m_frame) {
        // FIXME: Fake modifier keys here.
        PlatformMouseEvent event(IntPoint(windowLoc), globalPoint([self window], windowLoc),
            LeftButton, 0, false, false, false, false);
        m_frame->dragSourceMovedTo(event);
    }
}

- (void)dragSourceEndedAt:(NSPoint)windowLoc operation:(NSDragOperation)operation
{
    if (m_frame) {
        // FIXME: Fake modifier keys here.
        PlatformMouseEvent event(IntPoint(windowLoc), globalPoint([self window], windowLoc),
            LeftButton, 0, false, false, false, false);
        m_frame->dragSourceEndedAt(event, operation);
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

    return [DOMRange _rangeWith:makeRange(previous, next).get()];
}

- (NSMutableDictionary *)dashboardRegions
{
    return m_frame->dashboardRegionsDictionary();
}

@end

@implementation WebCoreFrameBridge (WebCoreBridgeInternal)

- (RootObject *)executionContextForView:(NSView *)aView
{
    FrameMac *frame = [self impl];
    RootObject *root = new RootObject(aView);    // The root gets deleted by JavaScriptCore.
    root->setRootObjectImp(Window::retrieveWindow(frame));
    root->setInterpreter(frame->jScript()->interpreter());
    frame->addPluginRootObject(root);
    return root;
}

- (RenderObject::NodeInfo)nodeInfoAtPoint:(NSPoint)point allowShadowContent:(BOOL)allow
{
    RenderObject *renderer = m_frame->renderer();

    RenderObject::NodeInfo nodeInfo(true, true);
    renderer->layer()->hitTest(nodeInfo, (int)point.x, (int)point.y);

    Node *n;
    Widget *widget = 0;
    IntPoint widgetPoint(point);
    
    while (true) {
        n = nodeInfo.innerNode();
        if (!n || !n->renderer() || !n->renderer()->isWidget())
            break;
        widget = static_cast<RenderWidget *>(n->renderer())->widget();
        if (!widget || !widget->isFrameView())
            break;
        Frame* frame = static_cast<HTMLFrameElement *>(n)->contentFrame();
        if (!frame || !frame->renderer())
            break;
        int absX, absY;
        n->renderer()->absolutePosition(absX, absY, true);
        FrameView *view = static_cast<FrameView *>(widget);
        widgetPoint.setX(widgetPoint.x() - absX + view->contentsX());
        widgetPoint.setY(widgetPoint.y() - absY + view->contentsY());

        RenderObject::NodeInfo widgetNodeInfo(true, true);
        frame->renderer()->layer()->hitTest(widgetNodeInfo, widgetPoint.x(), widgetPoint.y());
        nodeInfo = widgetNodeInfo;
    }
    
    if (!allow) {
        Node* node = nodeInfo.innerNode();
        if (node)
            node = node->shadowAncestorNode();
        nodeInfo.setInnerNode(node);
        node = nodeInfo.innerNonSharedNode();
        if (node)
            node = node->shadowAncestorNode();
        nodeInfo.setInnerNonSharedNode(node); 
    }
    return nodeInfo;
}

@end

@implementation WebCoreFrameBridge (WebCoreInternalUse)

- (FrameMac*)impl
{
    return m_frame;
}

@end
