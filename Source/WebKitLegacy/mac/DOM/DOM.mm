/*
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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
#import <WebCore/CachedImage.h>
#import <WebCore/DragImage.h>
#import <WebCore/FocusController.h>
#import <WebCore/FontCascade.h>
#import <WebCore/Frame.h>
#import <WebCore/HTMLLinkElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLParserIdioms.h>
#import <WebCore/HTMLTableCellElement.h>
#import <WebCore/Image.h>
#import <WebCore/JSNode.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/MediaList.h>
#import <WebCore/MediaQueryEvaluator.h>
#import <WebCore/NodeFilter.h>
#import <WebCore/NodeRenderStyle.h>
#import <WebCore/Page.h>
#import <WebCore/Range.h>
#import <WebCore/RenderImage.h>
#import <WebCore/RenderView.h>
#import <WebCore/ScriptController.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/Touch.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/HashMap.h>

#if PLATFORM(IOS_FAMILY)
#import <WebCore/WAKAppKitStubs.h>
#import <WebCore/WAKWindow.h>
#import <WebCore/WebCoreThreadMessage.h>
#endif

using namespace JSC;
using namespace WebCore;

// FIXME: These methods should move into the implementation files of the DOM classes
// and this file should be eliminated.

//------------------------------------------------------------------------------------------
// DOMNode

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
    addElementClass(HTMLNames::videoTag, [DOMHTMLVideoElement class]);
    addElementClass(HTMLNames::xmpTag, [DOMHTMLPreElement class]);
}

static Class lookupElementClass(const QualifiedName& tag)
{
    // Do a special lookup to ignore element prefixes
    if (tag.hasPrefix())
        return elementClassMap->get(QualifiedName(nullAtom(), tag.localName(), tag.namespaceURI()).impl());
    
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

#if PLATFORM(IOS_FAMILY)

static WKQuad wkQuadFromFloatQuad(const FloatQuad& inQuad)
{
    return { inQuad.p1(), inQuad.p2(), inQuad.p3(), inQuad.p4() };
}

static NSArray *kit(const Vector<FloatQuad>& quads)
{
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:quads.size()];
    for (auto& quad : quads) {
        WKQuadObject *quadObject = [[WKQuadObject alloc] initWithQuad:wkQuadFromFloatQuad(quad)];
        [array addObject:quadObject];
        [quadObject release];
    }
    return array;
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

- (Bindings::RootObject*)_rootObject
{
    auto* frame = core(self)->document().frame();
    if (!frame)
        return nullptr;
    return frame->script().bindingRootObject();
}

@end

Class kitClass(Node* impl)
{
    switch (impl->nodeType()) {
        case Node::ELEMENT_NODE:
            if (is<HTMLElement>(*impl))
                return elementClass(downcast<HTMLElement>(*impl).tagQName(), [DOMHTMLElement class]);
            return [DOMElement class];
        case Node::ATTRIBUTE_NODE:
            return [DOMAttr class];
        case Node::TEXT_NODE:
            return [DOMText class];
        case Node::CDATA_SECTION_NODE:
            return [DOMCDATASection class];
        case Node::PROCESSING_INSTRUCTION_NODE:
            return [DOMProcessingInstruction class];
        case Node::COMMENT_NODE:
            return [DOMComment class];
        case Node::DOCUMENT_NODE:
            if (static_cast<Document*>(impl)->isHTMLDocument())
                return [DOMHTMLDocument class];
            return [DOMDocument class];
        case Node::DOCUMENT_TYPE_NODE:
            return [DOMDocumentType class];
        case Node::DOCUMENT_FRAGMENT_NODE:
            return [DOMDocumentFragment class];
    }
    ASSERT_NOT_REACHED();
    return nil;
}

id <DOMEventTarget> kit(EventTarget* target)
{
    // We don't have Objective-C bindings for XMLHttpRequest, DOMWindow, and other non-Node targets.
    return is<Node>(target) ? kit(downcast<Node>(target)) : nil;
}

@implementation DOMNode (DOMNodeExtensions)

#if PLATFORM(IOS_FAMILY)
- (CGRect)boundingBox
#else
- (NSRect)boundingBox
#endif
{
    auto& node = *core(self);
    node.document().updateLayoutIgnorePendingStylesheets();
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
    node.document().updateLayoutIgnorePendingStylesheets();
    auto* renderer = node.renderer();
    if (!renderer) {
        if (insideFixed)
            *insideFixed = false;
        return zeroQuad();
    }

    Vector<FloatQuad> quads;
    bool wasFixed = false;
    renderer->absoluteQuads(quads, &wasFixed);
    if (insideFixed)
        *insideFixed = wasFixed;

    if (quads.size() == 0)
        return zeroQuad();

    if (quads.size() == 1)
        return wkQuadFromFloatQuad(quads[0]);

    auto boundingRect = quads[0].boundingBox();
    for (size_t i = 1; i < quads.size(); ++i)
        boundingRect.unite(quads[i].boundingBox());
    return wkQuadFromFloatQuad(boundingRect);
}

// this method is like - (CGRect)boundingBox, but it accounts for for transforms
- (CGRect)boundingBoxUsingTransforms
{
    auto& node = *core(self);
    node.document().updateLayoutIgnorePendingStylesheets();
    auto* renderer = node.renderer();
    if (!renderer)
        return CGRectZero;
    return renderer->absoluteBoundingBoxRect(true);
}

// returns array of WKQuadObject
- (NSArray *)lineBoxQuads
{
    auto& node = *core(self);
    node.document().updateLayoutIgnorePendingStylesheets();
    WebCore::RenderObject *renderer = node.renderer();
    if (!renderer)
        return nil;
    Vector<WebCore::FloatQuad> quads;
    renderer->absoluteQuads(quads);
    return kit(quads);
}

- (Element*)_linkElement
{
    for (auto* node = core(self); node; node = node->parentNode()) {
        if (node->isLink())
            return &downcast<Element>(*node);
    }
    return nullptr;
}

- (NSURL *)hrefURL
{
    auto* link = [self _linkElement];
    if (!link)
        return nil;
    return link->document().completeURL(stripLeadingAndTrailingHTMLSpaces(link->getAttribute(HTMLNames::hrefAttr)));
}

- (NSString *)hrefTarget
{
    auto* link = [self _linkElement];
    if (!link)
        return nil;
    return link->getAttribute(HTMLNames::targetAttr);
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
    return link->textContent();
}

- (NSString *)hrefTitle
{
    auto* link = [self _linkElement];
    if (!is<HTMLElement>(link))
        return nil;
    return link->document().displayStringModifiedByEncoding(downcast<HTMLElement>(*link).title());
}

- (CGRect)boundingFrame
{
    return [self boundingBox];
}

- (WKQuad)innerFrameQuad // takes transforms into account
{
    auto& node = *core(self);
    node.document().updateLayoutIgnorePendingStylesheets();
    auto* renderer = node.renderer();
    if (!renderer)
        return zeroQuad();

    auto& style = renderer->style();
    IntRect boundingBox = renderer->absoluteBoundingBoxRect(true /* use transforms*/);

    boundingBox.move(style.borderLeftWidth(), style.borderTopWidth());
    boundingBox.setWidth(boundingBox.width() - style.borderLeftWidth() - style.borderRightWidth());
    boundingBox.setHeight(boundingBox.height() - style.borderBottomWidth() - style.borderTopWidth());

    // FIXME: This function advertises returning a quad, but it actually returns a bounding box (so there is no rotation, for instance).
    return wkQuadFromFloatQuad(FloatQuad(boundingBox));
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
    Page* page = core(self)->document().page();
    if (!page)
        return nil;
    return kit(page->focusController().nextFocusableElement(*core(self)));
}

- (DOMNode *)previousFocusNode
{
    Page* page = core(self)->document().page();
    if (!page)
        return nil;
    return kit(page->focusController().previousFocusableElement(*core(self)));
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
    node.document().updateLayoutIgnorePendingStylesheets();
    if (!node.renderer())
        return nil;
    Vector<WebCore::IntRect> rects;
    node.textRects(rects);
    return kit(rects);
}

@end

@implementation DOMNode (WebPrivate)

+ (id)_nodeFromJSWrapper:(JSObjectRef)jsWrapper
{
    JSObject* object = toJS(jsWrapper);
    if (!object->inherits<JSNode>(object->vm()))
        return nil;
    return kit(&jsCast<JSNode*>(object)->wrapped());
}

- (void)getPreviewSnapshotImage:(CGImageRef*)cgImage andRects:(NSArray **)rects
{
    if (!cgImage || !rects)
        return;

    *cgImage = nullptr;
    *rects = nullptr;

    auto& node = *core(self);

    auto range = rangeOfContents(node);

    const float margin = 4 / node.document().page()->pageScaleFactor();
    auto textIndicator = TextIndicator::createWithRange(range, TextIndicatorOptionTightlyFitContent |
        TextIndicatorOptionRespectTextColor |
        TextIndicatorOptionPaintBackgrounds |
        TextIndicatorOptionUseBoundingRectAndPaintAllContentForComplexRanges |
        TextIndicatorOptionIncludeMarginIfRangeMatchesSelection,
        TextIndicatorPresentationTransition::None, FloatSize(margin, margin));

    if (textIndicator) {
        if (Image* image = textIndicator->contentImage())
            *cgImage = image->nativeImage().autorelease();
    }

    RetainPtr<NSMutableArray> rectArray = adoptNS([[NSMutableArray alloc] init]);

    if (!*cgImage) {
        if (auto* renderer = node.renderer()) {
            FloatRect boundingBox;
            if (renderer->isRenderImage())
                boundingBox = downcast<RenderImage>(*renderer).absoluteContentQuad().enclosingBoundingBox();
            else
                boundingBox = renderer->absoluteBoundingBoxRect();

            boundingBox.inflate(margin);

            CGRect cgRect = node.document().frame()->view()->contentsToWindow(enclosingIntRect(boundingBox));
            [rectArray addObject:[NSValue value:&cgRect withObjCType:@encode(CGRect)]];

            *rects = rectArray.autorelease();
        }
        return;
    }

    FloatPoint origin = textIndicator->textBoundingRectInRootViewCoordinates().location();
    for (const FloatRect& rect : textIndicator->textRectsInBoundingRectCoordinates()) {
        CGRect cgRect = rect;
        cgRect.origin.x += origin.x();
        cgRect.origin.y += origin.y();
        cgRect = node.document().frame()->view()->contentsToWindow(enclosingIntRect(cgRect));
        [rectArray addObject:[NSValue value:&cgRect withObjCType:@encode(CGRect)]];
    }

    *rects = rectArray.autorelease();
}

@end

@implementation DOMRange (DOMRangeExtensions)

#if PLATFORM(IOS_FAMILY)
- (CGRect)boundingBox
#else
- (NSRect)boundingBox
#endif
{
    // FIXME: The call to updateLayoutIgnorePendingStylesheets should be moved into WebCore::Range.
    auto& range = *core(self);
    range.ownerDocument().updateLayoutIgnorePendingStylesheets();
    return range.absoluteBoundingBox();
}

#if PLATFORM(MAC)
- (NSImage *)renderedImageForcingBlackText:(BOOL)forceBlackText
#else
- (CGImageRef)renderedImageForcingBlackText:(BOOL)forceBlackText
#endif
{
    auto& range = *core(self);
    auto* frame = range.ownerDocument().frame();
    if (!frame)
        return nil;

    Ref<Frame> protectedFrame(*frame);

    // iOS uses CGImageRef for drag images, which doesn't support separate logical/physical sizes.
#if PLATFORM(MAC)
    RetainPtr<NSImage> renderedImage = createDragImageForRange(*frame, range, forceBlackText);

    IntSize size([renderedImage size]);
    size.scale(1 / frame->page()->deviceScaleFactor());
    [renderedImage setSize:size];

    return renderedImage.autorelease();
#else
    return createDragImageForRange(*frame, range, forceBlackText).autorelease();
#endif
}

- (NSArray *)textRects
{
    // FIXME: The call to updateLayoutIgnorePendingStylesheets should be moved into WebCore::Range.
    auto& range = *core(self);
    Vector<WebCore::IntRect> rects;
    range.ownerDocument().updateLayoutIgnorePendingStylesheets();
    range.absoluteTextRects(rects);
    return kit(rects);
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
    if (!is<RenderImage>(renderer))
        return nil;
    auto* cachedImage = downcast<RenderImage>(*renderer).cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return nil;
    return cachedImage->imageForRenderer(renderer)->nsImage();
}

#endif

@end

@implementation DOMElement (WebPrivate)

- (CTFontRef)_font
{
    auto* renderer = core(self)->renderer();
    if (!renderer)
        return nil;
    return renderer->style().fontCascade().primaryFont().getCTFont();
}

#if PLATFORM(MAC)

- (NSData *)_imageTIFFRepresentation
{
    // FIXME: Could we move this function to WebCore::Element and autogenerate?
    auto* renderer = core(self)->renderer();
    if (!is<RenderImage>(renderer))
        return nil;
    auto* cachedImage = downcast<RenderImage>(*renderer).cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return nil;
    return (__bridge NSData *)cachedImage->imageForRenderer(renderer)->tiffRepresentation();
}

#endif

- (NSURL *)_getURLAttribute:(NSString *)name
{
    auto& element = *core(self);
    return element.document().completeURL(stripLeadingAndTrailingHTMLSpaces(element.getAttribute(name)));
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
    Document& document = static_cast<HTMLLinkElement*>(core(self))->document();
    FrameView* frameView = document.frame() ? document.frame()->view() : 0;
    if (!frameView)
        return false;
    int layoutWidth = frameView->layoutWidth();
    int layoutHeight = frameView->layoutHeight();
    IntSize savedFixedLayoutSize = frameView->fixedLayoutSize();
    bool savedUseFixedLayout = frameView->useFixedLayout();
    if ((orientation == WebMediaQueryOrientationPortrait && layoutWidth > layoutHeight) ||
        (orientation == WebMediaQueryOrientationLandscape && layoutWidth < layoutHeight)) {
        // temporarily swap the orientation for the evaluation
        frameView->setFixedLayoutSize(IntSize(layoutHeight, layoutWidth));
        frameView->setUseFixedLayout(true);
    }
        
    bool result = [self _mediaQueryMatches];

    frameView->setFixedLayoutSize(savedFixedLayoutSize);
    frameView->setUseFixedLayout(savedUseFixedLayout);

    return result;
}

- (BOOL)_mediaQueryMatches
{
    HTMLLinkElement& link = *static_cast<HTMLLinkElement*>(core(self));

    auto& media = link.attributeWithoutSynchronization(HTMLNames::mediaAttr);
    if (media.isEmpty())
        return true;

    Document& document = link.document();
    auto mediaQuerySet = MediaQuerySet::create(media, MediaQueryParserContext(document));
    return MediaQueryEvaluator { "screen", document, document.renderView() ? &document.renderView()->style() : nullptr }.evaluate(mediaQuerySet.get());
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
    
    if (DOMNodeFilter *wrapper = getDOMWrapper(impl))
        return [[wrapper retain] autorelease];
    
    DOMNodeFilter *wrapper = [[DOMNodeFilter alloc] _init];
    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(impl);
    impl->ref();
    addDOMWrapper(wrapper, impl);
    return [wrapper autorelease];
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
    
    auto result = core(self)->acceptNode(*core(node));
    return result.type() == CallbackResultType::Success ? result.releaseReturnValue() : NodeFilter::FILTER_REJECT;
}

@end
