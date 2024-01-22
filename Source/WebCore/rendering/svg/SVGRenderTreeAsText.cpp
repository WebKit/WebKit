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
    ts << "{";
    DashArray::const_iterator end = a.end();
    for (DashArray::const_iterator it = a.begin(); it != end; ++it) {
        if (it != a.begin())
            ts << ", ";
        ts << *it;
    }
    ts << "}";
    return ts;
}

template<typename ValueType>
static void writeNameValuePair(TextStream& ts, const char* name, ValueType value)
{
    ts << " [" << name << "=" << value << "]";
}

template<typename ValueType>
static void writeNameAndQuotedValue(TextStream& ts, const char* name, ValueType value)
{
    ts << " [" << name << "=\"" << value << "\"]";
}

static void writeIfNotEmpty(TextStream& ts, const char* name, const String& value)
{
    if (!value.isEmpty())
        writeNameValuePair(ts, name, value);
}

template<typename ValueType>
static void writeIfNotDefault(TextStream& ts, const char* name, ValueType value, ValueType defaultValue)
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
        ts << "[type=SOLID] [color=" << solidColorResource->color() << "]";
        return;
    }

    auto resourceType = resource.resourceType();
    if (resourceType == PatternResourceType)
        ts << "[type=PATTERN]";
    else if (resourceType == LinearGradientResourceType)
        ts << "[type=LINEAR-GRADIENT]";
    else if (resourceType == RadialGradientResourceType)
        ts << "[type=RADIAL-GRADIENT]";

    // All other resources derive from LegacyRenderSVGResourceContainer.
    // FIXME: This should use a safe cast.
    const auto& container = static_cast<const LegacyRenderSVGResourceContainer&>(resource);
    ts << " [id=\"" << container.element().getIdAttribute() << "\"]";
}

static void writeSVGFillPaintingResource(TextStream& ts, const RenderElement& renderer, const LegacyRenderSVGResource& fillPaintingResource)
{
    TextStreamSeparator s(' ');
    ts << " [fill={" << s;
    writeSVGPaintingResource(ts, fillPaintingResource);

    const auto& svgStyle = renderer.style().svgStyle();
    writeIfNotDefault(ts, "opacity", svgStyle.fillOpacity(), 1.0f);
    writeIfNotDefault(ts, "fill rule", svgStyle.fillRule(), WindRule::NonZero);
    ts << "}]";
}

static void writeSVGStrokePaintingResource(TextStream& ts, const RenderElement& renderer, const LegacyRenderSVGResource& strokePaintingResource, const SVGGraphicsElement& shape)
{
    TextStreamSeparator s(' ');
    ts << " [stroke={" << s;
    writeSVGPaintingResource(ts, strokePaintingResource);

    const auto& style = renderer.style();
    const auto& svgStyle = style.svgStyle();

    SVGLengthContext lengthContext(&shape);
    double dashOffset = lengthContext.valueForLength(svgStyle.strokeDashOffset());
    double strokeWidth = lengthContext.valueForLength(style.strokeWidth());
    auto dashArray = svgStyle.strokeDashArray().map([&](auto& length) -> DashArrayElement {
        return length.value(lengthContext);
    });

    writeIfNotDefault(ts, "opacity", svgStyle.strokeOpacity(), 1.0f);
    writeIfNotDefault(ts, "stroke width", strokeWidth, 1.0);
    writeIfNotDefault(ts, "miter limit", style.strokeMiterLimit(), 4.0f);
    writeIfNotDefault(ts, "line cap", style.capStyle(), LineCap::Butt);
    writeIfNotDefault(ts, "line join", style.joinStyle(), LineJoin::Miter);
    writeIfNotDefault(ts, "dash offset", dashOffset, 0.0);
    if (!dashArray.isEmpty())
        writeNameValuePair(ts, "dash array", dashArray);

    if (auto* element = dynamicDowncast<SVGGeometryElement>(shape)) {
        double pathLength = element->pathLength();
        writeIfNotDefault(ts, "path length", pathLength, 0.0);
    }

    ts << "}]";
}

void writeSVGPaintingFeatures(TextStream& ts, const RenderElement& renderer, OptionSet<RenderAsTextFlag>)
{
    const RenderStyle& style = renderer.style();
    const SVGRenderStyle& svgStyle = style.svgStyle();

    if (!renderer.localTransform().isIdentity())
        writeNameValuePair(ts, "transform", renderer.localTransform());
    writeIfNotDefault(ts, "image rendering", style.imageRendering(), RenderStyle::initialImageRendering());
    writeIfNotDefault(ts, "opacity", style.opacity(), RenderStyle::initialOpacity());

    if (auto* shape = dynamicDowncast<LegacyRenderSVGShape>(renderer)) {
        Color fallbackColor;
        if (auto* strokePaintingResource = LegacyRenderSVGResource::strokePaintingResource(const_cast<LegacyRenderSVGShape&>(*shape), shape->style(), fallbackColor))
            writeSVGStrokePaintingResource(ts, renderer, *strokePaintingResource, shape->graphicsElement());

        if (auto* fillPaintingResource = LegacyRenderSVGResource::fillPaintingResource(const_cast<LegacyRenderSVGShape&>(*shape), shape->style(), fallbackColor))
            writeSVGFillPaintingResource(ts, renderer, *fillPaintingResource);

        writeIfNotDefault(ts, "clip rule", svgStyle.clipRule(), WindRule::NonZero);
    }

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    else if (auto* shape = dynamicDowncast<RenderSVGShape>(renderer)) {
        Color fallbackColor;
        if (auto* strokePaintingResource = LegacyRenderSVGResource::strokePaintingResource(const_cast<RenderSVGShape&>(*shape), shape->style(), fallbackColor))
            writeSVGStrokePaintingResource(ts, renderer, *strokePaintingResource, shape->graphicsElement());

        if (auto* fillPaintingResource = LegacyRenderSVGResource::fillPaintingResource(const_cast<RenderSVGShape&>(*shape), shape->style(), fallbackColor))
            writeSVGFillPaintingResource(ts, renderer, *fillPaintingResource);

        writeIfNotDefault(ts, "clip rule", svgStyle.clipRule(), WindRule::NonZero);
    }
#endif

    auto writeMarker = [&](const char* name, const String& value) {
        auto* element = renderer.element();
        if (!element)
            return;

        auto fragment = SVGURIReference::fragmentIdentifierFromIRIString(value, element->document());
        writeIfNotEmpty(ts, name, fragment);
    };

    writeMarker("start marker", svgStyle.markerStartResource());
    writeMarker("middle marker", svgStyle.markerMidResource());
    writeMarker("end marker", svgStyle.markerEndResource());
}

static TextStream& writePositionAndStyle(TextStream& ts, const RenderElement& renderer, OptionSet<RenderAsTextFlag> behavior = { })
{
    if (behavior.contains(RenderAsTextFlag::ShowSVGGeometry)) {
        if (auto* box = dynamicDowncast<RenderBox>(renderer))
            ts << " " << enclosingIntRect(box->frameRect());
        ts << " clipped";
    }

    ts << " " << enclosingIntRect(renderer.absoluteClippedOverflowRectForRenderTreeAsText());

    writeSVGPaintingFeatures(ts, renderer, behavior);
    return ts;
}

void writeSVGGraphicsElement(TextStream& ts, const SVGGraphicsElement& svgElement)
{
    SVGLengthContext lengthContext(&svgElement);

    if (auto* element = dynamicDowncast<SVGRectElement>(svgElement)) {
        writeNameValuePair(ts, "x", element->x().value(lengthContext));
        writeNameValuePair(ts, "y", element->y().value(lengthContext));
        writeNameValuePair(ts, "width", element->width().value(lengthContext));
        writeNameValuePair(ts, "height", element->height().value(lengthContext));
    } else if (auto* element = dynamicDowncast<SVGLineElement>(svgElement)) {
        writeNameValuePair(ts, "x1", element->x1().value(lengthContext));
        writeNameValuePair(ts, "y1", element->y1().value(lengthContext));
        writeNameValuePair(ts, "x2", element->x2().value(lengthContext));
        writeNameValuePair(ts, "y2", element->y2().value(lengthContext));
    } else if (auto* element = dynamicDowncast<SVGEllipseElement>(svgElement)) {
        writeNameValuePair(ts, "cx", element->cx().value(lengthContext));
        writeNameValuePair(ts, "cy", element->cy().value(lengthContext));
        writeNameValuePair(ts, "rx", element->rx().value(lengthContext));
        writeNameValuePair(ts, "ry", element->ry().value(lengthContext));
    } else if (auto* element = dynamicDowncast<SVGCircleElement>(svgElement)) {
        writeNameValuePair(ts, "cx", element->cx().value(lengthContext));
        writeNameValuePair(ts, "cy", element->cy().value(lengthContext));
        writeNameValuePair(ts, "r", element->r().value(lengthContext));
    } else if (auto* element = dynamicDowncast<SVGPolyElement>(svgElement))
        writeNameAndQuotedValue(ts, "points", element->points().valueAsString());
    else if (auto* element = dynamicDowncast<SVGPathElement>(svgElement)) {
        String pathString;
        // FIXME: We should switch to UnalteredParsing here - this will affect the path dumping output of dozens of tests.
        buildStringFromByteStream(element->pathByteStream(), pathString, NormalizedParsing);
        writeNameAndQuotedValue(ts, "data", pathString);
    } else
        ASSERT_NOT_REACHED();
}

static TextStream& operator<<(TextStream& ts, const LegacyRenderSVGShape& shape)
{
    writePositionAndStyle(ts, shape);
    writeSVGGraphicsElement(ts, shape.graphicsElement());
    return ts;
}

static void writeRenderSVGTextBox(TextStream& ts, const RenderSVGText& text)
{
    auto* box = downcast<SVGRootInlineBox>(text.firstRootBox());
    if (!box)
        return;

    ts << " " << enclosingIntRect(FloatRect(text.location(), FloatSize(box->logicalWidth(), box->logicalHeight())));
    
    // FIXME: Remove this hack, once the new text layout engine is completly landed. We want to preserve the old layout test results for now.
    ts << " contains 1 chunk(s)";

    if (text.parent() && (text.parent()->style().visitedDependentColor(CSSPropertyColor) != text.style().visitedDependentColor(CSSPropertyColor)))
        writeNameValuePair(ts, "color", serializationForRenderTreeAsText(text.style().visitedDependentColor(CSSPropertyColor)));
}

static inline void writeSVGInlineTextBox(TextStream& ts, SVGInlineTextBox* textBox)
{
    Vector<SVGTextFragment>& fragments = textBox->textFragments();
    if (fragments.isEmpty())
        return;

    const SVGRenderStyle& svgStyle = textBox->renderer().style().svgStyle();
    String text = textBox->renderer().text();

    TextStream::IndentScope indentScope(ts);

    unsigned fragmentsSize = fragments.size();
    for (unsigned i = 0; i < fragmentsSize; ++i) {
        SVGTextFragment& fragment = fragments.at(i);
        ts << indent;

        unsigned startOffset = fragment.characterOffset;
        unsigned endOffset = fragment.characterOffset + fragment.length;

        // FIXME: Remove this hack, once the new text layout engine is completly landed. We want to preserve the old layout test results for now.
        ts << "chunk 1 ";
        TextAnchor anchor = svgStyle.textAnchor();
        bool isVerticalText = textBox->renderer().style().isVerticalWritingMode();
        if (anchor == TextAnchor::Middle) {
            ts << "(middle anchor";
            if (isVerticalText)
                ts << ", vertical";
            ts << ") ";
        } else if (anchor == TextAnchor::End) {
            ts << "(end anchor";
            if (isVerticalText)
                ts << ", vertical";
            ts << ") ";
        } else if (isVerticalText)
            ts << "(vertical) ";
        startOffset -= textBox->start();
        endOffset -= textBox->start();
        // </hack>

        ts << "text run " << i + 1 << " at (" << fragment.x << "," << fragment.y << ")";
        ts << " startOffset " << startOffset << " endOffset " << endOffset;
        if (isVerticalText)
            ts << " height " << fragment.height;
        else
            ts << " width " << fragment.width;

        if (!textBox->isLeftToRightDirection())
            ts << " RTL";

        ts << ": " << quoteAndEscapeNonPrintables(text.substring(fragment.characterOffset, fragment.length)) << "\n";
    }
}

static inline void writeSVGInlineTextBoxes(TextStream& ts, const RenderText& text)
{
    for (auto* box = text.firstTextBox(); box; box = box->nextTextBox()) {
        if (auto* inlineTextBox = dynamicDowncast<SVGInlineTextBox>(*box))
            writeSVGInlineTextBox(ts, inlineTextBox);
    }
}

enum class WriteIndentOrNot : bool { No, Yes };

static void writeStandardPrefix(TextStream& ts, const RenderObject& object, OptionSet<RenderAsTextFlag> behavior, WriteIndentOrNot writeIndent = WriteIndentOrNot::Yes)
{
    if (writeIndent == WriteIndentOrNot::Yes)
        ts << indent;

    ts << object.renderName().characters();

    if (behavior.contains(RenderAsTextFlag::ShowAddresses))
        ts << " " << &object;

    if (object.node())
        ts << " {" << object.node()->nodeName() << "}";

    writeDebugInfo(ts, object, behavior);
}

static void writeChildren(TextStream& ts, const RenderElement& parent, OptionSet<RenderAsTextFlag> behavior)
{
    TextStream::IndentScope indentScope(ts);

    for (const auto& child : childrenOfType<RenderObject>(parent)) {
#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (parent.document().settings().layerBasedSVGEngineEnabled() && child.hasLayer())
            continue;
#endif
        write(ts, child, behavior);
    }
}

static inline void writeCommonGradientProperties(TextStream& ts, SVGSpreadMethodType spreadMethod, const AffineTransform& gradientTransform, SVGUnitTypes::SVGUnitType gradientUnits)
{
    writeNameValuePair(ts, "gradientUnits", gradientUnits);

    if (spreadMethod != SVGSpreadMethodPad)
        ts << " [spreadMethod=" << spreadMethod << "]";

    if (!gradientTransform.isIdentity())
        ts << " [gradientTransform=" << gradientTransform << "]";
}

void writeSVGResourceContainer(TextStream& ts, const LegacyRenderSVGResourceContainer& resource, OptionSet<RenderAsTextFlag> behavior)
{
    writeStandardPrefix(ts, resource, behavior);

    const AtomString& id = resource.element().getIdAttribute();
    writeNameAndQuotedValue(ts, "id", id);    

    if (resource.resourceType() == MaskerResourceType) {
        const auto& masker = static_cast<const LegacyRenderSVGResourceMasker&>(resource);
        writeNameValuePair(ts, "maskUnits", masker.maskUnits());
        writeNameValuePair(ts, "maskContentUnits", masker.maskContentUnits());
        ts << "\n";
    } else if (resource.resourceType() == FilterResourceType) {
        const auto& filter = static_cast<const LegacyRenderSVGResourceFilter&>(resource);
        writeNameValuePair(ts, "filterUnits", filter.filterUnits());
        writeNameValuePair(ts, "primitiveUnits", filter.primitiveUnits());
        ts << "\n";
        // Creating a placeholder filter which is passed to the builder.
        FloatRect dummyRect;
        FloatSize dummyScale(1, 1);
        auto dummyFilter = SVGFilter::create(filter.filterElement(), FilterRenderingMode::Software, dummyScale, dummyRect, dummyRect, NullGraphicsContext());
        if (dummyFilter) {
            TextStream::IndentScope indentScope(ts);
            dummyFilter->externalRepresentation(ts, FilterRepresentation::TestOutput);
        }
    } else if (resource.resourceType() == ClipperResourceType) {
        const auto& clipper = static_cast<const LegacyRenderSVGResourceClipper&>(resource);
        writeNameValuePair(ts, "clipPathUnits", clipper.clipPathUnits());
        ts << "\n";
    } else if (resource.resourceType() == MarkerResourceType) {
        const auto& marker = static_cast<const LegacyRenderSVGResourceMarker&>(resource);
        writeNameValuePair(ts, "markerUnits", marker.markerUnits());
        ts << " [ref at " << marker.referencePoint() << "]";
        ts << " [angle=";
        if (auto angle = marker.angle())
            ts << *angle << "]\n";
        else
            ts << "auto" << "]\n";
    } else if (resource.resourceType() == PatternResourceType) {
        const auto& pattern = static_cast<const LegacyRenderSVGResourcePattern&>(resource);

        // Dump final results that are used for rendering. No use in asking SVGPatternElement for its patternUnits(), as it may
        // link to other patterns using xlink:href, we need to build the full inheritance chain, aka. collectPatternProperties()
        PatternAttributes attributes;
        pattern.collectPatternAttributes(attributes);

        writeNameValuePair(ts, "patternUnits", attributes.patternUnits());
        writeNameValuePair(ts, "patternContentUnits", attributes.patternContentUnits());

        AffineTransform transform = attributes.patternTransform();
        if (!transform.isIdentity())
            ts << " [patternTransform=" << transform << "]";
        ts << "\n";
    } else if (resource.resourceType() == LinearGradientResourceType) {
        const auto& gradient = static_cast<const LegacyRenderSVGResourceLinearGradient&>(resource);

        // Dump final results that are used for rendering. No use in asking SVGGradientElement for its gradientUnits(), as it may
        // link to other gradients using xlink:href, we need to build the full inheritance chain, aka. collectGradientProperties()
        LinearGradientAttributes attributes;
        gradient.linearGradientElement().collectGradientAttributes(attributes);
        writeCommonGradientProperties(ts, attributes.spreadMethod(), attributes.gradientTransform(), attributes.gradientUnits());

        ts << " [start=" << gradient.startPoint(attributes) << "] [end=" << gradient.endPoint(attributes) << "]\n";
    }  else if (resource.resourceType() == RadialGradientResourceType) {
        const auto& gradient = static_cast<const LegacyRenderSVGResourceRadialGradient&>(resource);

        // Dump final results that are used for rendering. No use in asking SVGGradientElement for its gradientUnits(), as it may
        // link to other gradients using xlink:href, we need to build the full inheritance chain, aka. collectGradientProperties()
        RadialGradientAttributes attributes;
        gradient.radialGradientElement().collectGradientAttributes(attributes);
        writeCommonGradientProperties(ts, attributes.spreadMethod(), attributes.gradientTransform(), attributes.gradientUnits());

        FloatPoint focalPoint = gradient.focalPoint(attributes);
        FloatPoint centerPoint = gradient.centerPoint(attributes);
        float radius = gradient.radius(attributes);
        float focalRadius = gradient.focalRadius(attributes);

        ts << " [center=" << centerPoint << "] [focal=" << focalPoint << "] [radius=" << radius << "] [focalRadius=" << focalRadius << "]\n";
    } else
        ts << "\n";
    writeChildren(ts, resource, behavior);
}

void writeSVGContainer(TextStream& ts, const LegacyRenderSVGContainer& container, OptionSet<RenderAsTextFlag> behavior)
{
    // Currently RenderSVGResourceFilterPrimitive has no meaningful output.
    if (container.isRenderSVGResourceFilterPrimitive())
        return;
    writeStandardPrefix(ts, container, behavior);
    writePositionAndStyle(ts, container, behavior);
    ts << "\n";
    writeResources(ts, container, behavior);
    writeChildren(ts, container, behavior);
}

void write(TextStream& ts, const LegacyRenderSVGRoot& root, OptionSet<RenderAsTextFlag> behavior)
{
    writeStandardPrefix(ts, root, behavior);
    writePositionAndStyle(ts, root, behavior);
    ts << "\n";
    writeChildren(ts, root, behavior);
}

void writeSVGText(TextStream& ts, const RenderSVGText& text, OptionSet<RenderAsTextFlag> behavior)
{
    writeStandardPrefix(ts, text, behavior);
    writeRenderSVGTextBox(ts, text);
    ts << "\n";
    writeResources(ts, text, behavior);
    writeChildren(ts, text, behavior);
}

void writeSVGInlineText(TextStream& ts, const RenderSVGInlineText& text, OptionSet<RenderAsTextFlag> behavior)
{
    writeStandardPrefix(ts, text, behavior);
    ts << " " << enclosingIntRect(FloatRect(text.firstRunLocation(), text.floatLinesBoundingBox().size())) << "\n";
    writeResources(ts, text, behavior);
    writeSVGInlineTextBoxes(ts, text);
}

void writeSVGImage(TextStream& ts, const LegacyRenderSVGImage& image, OptionSet<RenderAsTextFlag> behavior)
{
    writeStandardPrefix(ts, image, behavior);
    writePositionAndStyle(ts, image, behavior);
    ts << "\n";
    writeResources(ts, image, behavior);
}

void write(TextStream& ts, const LegacyRenderSVGShape& shape, OptionSet<RenderAsTextFlag> behavior)
{
    writeStandardPrefix(ts, shape, behavior);
    ts << shape << "\n";
    writeResources(ts, shape, behavior);
}

void writeSVGGradientStop(TextStream& ts, const RenderSVGGradientStop& stop, OptionSet<RenderAsTextFlag> behavior)
{
    writeStandardPrefix(ts, stop, behavior);

    ts << " [offset=" << stop.element().offset() << "] [color=" << stop.element().stopColorIncludingOpacity() << "]\n";
}

void writeResources(TextStream& ts, const RenderObject& renderer, OptionSet<RenderAsTextFlag> behavior)
{
    const RenderStyle& style = renderer.style();

    // FIXME: We want to use SVGResourcesCache to determine which resources are present, instead of quering the resource <-> id cache.
    // For now leave the DRT output as is, but later on we should change this so cycles are properly ignored in the DRT output.
    if (style.hasPositionedMask()) {
        auto* maskImage = style.maskImage();
        auto& document = renderer.document();
        auto reresolvedURL = maskImage ? maskImage->reresolvedURL(document) : URL();

        if (!reresolvedURL.isEmpty()) {
            auto resourceID = SVGURIReference::fragmentIdentifierFromIRIString(reresolvedURL.string(), document);
            if (auto* masker = getRenderSVGResourceById<LegacyRenderSVGResourceMasker>(renderer.treeScopeForSVGReferences(), resourceID)) {
                ts << indent << " ";
                writeNameAndQuotedValue(ts, "masker", resourceID);
                ts << " ";
                writeStandardPrefix(ts, *masker, behavior, WriteIndentOrNot::No);
                ts << " " << masker->resourceBoundingBox(renderer, RepaintRectCalculation::Accurate) << "\n";
            }
        }
    }
    if (auto* resourceClipPath = dynamicDowncast<ReferencePathOperation>(style.clipPath())) {
        AtomString id = resourceClipPath->fragment();
        if (LegacyRenderSVGResourceClipper* clipper = getRenderSVGResourceById<LegacyRenderSVGResourceClipper>(renderer.treeScopeForSVGReferences(), id)) {
            ts << indent << " ";
            writeNameAndQuotedValue(ts, "clipPath", resourceClipPath->fragment());
            ts << " ";
            writeStandardPrefix(ts, *clipper, behavior, WriteIndentOrNot::No);
            ts << " " << clipper->resourceBoundingBox(renderer, RepaintRectCalculation::Accurate) << "\n";
        }
    }
    if (style.hasFilter()) {
        const FilterOperations& filterOperations = style.filter();
        if (filterOperations.size() == 1) {
            const FilterOperation& filterOperation = *filterOperations.at(0);
            if (auto* referenceFilterOperation = dynamicDowncast<ReferenceFilterOperation>(filterOperation)) {
                AtomString id = SVGURIReference::fragmentIdentifierFromIRIString(referenceFilterOperation->url(), renderer.document());
                if (LegacyRenderSVGResourceFilter* filter = getRenderSVGResourceById<LegacyRenderSVGResourceFilter>(renderer.treeScopeForSVGReferences(), id)) {
                    ts << indent << " ";
                    writeNameAndQuotedValue(ts, "filter", id);
                    ts << " ";
                    writeStandardPrefix(ts, *filter, behavior, WriteIndentOrNot::No);
                    ts << " " << filter->resourceBoundingBox(renderer, RepaintRectCalculation::Accurate) << "\n";
                }
            }
        }
    }
}

} // namespace WebCore
