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
#import "ClipboardMac.h"
#import "DOMImplementation.h"
#import "DOMInternal.h"
#import "TextResourceDecoder.h"
#import "DeleteSelectionCommand.h"
#import "DocLoader.h"
#import "DocumentFragment.h"
#import "DocumentType.h"
#import "Editor.h"
#import "EditorClient.h"
#import "EventHandler.h"
#import "FloatRect.h"
#import "FontData.h"
#import "FrameLoader.h"
#import "FrameLoaderClient.h"
#import "FrameMac.h"
#import "FrameTree.h"
#import "FrameView.h"
#import "GraphicsContext.h"
#import "HTMLDocument.h"
#import "HTMLFormElement.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
#import "HitTestResult.h"
#import "Image.h"
#import "LoaderNSURLExtras.h"
#import "ModifySelectionListLevel.h"
#import "MoveSelectionCommand.h"
#import "Page.h"
#import "PlatformMouseEvent.h"
#import "PlugInInfoStore.h"
#import "RenderImage.h"
#import "RenderPart.h"
#import "RenderTreeAsText.h"
#import "RenderView.h"
#import "RenderWidget.h"
#import "ReplaceSelectionCommand.h"
#import "ResourceRequest.h"
#import "RetainPtr.h"
#import "Screen.h"
#import "SelectionController.h"
#import "TextEncoding.h"
#import "TextIterator.h"
#import "TypingCommand.h"
#import "WebCoreSystemInterface.h"
#import "WebCoreViewFactory.h"
#import "DocumentLoader.h"
#import "FormDataStreamMac.h"
#import "SubresourceLoader.h"
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

@class NSView;

using namespace std;
using namespace WebCore;
using namespace HTMLNames;

using KJS::ArrayInstance;
using KJS::BooleanType;
using KJS::DateInstance;
using KJS::ExecState;
using KJS::GetterSetterType;
using KJS::JSLock;
using KJS::JSObject;
using KJS::JSValue;
using KJS::NullType;
using KJS::NumberType;
using KJS::ObjectType;
using KJS::SavedBuiltins;
using KJS::SavedProperties;
using KJS::StringType;
using KJS::UndefinedType;
using KJS::UnspecifiedType;
using KJS::Window;

using KJS::Bindings::RootObject;

NSString *WebCorePageCacheStateKey = @"WebCorePageCacheState";

static RootObject* createRootObject(void* nativeHandle)
{
    NSView *view = (NSView *)nativeHandle;
    WebCoreFrameBridge *bridge = [[WebCoreViewFactory sharedFactory] bridgeForView:view];
    if (!bridge)
        return 0;

    FrameMac* frame = [bridge _frame];
    RootObject* rootObject = new RootObject(nativeHandle, frame->scriptProxy()->interpreter());
    frame->addPluginRootObject(rootObject); // The Frame owns the RootObject.
    return rootObject;
}

static pthread_t mainThread = 0;

static void updateRenderingForBindings(ExecState* exec, JSObject* rootObject)
{
    if (pthread_self() != mainThread)
        return;
        
    if (!rootObject)
        return;
        
    Window* window = static_cast<Window*>(rootObject);
    if (!window)
        return;
        
    Document* doc = static_cast<Document*>(window->frame()->document());
    if (doc)
        doc->updateRendering();
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

static inline WebCoreFrameBridge *bridge(Frame *frame)
{
    if (!frame)
        return nil;
    return Mac(frame)->bridge();
}

- (NSString *)domain
{
    Document *doc = m_frame->document();
    if (doc)
        return doc->domain();
    return nil;
}

+ (WebCoreFrameBridge *)bridgeForDOMDocument:(DOMDocument *)document
{
    return bridge([document _document]->frame());
}

- (id)init
{
    static bool initializedKJS;
    if (!initializedKJS) {
        initializedKJS = true;

        mainThread = pthread_self();
        RootObject::setCreateRootObject(createRootObject);
        KJS::Bindings::Instance::setDidExecuteFunction(updateRenderingForBindings);
    }
    
    if (!(self = [super init]))
        return nil;

    _shouldCreateRenderers = YES;
    return self;
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
    [self clearFrame];
    _closed = YES;
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
        m_frame->loader()->addData((const char *)[data bytes], [data length]);
    }
}

- (BOOL)scrollOverflowInDirection:(WebScrollDirection)direction granularity:(WebScrollGranularity)granularity
{
    if (!m_frame)
        return NO;
    return m_frame->eventHandler()->scrollOverflow((ScrollDirection)direction, (ScrollGranularity)granularity);
}

- (void)clearFrame
{
    m_frame = 0;
}

- (void)createFrameViewWithNSView:(NSView *)view marginWidth:(int)mw marginHeight:(int)mh
{
    // If we own the view, delete the old one - otherwise the render m_frame will take care of deleting the view.
    if (m_frame)
        m_frame->setView(0);

    FrameView* frameView = new FrameView(m_frame);
    m_frame->setView(frameView);
    frameView->deref();

    frameView->setView(view);
    if (mw >= 0)
        frameView->setMarginWidth(mw);
    if (mh >= 0)
        frameView->setMarginHeight(mh);
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

- (void)reapplyStylesForDeviceType:(WebCoreDeviceType)deviceType
{
    if (m_frame->view())
        m_frame->view()->setMediaType(deviceType == WebCoreDeviceScreen ? "screen" : "print");
    Document *doc = m_frame->document();
    if (doc)
        doc->setPrinting(deviceType == WebCoreDevicePrinter);
    m_frame->reparseConfiguration();
}

- (void)forceLayoutAdjustingViewSize:(BOOL)flag
{
    m_frame->forceLayout();
    if (flag)
        m_frame->view()->adjustViewSize();
}

- (void)forceLayoutWithMinimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustingViewSize:(BOOL)flag
{
    m_frame->forceLayoutWithPageWidthRange(minPageWidth, maxPageWidth);
    if (flag)
        m_frame->view()->adjustViewSize();
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
    
    m_frame->paint(&context, enclosingIntRect(rect));
}

// Used by pagination code called from AppKit when a standalone web page is printed.
- (NSArray*)computePageRectsWithPrintWidthScaleFactor:(float)printWidthScaleFactor printHeight:(float)printHeight
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

    if (!m_frame || !m_frame->document() || !m_frame->view()) return pages;
    RenderView* root = static_cast<RenderView *>(m_frame->document()->renderer());
    if (!root) return pages;
    
    FrameView* view = m_frame->view();
    if (!view)
        return pages;

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
    
    return pages;
}

// This is to support the case where a webview is embedded in the view that's being printed
- (void)adjustPageHeightNew:(float *)newBottom top:(float)oldTop bottom:(float)oldBottom limit:(float)bottomLimit
{
    m_frame->adjustPageHeight(newBottom, oldTop, oldBottom, bottomLimit);
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

- (void)installInFrame:(NSView *)view
{
    // If this isn't the main frame, it must have a render m_frame set, or it
    // won't ever get installed in the view hierarchy.
    ASSERT(m_frame == m_frame->page()->mainFrame() || m_frame->ownerElement());

    m_frame->view()->setView(view);
    // FIXME: frame tries to do this too, is it needed?
    if (m_frame->ownerRenderer()) {
        m_frame->ownerRenderer()->setWidget(m_frame->view());
        // Now the render part owns the view, so we don't any more.
    }

    m_frame->view()->initScrollbars();
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

- (NSURL *)URLWithAttributeString:(NSString *)string
{
    Document *doc = m_frame->document();
    if (!doc)
        return nil;
    // FIXME: is parseURL appropriate here?
    DeprecatedString rel = parseURL(string).deprecatedString();
    return KURL(doc->completeURL(rel)).getNSURL();
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag startInSelection:(BOOL)startInSelection
{
    return m_frame->findString(string, forward, caseFlag, wrapFlag, startInSelection);
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

- (void)setTextSizeMultiplier:(float)multiplier
{
    int newZoomFactor = (int)rint(multiplier * 100);
    if (m_frame->zoomFactor() == newZoomFactor) {
        return;
    }
    m_frame->setZoomFactor(newZoomFactor);
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string
{
    return [self stringByEvaluatingJavaScriptFromString:string forceUserGesture:true];
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string forceUserGesture:(BOOL)forceUserGesture
{
    m_frame->loader()->createEmptyDocument();
    JSValue* result = m_frame->loader()->executeScript(0, string, forceUserGesture);
    if (!result || !result->isString())
        return 0;
    JSLock lock;
    return String(result->getString());
}

- (NSAppleEventDescriptor *)aeDescByEvaluatingJavaScriptFromString:(NSString *)string
{
    m_frame->loader()->createEmptyDocument();
    JSValue* result = m_frame->loader()->executeScript(0, string, true);
    if (!result) // FIXME: pass errors
        return 0;
    JSLock lock;
    return aeDescFromJSValue(m_frame->scriptProxy()->interpreter()->globalExec(), result);
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

- (NSURL *)baseURL
{
    return m_frame->loader()->completeURL(m_frame->document()->baseURL()).getNSURL();
}

- (NSString *)stringWithData:(NSData *)data
{
    Document* doc = m_frame->document();
    if (!doc)
        return nil;
    TextResourceDecoder* decoder = doc->decoder();
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

- (NSString *)renderTreeAsExternalRepresentation
{
    return externalRepresentation(m_frame->renderer()).getNSString();
}

- (void)setShouldCreateRenderers:(BOOL)f
{
    _shouldCreateRenderers = f;
}

- (BOOL)shouldCreateRenderers
{
    return _shouldCreateRenderers;
}

- (NSColor *)selectionColor
{
    return m_frame->isActive() ? [NSColor selectedTextBackgroundColor] : [NSColor secondarySelectedControlColor];
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

- (DOMRange *)rangeByAlteringCurrentSelection:(SelectionController::EAlteration)alteration direction:(SelectionController::EDirection)direction granularity:(TextGranularity)granularity
{
    if (m_frame->selectionController()->isNone())
        return nil;

    // NOTE: The enums *must* match the very similar ones declared in SelectionController.h
    SelectionController selectionController;
    selectionController.setSelection(m_frame->selectionController()->selection());
    selectionController.modify(alteration, direction, granularity);
    return [DOMRange _rangeWith:selectionController.toRange().get()];
}

- (void)alterCurrentSelection:(SelectionController::EAlteration)alteration verticalDistance:(float)verticalDistance
{
    if (m_frame->selectionController()->isNone())
        return;
    SelectionController* selectionController = m_frame->selectionController();
    selectionController->modify(alteration, static_cast<int>(verticalDistance), true);
}

- (TextGranularity)selectionGranularity
{
    // NOTE: The enums *must* match the very similar ones declared in SelectionController.h
    return m_frame->selectionGranularity();
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
    if (m_frame->selectionController()->isNone())
        return;
    
    int exception = 0;

    Range *markedTextRange = m_frame->markedTextRange();
    if (markedTextRange && !markedTextRange->collapsed(exception))
        TypingCommand::deleteKeyPressed(m_frame->document(), NO);
    
    if ([text length] > 0)
        TypingCommand::insertText(m_frame->document(), text, YES);
    
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
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
    if (m_frame->selectionController()->isNone() || !fragment)
        return;
    
    applyCommand(new ReplaceSelectionCommand(m_frame->document(), [fragment _documentFragment], selectReplacement, smartReplace, matchStyle));
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (void)replaceSelectionWithNode:(DOMNode *)node selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle
{
    DOMDocumentFragment *fragment = [DOMDocumentFragment _documentFragmentWith:m_frame->document()->createDocumentFragment().get()];
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
    if (m_frame->selectionController()->isNone())
        return nil;
    
    Node* newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevel(m_frame->document());
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
    return [DOMNode _nodeWith:newList];
}

- (DOMNode *)increaseSelectionListLevelOrdered;
{
    if (m_frame->selectionController()->isNone())
        return nil;
    
    Node* newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevelOrdered(m_frame->document());
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
    return [DOMNode _nodeWith:newList];
}

- (DOMNode *)increaseSelectionListLevelUnordered;
{
    if (m_frame->selectionController()->isNone())
        return nil;
    
    Node* newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevelUnordered(m_frame->document());
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
    return [DOMNode _nodeWith:newList];
}

- (void)decreaseSelectionListLevel
{
    if (m_frame->selectionController()->isNone())
        return;
    
    DecreaseSelectionListLevelCommand::decreaseSelectionListLevel(m_frame->document());
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (void)insertLineBreak
{
    if (m_frame->selectionController()->isNone())
        return;
    
    TypingCommand::insertLineBreak(m_frame->document());
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (void)insertParagraphSeparator
{
    if (m_frame->selectionController()->isNone())
        return;
    
    TypingCommand::insertParagraphSeparator(m_frame->document());
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (void)insertParagraphSeparatorInQuotedContent
{
    if (m_frame->selectionController()->isNone())
        return;
    
    TypingCommand::insertParagraphSeparatorInQuotedContent(m_frame->document());
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (void)insertText:(NSString *)text selectInsertedText:(BOOL)selectInsertedText
{
    if (m_frame->selectionController()->isNone())
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
    HitTestResult result = m_frame->eventHandler()->hitTestResultAtPoint(outerPoint, true);
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

- (void)moveDragCaretToPoint:(NSPoint)point
{   
    Selection dragCaret([self _visiblePositionForPoint:point]);
    m_frame->dragCaretController()->setSelection(dragCaret);
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

- (void)deleteKeyPressedWithSmartDelete:(BOOL)smartDelete granularity:(TextGranularity)granularity
{
    if (!m_frame || !m_frame->document())
        return;
    
    TypingCommand::deleteKeyPressed(m_frame->document(), smartDelete, granularity);
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (void)forwardDeleteKeyPressedWithSmartDelete:(BOOL)smartDelete granularity:(TextGranularity)granularity
{
    if (!m_frame || !m_frame->document())
        return;
    
    TypingCommand::forwardDeleteKeyPressed(m_frame->document(), smartDelete, granularity);
    m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
}

- (DOMCSSStyleDeclaration *)typingStyle
{
    if (!m_frame || !m_frame->typingStyle())
        return nil;
    return [DOMCSSStyleDeclaration _CSSStyleDeclarationWith:m_frame->typingStyle()->copy().get()];
}

- (void)setTypingStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(EditAction)undoAction
{
    if (!m_frame)
        return;
    m_frame->computeAndSetTypingStyle([style _CSSStyleDeclaration], undoAction);
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

- (NSFont *)fontForSelection:(BOOL *)hasMultipleFonts
{
    bool multipleFonts = false;
    NSFont *font = nil;
    if (m_frame) {
        const FontData* fd = m_frame->editor()->fontForSelection(multipleFonts);
        if (fd)
            font = fd->getNSFont();
    }
    
    if (hasMultipleFonts)
        *hasMultipleFonts = multipleFonts;
    return font;
}

- (NSWritingDirection)baseWritingDirectionForSelectionStart
{
    return m_frame ? m_frame->baseWritingDirectionForSelectionStart() : (NSWritingDirection)NSWritingDirectionLeftToRight;
}

- (void)dragSourceMovedTo:(NSPoint)windowLoc
{
    if (m_frame) {
        // FIXME: Fake modifier keys here.
        PlatformMouseEvent event(IntPoint(windowLoc), globalPoint(windowLoc, [self window]),
            LeftButton, 0, false, false, false, false);
        m_frame->eventHandler()->dragSourceMovedTo(event);
    }
}

- (void)dragSourceEndedAt:(NSPoint)windowLoc operation:(NSDragOperation)operation
{
    if (m_frame) {
        // FIXME: Fake modifier keys here.
        PlatformMouseEvent event(IntPoint(windowLoc), globalPoint(windowLoc, [self window]),
            LeftButton, 0, false, false, false, false);
        m_frame->eventHandler()->dragSourceEndedAt(event, operation);
    }
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

// FIXME: The following 2 functions are copied from AppKit. It would be best to share code.

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
    Document* doc = m_frame->document();
    if (!doc)
        return NO;

    CachedResource* resource = doc->docLoader()->cachedResource([URL absoluteString]);
    if (!resource)
        return NO;

    SharedBuffer* buffer = resource->allData();
    if (buffer)
        *data = [buffer->createNSData() autorelease];
    else
        *data = nil;

    *response = resource->response().nsURLResponse();
    return YES;
}

- (void)getAllResourceDatas:(NSArray **)datas andResponses:(NSArray **)responses
{
    Document* doc = m_frame->document();
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
        SharedBuffer* buffer = it->second->allData();
        NSData *data;
        
        if (buffer)
            data = buffer->createNSData();
        else
            data = nil;
        
        [d addObject:data];
        [data release];
        [r addObject:it->second->response().nsURLResponse()];
    }

    *datas = [d autorelease];
    *responses = [r autorelease];
}

- (BOOL)canProvideDocumentSource
{
    String mimeType = m_frame->loader()->responseMIMEType();
    
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

- (void)receivedData:(NSData *)data textEncodingName:(NSString *)textEncodingName
{
    // Set the encoding. This only needs to be done once, but it's harmless to do it again later.
    String encoding;
    if (m_frame)
        encoding = m_frame->loader()->documentLoader()->overrideEncoding();
    bool userChosen = !encoding.isNull();
    if (encoding.isNull())
        encoding = textEncodingName;
    m_frame->loader()->setEncoding(encoding, userChosen);
    [self addData:data];
}

// -------------------

- (FrameMac*)_frame
{
    return m_frame;
}

@end
