/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 James G. Speth (speth@end.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
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
#import "DOM.h"

#import "CDATASection.h"
#import "CSSHelper.h"
#import "CSSStyleSheet.h"
#import "Comment.h"
#import "DOMHTMLCanvasElement.h"
#import "DOMInternal.h"
#import "DOMPrivate.h"
#import "Document.h"
#import "DocumentFragment.h"
#import "DocumentType.h"
#import "EntityReference.h"
#import "Event.h"
#import "EventListener.h"
#import "EventTarget.h"
#import "ExceptionHandlers.h"
#import "FoundationExtras.h"
#import "Frame.h"
#import "FrameView.h"
#import "HTMLDocument.h"
#import "HTMLNames.h"
#import "HTMLPlugInElement.h"
#import "Image.h"
#import "IntRect.h"
#import "NodeFilter.h"
#import "NodeFilterCondition.h"
#import "NodeIterator.h"
#import "NodeList.h"
#import "ProcessingInstruction.h"
#import "QualifiedName.h"
#import "Range.h"
#import "RenderImage.h"
#import "RenderView.h"
#import "SimpleFontData.h"
#import "Text.h"
#import "TreeWalker.h"
#import "WebScriptObjectPrivate.h"
#import <objc/objc-class.h>
#import <wtf/HashMap.h>

#if ENABLE(SVG)
#import "SVGDocument.h"
#import "SVGElement.h"
#import "SVGNames.h"
#import "DOMSVG.h"
#endif

namespace WebCore {

class ObjCEventListener : public EventListener {
public:
    static ObjCEventListener* find(id <DOMEventListener>);
    static ObjCEventListener* create(id <DOMEventListener>);

private:
    ObjCEventListener(id <DOMEventListener>);
    virtual ~ObjCEventListener();

    virtual void handleEvent(Event*, bool isWindowEvent);

    id <DOMEventListener> m_listener;
};

typedef HashMap<id, ObjCEventListener*> ListenerMap;
static ListenerMap* listenerMap;

} // namespace WebCore


//------------------------------------------------------------------------------------------
// DOMNode

namespace WebCore {

typedef HashMap<const QualifiedName::QualifiedNameImpl*, Class> ObjCClassMap;
static ObjCClassMap* elementClassMap;

static void addElementClass(const QualifiedName& tag, Class objCClass)
{
    elementClassMap->set(tag.impl(), objCClass);
}

static void createElementClassMap()
{
    // Create the table.
    elementClassMap = new ObjCClassMap;

    // FIXME: Reflect marquee once the API has been determined.

    // Populate it with HTML and SVG element classes.
    addElementClass(HTMLNames::aTag, [DOMHTMLAnchorElement class]);
    addElementClass(HTMLNames::appletTag, [DOMHTMLAppletElement class]);
    addElementClass(HTMLNames::areaTag, [DOMHTMLAreaElement class]);
    addElementClass(HTMLNames::baseTag, [DOMHTMLBaseElement class]);
    addElementClass(HTMLNames::basefontTag, [DOMHTMLBaseFontElement class]);
    addElementClass(HTMLNames::bodyTag, [DOMHTMLBodyElement class]);
    addElementClass(HTMLNames::brTag, [DOMHTMLBRElement class]);
    addElementClass(HTMLNames::buttonTag, [DOMHTMLButtonElement class]);
    addElementClass(HTMLNames::canvasTag, [DOMHTMLCanvasElement class]);
    addElementClass(HTMLNames::captionTag, [DOMHTMLTableCaptionElement class]);
    addElementClass(HTMLNames::colTag, [DOMHTMLTableColElement class]);
    addElementClass(HTMLNames::colgroupTag, [DOMHTMLTableColElement class]);
    addElementClass(HTMLNames::delTag, [DOMHTMLModElement class]);
    addElementClass(HTMLNames::dirTag, [DOMHTMLDirectoryElement class]);
    addElementClass(HTMLNames::divTag, [DOMHTMLDivElement class]);
    addElementClass(HTMLNames::dlTag, [DOMHTMLDListElement class]);
    addElementClass(HTMLNames::embedTag, [DOMHTMLEmbedElement class]);
    addElementClass(HTMLNames::fieldsetTag, [DOMHTMLFieldSetElement class]);
    addElementClass(HTMLNames::fontTag, [DOMHTMLFontElement class]);
    addElementClass(HTMLNames::formTag, [DOMHTMLFormElement class]);
    addElementClass(HTMLNames::frameTag, [DOMHTMLFrameElement class]);
    addElementClass(HTMLNames::framesetTag, [DOMHTMLFrameSetElement class]);
    addElementClass(HTMLNames::h1Tag, [DOMHTMLHeadingElement class]);
    addElementClass(HTMLNames::h2Tag, [DOMHTMLHeadingElement class]);
    addElementClass(HTMLNames::h3Tag, [DOMHTMLHeadingElement class]);
    addElementClass(HTMLNames::h4Tag, [DOMHTMLHeadingElement class]);
    addElementClass(HTMLNames::h5Tag, [DOMHTMLHeadingElement class]);
    addElementClass(HTMLNames::h6Tag, [DOMHTMLHeadingElement class]);
    addElementClass(HTMLNames::headTag, [DOMHTMLHeadElement class]);
    addElementClass(HTMLNames::hrTag, [DOMHTMLHRElement class]);
    addElementClass(HTMLNames::htmlTag, [DOMHTMLHtmlElement class]);
    addElementClass(HTMLNames::iframeTag, [DOMHTMLIFrameElement class]);
    addElementClass(HTMLNames::imgTag, [DOMHTMLImageElement class]);
    addElementClass(HTMLNames::inputTag, [DOMHTMLInputElement class]);
    addElementClass(HTMLNames::insTag, [DOMHTMLModElement class]);
    addElementClass(HTMLNames::isindexTag, [DOMHTMLIsIndexElement class]);
    addElementClass(HTMLNames::labelTag, [DOMHTMLLabelElement class]);
    addElementClass(HTMLNames::legendTag, [DOMHTMLLegendElement class]);
    addElementClass(HTMLNames::liTag, [DOMHTMLLIElement class]);
    addElementClass(HTMLNames::linkTag, [DOMHTMLLinkElement class]);
    addElementClass(HTMLNames::listingTag, [DOMHTMLPreElement class]);
    addElementClass(HTMLNames::mapTag, [DOMHTMLMapElement class]);
    addElementClass(HTMLNames::marqueeTag, [DOMHTMLMarqueeElement class]);
    addElementClass(HTMLNames::menuTag, [DOMHTMLMenuElement class]);
    addElementClass(HTMLNames::metaTag, [DOMHTMLMetaElement class]);
    addElementClass(HTMLNames::objectTag, [DOMHTMLObjectElement class]);
    addElementClass(HTMLNames::olTag, [DOMHTMLOListElement class]);
    addElementClass(HTMLNames::optgroupTag, [DOMHTMLOptGroupElement class]);
    addElementClass(HTMLNames::optionTag, [DOMHTMLOptionElement class]);
    addElementClass(HTMLNames::pTag, [DOMHTMLParagraphElement class]);
    addElementClass(HTMLNames::paramTag, [DOMHTMLParamElement class]);
    addElementClass(HTMLNames::preTag, [DOMHTMLPreElement class]);
    addElementClass(HTMLNames::qTag, [DOMHTMLQuoteElement class]);
    addElementClass(HTMLNames::scriptTag, [DOMHTMLScriptElement class]);
    addElementClass(HTMLNames::keygenTag, [DOMHTMLSelectElement class]);
    addElementClass(HTMLNames::selectTag, [DOMHTMLSelectElement class]);
    addElementClass(HTMLNames::styleTag, [DOMHTMLStyleElement class]);
    addElementClass(HTMLNames::tableTag, [DOMHTMLTableElement class]);
    addElementClass(HTMLNames::tbodyTag, [DOMHTMLTableSectionElement class]);
    addElementClass(HTMLNames::tdTag, [DOMHTMLTableCellElement class]);
    addElementClass(HTMLNames::textareaTag, [DOMHTMLTextAreaElement class]);
    addElementClass(HTMLNames::tfootTag, [DOMHTMLTableSectionElement class]);
    addElementClass(HTMLNames::thTag, [DOMHTMLTableCellElement class]);
    addElementClass(HTMLNames::theadTag, [DOMHTMLTableSectionElement class]);
    addElementClass(HTMLNames::titleTag, [DOMHTMLTitleElement class]);
    addElementClass(HTMLNames::trTag, [DOMHTMLTableRowElement class]);
    addElementClass(HTMLNames::ulTag, [DOMHTMLUListElement class]);
    addElementClass(HTMLNames::xmpTag, [DOMHTMLPreElement class]);

#if ENABLE(SVG)
    addElementClass(SVGNames::aTag, [DOMSVGAElement class]);
#if ENABLE(SVG_ANIMATION)
    addElementClass(SVGNames::animateTag, [DOMSVGAnimateElement class]);
    addElementClass(SVGNames::animateColorTag, [DOMSVGAnimateColorElement class]);
    addElementClass(SVGNames::animateTransformTag, [DOMSVGAnimateTransformElement class]);
#endif
    addElementClass(SVGNames::circleTag, [DOMSVGCircleElement class]);
    addElementClass(SVGNames::clipPathTag, [DOMSVGClipPathElement class]);
    addElementClass(SVGNames::cursorTag, [DOMSVGCursorElement class]);
#if ENABLE(SVG_FONTS)
    addElementClass(SVGNames::definition_srcTag, [DOMSVGDefinitionSrcElement class]);
#endif
    addElementClass(SVGNames::defsTag, [DOMSVGDefsElement class]);
    addElementClass(SVGNames::descTag, [DOMSVGDescElement class]);
    addElementClass(SVGNames::ellipseTag, [DOMSVGEllipseElement class]);
#if ENABLE(SVG_FILTERS)
    addElementClass(SVGNames::feBlendTag, [DOMSVGFEBlendElement class]);
    addElementClass(SVGNames::feColorMatrixTag, [DOMSVGFEColorMatrixElement class]);
    addElementClass(SVGNames::feComponentTransferTag, [DOMSVGFEComponentTransferElement class]);
    addElementClass(SVGNames::feCompositeTag, [DOMSVGFECompositeElement class]);
    addElementClass(SVGNames::feDiffuseLightingTag, [DOMSVGFEDiffuseLightingElement class]);
    addElementClass(SVGNames::feDisplacementMapTag, [DOMSVGFEDisplacementMapElement class]);
    addElementClass(SVGNames::feDistantLightTag, [DOMSVGFEDistantLightElement class]);
    addElementClass(SVGNames::feFloodTag, [DOMSVGFEFloodElement class]);
    addElementClass(SVGNames::feFuncATag, [DOMSVGFEFuncAElement class]);
    addElementClass(SVGNames::feFuncBTag, [DOMSVGFEFuncBElement class]);
    addElementClass(SVGNames::feFuncGTag, [DOMSVGFEFuncGElement class]);
    addElementClass(SVGNames::feFuncRTag, [DOMSVGFEFuncRElement class]);
    addElementClass(SVGNames::feGaussianBlurTag, [DOMSVGFEGaussianBlurElement class]);
    addElementClass(SVGNames::feImageTag, [DOMSVGFEImageElement class]);
    addElementClass(SVGNames::feMergeTag, [DOMSVGFEMergeElement class]);
    addElementClass(SVGNames::feMergeNodeTag, [DOMSVGFEMergeNodeElement class]);
    addElementClass(SVGNames::feOffsetTag, [DOMSVGFEOffsetElement class]);
    addElementClass(SVGNames::fePointLightTag, [DOMSVGFEPointLightElement class]);
    addElementClass(SVGNames::feSpecularLightingTag, [DOMSVGFESpecularLightingElement class]);
    addElementClass(SVGNames::feSpotLightTag, [DOMSVGFESpotLightElement class]);
    addElementClass(SVGNames::feTileTag, [DOMSVGFETileElement class]);
    addElementClass(SVGNames::feTurbulenceTag, [DOMSVGFETurbulenceElement class]);
    addElementClass(SVGNames::filterTag, [DOMSVGFilterElement class]);
#endif
#if ENABLE(SVG_FONTS)
    addElementClass(SVGNames::fontTag, [DOMSVGFontElement class]);
    addElementClass(SVGNames::font_faceTag, [DOMSVGFontFaceElement class]);
    addElementClass(SVGNames::font_face_formatTag, [DOMSVGFontFaceFormatElement class]);
    addElementClass(SVGNames::font_face_nameTag, [DOMSVGFontFaceNameElement class]);
    addElementClass(SVGNames::font_face_srcTag, [DOMSVGFontFaceSrcElement class]);
    addElementClass(SVGNames::font_face_uriTag, [DOMSVGFontFaceUriElement class]);
    addElementClass(SVGNames::glyphTag, [DOMSVGGlyphElement class]);
#endif
    addElementClass(SVGNames::gTag, [DOMSVGGElement class]);
    addElementClass(SVGNames::imageTag, [DOMSVGImageElement class]);
    addElementClass(SVGNames::lineTag, [DOMSVGLineElement class]);
    addElementClass(SVGNames::linearGradientTag, [DOMSVGLinearGradientElement class]);
    addElementClass(SVGNames::markerTag, [DOMSVGMarkerElement class]);
    addElementClass(SVGNames::maskTag, [DOMSVGMaskElement class]);
    addElementClass(SVGNames::metadataTag, [DOMSVGMetadataElement class]);
#if ENABLE(SVG_FONTS)
    addElementClass(SVGNames::missing_glyphTag, [DOMSVGMissingGlyphElement class]);
#endif
    addElementClass(SVGNames::pathTag, [DOMSVGPathElement class]);
    addElementClass(SVGNames::patternTag, [DOMSVGPatternElement class]);
    addElementClass(SVGNames::polygonTag, [DOMSVGPolygonElement class]);
    addElementClass(SVGNames::polylineTag, [DOMSVGPolylineElement class]);
    addElementClass(SVGNames::radialGradientTag, [DOMSVGRadialGradientElement class]);
    addElementClass(SVGNames::rectTag, [DOMSVGRectElement class]);
    addElementClass(SVGNames::scriptTag, [DOMSVGScriptElement class]);
    addElementClass(SVGNames::setTag, [DOMSVGSetElement class]);
    addElementClass(SVGNames::stopTag, [DOMSVGStopElement class]);
    addElementClass(SVGNames::styleTag, [DOMSVGStyleElement class]);
    addElementClass(SVGNames::svgTag, [DOMSVGSVGElement class]);
    addElementClass(SVGNames::switchTag, [DOMSVGSwitchElement class]);
    addElementClass(SVGNames::symbolTag, [DOMSVGSymbolElement class]);
    addElementClass(SVGNames::textTag, [DOMSVGTextElement class]);
    addElementClass(SVGNames::titleTag, [DOMSVGTitleElement class]);
    addElementClass(SVGNames::trefTag, [DOMSVGTRefElement class]);
    addElementClass(SVGNames::tspanTag, [DOMSVGTSpanElement class]);
    addElementClass(SVGNames::textPathTag, [DOMSVGTextPathElement class]);
    addElementClass(SVGNames::useTag, [DOMSVGUseElement class]);
    addElementClass(SVGNames::viewTag, [DOMSVGViewElement class]);
#endif
}

static Class lookupElementClass(const QualifiedName& tag)
{
    // Do a special lookup to ignore element prefixes
    if (tag.hasPrefix())
        return elementClassMap->get(QualifiedName(nullAtom, tag.localName(), tag.namespaceURI()).impl());
    
    return elementClassMap->get(tag.impl());
}

static Class elementClass(const QualifiedName& tag, Class defaultClass)
{
    if (!elementClassMap)
        createElementClassMap();
    Class objcClass = lookupElementClass(tag);
    if (!objcClass)
        objcClass = defaultClass;
    return objcClass;
}

static NSArray *kit(const Vector<IntRect>& rects)
{
    size_t size = rects.size();
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:size];
    for (size_t i = 0; i < size; ++i)
        [array addObject:[NSValue valueWithRect:rects[i]]];
    return array;
}

} // namespace WebCore

@implementation DOMNode (WebCoreInternal)

// FIXME: should this go in the main implementation?
- (NSString *)description
{
    if (!_internal)
        return [NSString stringWithFormat:@"<%@: null>", [[self class] description], self];

    NSString *value = [self nodeValue];
    if (value)
        return [NSString stringWithFormat:@"<%@ [%@]: %p '%@'>",
            [[self class] description], [self nodeName], _internal, value];

    return [NSString stringWithFormat:@"<%@ [%@]: %p>", [[self class] description], [self nodeName], _internal];
}

- (id)_initWithNode:(WebCore::Node *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = reinterpret_cast<DOMObjectInternal*>(impl);
    impl->ref();
    WebCore::addDOMWrapper(self, impl);
    return self;
}

+ (DOMNode *)_wrapNode:(WebCore::Node *)impl
{
    if (!impl)
        return nil;

    id cachedInstance;
    cachedInstance = WebCore::getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];

    Class wrapperClass = nil;
    switch (impl->nodeType()) {
        case WebCore::Node::ELEMENT_NODE:
            if (impl->isHTMLElement())
                wrapperClass = WebCore::elementClass(static_cast<WebCore::HTMLElement*>(impl)->tagQName(), [DOMHTMLElement class]);
#if ENABLE(SVG)
            else if (impl->isSVGElement())
                wrapperClass = WebCore::elementClass(static_cast<WebCore::SVGElement*>(impl)->tagQName(), [DOMSVGElement class]);
#endif
            else
                wrapperClass = [DOMElement class];
            break;
        case WebCore::Node::ATTRIBUTE_NODE:
            wrapperClass = [DOMAttr class];
            break;
        case WebCore::Node::TEXT_NODE:
            wrapperClass = [DOMText class];
            break;
        case WebCore::Node::CDATA_SECTION_NODE:
            wrapperClass = [DOMCDATASection class];
            break;
        case WebCore::Node::ENTITY_REFERENCE_NODE:
            wrapperClass = [DOMEntityReference class];
            break;
        case WebCore::Node::ENTITY_NODE:
            wrapperClass = [DOMEntity class];
            break;
        case WebCore::Node::PROCESSING_INSTRUCTION_NODE:
            wrapperClass = [DOMProcessingInstruction class];
            break;
        case WebCore::Node::COMMENT_NODE:
            wrapperClass = [DOMComment class];
            break;
        case WebCore::Node::DOCUMENT_NODE:
            if (static_cast<WebCore::Document*>(impl)->isHTMLDocument())
                wrapperClass = [DOMHTMLDocument class];
#if ENABLE(SVG)
            else if (static_cast<WebCore::Document*>(impl)->isSVGDocument())
                wrapperClass = [DOMSVGDocument class];
#endif
            else
                wrapperClass = [DOMDocument class];
            break;
        case WebCore::Node::DOCUMENT_TYPE_NODE:
            wrapperClass = [DOMDocumentType class];
            break;
        case WebCore::Node::DOCUMENT_FRAGMENT_NODE:
            wrapperClass = [DOMDocumentFragment class];
            break;
        case WebCore::Node::NOTATION_NODE:
            wrapperClass = [DOMNotation class];
            break;
        case WebCore::Node::XPATH_NAMESPACE_NODE:
            // FIXME: Create an XPath objective C wrapper
            // See http://bugs.webkit.org/show_bug.cgi?id=8755
            return nil;
    }
    return [[[wrapperClass alloc] _initWithNode:impl] autorelease];
}

+ (id <DOMEventTarget>)_wrapEventTarget:(WebCore::EventTarget *)eventTarget
{
    if (!eventTarget)
        return nil;
    
    // We don't have an ObjC binding for XMLHttpRequest
    return [DOMNode _wrapNode:eventTarget->toNode()];
}

- (WebCore::Node *)_node
{
    return reinterpret_cast<WebCore::Node*>(_internal);
}

- (KJS::Bindings::RootObject*)_rootObject
{
    if (WebCore::Node *n = [self _node]) {
        if (WebCore::Frame* frame = n->document()->frame())
            return frame->bindingRootObject();
    }
    return 0;
}

@end

@implementation DOMNode (DOMNodeExtensions)

// FIXME: This should be implemented in Node so we don't have to fetch the renderer.
// If it was, we could even autogenerate.
- (NSRect)boundingBox
{
    [self _node]->document()->updateLayoutIgnorePendingStylesheets();
    WebCore::RenderObject *renderer = [self _node]->renderer();
    if (renderer)
        return renderer->absoluteBoundingBoxRect();
    return NSZeroRect;
}

// FIXME: This should be implemented in Node so we don't have to fetch the renderer.
// If it was, we could even autogenerate.
- (NSArray *)lineBoxRects
{
    [self _node]->document()->updateLayoutIgnorePendingStylesheets();
    WebCore::RenderObject *renderer = [self _node]->renderer();
    if (renderer) {
        Vector<WebCore::IntRect> rects;
        renderer->addLineBoxRects(rects);
        return kit(rects);
    }
    return nil;
}

@end

@implementation DOMRange (DOMRangeExtensions)

- (NSRect)boundingBox
{
    [self _range]->ownerDocument()->updateLayoutIgnorePendingStylesheets();
    return [self _range]->boundingBox();
}

- (NSArray *)lineBoxRects
{
    Vector<WebCore::IntRect> rects;
    [self _range]->ownerDocument()->updateLayoutIgnorePendingStylesheets();
    [self _range]->addLineBoxRects(rects);
    return kit(rects);
}

@end

// FIXME: this should be auto-generated
@implementation DOMNode (DOMEventTarget)

- (void)addEventListener:(NSString *)type listener:(id <DOMEventListener>)listener useCapture:(BOOL)useCapture
{
    if (![self _node]->isEventTargetNode())
        WebCore::raiseDOMException(DOM_NOT_SUPPORTED_ERR);
    
    WebCore::EventListener *wrapper = WebCore::ObjCEventListener::create(listener);
    WebCore::EventTargetNodeCast([self _node])->addEventListener(type, wrapper, useCapture);
    wrapper->deref();
}

- (void)addEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture
{
    // FIXME: this method can be removed once Mail changes to use the new method <rdar://problem/4746649>
    [self addEventListener:type listener:listener useCapture:useCapture];
}

- (void)removeEventListener:(NSString *)type listener:(id <DOMEventListener>)listener useCapture:(BOOL)useCapture
{
    if (![self _node]->isEventTargetNode())
        WebCore::raiseDOMException(DOM_NOT_SUPPORTED_ERR);

    if (WebCore::EventListener *wrapper = WebCore::ObjCEventListener::find(listener))
        WebCore::EventTargetNodeCast([self _node])->removeEventListener(type, wrapper, useCapture);
}

- (void)removeEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture
{
    // FIXME: this method can be removed once Mail changes to use the new method <rdar://problem/4746649>
    [self removeEventListener:type listener:listener useCapture:useCapture];
}

- (BOOL)dispatchEvent:(DOMEvent *)event
{
    if (![self _node]->isEventTargetNode())
        WebCore::raiseDOMException(DOM_NOT_SUPPORTED_ERR);

    WebCore::ExceptionCode ec = 0;
    BOOL result = WebCore::EventTargetNodeCast([self _node])->dispatchEvent([event _event], ec);
    WebCore::raiseOnDOMError(ec);
    return result;
}

@end

//------------------------------------------------------------------------------------------
// DOMElement

// FIXME: this should be auto-generated in DOMElement.mm
@implementation DOMElement (DOMElementAppKitExtensions)

// FIXME: this should be implemented in the implementation
- (NSImage*)image
{
    WebCore::RenderObject* renderer = [self _element]->renderer();
    if (renderer && renderer->isImage()) {
        WebCore::RenderImage* img = static_cast<WebCore::RenderImage*>(renderer);
        if (img->cachedImage() && !img->cachedImage()->errorOccurred())
            return img->cachedImage()->image()->getNSImage();
    }
    return nil;
}

@end

@implementation DOMElement (WebPrivate)

// FIXME: this should be implemented in the implementation
- (NSFont *)_font
{
    WebCore::RenderObject* renderer = [self _element]->renderer();
    if (renderer)
        return renderer->style()->font().primaryFont()->getNSFont();
    return nil;
}

// FIXME: this should be implemented in the implementation
- (NSData *)_imageTIFFRepresentation
{
    WebCore::RenderObject* renderer = [self _element]->renderer();
    if (renderer && renderer->isImage()) {
        WebCore::RenderImage* img = static_cast<WebCore::RenderImage*>(renderer);
        if (img->cachedImage() && !img->cachedImage()->errorOccurred())
            return (NSData*)(img->cachedImage()->image()->getTIFFRepresentation());
    }
    return nil;
}

- (NSRect)_windowClipRect
{
    WebCore::RenderObject* renderer = [self _element]->renderer();
    if (renderer && renderer->view()) {
        WebCore::FrameView* frameView = renderer->view()->frameView();
        if (!frameView)
            return WebCore::IntRect();
        return frameView->windowClipRectForLayer(renderer->enclosingLayer(), true);
    }
    return WebCore::IntRect();
}

// FIXME: this should be implemented in the implementation
- (NSURL *)_getURLAttribute:(NSString *)name
{
    ASSERT(name);
    WebCore::Element* element = [self _element];
    ASSERT(element);
    return WebCore::KURL(element->document()->completeURL(parseURL(element->getAttribute(name)).deprecatedString())).getNSURL();
}

// FIXME: this should be implemented in the implementation
- (void *)_NPObject
{
#if USE(NPOBJECT)
    WebCore::Element* element = [self _element];
    if (element->hasTagName(WebCore::HTMLNames::appletTag) || element->hasTagName(WebCore::HTMLNames::embedTag) || element->hasTagName(WebCore::HTMLNames::objectTag))
        return static_cast<WebCore::HTMLPlugInElement*>(element)->getNPObject();
#endif
    return 0;
}

// FIXME: this should be implemented in the implementation
- (BOOL)isFocused
{
    WebCore::Element* impl = [self _element];
    if (impl->document()->focusedNode() == impl)
        return YES;
    return NO;
}

@end


//------------------------------------------------------------------------------------------
// DOMRange

@implementation DOMRange (WebPrivate)

- (NSString *)description
{
    if (!_internal)
        return @"<DOMRange: null>";
    return [NSString stringWithFormat:@"<DOMRange: %@ %d %@ %d>",
               [self startContainer], [self startOffset], [self endContainer], [self endOffset]];
}

// FIXME: this should be removed as soon as all internal Apple uses of it have been replaced with
// calls to the public method - (NSString *)text.
- (NSString *)_text
{
    return [self text];
}

@end


//------------------------------------------------------------------------------------------
// DOMNodeFilter

// FIXME: This implementation should be in it's own file.

@implementation DOMNodeFilter

- (id)_initWithNodeFilter:(WebCore::NodeFilter *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = reinterpret_cast<DOMObjectInternal*>(impl);
    impl->ref();
    WebCore::addDOMWrapper(self, impl);
    return self;
}

+ (DOMNodeFilter *)_wrapNodeFilter:(WebCore::NodeFilter *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = WebCore::getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithNodeFilter:impl] autorelease];
}

- (WebCore::NodeFilter *)_nodeFilter
{
    return reinterpret_cast<WebCore::NodeFilter*>(_internal);
}

- (void)dealloc
{
    if (_internal)
        reinterpret_cast<WebCore::NodeFilter*>(_internal)->deref();
    [super dealloc];
}

- (void)finalize
{
    if (_internal)
        reinterpret_cast<WebCore::NodeFilter*>(_internal)->deref();
    [super finalize];
}

- (short)acceptNode:(DOMNode *)node
{
    return [self _nodeFilter]->acceptNode([node _node]);
}

@end


//------------------------------------------------------------------------------------------
// ObjCNodeFilterCondition

class ObjCNodeFilterCondition : public WebCore::NodeFilterCondition {
public:
    ObjCNodeFilterCondition(id <DOMNodeFilter>);
    virtual ~ObjCNodeFilterCondition();
    virtual short acceptNode(WebCore::Node*) const;

private:
    ObjCNodeFilterCondition(const ObjCNodeFilterCondition&);
    ObjCNodeFilterCondition &operator=(const ObjCNodeFilterCondition&);

    id <DOMNodeFilter> m_filter;
};

ObjCNodeFilterCondition::ObjCNodeFilterCondition(id <DOMNodeFilter> filter)
    : m_filter(filter)
{
    ASSERT(m_filter);
    HardRetain(m_filter);
}

ObjCNodeFilterCondition::~ObjCNodeFilterCondition()
{
    HardRelease(m_filter);
}

short ObjCNodeFilterCondition::acceptNode(WebCore::Node* node) const
{
    if (!node)
        return WebCore::NodeFilter::FILTER_REJECT;
    return [m_filter acceptNode:[DOMNode _wrapNode:node]];
}


//------------------------------------------------------------------------------------------
// DOMDocument (DOMDocumentTraversal)

// FIXME: this should be auto-generated in DOMDocument.mm
@implementation DOMDocument (DOMDocumentTraversal)

- (DOMNodeIterator *)createNodeIterator:(DOMNode *)root whatToShow:(unsigned)whatToShow filter:(id <DOMNodeFilter>)filter expandEntityReferences:(BOOL)expandEntityReferences
{
    WebCore::NodeFilter* cppFilter = 0;
    if (filter)
        cppFilter = new WebCore::NodeFilter(new ObjCNodeFilterCondition(filter));
    WebCore::ExceptionCode ec = 0;
    RefPtr<WebCore::NodeIterator> impl = [self _document]->createNodeIterator([root _node], whatToShow, cppFilter, expandEntityReferences, ec);
    WebCore::raiseOnDOMError(ec);
    return [DOMNodeIterator _wrapNodeIterator:impl.get() filter:filter];
}

- (DOMTreeWalker *)createTreeWalker:(DOMNode *)root whatToShow:(unsigned)whatToShow filter:(id <DOMNodeFilter>)filter expandEntityReferences:(BOOL)expandEntityReferences
{
    WebCore::NodeFilter* cppFilter = 0;
    if (filter)
        cppFilter = new WebCore::NodeFilter(new ObjCNodeFilterCondition(filter));
    WebCore::ExceptionCode ec = 0;
    RefPtr<WebCore::TreeWalker> impl = [self _document]->createTreeWalker([root _node], whatToShow, cppFilter, expandEntityReferences, ec);
    WebCore::raiseOnDOMError(ec);
    return [DOMTreeWalker _wrapTreeWalker:impl.get() filter:filter];
}

@end

@implementation DOMDocument (DOMDocumentTraversalDeprecated)

- (DOMNodeIterator *)createNodeIterator:(DOMNode *)root :(unsigned)whatToShow :(id <DOMNodeFilter>)filter :(BOOL)expandEntityReferences
{
    return [self createNodeIterator:root whatToShow:whatToShow filter:filter expandEntityReferences:expandEntityReferences];
}

- (DOMTreeWalker *)createTreeWalker:(DOMNode *)root :(unsigned)whatToShow :(id <DOMNodeFilter>)filter :(BOOL)expandEntityReferences
{
    return [self createTreeWalker:root whatToShow:whatToShow filter:filter expandEntityReferences:expandEntityReferences];
}

@end


//------------------------------------------------------------------------------------------
// ObjCEventListener

namespace WebCore {

ObjCEventListener* ObjCEventListener::find(id <DOMEventListener> listener)
{
    if (ListenerMap* map = listenerMap)
        return map->get(listener);
    return 0;
}

ObjCEventListener *ObjCEventListener::create(id <DOMEventListener> listener)
{
    ObjCEventListener* wrapper = find(listener);
    if (!wrapper)
        wrapper = new ObjCEventListener(listener);
    wrapper->ref();
    return wrapper;
}

ObjCEventListener::ObjCEventListener(id <DOMEventListener> listener)
    : m_listener([listener retain])
{
    ListenerMap* map = listenerMap;
    if (!map) {
        map = new ListenerMap;
        listenerMap = map;
    }
    map->set(listener, this);
}

ObjCEventListener::~ObjCEventListener()
{
    listenerMap->remove(m_listener);
    [m_listener release];
}

void ObjCEventListener::handleEvent(Event* event, bool)
{
    [m_listener handleEvent:[DOMEvent _wrapEvent:event]];
}

} // namespace WebCore
