/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2005, 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2006 David Smith (catfish.man@gmail.com)
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

#import "AXObjectCache.h"
#import "Cache.h"
#import "DOMImplementation.h"
#import "DOMInternal.h"
#import "Decoder.h"
#import "DeleteSelectionCommand.h"
#import "DocLoader.h"
#import "DocumentFragment.h"
#import "DocumentType.h"
#import "EditorClient.h"
#import "FloatRect.h"
#import "FoundationExtras.h"
#import "FrameMac.h"
#import "FrameTree.h"
#import "GraphicsContext.h"
#import "HTMLDocument.h"
#import "HTMLFormElement.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
#import "Image.h"
#import "LoaderFunctions.h"
#import "LoaderNSURLExtras.h"
#import "LoaderNSURLRequestExtras.h"
#import "ModifySelectionListLevel.h"
#import "MoveSelectionCommand.h"
#import "Page.h"
#import "PlugInInfoStore.h"
#import "RenderImage.h"
#import "RenderPart.h"
#import "RenderTreeAsText.h"
#import "RenderView.h"
#import "RenderWidget.h"
#import "RetainPtr.h"
#import "ReplaceSelectionCommand.h"
#import "ResourceRequest.h"
#import "Screen.h"
#import "SelectionController.h"
#import "TextEncoding.h"
#import "TextIterator.h"
#import "TypingCommand.h"
#import "WebCoreEditCommand.h"
#import "WebCorePageBridge.h"
#import "WebCorePageState.h"
#import "WebCoreSettings.h"
#import "WebCoreSystemInterface.h"
#import "WebCoreViewFactory.h"
#import "WebCoreWidgetHolder.h"
#import "WebDocumentLoader.h"
#import "WebFormDataStream.h"
#import "WebFrameLoader.h"
#import "WebFrameLoaderClient.h"
#import "WebSubresourceLoader.h"
#import "XMLTokenizer.h"
#import "csshelper.h"
#import "htmlediting.h"
#import "kjs_proxy.h"
#import "kjs_window.h"
#import "markup.h"
#import "visible_units.h"
#import <JavaScriptCore/array_instance.h>
#import <JavaScriptCore/date_object.h>
#import <JavaScriptCore/runtime_root.h>
#import <kjs/SavedBuiltins.h>

@class NSView;

using namespace std;
using namespace WebCore;
using namespace HTMLNames;

using KJS::ArrayInstance;
using KJS::BooleanType;
using KJS::DateInstance;
using KJS::ExecState;
using KJS::GetterSetterType;
using KJS::Identifier;
using KJS::Interpreter;
using KJS::JSLock;
using KJS::JSObject;
using KJS::JSType;
using KJS::JSValue;
using KJS::List;
using KJS::NullType;
using KJS::NumberType;
using KJS::ObjectType;
using KJS::SavedBuiltins;
using KJS::SavedProperties;
using KJS::StringType;
using KJS::UString;
using KJS::UndefinedType;
using KJS::UnspecifiedType;
using KJS::Window;

using KJS::Bindings::RootObject;

NSString *WebCorePageCacheStateKey = @"WebCorePageCacheState";

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
        
    Document *doc = static_cast<Document*>(window->frame()->document());
    if (doc)
        doc->updateRendering();
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
            double value = jsValue->getNumber();
            int intValue = (int)value;
            if (value == intValue)
                aeDesc = [NSAppleEventDescriptor descriptorWithDescriptorType:typeSInt32 bytes:&intValue length:sizeof(intValue)];
            else
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
            else if (object->inherits(&ArrayInstance::info)) {
                static HashSet<JSObject*> visitedElems;
                if (!visitedElems.contains(object)) {
                    visitedElems.add(object);
                    
                    ArrayInstance* array = static_cast<ArrayInstance*>(object);
                    aeDesc = [NSAppleEventDescriptor listDescriptor];
                    unsigned numItems = array->getLength();
                    for (unsigned i = 0; i < numItems; ++i)
                        [aeDesc insertDescriptor:aeDescFromJSValue(exec, array->getItem(i)) atIndex:0];
                    
                    visitedElems.remove(object);
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
        case UndefinedType:
            aeDesc = [NSAppleEventDescriptor descriptorWithTypeCode:cMissingValue];
            break;
        default:
            LOG_ERROR("Unknown JavaScript type: %d", jsValue->type());
            // no break;
        case UnspecifiedType:
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
    return m_frame->tree()->isDescendantOf(ancestor->m_frame);
}

- (WebCoreFrameBridge *)traverseNextFrameStayWithin:(WebCoreFrameBridge *)stayWithin
{
    return bridge(m_frame->tree()->traverseNext(stayWithin->m_frame));
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

- (WebCoreFrameBridge *)nextFrameWithWrap:(BOOL)wrapFlag
{
    return bridge(m_frame->tree()->traverseNextWithWrap(wrapFlag));
}

- (WebCoreFrameBridge *)previousFrameWithWrap:(BOOL)wrapFlag
{
    return bridge(m_frame->tree()->traversePreviousWithWrap(wrapFlag));
}

- (NSString *)domain
{
    Document *doc = m_frame->document();
    if (doc)
        return doc->domain();
    return nil;
}

// FIXME: this is not getting called any more! security regression...
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

- (BOOL)canTargetLoadInFrame:(WebCoreFrameBridge *)targetFrame
{
    // This method prevents this exploit:
    // <rdar://problem/3715785> multiple frame injection vulnerability reported by Secunia, affects almost all browsers
    
    // don't mess with navigation within the same page/frameset
    if ([self page] == [targetFrame page])
        return YES;

    // Normally, domain should be called on the DOMDocument since it is a DOM method, but this fix is needed for
    // Jaguar as well where the DOM API doesn't exist.
    NSString *thisDomain = [self domain];
    if ([thisDomain length] == 0) {
        // Allow if the request is made from a local file.
        return YES;
    }
    
    WebCoreFrameBridge *parentBridge = [targetFrame parent];
    // Allow if target is an entire window.
    if (!parentBridge)
        return YES;
    
    NSString *parentDomain = [parentBridge domain];
    // Allow if the domain of the parent of the targeted frame equals this domain.
    if (parentDomain && isCaseSensitiveEqual(thisDomain, parentDomain))
        return YES;

    return NO;
}

- (WebCoreFrameBridge *)findFrameNamed:(NSString *)name
{
    return bridge(m_frame->tree()->find(name));
}

+ (NSArray *)supportedNonImageMIMETypes
{
    return [NSArray arrayWithObjects:        
        @"text/html",
        @"text/xml",
        @"text/xsl",
        @"text/",
        @"application/x-javascript",
        @"application/xml",
        @"application/xhtml+xml",
        @"application/rss+xml",
        @"application/atom+xml",
        @"application/x-webarchive",
        @"multipart/x-mixed-replace",
#ifdef SVG_SUPPORT
        @"image/svg+xml",
#endif
        nil];
}

+ (NSArray *)supportedImageResourceMIMETypes
{
    static RetainPtr<NSArray> supportedTypes;
    if (!supportedTypes) {
        NSMutableSet* set = [[NSMutableSet alloc] init];

        // FIXME: Doesn't make sense to ask NSImage for a list of file types and extensions
        // because we aren't using NSImage to decode the images any more.
        NSEnumerator* enumerator = [[NSImage imageFileTypes] objectEnumerator];
        while (NSString* type = [enumerator nextObject]) {
            NSString* mime = wkGetMIMETypeForExtension(type);
            if (mime)
                [set addObject:mime];
        }

        // image/pjpeg is the MIME type for progressive jpeg. These files have the jpg file extension.
        // I believe we need this this to work around wkGetMIMETypeForExtension's limitation of only
        // providing one MIME type for each extension.
        [set addObject:@"image/pjpeg"];

        [set removeObject:@"application/octet-stream"];

        supportedTypes = [set allObjects];

        [set release];
    }

    return supportedTypes.get();
}

+ (NSArray *)supportedImageMIMETypes
{
    static RetainPtr<NSArray> supportedTypes;
    if (!supportedTypes) {
        NSMutableArray* types = [[self supportedImageResourceMIMETypes] mutableCopy];
        [types removeObject:@"application/pdf"];
        [types removeObject:@"application/postscript"];
        NSArray* copy = [types copy];
        [types release];

        supportedTypes = copy;

        [copy release];
    }

    return supportedTypes.get();
}

+ (WebCoreFrameBridge *)bridgeForDOMDocument:(DOMDocument *)document
{
    return bridge([document _document]->frame());
}

- (id)initMainFrameWithPage:(WebCorePageBridge *)page withEditorClient:(WebCoreEditorClient *)client
{
    if (!initializedKJS) {
        mainThread = pthread_self();
        RootObject::setFindRootObjectForNativeHandleFunction(rootForView);
        KJS::Bindings::Instance::setDidExecuteFunction(updateRenderingForBindings);
        initializedKJS = true;
    }
    
    if (!(self = [super init]))
        return nil;

    m_frame = new FrameMac([page impl], 0, client);
    m_frame->setBridge(self);
    _shouldCreateRenderers = YES;

    // FIXME: This is one-time initialization, but it gets the value of the setting from the
    // current WebView. That's a mismatch and not good!
    if (!initializedObjectCacheSize) {
        WebCore::cache()->setMaximumSize([self getObjectCacheSize]);
        initializedObjectCacheSize = true;
    }
    
    return self;
}

- (id)initSubframeWithOwnerElement:(Element *)ownerElement withEditorClient:(WebCoreEditorClient *)client
{
    if (!(self = [super init]))
        return nil;
    
    m_frame = new FrameMac(ownerElement->document()->frame()->page(), ownerElement, client);
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
    ASSERT(_closed);
    [super dealloc];
}

- (void)finalize
{
    ASSERT(_closed);
    [super finalize];
}

- (void)close
{
    [self removeFromFrame];
    [self clearFrame];
    _closed = YES;
}

- (WebCoreFrameBridge *)parent
{
    return bridge(m_frame->tree()->parent());
}

- (void)provisionalLoadStarted
{
    m_frame->provisionalLoadStarted();
}

- (void)openURL:(NSURL *)URL reload:(BOOL)reload contentType:(NSString *)contentType refresh:(NSString *)refresh lastModified:(NSDate *)lastModified pageCache:(NSDictionary *)pageCache
{
    if (pageCache) {
        WebCorePageState *state = [pageCache objectForKey:WebCorePageCacheStateKey];
        m_frame->openURLFromPageCache(state);
        [state invalidate];
        return;
    }
        
    m_frame->setResponseMIMEType(contentType);
    
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
    m_frame->setEncoding(encoding, userChosen);
}

- (void)addData:(NSData *)data
{
    Document *doc = m_frame->document();
    
    // Document may be nil if the part is about to redirect
    // as a result of JS executing during load, i.e. one frame
    // changing another's location before the frame's document
    // has been created. 
    if (doc) {
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

- (void)didNotOpenURL:(NSURL *)URL
{
    m_frame->didNotOpenURL(KURL(URL).url());
}

- (void)invalidatePageCache:(NSDictionary *)pageCache
{
    // We might have made a page cache item, but now we're bailing out due to an error before we ever
    // transitioned to the new page (before WebFrameState==commit).  The goal here is to restore any state
    // so that the existing view (that wenever got far enough to replace) can continue being used.
    Document *doc = m_frame->document();
    if (doc)
        doc->setInPageCache(NO);

    WebCorePageState *state = [pageCache objectForKey:WebCorePageCacheStateKey];

    // FIXME: This is a grotesque hack to fix <rdar://problem/4059059> Crash in RenderFlow::detach
    // Somehow the WebCorePageState object is not properly updated, and is holding onto a stale document
    // both Xcode and FileMaker see this crash, Safari does not.
    // This if check MUST be removed as part of re-writing the loader down in WebCore
    ASSERT(!state || ([state document] == doc));
    if ([state document] == doc)
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
    Vector<String> stateVector;
    if (Document* doc = m_frame->document())
        stateVector = doc->formElementsState();
    size_t size = stateVector.size();
    NSMutableArray* stateArray = [[NSMutableArray alloc] initWithCapacity:size];
    for (size_t i = 0; i < size; ++i) {
        NSString* s = stateVector[i];
        id o = s ? (id)s : (id)[NSNull null];
        [stateArray addObject:o];
    }
    [self saveDocumentState:stateArray];
    [stateArray release];
}

- (void)restoreDocumentState
{
    Document* doc = m_frame->document();
    if (!doc)
        return;
    NSArray* stateArray = [self documentState];
    size_t size = [stateArray count];
    Vector<String> stateVector;
    stateVector.reserveCapacity(size);
    for (size_t i = 0; i < size; ++i) {
        id o = [stateArray objectAtIndex:i];
        NSString* s = [o isKindOfClass:[NSString class]] ? o : 0;
        stateVector.append(s);
    }
    doc->setStateForNewFormElements(stateVector);
}

- (void)scrollToAnchorWithURL:(NSURL *)URL
{
    m_frame->scrollToAnchor(KURL(URL).url().latin1());
}

- (BOOL)scrollOverflowInDirection:(WebScrollDirection)direction granularity:(WebScrollGranularity)granularity
{
    if (!m_frame)
        return NO;
    return m_frame->scrollOverflow((ScrollDirection)direction, (ScrollGranularity)granularity);
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

    WebCorePageState *pageState = [[WebCorePageState alloc] initWithDocument:doc
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

- (BOOL)isSelectionInPasswordField
{
    return m_frame->isSelectionInPasswordField();
}

- (BOOL)isSelectionEditable
{
    return m_frame->selectionController()->isContentEditable();
}

- (BOOL)isSelectionRichlyEditable
{
    return m_frame->selectionController()->isContentRichlyEditable();
}

- (WebSelectionState)selectionState
{
    switch (m_frame->selectionController()->state()) {
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

- (BOOL)mayCopy
{
    return m_frame->mayCopy();
}

- (NSString *)_stringWithDocumentTypeStringAndMarkupString:(NSString *)markupString
{
    return m_frame->documentTypeString() + markupString;
}

- (NSArray *)nodesFromList:(Vector<Node*> *)nodesVector
{
    size_t size = nodesVector->size();
    NSMutableArray *nodes = [NSMutableArray arrayWithCapacity:size];
    for (size_t i = 0; i < size; ++i)
        [nodes addObject:[DOMNode _nodeWith:(*nodesVector)[i]]];
    return nodes;
}

- (NSString *)markupStringFromNode:(DOMNode *)node nodes:(NSArray **)nodes
{
    // FIXME: This is never "for interchange". Is that right? See the next method.
    Vector<Node*> nodeList;
    NSString *markupString = createMarkup([node _node], IncludeNode, nodes ? &nodeList : 0).getNSString();
    if (nodes)
        *nodes = [self nodesFromList:&nodeList];

    return [self _stringWithDocumentTypeStringAndMarkupString:markupString];
}

- (NSString *)markupStringFromRange:(DOMRange *)range nodes:(NSArray **)nodes
{
    // FIXME: This is always "for interchange". Is that right? See the previous method.
    Vector<Node*> nodeList;
    NSString *markupString = createMarkup([range _range], nodes ? &nodeList : 0, AnnotateForInterchange).getNSString();
    if (nodes)
        *nodes = [self nodesFromList:&nodeList];

    return [self _stringWithDocumentTypeStringAndMarkupString:markupString];
}

- (NSString *)selectedString
{
    String text = m_frame->selectedText();
    text.replace('\\', m_frame->backslashAsCurrencySymbol());
    return [[(NSString*)text copy] autorelease];
}

- (NSString *)stringForRange:(DOMRange *)range
{
    String text = plainText([range _range]);
    text.replace('\\', m_frame->backslashAsCurrencySymbol());
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
    m_frame->selectionController()->clear();
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
        RenderView *root = static_cast<RenderView *>(m_frame->document()->renderer());
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
    PlatformGraphicsContext* platformContext = static_cast<PlatformGraphicsContext*>([[NSGraphicsContext currentContext] graphicsPort]);
    ASSERT([[NSGraphicsContext currentContext] isFlipped]);
    GraphicsContext context(platformContext);
    
    [self _setupRootForPrinting:YES];
    
    ASSERT(!m_frame->document() || m_frame->document()->printing() == m_frame->document()->renderer()->view()->printingMode());
    
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
    RenderView* root = static_cast<RenderView *>(m_frame->document()->renderer());
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
        float proposedBottom = min(docHeight, i + printHeight);
        m_frame->adjustPageHeight(&proposedBottom, i, proposedBottom, i);
        currPageHeight = max(1.0f, proposedBottom - i);
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
    
    RenderWidget* renderWidget = node->isWidget() ? static_cast<RenderWidget*>(node) : 0;
    Widget* widget = renderWidget ? renderWidget->widget() : 0;
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
    ASSERT(self == [[self page] mainFrame] || m_frame->ownerElement());

    m_frame->view()->setView(view);
    // FIXME: frame tries to do this too, is it needed?
    if (m_frame->ownerRenderer()) {
        m_frame->ownerRenderer()->setWidget(m_frame->view());
        // Now the render part owns the view, so we don't any more.
    }

    m_frame->view()->initScrollbars();
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
    return [DOMElement _elementWith:m_frame->currentForm()];
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

    RenderObject::NodeInfo nodeInfo = m_frame->nodeInfoAtPoint(IntPoint(point), allow);
    *innerNonSharedNode = [DOMNode _nodeWith:nodeInfo.innerNonSharedNode()];
    *innerNode = [DOMNode _nodeWith:nodeInfo.innerNode()];
    *URLElement = [DOMElement _elementWith:nodeInfo.URLElement()];
}

- (BOOL)isPointInsideSelection:(NSPoint)point
{
    return m_frame->isPointInsideSelection(IntPoint(point));
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
    return m_frame->findString(String(string), forward, caseFlag, wrapFlag);
}

- (unsigned)markAllMatchesForText:(NSString *)string caseSensitive:(BOOL)caseFlag limit:(unsigned)limit
{
    return m_frame->markAllMatchesForText(string, caseFlag, limit);
}

- (BOOL)markedTextMatchesAreHighlighted
{
    return m_frame->markedTextMatchesAreHighlighted();
}

- (void)setMarkedTextMatchesAreHighlighted:(BOOL)doHighlight
{
    m_frame->setMarkedTextMatchesAreHighlighted(doHighlight);
}

- (void)unmarkAllTextMatches
{
    Document *doc = m_frame->document();
    if (!doc) {
        return;
    }
    doc->removeMarkers(DocumentMarker::TextMatch);
}

- (NSArray *)rectsForTextMatches
{
    Document *doc = m_frame->document();
    if (!doc)
        return [NSArray array];
    
    NSMutableArray *result = [NSMutableArray array];
    Vector<IntRect> rects = doc->renderedRectsForMarkers(DocumentMarker::TextMatch);
    unsigned count = rects.size();
    for (unsigned index = 0; index < count; ++index)
        [result addObject:[NSValue valueWithRect:rects[index]]];
    
    return result;
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

- (NSView *)nextKeyView
{
    Document *doc = m_frame->document();
    if (!doc)
        return nil;
    return m_frame->nextKeyView(doc->focusNode(), SelectingNext);
}

- (NSView *)previousKeyView
{
    Document *doc = m_frame->document();
    if (!doc)
        return nil;
    return m_frame->nextKeyView(doc->focusNode(), SelectingPrevious);
}

- (NSView *)nextKeyViewInsideWebFrameViews
{
    Document *doc = m_frame->document();
    if (!doc)
        return nil;
    return m_frame->nextKeyViewInFrameHierarchy(doc->focusNode(), SelectingNext);
}

- (NSView *)previousKeyViewInsideWebFrameViews
{
    Document *doc = m_frame->document();
    if (!doc)
        return nil;
    return m_frame->nextKeyViewInFrameHierarchy(doc->focusNode(), SelectingPrevious);
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string
{
    return [self stringByEvaluatingJavaScriptFromString:string forceUserGesture:true];
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string forceUserGesture:(BOOL)forceUserGesture
{
    m_frame->createEmptyDocument();
    JSValue* result = m_frame->executeScript(0, string, forceUserGesture);
    if (!result || !result->isString())
        return 0;
    JSLock lock;
    return String(result->getString());
}

- (NSAppleEventDescriptor *)aeDescByEvaluatingJavaScriptFromString:(NSString *)string
{
    m_frame->createEmptyDocument();
    JSValue* result = m_frame->executeScript(0, string, true);
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
    // Not [[self DOMDocument] _ownerElement], since our doc is not set up at the start of our own load.
    // FIXME: There really is no guarantee this is an HTML element.
    // For example, it could be something like an SVG foreign object element.
    // Because of that, I believe the cast here is wrong and also the public API
    // of WebKit might have to be changed.
    return (DOMHTMLElement *)[DOMElement _elementWith:m_frame->ownerElement()];
}

- (NSAttributedString *)selectedAttributedString
{
    // FIXME: should be a no-arg version of attributedString() that does this
    Selection selection = m_frame->selectionController()->selection();
    return m_frame->attributedString(selection.start().node(), selection.start().offset(), selection.end().node(), selection.end().offset());
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
    m_frame->revealSelection(RenderLayer::gAlignCenterAlways);
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

- (void)scrollDOMRangeToVisible:(DOMRange *)range
{
    NSRect rangeRect = [self firstRectForDOMRange:range];    
    Node *startNode = [[range startContainer] _node];
        
    if (startNode && startNode->renderer()) {
        RenderLayer *layer = startNode->renderer()->enclosingLayer();
        if (layer)
            layer->scrollRectToVisible(enclosingIntRect(rangeRect), RenderLayer::gAlignToEdgeIfNeeded, RenderLayer::gAlignToEdgeIfNeeded);
    }
}

- (NSImage *)selectionImageForcingWhiteText:(BOOL)forceWhiteText;
{
    return m_frame->selectionImage(forceWhiteText);
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
    return m_frame->referrer();
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
    if (Frame* f = [bridge impl])
        f->setOpener(m_frame);
}

- (NSString *)stringWithData:(NSData *)data
{
    Document* doc = m_frame->document();
    if (!doc)
        return nil;
    Decoder* decoder = doc->decoder();
    if (!decoder)
        return nil;
    return decoder->encoding().decode(reinterpret_cast<const char*>([data bytes]), [data length]);
}

+ (NSString *)stringWithData:(NSData *)data textEncodingName:(NSString *)textEncodingName
{
    NSString* name = textEncodingName ? textEncodingName : @"iso-8859-1";
    return WebCore::TextEncoding(name).decode(reinterpret_cast<const char*>([data bytes]), [data length]);
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

- (void)setIsActive:(BOOL)flag
{
    m_frame->setIsActive(flag);
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
        return NumberOfPendingOrLoadingRequests (doc->docLoader());
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

// FIXME: Not sure what this method is for.  It seems to only be used by plug-ins over in WebKit.
- (NSColor *)selectionColor
{
    return m_frame->isActive() ? [NSColor selectedTextBackgroundColor] : [NSColor secondarySelectedControlColor];
}

- (void)adjustViewSize
{
    FrameView *view = m_frame->view();
    if (view)
        view->adjustViewSize();
}

- (id)accessibilityTree
{
    AXObjectCache::enableAccessibility();
    if (!m_frame || !m_frame->document())
        return nil;
    RenderView* root = static_cast<RenderView *>(m_frame->document()->renderer());
    if (!root)
        return nil;
    return m_frame->document()->axObjectCache()->get(root);
}

- (void)setBaseBackgroundColor:(NSColor *)backgroundColor
{
    if (m_frame && m_frame->view()) {
        NSColor *deviceColor = [backgroundColor colorUsingColorSpaceName:NSDeviceRGBColorSpace];
        Color color = Color(makeRGBA((int)(255 * [deviceColor redComponent]),
                                     (int)(255 * [deviceColor blueComponent]),
                                     (int)(255 * [deviceColor greenComponent]),
                                     (int)(255 * [deviceColor alphaComponent])));
        m_frame->view()->setBaseBackgroundColor(color);
    }
}

- (void)setDrawsBackground:(BOOL)drawsBackground
{
    if (m_frame && m_frame->view())
        m_frame->view()->setTransparent(!drawsBackground);
}

- (void)undoEditing:(id)arg
{
    ASSERT([arg isKindOfClass:[WebCoreEditCommand class]]);
    [arg command]->unapply();
}

- (void)redoEditing:(id)arg
{
    ASSERT([arg isKindOfClass:[WebCoreEditCommand class]]);
    [arg command]->reapply();
}

- (DOMRange *)rangeByExpandingSelectionWithGranularity:(WebBridgeSelectionGranularity)granularity
{
    if (!m_frame->hasSelection())
        return nil;

    // NOTE: The enums *must* match the very similar ones declared in SelectionController.h
    Selection selection(m_frame->selectionController()->selection());
    selection.expandUsingGranularity(static_cast<TextGranularity>(granularity));
    return [DOMRange _rangeWith:selection.toRange().get()];
}

- (DOMRange *)rangeByAlteringCurrentSelection:(WebSelectionAlteration)alteration direction:(WebBridgeSelectionDirection)direction granularity:(WebBridgeSelectionGranularity)granularity
{
    if (!m_frame->hasSelection())
        return nil;

    // NOTE: The enums *must* match the very similar ones declared in SelectionController.h
    SelectionController selectionController;
    selectionController.setSelection(m_frame->selectionController()->selection());
    selectionController.modify(static_cast<SelectionController::EAlter>(alteration), 
                               static_cast<SelectionController::EDirection>(direction), 
                               static_cast<TextGranularity>(granularity));
    return [DOMRange _rangeWith:selectionController.toRange().get()];
}

- (void)alterCurrentSelection:(WebSelectionAlteration)alteration direction:(WebBridgeSelectionDirection)direction granularity:(WebBridgeSelectionGranularity)granularity
{
    if (!m_frame->hasSelection())
        return;

    // NOTE: The enums *must* match the very similar ones declared in SelectionController.h
    SelectionController* selectionController = m_frame->selectionController();
    selectionController->modify(static_cast<SelectionController::EAlter>(alteration), 
                                static_cast<SelectionController::EDirection>(direction), 
                                static_cast<TextGranularity>(granularity), true);
}

- (void)alterCurrentSelection:(WebSelectionAlteration)alteration verticalDistance:(float)verticalDistance
{
    if (!m_frame->hasSelection())
        return;
    SelectionController* selectionController = m_frame->selectionController();
    selectionController->modify(static_cast<SelectionController::EAlter>(alteration), static_cast<int>(verticalDistance), true);
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
    ASSERT(startContainer->document() == endContainer->document());
    
    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    EAffinity affinity = static_cast<EAffinity>(selectionAffinity);
    
    // Non-collapsed ranges are not allowed to start at the end of a line that is wrapped,
    // they start at the beginning of the next line instead
    if (![range collapsed])
        affinity = DOWNSTREAM;
    
    // FIXME: Can we provide extentAffinity?
    VisiblePosition visibleStart(startContainer, [range startOffset], affinity);
    VisiblePosition visibleEnd(endContainer, [range endOffset], SEL_DEFAULT_AFFINITY);
    Selection selection(visibleStart, visibleEnd);
    m_frame->selectionController()->setSelection(selection, closeTyping);
}

- (DOMRange *)selectedDOMRange
{
    return [DOMRange _rangeWith:m_frame->selectionController()->toRange().get()];
}

- (NSRange)convertToNSRange:(Range *)range
{
    int exception = 0;

    if (!range || range->isDetached())
        return NSMakeRange(NSNotFound, 0);

    Element* selectionRoot = m_frame->selectionController()->rootEditableElement();
    Element* scope = selectionRoot ? selectionRoot : m_frame->document()->documentElement();
    
    // our critical assumption is that we are only called by input methods that
    // concentrate on a given area containing the selection.  See comments in convertToDOMRange.
    ASSERT(range->startContainer(exception) == scope || range->startContainer(exception)->isDescendantOf(scope));
    ASSERT(range->endContainer(exception) == scope || range->endContainer(exception)->isDescendantOf(scope));
    
    RefPtr<Range> testRange = new Range(scope->document(), scope, 0, range->startContainer(exception), range->startOffset(exception));
    ASSERT(testRange->startContainer(exception) == scope);
    int startPosition = TextIterator::rangeLength(testRange.get());

    testRange->setEnd(range->endContainer(exception), range->endOffset(exception), exception);
    ASSERT(testRange->startContainer(exception) == scope);
    int endPosition = TextIterator::rangeLength(testRange.get());

    return NSMakeRange(startPosition, endPosition - startPosition);
}

- (PassRefPtr<Range>)convertToDOMRange:(NSRange)nsrange
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
    Element* selectionRoot = m_frame->selectionController()->rootEditableElement();
    Element* scope = selectionRoot ? selectionRoot : m_frame->document()->documentElement();
    return TextIterator::rangeFromLocationAndLength(scope, nsrange.location, nsrange.length);
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
    m_frame->selectionController()->setSelection(Selection([self convertToDOMRange:range].get(), SEL_DEFAULT_AFFINITY));
}

- (NSRange)selectedNSRange
{
    return [self convertToNSRange:m_frame->selectionController()->toRange().get()];
}

- (NSSelectionAffinity)selectionAffinity
{
    return static_cast<NSSelectionAffinity>(m_frame->selectionController()->affinity());
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
    if (!m_frame->hasSelection())
        return;
    
    int exception = 0;

    Range *markedTextRange = m_frame->markedTextRange();
    if (markedTextRange && !markedTextRange->collapsed(exception))
        TypingCommand::deleteKeyPressed(m_frame->document(), NO);
    
    if ([text length] > 0)
        TypingCommand::insertText(m_frame->document(), text, YES);
    
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
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
        VisiblePosition previous = start.previous();
        // FIXME: We sometimes allow deletions at the start of editable roots, like when the caret is in an empty list item.
        if (previous.isNull() || previous.deepEquivalent().node()->rootEditableElement() != startContainer->rootEditableElement())
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

    ASSERT(startContainer->document() == endContainer->document());
    
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
    if (addLeadingSpace)
        if (UChar previousChar = startVisiblePos.previous().characterAfter())
            addLeadingSpace = !m_frame->isCharacterSmartReplaceExempt(previousChar, true);
    
    bool addTrailingSpace = endPos.trailingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull() && !isEndOfParagraph(endVisiblePos);
    if (addTrailingSpace)
        if (UChar thisChar = endVisiblePos.characterAfter())
            addTrailingSpace = !m_frame->isCharacterSmartReplaceExempt(thisChar, false);
    
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

    return [DOMDocumentFragment _documentFragmentWith:createFragmentFromMarkup(m_frame->document(), markupString, baseURLString).get()];
}

- (DOMDocumentFragment *)documentFragmentWithText:(NSString *)text inContext:(DOMRange *)context
{
    return [DOMDocumentFragment _documentFragmentWith:createFragmentFromText([context _range], text).get()];
}

- (DOMDocumentFragment *)documentFragmentWithNodesAsParagraphs:(NSArray *)nodes
{
    if (!m_frame || !m_frame->document())
        return 0;
    
    NSEnumerator *nodeEnum = [nodes objectEnumerator];
    Vector<Node*> nodesVector;
    DOMNode *node;
    while ((node = [nodeEnum nextObject]))
        nodesVector.append([node _node]);
    
    return [DOMDocumentFragment _documentFragmentWith:createFragmentFromNodes(m_frame->document(), nodesVector).get()];
}

- (void)replaceSelectionWithFragment:(DOMDocumentFragment *)fragment selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle
{
    if (!m_frame->hasSelection() || !fragment)
        return;
    
    applyCommand(new ReplaceSelectionCommand(m_frame->document(), [fragment _documentFragment], selectReplacement, smartReplace, matchStyle));
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (void)replaceSelectionWithNode:(DOMNode *)node selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle
{
    DOMDocumentFragment *fragment = [[self DOMDocument] createDocumentFragment];
    [fragment appendChild:node];
    [self replaceSelectionWithFragment:fragment selectReplacement:selectReplacement smartReplace:smartReplace matchStyle:matchStyle];
}

- (void)replaceSelectionWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace
{
    DOMDocumentFragment *fragment = [self documentFragmentWithMarkupString:markupString baseURLString:baseURLString];
    [self replaceSelectionWithFragment:fragment selectReplacement:selectReplacement smartReplace:smartReplace matchStyle:NO];
}

- (void)replaceSelectionWithText:(NSString *)text selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace
{
    [self replaceSelectionWithFragment:[self documentFragmentWithText:text
        inContext:[DOMRange _rangeWith:m_frame->selectionController()->toRange().get()]]
        selectReplacement:selectReplacement smartReplace:smartReplace matchStyle:YES];
}

- (bool)canIncreaseSelectionListLevel
{
    return IncreaseSelectionListLevelCommand::canIncreaseSelectionListLevel(m_frame->document());
}

- (bool)canDecreaseSelectionListLevel
{
    return DecreaseSelectionListLevelCommand::canDecreaseSelectionListLevel(m_frame->document());
}

- (DOMNode *)increaseSelectionListLevel;
{
    if (!m_frame->hasSelection())
        return nil;
    
    Node* newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevel(m_frame->document());
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
    return [DOMNode _nodeWith:newList];
}

- (DOMNode *)increaseSelectionListLevelOrdered;
{
    if (!m_frame->hasSelection())
        return nil;
    
    Node* newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevelOrdered(m_frame->document());
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
    return [DOMNode _nodeWith:newList];
}

- (DOMNode *)increaseSelectionListLevelUnordered;
{
    if (!m_frame->hasSelection())
        return nil;
    
    Node* newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevelUnordered(m_frame->document());
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
    return [DOMNode _nodeWith:newList];
}

- (void)decreaseSelectionListLevel
{
    if (!m_frame->hasSelection())
        return;
    
    DecreaseSelectionListLevelCommand::decreaseSelectionListLevel(m_frame->document());
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (void)insertLineBreak
{
    if (!m_frame->hasSelection())
        return;
    
    TypingCommand::insertLineBreak(m_frame->document());
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (void)insertParagraphSeparator
{
    if (!m_frame->hasSelection())
        return;
    
    TypingCommand::insertParagraphSeparator(m_frame->document());
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (void)insertParagraphSeparatorInQuotedContent
{
    if (!m_frame->hasSelection())
        return;
    
    TypingCommand::insertParagraphSeparatorInQuotedContent(m_frame->document());
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (void)insertText:(NSString *)text selectInsertedText:(BOOL)selectInsertedText
{
    if (!m_frame->hasSelection())
        return;
    
    TypingCommand::insertText(m_frame->document(), text, selectInsertedText);
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (void)setSelectionToDragCaret
{
    m_frame->selectionController()->setSelection(m_frame->dragCaretController()->selection());
}

- (void)moveSelectionToDragCaret:(DOMDocumentFragment *)selectionFragment smartMove:(BOOL)smartMove
{
    applyCommand(new MoveSelectionCommand([selectionFragment _documentFragment], m_frame->dragCaretController()->base(), smartMove));
}

- (VisiblePosition)_visiblePositionForPoint:(NSPoint)point
{
    IntPoint outerPoint(point);
    Node* node = m_frame->nodeInfoAtPoint(outerPoint, true).innerNode();
    if (!node)
        return VisiblePosition();
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();
    FrameView* outerView = m_frame->view();
    FrameView* innerView = node->document()->view();
    IntPoint innerPoint = innerView->windowToContents(outerView->contentsToWindow(outerPoint));
    return renderer->positionForCoordinates(innerPoint.x(), innerPoint.y());
}

- (void)moveDragCaretToPoint:(NSPoint)point
{   
    Selection dragCaret([self _visiblePositionForPoint:point]);
    m_frame->dragCaretController()->setSelection(dragCaret);
}

- (void)removeDragCaret
{
    m_frame->dragCaretController()->clear();
}

- (DOMRange *)dragCaretDOMRange
{
    return [DOMRange _rangeWith:m_frame->dragCaretController()->toRange().get()];
}

- (BOOL)isDragCaretRichlyEditable
{
    return m_frame->dragCaretController()->isContentRichlyEditable();
}

- (DOMRange *)editableDOMRangeForPoint:(NSPoint)point
{
    VisiblePosition position = [self _visiblePositionForPoint:point];
    return position.isNull() ? nil : [DOMRange _rangeWith:Selection(position).toRange().get()];
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
    if (!m_frame->hasSelection())
        return;
    
    applyCommand(new DeleteSelectionCommand(m_frame->document(), smartDelete));
}

- (void)deleteKeyPressedWithSmartDelete:(BOOL)smartDelete granularity:(WebBridgeSelectionGranularity)granularity
{
    if (!m_frame || !m_frame->document())
        return;
    
    TypingCommand::deleteKeyPressed(m_frame->document(), smartDelete, static_cast<TextGranularity>(granularity));
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (void)forwardDeleteKeyPressedWithSmartDelete:(BOOL)smartDelete granularity:(WebBridgeSelectionGranularity)granularity
{
    if (!m_frame || !m_frame->document())
        return;
    
    TypingCommand::forwardDeleteKeyPressed(m_frame->document(), smartDelete, static_cast<TextGranularity>(granularity));
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (DOMCSSStyleDeclaration *)typingStyle
{
    if (!m_frame || !m_frame->typingStyle())
        return nil;
    return [DOMCSSStyleDeclaration _CSSStyleDeclarationWith:m_frame->typingStyle()->copy().get()];
}

- (void)setTypingStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction
{
    if (!m_frame)
        return;
    m_frame->computeAndSetTypingStyle([style _CSSStyleDeclaration], static_cast<EditAction>(undoAction));
}

- (void)applyStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction
{
    if (!m_frame)
        return;
    m_frame->applyStyle([style _CSSStyleDeclaration], static_cast<EditAction>(undoAction));
}

- (void)applyParagraphStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction
{
    if (!m_frame)
        return;
    m_frame->applyParagraphStyle([style _CSSStyleDeclaration], static_cast<EditAction>(undoAction));
}

- (void)indent
{
    if (!m_frame)
        return;
    m_frame->indent();
}

- (void)outdent
{
    if (!m_frame)
        return;
    m_frame->outdent();
}

- (BOOL)selectionStartHasStyle:(DOMCSSStyleDeclaration *)style
{
    if (!m_frame)
        return NO;
    return m_frame->selectionStartHasStyle([style _CSSStyleDeclaration]);
}

- (NSCellStateValue)selectionHasStyle:(DOMCSSStyleDeclaration *)style
{
    if (!m_frame)
        return NSOffState;
    switch (m_frame->selectionHasStyle([style _CSSStyleDeclaration])) {
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
    // FIXME: remove this NSWritingDirection cast once <rdar://problem/4509035> is fixed
    return m_frame ? m_frame->baseWritingDirectionForSelectionStart() : (NSWritingDirection)NSWritingDirectionLeftToRight;
}

- (BOOL)eventMayStartDrag:(NSEvent *)event
{
    return m_frame ? m_frame->eventMayStartDrag(event) : NO;
}

static IntPoint globalPoint(NSWindow* window, NSPoint windowPoint)
{
    return IntPoint(flipScreenPoint([window convertBaseToScreen:windowPoint]));
}

static PlatformMouseEvent createMouseEventFromDraggingInfo(NSWindow* window, id <NSDraggingInfo> info)
{
    // FIXME: Fake modifier keys here.
    // [info draggingLocation] is in window coords
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
    return m_frame->mayDHTMLCut();
}

- (BOOL)mayDHTMLCopy
{
    return m_frame->mayDHTMLCopy();
}

- (BOOL)mayDHTMLPaste
{
    return m_frame->mayDHTMLPaste();
}

- (BOOL)tryDHTMLCut
{
    return m_frame->tryDHTMLCut();
}

- (BOOL)tryDHTMLCopy
{
    return m_frame->tryDHTMLCopy();
}

- (BOOL)tryDHTMLPaste
{
    return m_frame->tryDHTMLPaste();
}

- (DOMRange *)rangeOfCharactersAroundCaret
{
    if (!m_frame)
        return nil;
        
    Selection selection(m_frame->selectionController()->selection());
    if (!selection.isCaret())
        return nil;

    VisiblePosition caret(selection.visibleStart());
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

// FIXME: The following 2 functions are copied from AppKit. It would be best share code.

// MF:!!! For now we will use static character sets for the computation, but we should eventually probably make these keys in the language dictionaries.
// MF:!!! The following characters (listed with their nextstep encoding values) were in the preSmartTable in the old text objet, but aren't yet in the new text object: NS_FIGSPACE (0x80), exclamdown (0xa1), sterling (0xa3), yen (0xa5), florin (0xa6) section (0xa7), currency (0xa8), quotesingle (0xa9), quotedblleft (0xaa), guillemotleft (0xab), guilsinglleft (0xac), endash (0xb1), quotesinglbase (0xb8), quotedblbase (0xb9), questiondown (0xbf), emdash (0xd0), plusminus (0xd1).
// MF:!!! The following characters (listed with their nextstep encoding values) were in the postSmartTable in the old text objet, but aren't yet in the new text object: NS_FIGSPACE (0x80), cent (0xa2), guilsinglright (0xad), registered (0xb0), dagger (0xa2), daggerdbl (0xa3), endash (0xb1), quotedblright (0xba), guillemotright (0xbb), perthousand (0xbd), onesuperior (0xc0), twosuperior (0xc9), threesuperior (0xcc), emdash (0xd0), ordfeminine (0xe3), ordmasculine (0xeb).
// MF:!!! Another difference in both of these sets from the old text object is we include all the whitespace in whitespaceAndNewlineCharacterSet.
#define _preSmartString @"([\"\'#$/-`{"
#define _postSmartString @")].,;:?\'!\"%*-/}"

static NSCharacterSet *_getPreSmartSet(void)
{
    static NSMutableCharacterSet *_preSmartSet = nil;
    if (!_preSmartSet) {
        _preSmartSet = [[NSMutableCharacterSet characterSetWithCharactersInString:_preSmartString] retain];
        [_preSmartSet formUnionWithCharacterSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        // Adding CJK ranges
        [_preSmartSet addCharactersInRange:NSMakeRange(0x1100, 256)]; // Hangul Jamo (0x1100 - 0x11FF)
        [_preSmartSet addCharactersInRange:NSMakeRange(0x2E80, 352)]; // CJK & Kangxi Radicals (0x2E80 - 0x2FDF)
        [_preSmartSet addCharactersInRange:NSMakeRange(0x2FF0, 464)]; // Ideograph Descriptions, CJK Symbols, Hiragana, Katakana, Bopomofo, Hangul Compatibility Jamo, Kanbun, & Bopomofo Ext (0x2FF0 - 0x31BF)
        [_preSmartSet addCharactersInRange:NSMakeRange(0x3200, 29392)]; // Enclosed CJK, CJK Ideographs (Uni Han & Ext A), & Yi (0x3200 - 0xA4CF)
        [_preSmartSet addCharactersInRange:NSMakeRange(0xAC00, 11183)]; // Hangul Syllables (0xAC00 - 0xD7AF)
        [_preSmartSet addCharactersInRange:NSMakeRange(0xF900, 352)]; // CJK Compatibility Ideographs (0xF900 - 0xFA5F)
        [_preSmartSet addCharactersInRange:NSMakeRange(0xFE30, 32)]; // CJK Compatibility From (0xFE30 - 0xFE4F)
        [_preSmartSet addCharactersInRange:NSMakeRange(0xFF00, 240)]; // Half/Full Width Form (0xFF00 - 0xFFEF)
        [_preSmartSet addCharactersInRange:NSMakeRange(0x20000, 0xA6D7)]; // CJK Ideograph Exntension B
        [_preSmartSet addCharactersInRange:NSMakeRange(0x2F800, 0x021E)]; // CJK Compatibility Ideographs (0x2F800 - 0x2FA1D)
    }
    return _preSmartSet;
}

static NSCharacterSet *_getPostSmartSet(void)
{
    static NSMutableCharacterSet *_postSmartSet = nil;
    if (!_postSmartSet) {
        _postSmartSet = [[NSMutableCharacterSet characterSetWithCharactersInString:_postSmartString] retain];
        [_postSmartSet formUnionWithCharacterSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        [_postSmartSet addCharactersInRange:NSMakeRange(0x1100, 256)]; // Hangul Jamo (0x1100 - 0x11FF)
        [_postSmartSet addCharactersInRange:NSMakeRange(0x2E80, 352)]; // CJK & Kangxi Radicals (0x2E80 - 0x2FDF)
        [_postSmartSet addCharactersInRange:NSMakeRange(0x2FF0, 464)]; // Ideograph Descriptions, CJK Symbols, Hiragana, Katakana, Bopomofo, Hangul Compatibility Jamo, Kanbun, & Bopomofo Ext (0x2FF0 - 0x31BF)
        [_postSmartSet addCharactersInRange:NSMakeRange(0x3200, 29392)]; // Enclosed CJK, CJK Ideographs (Uni Han & Ext A), & Yi (0x3200 - 0xA4CF)
        [_postSmartSet addCharactersInRange:NSMakeRange(0xAC00, 11183)]; // Hangul Syllables (0xAC00 - 0xD7AF)
        [_postSmartSet addCharactersInRange:NSMakeRange(0xF900, 352)]; // CJK Compatibility Ideographs (0xF900 - 0xFA5F)
        [_postSmartSet addCharactersInRange:NSMakeRange(0xFE30, 32)]; // CJK Compatibility From (0xFE30 - 0xFE4F)
        [_postSmartSet addCharactersInRange:NSMakeRange(0xFF00, 240)]; // Half/Full Width Form (0xFF00 - 0xFFEF)
        [_postSmartSet addCharactersInRange:NSMakeRange(0x20000, 0xA6D7)]; // CJK Ideograph Exntension B
        [_postSmartSet addCharactersInRange:NSMakeRange(0x2F800, 0x021E)]; // CJK Compatibility Ideographs (0x2F800 - 0x2FA1D)        
        [_postSmartSet formUnionWithCharacterSet:[NSCharacterSet punctuationCharacterSet]];
    }
    return _postSmartSet;
}

- (BOOL)isCharacterSmartReplaceExempt:(unichar)c isPreviousCharacter:(BOOL)isPreviousCharacter
{
    return [isPreviousCharacter ? _getPreSmartSet() : _getPostSmartSet() characterIsMember:c];
}

- (BOOL)getData:(NSData **)data andResponse:(NSURLResponse **)response forURL:(NSURL *)URL
{
    Document* doc = [self impl]->document();
    if (!doc)
        return NO;

    CachedResource* resource = doc->docLoader()->cachedResource([URL absoluteString]);
    if (!resource)
        return NO;

    *data = resource->allData();
    *response = resource->response();
    return YES;
}

- (void)getAllResourceDatas:(NSArray **)datas andResponses:(NSArray **)responses
{
    Document* doc = [self impl]->document();
    if (!doc) {
        NSArray* emptyArray = [NSArray array];
        *datas = emptyArray;
        *responses = emptyArray;
        return;
    }

    const HashMap<String, CachedResource*>& allResources = doc->docLoader()->allCachedResources();

    NSMutableArray *d = [[NSMutableArray alloc] initWithCapacity:allResources.size()];
    NSMutableArray *r = [[NSMutableArray alloc] initWithCapacity:allResources.size()];

    HashMap<String, CachedResource*>::const_iterator end = allResources.end();
    for (HashMap<String, CachedResource*>::const_iterator it = allResources.begin(); it != end; ++it) {
        [d addObject:it->second->allData()];
        [r addObject:it->second->response()];
    }

    *datas = [d autorelease];
    *responses = [r autorelease];
}

- (BOOL)canProvideDocumentSource
{
    String mimeType = m_frame->responseMIMEType();
    
    if (WebCore::DOMImplementation::isTextMIMEType(mimeType) ||
        Image::supportsType(mimeType) ||
        PlugInInfoStore::supportsMIMEType(mimeType))
        return NO;
    
    return YES;
}

- (BOOL)canSaveAsWebArchive
{
    // Currently, all documents that we can view source for
    // (HTML and XML documents) can also be saved as web archives
    return [self canProvideDocumentSource];
}

- (BOOL)containsPlugins
{
    return m_frame->containsPlugins();
}

- (void)setInViewSourceMode:(BOOL)flag
{
    m_frame->setInViewSourceMode(flag);
}

- (BOOL)inViewSourceMode
{
    return m_frame->inViewSourceMode();
}

- (void)setProhibitsScrolling:(BOOL)prohibits
{
    m_frame->setProhibitsScrolling(prohibits);
}

- (BOOL)isMainFrame
{
    return m_frame->page()->mainFrame() == m_frame;
}

- (void)setFrameLoaderClient:(id<WebFrameLoaderClient>)client
{
    if (!m_frame)
        return;
    m_frame->frameLoader()->setFrameLoaderClient(client);
}

- (WebCoreFrameLoader *)frameLoader
{
    if (!m_frame)
        return 0;
    return m_frame->frameLoader();
}

static NSString *stringByCollapsingNonPrintingCharacters(NSString *string)
{
    NSMutableString *result = [NSMutableString string];
    static NSCharacterSet *charactersToTurnIntoSpaces = nil;
    static NSCharacterSet *charactersToNotTurnIntoSpaces = nil;
    
    if (charactersToTurnIntoSpaces == nil) {
        NSMutableCharacterSet *set = [[NSMutableCharacterSet alloc] init];
        [set addCharactersInRange:NSMakeRange(0x00, 0x21)];
        [set addCharactersInRange:NSMakeRange(0x7F, 0x01)];
        charactersToTurnIntoSpaces = [set copy];
        [set release];
        charactersToNotTurnIntoSpaces = [[charactersToTurnIntoSpaces invertedSet] retain];
    }
    
    unsigned length = [string length];
    unsigned position = 0;
    while (position != length) {
        NSRange nonSpace = [string rangeOfCharacterFromSet:charactersToNotTurnIntoSpaces
                                                 options:0 range:NSMakeRange(position, length - position)];
        if (nonSpace.location == NSNotFound) {
            break;
        }
        
        NSRange space = [string rangeOfCharacterFromSet:charactersToTurnIntoSpaces
                                              options:0 range:NSMakeRange(nonSpace.location, length - nonSpace.location)];
        if (space.location == NSNotFound) {
            space.location = length;
        }
        
        if (space.location > nonSpace.location) {
            if (position != 0) {
                [result appendString:@" "];
            }
            [result appendString:[string substringWithRange:
                NSMakeRange(nonSpace.location, space.location - nonSpace.location)]];
        }
        
        position = space.location;
    }
    
    return result;
}

- (void)setTitle:(NSString *)title
{
    if (!m_frame)
        return;
    m_frame->frameLoader()->documentLoader()->setTitle(stringByCollapsingNonPrintingCharacters(title));
}

- (void)didFirstLayout
{
    if (!m_frame)
        return;
    m_frame->frameLoader()->didFirstLayout();
}

- (void)notifyIconChanged:(NSURL*)iconURL
{
    if (!m_frame)
        return;
    m_frame->frameLoader()->notifyIconChanged(iconURL);
}

- (NSURL*)originalRequestURL
{
    return [m_frame->frameLoader()->activeDocumentLoader()->initialRequest() URL];
}

- (BOOL)isLoadTypeReload
{
    return m_frame->frameLoader()->loadType() == FrameLoadTypeReload;
}

- (void)frameDetached
{
    m_frame->frameLoader()->stopLoading();
    m_frame->frameLoader()->detachFromParent();
}

- (void)tokenizerProcessedData
{
    m_frame->frameLoader()->checkLoadComplete();
}

- (void)receivedData:(NSData *)data textEncodingName:(NSString *)textEncodingName
{
    // Set the encoding. This only needs to be done once, but it's harmless to do it again later.
    NSString *encoding = m_frame ? m_frame->frameLoader()->documentLoader()->overrideEncoding() : nil;
    bool userChosen = encoding != nil;
    if (!encoding)
        encoding = textEncodingName;
    [self setEncoding:encoding userChosen:userChosen];
    [self addData:data];
}

- (id <WebCoreResourceHandle>)startLoadingResource:(id <WebCoreResourceLoader>)resourceLoader withMethod:(NSString *)method URL:(NSURL *)URL customHeaders:(NSDictionary *)customHeaders
{
    // If we are no longer attached to a Page, this must be an attempted load from an
    // onUnload handler, so let's just block it.
    if ([self page] == nil)
        return nil;
    
    // Since this is a subresource, we can load any URL (we ignore the return value).
    // But we still want to know whether we should hide the referrer or not, so we call the canLoadURL method.
    BOOL hideReferrer;
    [self canLoadURL:URL fromReferrer:[self referrer] hideReferrer:&hideReferrer];
    
    return SubresourceLoader::create(m_frame, resourceLoader,
        method, URL, customHeaders, hideReferrer ? nil : [self referrer]);
}

- (void)objectLoadedFromCacheWithURL:(NSURL *)URL response:(NSURLResponse *)response data:(NSData *)data
{
    if (!m_frame)
        return;
        
    // FIXME: If the WebKit client changes or cancels the request, WebCore does not respect this and continues the load.
    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:URL];
    NSError *error;
    id identifier;
    m_frame->frameLoader()->requestFromDelegate(request, identifier, error);    
    m_frame->frameLoader()->sendRemainingDelegateMessages(identifier, response, [data length], error);
    [request release];
}

- (id <WebCoreResourceHandle>)startLoadingResource:(id <WebCoreResourceLoader>)resourceLoader withMethod:(NSString *)method URL:(NSURL *)URL
    customHeaders:(NSDictionary *)customHeaders postData:(NSArray *)postData
{
    // If we are no longer attached to a Page, this must be an attempted load from an
    // onUnload handler, so let's just block it.
    if ([self page] == nil)
        return nil;
    
    // Since this is a subresource, we can load any URL (we ignore the return value).
    // But we still want to know whether we should hide the referrer or not, so we call the canLoadURL method.
    BOOL hideReferrer;
    [self canLoadURL:URL fromReferrer:[self referrer] hideReferrer:&hideReferrer];
    
    return SubresourceLoader::create(m_frame, resourceLoader,
        method, URL, customHeaders, postData, hideReferrer ? nil : [self referrer]);
}

- (void)reportClientRedirectToURL:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date lockHistory:(BOOL)lockHistory isJavaScriptFormAction:(BOOL)isJavaScriptFormAction
{   
    m_frame->frameLoader()->clientRedirected(URL, seconds, date, lockHistory, isJavaScriptFormAction);
}

- (void)reportClientRedirectCancelled:(BOOL)cancelWithLoadInProgress
{
    m_frame->frameLoader()->clientRedirectCancelledOrFinished(cancelWithLoadInProgress);
}

- (NSData *)syncLoadResourceWithMethod:(NSString *)method URL:(NSURL *)URL customHeaders:(NSDictionary *)requestHeaders postData:(NSArray *)postData finalURL:(NSURL **)finalURL responseHeaders:(NSDictionary **)responseHeaderDict statusCode:(int *)statusCode
{
    // Since this is a subresource, we can load any URL (we ignore the return value).
    // But we still want to know whether we should hide the referrer or not, so we call the canLoadURL method.
    BOOL hideReferrer;
    [self canLoadURL:URL fromReferrer:[self referrer] hideReferrer:&hideReferrer];
    
    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:URL];
    [request setTimeoutInterval:10];
    
    // setHTTPMethod is not called for GET requests to work around <rdar://4464032>.
    if (![method isEqualToString:@"GET"])
        [request setHTTPMethod:method];
    
    if (postData)        
        webSetHTTPBody(request, postData);
    
    NSEnumerator *e = [requestHeaders keyEnumerator];
    NSString *key;
    while ((key = (NSString *)[e nextObject]) != nil) {
        [request addValue:[requestHeaders objectForKey:key] forHTTPHeaderField:key];
    }
    
    if (isConditionalRequest(request))
        [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];
    else
        [request setCachePolicy:[m_frame->frameLoader()->documentLoader()->request() cachePolicy]];
    
    if (!hideReferrer)
        setHTTPReferrer(request, [self referrer]);
    
    WebCorePageBridge *page = [self page];
    [request setMainDocumentURL:[[[page mainFrame] impl]->frameLoader()->documentLoader()->request() URL]];
    [request setValue:[self userAgentForURL:[request URL]] forHTTPHeaderField:@"User-Agent"];
    
    NSError *error = nil;
    id identifier = nil;    
    NSURLRequest *newRequest = m_frame->frameLoader()->requestFromDelegate(request, identifier, error);
    
    NSURLResponse *response = nil;
    NSData *result = nil;
    if (error == nil) {
        ASSERT(newRequest != nil);
        result = [NSURLConnection sendSynchronousRequest:newRequest returningResponse:&response error:&error];
    }
    
    if (error == nil) {
        *finalURL = [response URL];
        if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
            NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response; 
            *responseHeaderDict = [httpResponse allHeaderFields];
            *statusCode = [httpResponse statusCode];
        } else {
            *responseHeaderDict = [NSDictionary dictionary];
            *statusCode = 200;
        }
    } else {
        *finalURL = URL;
        *responseHeaderDict = [NSDictionary dictionary];
        if ([error domain] == NSURLErrorDomain) {
            *statusCode = [error code];
        } else {
            *statusCode = 404;
        }
    }
    
    m_frame->frameLoader()->sendRemainingDelegateMessages(identifier, response, [result length], error);
    [request release];
    
    return result;
}
// -------------------

- (NSString *)incomingReferrer
{
    return [m_frame->frameLoader()->documentLoader()->request() valueForHTTPHeaderField:@"Referer"];
}

- (BOOL)isReloading
{
    return [m_frame->frameLoader()->documentLoader()->request() cachePolicy] == NSURLRequestReloadIgnoringCacheData;
}

- (void)handledOnloadEvents
{
    [m_frame->frameLoader()->client() _dispatchDidHandleOnloadEventsForFrame];
}

- (NSURLResponse *)mainResourceURLResponse
{
    return m_frame->frameLoader()->documentLoader()->response();
}

- (void)loadEmptyDocumentSynchronously
{
    if (!m_frame)
        return;
        
    NSURL *url = [[NSURL alloc] initWithString:@""];
    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:url];
    m_frame->frameLoader()->load(request);
    [request release];
    [url release];
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

@end

@implementation WebCoreFrameBridge (WebCoreInternalUse)

- (FrameMac*)impl
{
    return m_frame;
}

@end
