/*
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Google Inc. All rights reserved.
 *           (C) 2005 Rob Buis <buis@kde.org>
 *           (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "config.h"
#include "SVGRenderTreeAsText.h"

#include "ColorSerialization.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorSVGTextBox.h"
#include "LegacyRenderSVGImage.h"
#include "LegacyRenderSVGResourceClipperInlines.h"
#include "LegacyRenderSVGResourceFilterInlines.h"
#include "LegacyRenderSVGResourceLinearGradientInlines.h"
#include "LegacyRenderSVGResourceMarkerInlines.h"
#include "LegacyRenderSVGResourceMaskerInlines.h"
#include "LegacyRenderSVGResourcePattern.h"
#include "LegacyRenderSVGResourceRadialGradientInlines.h"
#include "LegacyRenderSVGResourceSolidColor.h"
#include "LegacyRenderSVGRoot.h"
#include "LegacyRenderSVGShapeInlines.h"
#include "NodeRenderStyle.h"
#include "NullGraphicsContext.h"
#include "RenderImage.h"
#include "RenderIterator.h"
#include "RenderObjectInlines.h"
#include "RenderSVGContainer.h"
#include "RenderSVGGradientStopInlines.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGRoot.h"
#include "RenderSVGShapeInlines.h"
#include "RenderSVGText.h"
#include "RenderStyleInlines.h"
#include "SVGCircleElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGEllipseElement.h"
#include "SVGInlineTextBoxInlines.h"
#include "SVGLineElement.h"
#include "SVGPathElement.h"
#include "SVGPathUtilities.h"
#include "SVGPolyElement.h"
#include "SVGRectElement.h"
#include "SVGRenderStyle.h"
#include "SVGRootInlineBox.h"
#include "SVGStopElement.h"
#include "StyleCachedImage.h"
#include <math.h>

namespace WebCore {

/** class + iomanip to help streaming list separators, i.e. ", " in string "a, b, c, d"
 * Can be used in cases where you don't know which item in the list is the first
 * one to be printed, but still want to avoid strings like ", b, c".
 */
class TextStreamSeparator {
public:
    TextStreamSeparator(UChar s)
        : m_separator(s)
        , m_needToSeparate(false)
    {
    }

private:
    friend TextStream& operator<<(TextStream&, TextStreamSeparator&);

    UChar m_separator;
    bool m_needToSeparate;
};

TextStream& operator<<(TextStream& ts, TextStreamSeparator& sep)
{
    if (sep.m_needToSeparate)
        ts << sep.m_separator;
    else
        sep.m_needToSeparate = true;
    return ts;
}

static TextStream& operator<<(TextStream& ts, const DashArray& a)
{
    ts << '{';
    DashArray::const_iterator end = a.end();
    for (DashArray::const_iterator it = a.begin(); it != end; ++it) {
        if (it != a.begin())
            ts << ", "_s;
        ts << *it;
    }
    ts << '}';
    return ts;
}

template<typename ValueType>
static void writeNameValuePair(TextStream& ts, ASCIILiteral name, ValueType value)
{
    ts << " ["_s << name << '=' << value << ']';
}

template<typename ValueType>
static void writeNameAndQuotedValue(TextStream& ts, ASCIILiteral name, ValueType value)
{
    ts << " ["_s << name << "=\""_s << value << "\"]"_s;
}

static void writeIfNotEmpty(TextStream& ts, ASCIILiteral name, const String& value)
{
    if (!value.isEmpty())
        writeNameValuePair(ts, name, value);
}

template<typename ValueType>
static void writeIfNotDefault(TextStream& ts, ASCIILiteral name, ValueType value, ValueType defaultValue)
{
    if (value != defaultValue)
        writeNameValuePair(ts, name, value);
}

static TextStream& operator<<(TextStream& ts, const SVGUnitTypes::SVGUnitType& unitType)
{
    ts << SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::toString(unitType);
    return ts;
}

static TextStream& operator<<(TextStream& ts, const SVGMarkerUnitsType& markerUnit)
{
    ts << SVGPropertyTraits<SVGMarkerUnitsType>::toString(markerUnit);
    return ts;
}

static TextStream& operator<<(TextStream& ts, const SVGSpreadMethodType& type)
{
    ts << SVGPropertyTraits<SVGSpreadMethodType>::toString(type).convertToASCIIUppercase();
    return ts;
}

static void writeSVGPaintingResource(TextStream& ts, const LegacyRenderSVGResource& resource)
{
    if (auto* solidColorResource = dynamicDowncast<LegacyRenderSVGResourceSolidColor>(resource)) {
        ts << "[type=SOLID] [color="_s << solidColorResource->color() << ']';
        return;
    }

    auto resourceType = resource.resourceType();
    if (resourceType == PatternResourceType)
        ts << "[type=PATTERN]"_s;
    else if (resourceType == LinearGradientResourceType)
        ts << "[type=LINEAR-GRADIENT]"_s;
    else if (resourceType == RadialGradientResourceType)
        ts << "[type=RADIAL-GRADIENT]"_s;

    // All other resources derive from LegacyRenderSVGResourceContainer.
    // FIXME: This should use a safe cast.
    const auto& container = static_cast<const LegacyRenderSVGResourceContainer&>(resource);
    ts << " [id=\""_s << container.element().getIdAttribute() << "\"]"_s;
}

static void writeSVGFillPaintingResource(TextStream& ts, const RenderElement& renderer, const LegacyRenderSVGResource& fillPaintingResource)
{
    TextStreamSeparator s(' ');
    ts << " [fill={"_s << s;
    writeSVGPaintingResource(ts, fillPaintingResource);

    const auto& svgStyle = renderer.style().svgStyle();
    writeIfNotDefault(ts, "opacity"_s, svgStyle.fillOpacity(), 1.0f);
    writeIfNotDefault(ts, "fill rule"_s, svgStyle.fillRule(), WindRule::NonZero);
    ts << "}]"_s;
}

static void writeSVGStrokePaintingResource(TextStream& ts, const RenderElement& renderer, const LegacyRenderSVGResource& strokePaintingResource, const SVGGraphicsElement& shape)
{
    TextStreamSeparator s(' ');
    ts << " [stroke={"_s << s;
    writeSVGPaintingResource(ts, strokePaintingResource);

    const auto& style = renderer.style();
    const auto& svgStyle = style.svgStyle();

    SVGLengthContext lengthContext(&shape);
    double dashOffset = lengthContext.valueForLength(svgStyle.strokeDashOffset());
    double strokeWidth = lengthContext.valueForLength(style.strokeWidth());
    auto dashArray = svgStyle.strokeDashArray().map([&](auto& length) -> DashArrayElement {
        return length.value(lengthContext);
    });

    writeIfNotDefault(ts, "opacity"_s, svgStyle.strokeOpacity(), 1.0f);
    writeIfNotDefault(ts, "stroke width"_s, strokeWidth, 1.0);
    writeIfNotDefault(ts, "miter limit"_s, style.strokeMiterLimit(), 4.0f);
    writeIfNotDefault(ts, "line cap"_s, style.capStyle(), LineCap::Butt);
    writeIfNotDefault(ts, "line join"_s, style.joinStyle(), LineJoin::Miter);
    writeIfNotDefault(ts, "dash offset"_s, dashOffset, 0.0);
    if (!dashArray.isEmpty())
        writeNameValuePair(ts, "dash array"_s, dashArray);

    if (RefPtr element = dynamicDowncast<SVGGeometryElement>(shape)) {
        double pathLength = element->pathLength();
        writeIfNotDefault(ts, "path length"_s, pathLength, 0.0);
    }

    ts << "}]"_s;
}

void writeSVGPaintingFeatures(TextStream& ts, const RenderElement& renderer, OptionSet<RenderAsTextFlag>)
{
    const RenderStyle& style = renderer.style();
    Ref svgStyle = style.svgStyle();

    if (!renderer.localTransform().isIdentity())
        writeNameValuePair(ts, "transform"_s, renderer.localTransform());
    writeIfNotDefault(ts, "image rendering"_s, style.imageRendering(), RenderStyle::initialImageRendering());
    writeIfNotDefault(ts, "opacity"_s, style.opacity(), RenderStyle::initialOpacity());

    if (auto* shape = dynamicDowncast<LegacyRenderSVGShape>(renderer)) {
        Color fallbackColor;
        if (auto* strokePaintingResource = LegacyRenderSVGResource::strokePaintingResource(const_cast<LegacyRenderSVGShape&>(*shape), shape->style(), fallbackColor))
            writeSVGStrokePaintingResource(ts, renderer, *strokePaintingResource, shape->protectedGraphicsElement());

        if (auto* fillPaintingResource = LegacyRenderSVGResource::fillPaintingResource(const_cast<LegacyRenderSVGShape&>(*shape), shape->style(), fallbackColor))
            writeSVGFillPaintingResource(ts, renderer, *fillPaintingResource);

        writeIfNotDefault(ts, "clip rule"_s, svgStyle->clipRule(), WindRule::NonZero);
    } else if (auto* shape = dynamicDowncast<RenderSVGShape>(renderer)) {
        Color fallbackColor;
        if (auto* strokePaintingResource = LegacyRenderSVGResource::strokePaintingResource(const_cast<RenderSVGShape&>(*shape), shape->style(), fallbackColor))
            writeSVGStrokePaintingResource(ts, renderer, *strokePaintingResource, shape->protectedGraphicsElement());

        if (auto* fillPaintingResource = LegacyRenderSVGResource::fillPaintingResource(const_cast<RenderSVGShape&>(*shape), shape->style(), fallbackColor))
            writeSVGFillPaintingResource(ts, renderer, *fillPaintingResource);

        writeIfNotDefault(ts, "clip rule"_s, svgStyle->clipRule(), WindRule::NonZero);
    }

    auto writeMarker = [&](ASCIILiteral name, const String& value) {
        auto* element = renderer.element();
        if (!element)
            return;

        auto fragment = SVGURIReference::fragmentIdentifierFromIRIString(value, element->protectedDocument());
        writeIfNotEmpty(ts, name, fragment);
    };

    writeMarker("start marker"_s, svgStyle->markerStartResource());
    writeMarker("middle marker"_s, svgStyle->markerMidResource());
    writeMarker("end marker"_s, svgStyle->markerEndResource());
}

static TextStream& writePositionAndStyle(TextStream& ts, const RenderElement& renderer, OptionSet<RenderAsTextFlag> behavior = { })
{
    if (behavior.contains(RenderAsTextFlag::ShowSVGGeometry)) {
        if (auto* box = dynamicDowncast<RenderBox>(renderer))
            ts << ' ' << enclosingIntRect(box->frameRect());
        ts << " clipped"_s;
    }

    ts << ' ' << enclosingIntRect(renderer.absoluteClippedOverflowRectForRenderTreeAsText());

    writeSVGPaintingFeatures(ts, renderer, behavior);
    return ts;
}

void writeSVGGraphicsElement(TextStream& ts, const SVGGraphicsElement& svgElement)
{
    SVGLengthContext lengthContext(&svgElement);

    if (auto* element = dynamicDowncast<SVGRectElement>(svgElement)) {
        writeNameValuePair(ts, "x"_s, element->x().value(lengthContext));
        writeNameValuePair(ts, "y"_s, element->y().value(lengthContext));
        writeNameValuePair(ts, "width"_s, element->width().value(lengthContext));
        writeNameValuePair(ts, "height"_s, element->height().value(lengthContext));
    } else if (auto* element = dynamicDowncast<SVGLineElement>(svgElement)) {
        writeNameValuePair(ts, "x1"_s, element->x1().value(lengthContext));
        writeNameValuePair(ts, "y1"_s, element->y1().value(lengthContext));
        writeNameValuePair(ts, "x2"_s, element->x2().value(lengthContext));
        writeNameValuePair(ts, "y2"_s, element->y2().value(lengthContext));
    } else if (auto* element = dynamicDowncast<SVGEllipseElement>(svgElement)) {
        writeNameValuePair(ts, "cx"_s, element->cx().value(lengthContext));
        writeNameValuePair(ts, "cy"_s, element->cy().value(lengthContext));
        writeNameValuePair(ts, "rx"_s, element->rx().value(lengthContext));
        writeNameValuePair(ts, "ry"_s, element->ry().value(lengthContext));
    } else if (auto* element = dynamicDowncast<SVGCircleElement>(svgElement)) {
        writeNameValuePair(ts, "cx"_s, element->cx().value(lengthContext));
        writeNameValuePair(ts, "cy"_s, element->cy().value(lengthContext));
        writeNameValuePair(ts, "r"_s, element->r().value(lengthContext));
    } else if (auto* element = dynamicDowncast<SVGPolyElement>(svgElement))
        writeNameAndQuotedValue(ts, "points"_s, element->points().valueAsString());
    else if (auto* element = dynamicDowncast<SVGPathElement>(svgElement)) {
        String pathString;
        // FIXME: We should switch to UnalteredParsing here - this will affect the path dumping output of dozens of tests.
        buildStringFromByteStream(element->pathByteStream(), pathString, NormalizedParsing);
        writeNameAndQuotedValue(ts, "data"_s, pathString);
    } else
        ASSERT_NOT_REACHED();
}

static TextStream& operator<<(TextStream& ts, const LegacyRenderSVGShape& shape)
{
    writePositionAndStyle(ts, shape);
    writeSVGGraphicsElement(ts, shape.protectedGraphicsElement());
    return ts;
}

static void writeRenderSVGTextBox(TextStream& ts, const RenderSVGText& text)
{
    auto box = InlineIterator::firstRootInlineBoxFor(text);
    if (!box)
        return;

    ts << ' ' << enclosingIntRect(FloatRect(text.location(), FloatSize(box->logicalWidth(), box->logicalHeight())));
    
    // FIXME: Remove this hack, once the new text layout engine is completly landed. We want to preserve the old layout test results for now.
    ts << " contains 1 chunk(s)"_s;

    if (text.parent() && (text.parent()->style().visitedDependentColor(CSSPropertyColor) != text.style().visitedDependentColor(CSSPropertyColor)))
        writeNameValuePair(ts, "color"_s, serializationForRenderTreeAsText(text.style().visitedDependentColor(CSSPropertyColor)));
}

static inline void writeSVGInlineTextBox(TextStream& ts, const InlineIterator::SVGTextBox& textBox)
{
    auto& fragments = textBox.textFragments();
    if (fragments.isEmpty())
        return;

    Ref svgStyle = textBox.renderer().style().svgStyle();
    String text = textBox.renderer().text();

    TextStream::IndentScope indentScope(ts);

    unsigned fragmentsSize = fragments.size();
    for (unsigned i = 0; i < fragmentsSize; ++i) {
        auto& fragment = fragments.at(i);
        ts << indent;

        unsigned startOffset = fragment.characterOffset;
        unsigned endOffset = fragment.characterOffset + fragment.length;

        // FIXME: Remove this hack, once the new text layout engine is completly landed. We want to preserve the old layout test results for now.
        ts << "chunk 1 "_s;
        TextAnchor anchor = svgStyle->textAnchor();
        bool isVerticalText = textBox.renderer().writingMode().isVertical();
        if (anchor == TextAnchor::Middle) {
            ts << "(middle anchor"_s;
            if (isVerticalText)
                ts << ", vertical"_s;
            ts << ") "_s;
        } else if (anchor == TextAnchor::End) {
            ts << "(end anchor"_s;
            if (isVerticalText)
                ts << ", vertical"_s;
            ts << ") "_s;
        } else if (isVerticalText)
            ts << "(vertical) "_s;
        startOffset -= textBox.start();
        endOffset -= textBox.start();
        // </hack>

        ts << "text run "_s << i + 1 << " at ("_s << fragment.x << ',' << fragment.y << ')';
        ts << " startOffset "_s << startOffset << " endOffset "_s << endOffset;
        if (isVerticalText)
            ts << " height "_s << fragment.height;
        else
            ts << " width "_s << fragment.width;

        if (!textBox.isLeftToRightDirection())
            ts << " RTL"_s;

        ts << ": "_s << quoteAndEscapeNonPrintables(text.substring(fragment.characterOffset, fragment.length)) << '\n';
    }
}

static inline void writeSVGInlineTextBoxes(TextStream& ts, const RenderSVGInlineText& text)
{
    for (auto& box : InlineIterator::svgTextBoxesFor(text))
        writeSVGInlineTextBox(ts, box);
}

enum class WriteIndentOrNot : bool { No, Yes };

static void writeStandardPrefix(TextStream& ts, const RenderObject& object, OptionSet<RenderAsTextFlag> behavior, WriteIndentOrNot writeIndent = WriteIndentOrNot::Yes)
{
    if (writeIndent == WriteIndentOrNot::Yes)
        ts << indent;

    ts << object.renderName().characters();

    if (behavior.contains(RenderAsTextFlag::ShowAddresses))
        ts << ' ' << &object;

    if (object.node())
        ts << " {"_s << object.node()->nodeName() << '}';

    writeDebugInfo(ts, object, behavior);
}

static void writeChildren(TextStream& ts, const RenderElement& parent, OptionSet<RenderAsTextFlag> behavior)
{
    TextStream::IndentScope indentScope(ts);

    for (const auto& child : childrenOfType<RenderObject>(parent)) {
        if (parent.document().settings().layerBasedSVGEngineEnabled() && child.hasLayer())
            continue;
        write(ts, child, behavior);
    }
}

static inline void writeCommonGradientProperties(TextStream& ts, SVGSpreadMethodType spreadMethod, const AffineTransform& gradientTransform, SVGUnitTypes::SVGUnitType gradientUnits)
{
    writeNameValuePair(ts, "gradientUnits"_s, gradientUnits);

    if (spreadMethod != SVGSpreadMethodPad)
        ts << " [spreadMethod="_s << spreadMethod << ']';

    if (!gradientTransform.isIdentity())
        ts << " [gradientTransform="_s << gradientTransform << ']';
}

void writeSVGResourceContainer(TextStream& ts, const LegacyRenderSVGResourceContainer& resource, OptionSet<RenderAsTextFlag> behavior)
{
    writeStandardPrefix(ts, resource, behavior);

    const AtomString& id = resource.element().getIdAttribute();
    writeNameAndQuotedValue(ts, "id"_s, id);

    if (auto* masker = dynamicDowncast<const LegacyRenderSVGResourceMasker>(resource)) {
        writeNameValuePair(ts, "maskUnits"_s, masker->maskUnits());
        writeNameValuePair(ts, "maskContentUnits"_s, masker->maskContentUnits());
        ts << '\n';
    } else if (resource.resourceType() == FilterResourceType) {
        const auto& filter = static_cast<const LegacyRenderSVGResourceFilter&>(resource);
        writeNameValuePair(ts, "filterUnits"_s, filter.filterUnits());
        writeNameValuePair(ts, "primitiveUnits"_s, filter.primitiveUnits());
        ts << '\n';
        // Creating a placeholder filter which is passed to the builder.
        FloatRect dummyRect;
        FloatSize dummyScale(1, 1);
        auto dummyFilter = SVGFilter::create(filter.protectedFilterElement(), FilterRenderingMode::Software, dummyScale, dummyRect, dummyRect, NullGraphicsContext());
        if (dummyFilter) {
            TextStream::IndentScope indentScope(ts);
            dummyFilter->externalRepresentation(ts, FilterRepresentation::TestOutput);
        }
    } else if (resource.resourceType() == ClipperResourceType) {
        const auto& clipper = static_cast<const LegacyRenderSVGResourceClipper&>(resource);
        writeNameValuePair(ts, "clipPathUnits"_s, clipper.clipPathUnits());
        ts << '\n';
    } else if (resource.resourceType() == MarkerResourceType) {
        const auto& marker = static_cast<const LegacyRenderSVGResourceMarker&>(resource);
        writeNameValuePair(ts, "markerUnits"_s, marker.markerUnits());
        ts << " [ref at "_s << marker.referencePoint() << ']';
        ts << " [angle="_s;
        if (auto angle = marker.angle())
            ts << *angle << "]\n"_s;
        else
            ts << "auto"_s << "]\n"_s;
    } else if (auto* pattern = dynamicDowncast<const LegacyRenderSVGResourcePattern>(resource)) {
        // Dump final results that are used for rendering. No use in asking SVGPatternElement for its patternUnits(), as it may
        // link to other patterns using xlink:href, we need to build the full inheritance chain, aka. collectPatternProperties()
        PatternAttributes attributes;
        pattern->collectPatternAttributes(attributes);

        writeNameValuePair(ts, "patternUnits"_s, attributes.patternUnits());
        writeNameValuePair(ts, "patternContentUnits"_s, attributes.patternContentUnits());

        AffineTransform transform = attributes.patternTransform();
        if (!transform.isIdentity())
            ts << " [patternTransform="_s << transform << ']';
        ts << '\n';
    } else if (auto* gradient = dynamicDowncast<const LegacyRenderSVGResourceLinearGradient>(resource)) {
        // Dump final results that are used for rendering. No use in asking SVGGradientElement for its gradientUnits(), as it may
        // link to other gradients using xlink:href, we need to build the full inheritance chain, aka. collectGradientProperties()
        LinearGradientAttributes attributes;
        gradient->linearGradientElement().collectGradientAttributes(attributes);
        writeCommonGradientProperties(ts, attributes.spreadMethod(), attributes.gradientTransform(), attributes.gradientUnits());

        ts << " [start="_s << gradient->startPoint(attributes) << "] [end="_s << gradient->endPoint(attributes) << "]\n"_s;
    }  else if (auto* gradient = dynamicDowncast<const LegacyRenderSVGResourceRadialGradient>(resource)) {
        // Dump final results that are used for rendering. No use in asking SVGGradientElement for its gradientUnits(), as it may
        // link to other gradients using xlink:href, we need to build the full inheritance chain, aka. collectGradientProperties()
        RadialGradientAttributes attributes;
        gradient->radialGradientElement().collectGradientAttributes(attributes);
        writeCommonGradientProperties(ts, attributes.spreadMethod(), attributes.gradientTransform(), attributes.gradientUnits());

        FloatPoint focalPoint = gradient->focalPoint(attributes);
        FloatPoint centerPoint = gradient->centerPoint(attributes);
        float radius = gradient->radius(attributes);
        float focalRadius = gradient->focalRadius(attributes);

        ts << " [center="_s << centerPoint << "] [focal="_s << focalPoint << "] [radius="_s << radius << "] [focalRadius="_s << focalRadius << "]\n"_s;
    } else
        ts << '\n';
    writeChildren(ts, resource, behavior);
}

void writeSVGContainer(TextStream& ts, const LegacyRenderSVGContainer& container, OptionSet<RenderAsTextFlag> behavior)
{
    // Currently RenderSVGResourceFilterPrimitive has no meaningful output.
    if (container.isRenderOrLegacyRenderSVGResourceFilterPrimitive())
        return;
    writeStandardPrefix(ts, container, behavior);
    writePositionAndStyle(ts, container, behavior);
    ts << '\n';
    writeResources(ts, container, behavior);
    writeChildren(ts, container, behavior);
}

void write(TextStream& ts, const LegacyRenderSVGRoot& root, OptionSet<RenderAsTextFlag> behavior)
{
    writeStandardPrefix(ts, root, behavior);
    writePositionAndStyle(ts, root, behavior);
    ts << '\n';
    writeChildren(ts, root, behavior);
}

void writeSVGText(TextStream& ts, const RenderSVGText& text, OptionSet<RenderAsTextFlag> behavior)
{
    writeStandardPrefix(ts, text, behavior);
    writeRenderSVGTextBox(ts, text);
    ts << '\n';
    writeResources(ts, text, behavior);
    writeChildren(ts, text, behavior);
}

void writeSVGInlineText(TextStream& ts, const RenderSVGInlineText& text, OptionSet<RenderAsTextFlag> behavior)
{
    writeStandardPrefix(ts, text, behavior);
    ts << ' ' << enclosingIntRect(FloatRect(text.firstRunLocation(), text.floatLinesBoundingBox().size())) << '\n';
    writeResources(ts, text, behavior);
    writeSVGInlineTextBoxes(ts, text);
}

void writeSVGImage(TextStream& ts, const LegacyRenderSVGImage& image, OptionSet<RenderAsTextFlag> behavior)
{
    writeStandardPrefix(ts, image, behavior);
    writePositionAndStyle(ts, image, behavior);
    ts << '\n';
    writeResources(ts, image, behavior);
}

void write(TextStream& ts, const LegacyRenderSVGShape& shape, OptionSet<RenderAsTextFlag> behavior)
{
    writeStandardPrefix(ts, shape, behavior);
    ts << shape << '\n';
    writeResources(ts, shape, behavior);
}

void writeSVGGradientStop(TextStream& ts, const RenderSVGGradientStop& stop, OptionSet<RenderAsTextFlag> behavior)
{
    writeStandardPrefix(ts, stop, behavior);

    ts << " [offset="_s << stop.element().offset() << "] [color="_s << stop.element().stopColorIncludingOpacity() << "]\n"_s;
}

void writeResources(TextStream& ts, const RenderObject& renderer, OptionSet<RenderAsTextFlag> behavior)
{
    const RenderStyle& style = renderer.style();

    // FIXME: We want to use SVGResourcesCache to determine which resources are present, instead of quering the resource <-> id cache.
    // For now leave the DRT output as is, but later on we should change this so cycles are properly ignored in the DRT output.
    if (style.hasPositionedMask()) {
        RefPtr maskImage = style.maskImage();
        Ref document = renderer.document();
        auto reresolvedURL = maskImage ? maskImage->reresolvedURL(document) : URL();

        if (!reresolvedURL.isEmpty()) {
            auto resourceID = SVGURIReference::fragmentIdentifierFromIRIString(reresolvedURL.string(), document);
            if (auto* masker = getRenderSVGResourceById<LegacyRenderSVGResourceMasker>(renderer.treeScopeForSVGReferences(), resourceID)) {
                ts << indent << ' ';
                writeNameAndQuotedValue(ts, "masker"_s, resourceID);
                ts << ' ';
                writeStandardPrefix(ts, *masker, behavior, WriteIndentOrNot::No);
                ts << ' ' << masker->resourceBoundingBox(renderer, RepaintRectCalculation::Accurate) << '\n';
            }
        }
    }
    if (auto* resourceClipPath = dynamicDowncast<ReferencePathOperation>(style.clipPath())) {
        AtomString id = resourceClipPath->fragment();
        if (LegacyRenderSVGResourceClipper* clipper = getRenderSVGResourceById<LegacyRenderSVGResourceClipper>(renderer.treeScopeForSVGReferences(), id)) {
            ts << indent << ' ';
            writeNameAndQuotedValue(ts, "clipPath"_s, id);
            ts << ' ';
            writeStandardPrefix(ts, *clipper, behavior, WriteIndentOrNot::No);
            ts << ' ' << clipper->resourceBoundingBox(renderer, RepaintRectCalculation::Accurate) << '\n';
        }
    }
    if (style.hasFilter()) {
        const FilterOperations& filterOperations = style.filter();
        if (filterOperations.size() == 1) {
            if (RefPtr referenceFilterOperation = dynamicDowncast<ReferenceFilterOperation>(*filterOperations.at(0))) {
                AtomString id = SVGURIReference::fragmentIdentifierFromIRIString(referenceFilterOperation->url(), renderer.protectedDocument());
                if (LegacyRenderSVGResourceFilter* filter = getRenderSVGResourceById<LegacyRenderSVGResourceFilter>(renderer.treeScopeForSVGReferences(), id)) {
                    ts << indent << ' ';
                    writeNameAndQuotedValue(ts, "filter"_s, id);
                    ts << ' ';
                    writeStandardPrefix(ts, *filter, behavior, WriteIndentOrNot::No);
                    ts << ' ' << filter->resourceBoundingBox(renderer, RepaintRectCalculation::Accurate) << '\n';
                }
            }
        }
    }
}

} // namespace WebCore
