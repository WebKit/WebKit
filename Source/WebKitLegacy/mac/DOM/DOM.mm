/*
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2006 James G. Speth (speth@end.com)
 * Copyright (C) 2006-2026 Samuel Weinig (sam.weinig@gmail.com)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "DOM.h"

#import "ExceptionHandlers.h"
#import "DOMElementInternal.h"
#import "DOMHTMLCanvasElement.h"
#import "DOMHTMLTableCellElementInternal.h"
#import "DOMHTMLVideoElement.h"
#import "DOMInternal.h"
#import "DOMNodeInternal.h"
#import "DOMPrivate.h"
#import "DOMRangeInternal.h"
#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/JSCellInlines.h>
#import <WebCore/BoundaryPointInlines.h>
#import <WebCore/CachedImage.h>
#import <WebCore/ContainerNodeInlines.h>
#import <WebCore/DocumentView.h>
#import <WebCore/DragImage.h>
#import <WebCore/FocusController.h>
#import <WebCore/FontCascadeInlines.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/HTMLDocument.h>
#import <WebCore/HTMLLinkElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLTableCellElement.h>
#import <WebCore/Image.h>
#import <WebCore/ImageAdapter.h>
#import <WebCore/JSNode.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/NativeImage.h>
#import <WebCore/NodeFilter.h>
#import <WebCore/NodeRenderStyle.h>
#import <WebCore/Page.h>
#import <WebCore/Range.h>
#import <WebCore/RenderImage.h>
#import <WebCore/RenderObjectInlines.h>
#import <WebCore/RenderStyle+GettersInlines.h>
#import <WebCore/RenderView.h>
#import <WebCore/ScriptController.h>
#import <WebCore/SimpleRange.h>
#import <WebCore/StylePrimitiveNumericTypes+Evaluation.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/Touch.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/HashMap.h>
#import <wtf/cocoa/VectorCocoa.h>

#if PLATFORM(IOS_FAMILY)
#import <WebCore/WAKAppKitStubs.h>
#import <WebCore/WAKWindow.h>
#import <WebCore/WebCoreThreadMessage.h>
#endif


//------------------------------------------------------------------------------------------
// DOMNode

typedef HashMap<const WebCore::QualifiedName::QualifiedNameImpl*, Class> ObjCClassMap;
static ObjCClassMap* elementClassMap;

static void addElementClass(const WebCore::QualifiedName& tag, Class objCClass)
{
    elementClassMap->set(tag.impl(), objCClass);
}

static void createElementClassMap()
{
    // Create the table.
    elementClassMap = new ObjCClassMap;

    addElementClass(WebCore::HTMLNames::aTag, [DOMHTMLAnchorElement class]);
    addElementClass(WebCore::HTMLNames::appletTag, [DOMHTMLAppletElement class]);
    addElementClass(WebCore::HTMLNames::areaTag, [DOMHTMLAreaElement class]);
    addElementClass(WebCore::HTMLNames::baseTag, [DOMHTMLBaseElement class]);
    addElementClass(WebCore::HTMLNames::basefontTag, [DOMHTMLBaseFontElement class]);
    addElementClass(WebCore::HTMLNames::bodyTag, [DOMHTMLBodyElement class]);
    addElementClass(WebCore::HTMLNames::brTag, [DOMHTMLBRElement class]);
    addElementClass(WebCore::HTMLNames::buttonTag, [DOMHTMLButtonElement class]);
    addElementClass(WebCore::HTMLNames::canvasTag, [DOMHTMLCanvasElement class]);
    addElementClass(WebCore::HTMLNames::captionTag, [DOMHTMLTableCaptionElement class]);
    addElementClass(WebCore::HTMLNames::colTag, [DOMHTMLTableColElement class]);
    addElementClass(WebCore::HTMLNames::colgroupTag, [DOMHTMLTableColElement class]);
    addElementClass(WebCore::HTMLNames::delTag, [DOMHTMLModElement class]);
    addElementClass(WebCore::HTMLNames::dirTag, [DOMHTMLDirectoryElement class]);
    addElementClass(WebCore::HTMLNames::divTag, [DOMHTMLDivElement class]);
    addElementClass(WebCore::HTMLNames::dlTag, [DOMHTMLDListElement class]);
    addElementClass(WebCore::HTMLNames::embedTag, [DOMHTMLEmbedElement class]);
    addElementClass(WebCore::HTMLNames::fieldsetTag, [DOMHTMLFieldSetElement class]);
    addElementClass(WebCore::HTMLNames::fontTag, [DOMHTMLFontElement class]);
    addElementClass(WebCore::HTMLNames::formTag, [DOMHTMLFormElement class]);
    addElementClass(WebCore::HTMLNames::frameTag, [DOMHTMLFrameElement class]);
    addElementClass(WebCore::HTMLNames::framesetTag, [DOMHTMLFrameSetElement class]);
    addElementClass(WebCore::HTMLNames::h1Tag, [DOMHTMLHeadingElement class]);
    addElementClass(WebCore::HTMLNames::h2Tag, [DOMHTMLHeadingElement class]);
    addElementClass(WebCore::HTMLNames::h3Tag, [DOMHTMLHeadingElement class]);
    addElementClass(WebCore::HTMLNames::h4Tag, [DOMHTMLHeadingElement class]);
    addElementClass(WebCore::HTMLNames::h5Tag, [DOMHTMLHeadingElement class]);
    addElementClass(WebCore::HTMLNames::h6Tag, [DOMHTMLHeadingElement class]);
    addElementClass(WebCore::HTMLNames::headTag, [DOMHTMLHeadElement class]);
    addElementClass(WebCore::HTMLNames::hrTag, [DOMHTMLHRElement class]);
    addElementClass(WebCore::HTMLNames::htmlTag, [DOMHTMLHtmlElement class]);
    addElementClass(WebCore::HTMLNames::iframeTag, [DOMHTMLIFrameElement class]);
    addElementClass(WebCore::HTMLNames::imgTag, [DOMHTMLImageElement class]);
    addElementClass(WebCore::HTMLNames::inputTag, [DOMHTMLInputElement class]);
    addElementClass(WebCore::HTMLNames::insTag, [DOMHTMLModElement class]);
    addElementClass(WebCore::HTMLNames::labelTag, [DOMHTMLLabelElement class]);
    addElementClass(WebCore::HTMLNames::legendTag, [DOMHTMLLegendElement class]);
    addElementClass(WebCore::HTMLNames::liTag, [DOMHTMLLIElement class]);
    addElementClass(WebCore::HTMLNames::linkTag, [DOMHTMLLinkElement class]);
    addElementClass(WebCore::HTMLNames::listingTag, [DOMHTMLPreElement class]);
    addElementClass(WebCore::HTMLNames::mapTag, [DOMHTMLMapElement class]);
    addElementClass(WebCore::HTMLNames::marqueeTag, [DOMHTMLMarqueeElement class]);
    addElementClass(WebCore::HTMLNames::menuTag, [DOMHTMLMenuElement class]);
    addElementClass(WebCore::HTMLNames::metaTag, [DOMHTMLMetaElement class]);
    addElementClass(WebCore::HTMLNames::objectTag, [DOMHTMLObjectElement class]);
    addElementClass(WebCore::HTMLNames::olTag, [DOMHTMLOListElement class]);
    addElementClass(WebCore::HTMLNames::optgroupTag, [DOMHTMLOptGroupElement class]);
    addElementClass(WebCore::HTMLNames::optionTag, [DOMHTMLOptionElement class]);
    addElementClass(WebCore::HTMLNames::pTag, [DOMHTMLParagraphElement class]);
    addElementClass(WebCore::HTMLNames::paramTag, [DOMHTMLParamElement class]);
    addElementClass(WebCore::HTMLNames::preTag, [DOMHTMLPreElement class]);
    addElementClass(WebCore::HTMLNames::qTag, [DOMHTMLQuoteElement class]);
    addElementClass(WebCore::HTMLNames::scriptTag, [DOMHTMLScriptElement class]);
    addElementClass(WebCore::HTMLNames::selectTag, [DOMHTMLSelectElement class]);
    addElementClass(WebCore::HTMLNames::styleTag, [DOMHTMLStyleElement class]);
    addElementClass(WebCore::HTMLNames::tableTag, [DOMHTMLTableElement class]);
    addElementClass(WebCore::HTMLNames::tbodyTag, [DOMHTMLTableSectionElement class]);
    addElementClass(WebCore::HTMLNames::tdTag, [DOMHTMLTableCellElement class]);
    addElementClass(WebCore::HTMLNames::textareaTag, [DOMHTMLTextAreaElement class]);
    addElementClass(WebCore::HTMLNames::tfootTag, [DOMHTMLTableSectionElement class]);
    addElementClass(WebCore::HTMLNames::thTag, [DOMHTMLTableCellElement class]);
    addElementClass(WebCore::HTMLNames::theadTag, [DOMHTMLTableSectionElement class]);
    addElementClass(WebCore::HTMLNames::titleTag, [DOMHTMLTitleElement class]);
    addElementClass(WebCore::HTMLNames::trTag, [DOMHTMLTableRowElement class]);
    addElementClass(WebCore::HTMLNames::ulTag, [DOMHTMLUListElement class]);
    addElementClass(WebCore::HTMLNames::videoTag, [DOMHTMLVideoElement class]);
    addElementClass(WebCore::HTMLNames::xmpTag, [DOMHTMLPreElement class]);
}

static Class lookupElementClass(const WebCore::QualifiedName& tag)
{
    // Do a special lookup to ignore element prefixes
    if (tag.hasPrefix())
        return elementClassMap->get(WebCore::QualifiedName(nullAtom(), tag.localName(), tag.namespaceURI()).impl());
    
    return elementClassMap->get(tag.impl());
}

static Class elementClass(const WebCore::QualifiedName& tag, Class defaultClass)
{
    if (!elementClassMap)
        createElementClassMap();
    RetainPtr<Class> objcClass = lookupElementClass(tag);
    if (!objcClass)
        objcClass = defaultClass;
    return objcClass.autorelease();
}

#if PLATFORM(IOS_FAMILY)

static WKQuad wkQuadFromFloatQuad(const WebCore::FloatQuad& inQuad)
{
    return { inQuad.p1(), inQuad.p2(), inQuad.p3(), inQuad.p4() };
}

static NSArray *kit(const Vector<WebCore::FloatQuad>& quads)
{
    return createNSArray(quads, [] (auto& quad) {
        return adoptNS([[WKQuadObject alloc] initWithQuad:wkQuadFromFloatQuad(quad)]);
    }).autorelease();
}

static inline WKQuad zeroQuad()
{
    return { CGPointZero, CGPointZero, CGPointZero, CGPointZero };
}

@implementation WKQuadObject {
    WKQuad _quad;
}

- (id)initWithQuad:(WKQuad)quad
{
    if ((self = [super init]))
        _quad = quad;
    return self;
}

- (WKQuad)quad
{
    return _quad;
}

- (CGRect)boundingBox
{
    float left = std::min({ _quad.p1.x, _quad.p2.x, _quad.p3.x, _quad.p4.x });
    float top = std::min({ _quad.p1.y, _quad.p2.y, _quad.p3.y, _quad.p4.y });
    
    float right = std::max({ _quad.p1.x, _quad.p2.x, _quad.p3.x, _quad.p4.x });
    float bottom = std::max({ _quad.p1.y, _quad.p2.y, _quad.p3.y, _quad.p4.y });

    return CGRectMake(left, top, right - left, bottom - top);
}

@end

#endif

@implementation DOMNode (WebCoreInternal)

IGNORE_WARNINGS_BEGIN("objc-protocol-method-implementation")

- (NSString *)description
{
    if (!_internal)
        return [NSString stringWithFormat:@"<%@: null>", [[self class] description]];

    NSString *value = [self nodeValue];
    if (value)
        return [NSString stringWithFormat:@"<%@ [%@]: %p '%@'>", [[self class] description], [self nodeName], _internal, value];

    return [NSString stringWithFormat:@"<%@ [%@]: %p>", [[self class] description], [self nodeName], _internal];
}

IGNORE_WARNINGS_END

- (JSC::Bindings::RootObject*)_rootObject
{
    auto* frame = core(self)->document().frame();
    if (!frame)
        return nullptr;
    return frame->script().bindingRootObject();
}

@end

Class kitClass(WebCore::Node* impl)
{
    switch (impl->nodeType()) {
    case WebCore::NodeType::Element:
        if (RefPtr htmlElement = dynamicDowncast<WebCore::HTMLElement>(*impl))
            return elementClass(htmlElement->tagQName(), [DOMHTMLElement class]);
        return [DOMElement class];
    case WebCore::NodeType::Attribute:
        return [DOMAttr class];
    case WebCore::NodeType::Text:
        return [DOMText class];
    case WebCore::NodeType::CDATASection:
        return [DOMCDATASection class];
    case WebCore::NodeType::ProcessingInstruction:
        return [DOMProcessingInstruction class];
    case WebCore::NodeType::Comment:
        return [DOMComment class];
    case WebCore::NodeType::Document:
        if (is<WebCore::HTMLDocument>(impl))
            return [DOMHTMLDocument class];
        return [DOMDocument class];
    case WebCore::NodeType::DocumentType:
        return [DOMDocumentType class];
    case WebCore::NodeType::DocumentFragment:
        return [DOMDocumentFragment class];
    }
    ASSERT_NOT_REACHED();
    return nil;
}

id<DOMEventTarget> kit(WebCore::EventTarget* target)
{
    // We don't have Objective-C bindings for XMLHttpRequest, LocalDOMWindow, and other non-WebCore::Node targets.
    return is<WebCore::Node>(target) ? kit(downcast<WebCore::Node>(target)) : nil;
}

@implementation DOMNode (DOMNodeExtensions)

#if PLATFORM(IOS_FAMILY)
- (CGRect)boundingBox
#else
- (NSRect)boundingBox
#endif
{
    auto& node = *core(self);
    node.document().updateLayout(WebCore::LayoutOptions::IgnorePendingStylesheets);
    auto* renderer = node.renderer();
    if (!renderer)
#if PLATFORM(IOS_FAMILY)
        return CGRectZero;
#else
        return NSZeroRect;
#endif
    return renderer->absoluteBoundingBoxRect();
}

- (NSArray *)lineBoxRects
{
    return [self textRects];
}

#if PLATFORM(IOS_FAMILY)

// quad in page coordinates, taking transforms into account. c.f. - (NSRect)boundingBox;
- (WKQuad)absoluteQuad
{
    return [self absoluteQuadAndInsideFixedPosition:0];
}

- (WKQuad)absoluteQuadAndInsideFixedPosition:(BOOL *)insideFixed
{
    auto& node = *core(self);
    node.document().updateLayout(WebCore::LayoutOptions::IgnorePendingStylesheets);
    auto* renderer = node.renderer();
    if (!renderer) {
        if (insideFixed)
            *insideFixed = false;
        return zeroQuad();
    }

    Vector<WebCore::FloatQuad> quads;
    bool wasFixed = false;
    renderer->absoluteQuads(quads, &wasFixed);
    if (insideFixed)
        *insideFixed = wasFixed;

    if (quads.size() == 0)
        return zeroQuad();
    if (quads.size() == 1)
        return wkQuadFromFloatQuad(quads[0]);
    return wkQuadFromFloatQuad(unitedBoundingBoxes(quads));
}

// this method is like - (CGRect)boundingBox, but it accounts for for transforms
- (CGRect)boundingBoxUsingTransforms
{
    auto& node = *core(self);
    node.document().updateLayout(WebCore::LayoutOptions::IgnorePendingStylesheets);
    auto* renderer = node.renderer();
    if (!renderer)
        return CGRectZero;
    return renderer->absoluteBoundingBoxRect(true);
}

// returns array of WKQuadObject
- (NSArray *)lineBoxQuads
{
    auto& node = *core(self);
    node.document().updateLayout(WebCore::LayoutOptions::IgnorePendingStylesheets);
    WebCore::RenderObject *renderer = node.renderer();
    if (!renderer)
        return nil;
    Vector<WebCore::FloatQuad> quads;
    renderer->absoluteQuads(quads);
    return kit(quads);
}

- (WebCore::Element*)_linkElement
{
    for (auto* node = core(self); node; node = node->parentNode()) {
        if (auto* element = dynamicDowncast<WebCore::Element>(*node); element && element->isLink())
            return element;
    }
    return nullptr;
}

- (NSURL *)hrefURL
{
    auto* link = [self _linkElement];
    if (!link)
        return nil;
    return link->document().encodingParseURL(link->getAttribute(WebCore::HTMLNames::hrefAttr)).createNSURL().autorelease();
}

- (NSString *)hrefTarget
{
    auto* link = [self _linkElement];
    if (!link)
        return nil;
    return link->getAttribute(WebCore::HTMLNames::targetAttr).createNSString().autorelease();
}

- (CGRect)hrefFrame
{
    auto* link = [self _linkElement];
    if (!link)
        return CGRectZero;
    auto* renderer = link->renderer();
    if (!renderer)
        return CGRectZero;
    return renderer->absoluteBoundingBoxRect();
}

- (NSString *)hrefLabel
{
    auto* link = [self _linkElement];
    if (!link)
        return nil;
    return link->textContent().createNSString().autorelease();
}

- (NSString *)hrefTitle
{
    auto* link = [self _linkElement];
    if (!is<WebCore::HTMLElement>(link))
        return nil;
    return link->document().displayStringModifiedByEncoding(downcast<WebCore::HTMLElement>(*link).title()).createNSString().autorelease();
}

- (CGRect)boundingFrame
{
    return [self boundingBox];
}

- (WKQuad)innerFrameQuad // takes transforms into account
{
    auto& node = *core(self);
    node.document().updateLayout(WebCore::LayoutOptions::IgnorePendingStylesheets);
    auto* renderer = node.renderer();
    if (!renderer)
        return zeroQuad();

    auto& style = renderer->style();
    WebCore::IntRect boundingBox = renderer->absoluteBoundingBoxRect(true /* use transforms*/);

    boundingBox.move(WebCore::Style::evaluate<float>(style.usedBorderLeftWidth(), WebCore::Style::ZoomNeeded { }), WebCore::Style::evaluate<float>(style.usedBorderTopWidth(), WebCore::Style::ZoomNeeded { }));
    boundingBox.setWidth(boundingBox.width() - WebCore::Style::evaluate<float>(style.usedBorderLeftWidth(), WebCore::Style::ZoomNeeded { }) - WebCore::Style::evaluate<float>(style.usedBorderRightWidth(), WebCore::Style::ZoomNeeded { }));
    boundingBox.setHeight(boundingBox.height() - WebCore::Style::evaluate<float>(style.usedBorderBottomWidth(), WebCore::Style::ZoomNeeded { }) - WebCore::Style::evaluate<float>(style.usedBorderTopWidth(), WebCore::Style::ZoomNeeded { }));

    // FIXME: This function advertises returning a quad, but it actually returns a bounding box (so there is no rotation, for instance).
    return wkQuadFromFloatQuad(WebCore::FloatQuad(boundingBox));
}

- (float)computedFontSize
{
    auto* style = core(self)->renderStyle();
    if (!style)
        return 0.0f;
    return style->fontDescription().computedSize();
}

- (DOMNode *)nextFocusNode
{
    WebCore::Page* page = core(self)->document().page();
    if (!page)
        return nil;
    return kit(page->focusController().nextFocusableElement(*core(self)).element.get());
}

- (DOMNode *)previousFocusNode
{
    WebCore::Page* page = core(self)->document().page();
    if (!page)
        return nil;
    return kit(page->focusController().previousFocusableElement(*core(self)).element.get());
}

#endif // PLATFORM(IOS_FAMILY)

@end

@implementation DOMNode (DOMNodeExtensionsPendingPublic)

#if PLATFORM(MAC)

- (NSImage *)renderedImage
{
    auto& node = *core(self);
    auto* frame = node.document().frame();
    if (!frame)
        return nil;
    return createDragImageForNode(*frame, node).autorelease();
}

#endif

- (NSArray *)textRects
{
    auto& node = *core(self);
    node.document().updateLayout(WebCore::LayoutOptions::IgnorePendingStylesheets);
    if (!node.renderer())
        return nil;
    return createNSArray(WebCore::RenderObject::absoluteTextRects(makeRangeSelectingNodeContents(node))).autorelease();
}

@end

@implementation DOMNode (WebPrivate)

+ (id)_nodeFromJSWrapper:(JSObjectRef)jsWrapper
{
    JSC::JSObject* object = toJS(jsWrapper);
    if (!object->inherits<WebCore::JSNode>())
        return nil;
    return kit(&uncheckedDowncast<WebCore::JSNode>(object)->wrapped());
}

- (void)getPreviewSnapshotImage:(CGImageRef*)cgImage andRects:(NSArray **)rects
{
    if (!cgImage || !rects)
        return;

    *cgImage = nullptr;
    *rects = nullptr;

    auto& node = *core(self);

    constexpr OptionSet<WebCore::TextIndicatorOption> options {
        WebCore::TextIndicatorOption::TightlyFitContent,
        WebCore::TextIndicatorOption::RespectTextColor,
        WebCore::TextIndicatorOption::PaintBackgrounds,
        WebCore::TextIndicatorOption::UseBoundingRectAndPaintAllContentForComplexRanges,
        WebCore::TextIndicatorOption::IncludeMarginIfRangeMatchesSelection
    };
    const float margin = 4 / node.document().page()->pageScaleFactor();
    auto textIndicator = WebCore::TextIndicator::createWithRange(makeRangeSelectingNodeContents(node), options, WebCore::TextIndicatorPresentationTransition::None, WebCore::FloatSize(margin, margin));

    if (textIndicator) {
        if (WebCore::Image* image = textIndicator->contentImage()) {
            auto contentImage = image->nativeImage()->platformImage();
            *cgImage = contentImage.autorelease();
        }
    }

    if (!*cgImage) {
        if (auto* renderer = node.renderer()) {
            WebCore::FloatRect boundingBox;
            if (renderer->isRenderImage())
                boundingBox = downcast<WebCore::RenderImage>(*renderer).absoluteContentQuad().enclosingBoundingBox();
            else
                boundingBox = renderer->absoluteBoundingBoxRect();
            boundingBox.inflate(margin);
            *rects = @[makeNSArrayElement(node.document().frame()->view()->contentsToWindow(WebCore::enclosingIntRect(boundingBox)))];
        }
        return;
    }

    WebCore::FloatPoint origin = textIndicator->textBoundingRectInRootViewCoordinates().location();
    *rects = createNSArray(textIndicator->textRectsInBoundingRectCoordinates(), [&] (CGRect rect) {
        rect.origin.x += origin.x();
        rect.origin.y += origin.y();
        return makeNSArrayElement(node.document().frame()->view()->contentsToWindow(WebCore::enclosingIntRect(rect)));
    }).autorelease();
}

@end

@implementation DOMRange (DOMRangeExtensions)

#if PLATFORM(IOS_FAMILY)
- (CGRect)boundingBox
#else
- (NSRect)boundingBox
#endif
{
    auto range = makeSimpleRange(*core(self));
    range.start.document().updateLayout(WebCore::LayoutOptions::IgnorePendingStylesheets);
    return unionRect(WebCore::RenderObject::absoluteTextRects(range));
}

#if PLATFORM(MAC)
- (NSImage *)renderedImageForcingBlackText:(BOOL)forceBlackText
#else
- (CGImageRef)renderedImageForcingBlackText:(BOOL)forceBlackText
#endif
{
    auto range = makeSimpleRange(*core(self));
    RefPtr frame = range.start.document().frame();
    if (!frame)
        return nil;

    auto renderedImage = createDragImageForRange(*frame, range, forceBlackText);

#if PLATFORM(MAC)
    // iOS uses CGImageRef for drag images, which doesn't support separate logical/physical sizes.
    WebCore::IntSize size([renderedImage size]);
    size.scale(1 / frame->page()->deviceScaleFactor());
    [renderedImage setSize:size];
#endif

    return renderedImage.autorelease();
}

- (NSArray *)textRects
{
    auto range = makeSimpleRange(*core(self));
    range.start.document().updateLayout(WebCore::LayoutOptions::IgnorePendingStylesheets);
    return createNSArray(WebCore::RenderObject::absoluteTextRects(range)).autorelease();
}

- (NSArray *)lineBoxRects
{
    // FIXME: Remove this once all clients stop using it and we drop Leopard support.
    return [self textRects];
}

@end

//------------------------------------------------------------------------------------------
// DOMElement

@implementation DOMElement (DOMElementAppKitExtensions)

#if PLATFORM(MAC)

- (NSImage *)image
{
    auto* renderer = core(self)->renderer();
    if (!is<WebCore::RenderImage>(renderer))
        return nil;
    auto* cachedImage = downcast<WebCore::RenderImage>(*renderer).cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return nil;
    return cachedImage->imageForRenderer(renderer)->adapter().nsImage();
}

#endif

@end

@implementation DOMElement (WebPrivate)

- (CTFontRef)_font
{
    auto* renderer = core(self)->renderer();
    if (!renderer)
        return nil;
    return renderer->style().fontCascade().primaryFont().ctFont();
}

#if PLATFORM(MAC)

- (NSData *)_imageTIFFRepresentation
{
    // FIXME: Could we move this function to WebCore::Element and autogenerate?
    auto* renderer = core(self)->renderer();
    if (!is<WebCore::RenderImage>(renderer))
        return nil;
    auto* cachedImage = downcast<WebCore::RenderImage>(*renderer).cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return nil;
    return (__bridge NSData *)cachedImage->imageForRenderer(renderer)->adapter().tiffRepresentation();
}

#endif

- (NSURL *)_getURLAttribute:(NSString *)name
{
    auto& element = *core(self);
    return element.document().encodingParseURL(element.getAttribute(name)).createNSURL().autorelease();
}

- (BOOL)isFocused
{
    auto& element = *core(self);
    return element.document().focusedElement() == &element;
}

@end

#if PLATFORM(IOS_FAMILY)

@implementation DOMHTMLLinkElement (WebPrivate)

- (BOOL)_mediaQueryMatchesForOrientation:(int)orientation
{
    auto& document = static_cast<WebCore::HTMLLinkElement*>(core(self))->document();
    auto* frameView = document.frame() ? document.frame()->view() : 0;
    if (!frameView)
        return false;
    int layoutWidth = frameView->layoutWidth();
    int layoutHeight = frameView->layoutHeight();
    WebCore::IntSize savedFixedLayoutSize = frameView->fixedLayoutSize();
    bool savedUseFixedLayout = frameView->useFixedLayout();
    if ((orientation == WebMediaQueryOrientationPortrait && layoutWidth > layoutHeight) ||
        (orientation == WebMediaQueryOrientationLandscape && layoutWidth < layoutHeight)) {
        // temporarily swap the orientation for the evaluation
        frameView->setFixedLayoutSize(WebCore::IntSize(layoutHeight, layoutWidth));
        frameView->setUseFixedLayout(true);
    }
        
    bool result = [self _mediaQueryMatches];

    frameView->setFixedLayoutSize(savedFixedLayoutSize);
    frameView->setUseFixedLayout(savedUseFixedLayout);

    return result;
}

- (BOOL)_mediaQueryMatches
{
    return downcast<WebCore::HTMLLinkElement>(core(self))->mediaAttributeMatches();
}

@end

#endif

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

// FIXME: This should be removed as soon as all internal Apple uses of it have been replaced with
// calls to the public method - (NSString *)text.
- (NSString *)_text
{
    return [self text];
}

@end

//------------------------------------------------------------------------------------------
// DOMRGBColor

@implementation DOMRGBColor (WebPrivate)

#if PLATFORM(MAC)

// FIXME: This should be removed as soon as all internal Apple uses of it have been replaced with
// calls to the public method - (NSColor *)color.
- (NSColor *)_color
{
    return [self color];
}

#endif

@end

//------------------------------------------------------------------------------------------
// DOMHTMLTableCellElement

@implementation DOMHTMLTableCellElement (WebPrivate)

- (DOMHTMLTableCellElement *)_cellAbove
{
    return kit(core(self)->cellAbove());
}

@end

//------------------------------------------------------------------------------------------
// DOMNodeFilter

DOMNodeFilter *kit(WebCore::NodeFilter* impl)
{
    if (!impl)
        return nil;

    if (RetainPtr wrapper = getDOMWrapper(impl))
        return wrapper.autorelease();

    auto wrapper = adoptNS([[DOMNodeFilter alloc] _init]);
    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(impl);
    impl->ref();
    addDOMWrapper(wrapper.get(), impl);
    return wrapper.autorelease();
}

WebCore::NodeFilter* core(DOMNodeFilter *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::NodeFilter*>(wrapper->_internal) : 0;
}

@implementation DOMNodeFilter

- (void)dealloc
{
    if (_internal)
        reinterpret_cast<WebCore::NodeFilter*>(_internal)->deref();
    [super dealloc];
}

- (short)acceptNode:(DOMNode *)node
{
    if (!node)
        raiseTypeErrorException();
    
    auto result = core(self)->acceptNodeRethrowingException(*core(node));
    return result.type() == WebCore::CallbackResultType::Success ? result.releaseReturnValue() : WebCore::NodeFilter::FILTER_REJECT;
}

@end
