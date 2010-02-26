/*
 * Copyright (C) 2004, 2005, 2007, 2009 Apple Inc. All rights reserved.
 *           (C) 2005 Rob Buis <buis@kde.org>
 *           (C) 2006 Alexander Kellett <lypanov@kde.org>
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

#include "config.h"

#if ENABLE(SVG)
#include "SVGRenderTreeAsText.h"

#include "GraphicsTypes.h"
#include "HTMLNames.h"
#include "InlineTextBox.h"
#include "NodeRenderStyle.h"
#include "Path.h"
#include "RenderImage.h"
#include "RenderPath.h"
#include "RenderSVGContainer.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceMasker.h"
#include "RenderSVGRoot.h"
#include "RenderSVGText.h"
#include "RenderTreeAsText.h"
#include "SVGCharacterLayoutInfo.h"
#include "SVGInlineTextBox.h"
#include "SVGPaintServerGradient.h"
#include "SVGPaintServerPattern.h"
#include "SVGPaintServerSolid.h"
#include "SVGRootInlineBox.h"
#include "SVGStyledElement.h"
#include <math.h>

namespace WebCore {

/** class + iomanip to help streaming list separators, i.e. ", " in string "a, b, c, d"
 * Can be used in cases where you don't know which item in the list is the first
 * one to be printed, but still want to avoid strings like ", b, c".
 */
class TextStreamSeparator {
public:
    TextStreamSeparator(const String& s)
        : m_separator(s)
        , m_needToSeparate(false)
    {
    }

private:
    friend TextStream& operator<<(TextStream&, TextStreamSeparator&);

    String m_separator;
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

TextStream& operator<<(TextStream& ts, const IntPoint& p)
{
    return ts << "(" << p.x() << "," << p.y() << ")";
}

TextStream& operator<<(TextStream& ts, const IntRect& r)
{
    return ts << "at (" << r.x() << "," << r.y() << ") size " << r.width() << "x" << r.height();
}

static bool hasFractions(double val)
{
    double epsilon = 0.0001;
    int ival = static_cast<int>(val);
    double dval = static_cast<double>(ival);
    return fabs(val - dval) > epsilon;
}

TextStream& operator<<(TextStream& ts, const FloatRect &r)
{
    ts << "at ("; 
    if (hasFractions(r.x())) 
        ts << r.x();
    else 
        ts << int(r.x());
    ts << ",";
    if (hasFractions(r.y())) 
        ts << r.y();
    else 
        ts << int(r.y());
    ts << ") size ";
    if (hasFractions(r.width())) 
        ts << r.width(); 
    else 
        ts << int(r.width()); 
    ts << "x";
    if (hasFractions(r.height())) 
        ts << r.height();
    else 
        ts << int(r.height());
    return ts;
}

TextStream& operator<<(TextStream& ts, const FloatPoint& p)
{
    ts << "(";    
    if (hasFractions(p.x()))
        ts << p.x();
    else 
        ts << int(p.x());    
    ts << ",";
    if (hasFractions(p.y())) 
        ts << p.y();
    else 
        ts << int(p.y());    
    return ts << ")";
}

TextStream& operator<<(TextStream& ts, const FloatSize& s)
{
    ts << "width=";
    if (hasFractions(s.width()))
        ts << s.width();
    else
        ts << int(s.width());
    ts << " height=";
    if (hasFractions(s.height())) 
        ts << s.height();
    else
        ts << int(s.height());
    return ts;
}

TextStream& operator<<(TextStream& ts, const AffineTransform& transform)
{
    if (transform.isIdentity())
        ts << "identity";
    else
        ts << "{m=(("
           << transform.a() << "," << transform.b()
           << ")("
           << transform.c() << "," << transform.d()
           << ")) t=("
           << transform.e() << "," << transform.f()
           << ")}";

    return ts;
}

static TextStream& operator<<(TextStream& ts, const WindRule rule)
{
    switch (rule) {
    case RULE_NONZERO:
        ts << "NON-ZERO";
        break;
    case RULE_EVENODD:
        ts << "EVEN-ODD";
        break;
    }

    return ts;
}

static TextStream& operator<<(TextStream& ts, const SVGUnitTypes::SVGUnitType& unitType)
{
    switch (unitType) {
    case SVGUnitTypes::SVG_UNIT_TYPE_UNKNOWN:
        ts << "unknown";
        break;
    case SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE:
        ts << "userSpaceOnUse";
        break;
    case SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX:
        ts << "objectBoundingBox";
        break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, const Color& c)
{
    return ts << c.name();
}

static void writeIndent(TextStream& ts, int indent)
{
    for (int i = 0; i != indent; ++i)
        ts << "  ";
}

// FIXME: Maybe this should be in KCanvasRenderingStyle.cpp
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

// FIXME: Maybe this should be in GraphicsTypes.cpp
static TextStream& operator<<(TextStream& ts, LineCap style)
{
    switch (style) {
        case ButtCap:
            ts << "BUTT";
            break;
        case RoundCap:
            ts << "ROUND";
            break;
        case SquareCap:
            ts << "SQUARE";
            break;
    }
    return ts;
}

// FIXME: Maybe this should be in GraphicsTypes.cpp
static TextStream& operator<<(TextStream& ts, LineJoin style)
{
    switch (style) {
        case MiterJoin:
            ts << "MITER";
            break;
        case RoundJoin:
            ts << "ROUND";
            break;
        case BevelJoin:
            ts << "BEVEL";
            break;
    }
    return ts;
}

static void writeStyle(TextStream& ts, const RenderObject& object)
{
    const RenderStyle* style = object.style();
    const SVGRenderStyle* svgStyle = style->svgStyle();

    if (!object.localTransform().isIdentity())
        writeNameValuePair(ts, "transform", object.localTransform());
    writeIfNotDefault(ts, "image rendering", svgStyle->imageRendering(), SVGRenderStyle::initialImageRendering());
    writeIfNotDefault(ts, "opacity", style->opacity(), RenderStyle::initialOpacity());
    if (object.isRenderPath()) {
        const RenderPath& path = static_cast<const RenderPath&>(object);
        SVGPaintServer* strokePaintServer = SVGPaintServer::strokePaintServer(style, &path);
        if (strokePaintServer) {
            TextStreamSeparator s(" ");
            ts << " [stroke={";
            if (strokePaintServer)
                ts << s << *strokePaintServer;

            double dashOffset = SVGRenderStyle::cssPrimitiveToLength(&path, svgStyle->strokeDashOffset(), 0.0f);
            const DashArray& dashArray = dashArrayFromRenderingStyle(style, object.document()->documentElement()->renderStyle());
            double strokeWidth = SVGRenderStyle::cssPrimitiveToLength(&path, svgStyle->strokeWidth(), 1.0f);

            writeIfNotDefault(ts, "opacity", svgStyle->strokeOpacity(), 1.0f);
            writeIfNotDefault(ts, "stroke width", strokeWidth, 1.0);
            writeIfNotDefault(ts, "miter limit", svgStyle->strokeMiterLimit(), 4.0f);
            writeIfNotDefault(ts, "line cap", svgStyle->capStyle(), ButtCap);
            writeIfNotDefault(ts, "line join", svgStyle->joinStyle(), MiterJoin);
            writeIfNotDefault(ts, "dash offset", dashOffset, 0.0);
            if (!dashArray.isEmpty())
                writeNameValuePair(ts, "dash array", dashArray);

            ts << "}]";
        }
        SVGPaintServer* fillPaintServer = SVGPaintServer::fillPaintServer(style, &path);
        if (fillPaintServer) {
            TextStreamSeparator s(" ");
            ts << " [fill={";
            if (fillPaintServer)
                ts << s << *fillPaintServer;

            writeIfNotDefault(ts, "opacity", svgStyle->fillOpacity(), 1.0f);
            writeIfNotDefault(ts, "fill rule", svgStyle->fillRule(), RULE_NONZERO);
            ts << "}]";
        }
        writeIfNotDefault(ts, "clip rule", svgStyle->clipRule(), RULE_NONZERO);
    }

    writeIfNotEmpty(ts, "start marker", svgStyle->startMarker());
    writeIfNotEmpty(ts, "middle marker", svgStyle->midMarker());
    writeIfNotEmpty(ts, "end marker", svgStyle->endMarker());
    writeIfNotEmpty(ts, "filter", svgStyle->filter());
}

static TextStream& writePositionAndStyle(TextStream& ts, const RenderObject& object)
{
    ts << " " << const_cast<RenderObject&>(object).absoluteClippedOverflowRect();
    writeStyle(ts, object);
    return ts;
}

static TextStream& operator<<(TextStream& ts, const RenderPath& path)
{
    writePositionAndStyle(ts, path);
    writeNameAndQuotedValue(ts, "data", path.path().debugString());
    return ts;
}

static TextStream& operator<<(TextStream& ts, const RenderSVGRoot& root)
{
    return writePositionAndStyle(ts, root);
}

static void writeRenderSVGTextBox(TextStream& ts, const RenderBlock& text)
{
    SVGRootInlineBox* box = static_cast<SVGRootInlineBox*>(text.firstRootBox());

    if (!box)
        return;

    Vector<SVGTextChunk>& chunks = const_cast<Vector<SVGTextChunk>& >(box->svgTextChunks());
    ts << " at (" << text.x() << "," << text.y() << ") size " << box->width() << "x" << box->height() << " contains " << chunks.size() << " chunk(s)";

    if (text.parent() && (text.parent()->style()->color() != text.style()->color()))
        writeNameValuePair(ts, "color", text.style()->color().name());
}

static inline bool containsInlineTextBox(SVGTextChunk& chunk, SVGInlineTextBox* box)
{
    Vector<SVGInlineBoxCharacterRange>::iterator boxIt = chunk.boxes.begin();
    Vector<SVGInlineBoxCharacterRange>::iterator boxEnd = chunk.boxes.end();

    bool found = false;
    for (; boxIt != boxEnd; ++boxIt) {
        SVGInlineBoxCharacterRange& range = *boxIt;

        if (box == static_cast<SVGInlineTextBox*>(range.box)) {
            found = true;
            break;
        }
    }

    return found;
}

static inline void writeSVGInlineTextBox(TextStream& ts, SVGInlineTextBox* textBox, int indent)
{
    SVGRootInlineBox* rootBox = textBox->svgRootInlineBox();
    if (!rootBox)
        return;

    Vector<SVGTextChunk>& chunks = const_cast<Vector<SVGTextChunk>& >(rootBox->svgTextChunks());

    Vector<SVGTextChunk>::iterator it = chunks.begin();
    Vector<SVGTextChunk>::iterator end = chunks.end();

    // Write text chunks
    unsigned int i = 1;
    for (; it != end; ++it) {
        SVGTextChunk& cur = *it;

        // Write inline box character ranges
        Vector<SVGInlineBoxCharacterRange>::iterator boxIt = cur.boxes.begin();
        Vector<SVGInlineBoxCharacterRange>::iterator boxEnd = cur.boxes.end();

        if (!containsInlineTextBox(cur, textBox)) {
            i++;
            continue;
        }

        writeIndent(ts, indent + 1);

        unsigned int j = 1;
        ts << "chunk " << i << " ";

        if (cur.anchor == TA_MIDDLE) {
            ts << "(middle anchor";
            if (cur.isVerticalText)
                ts << ", vertical";
            ts << ") ";
        } else if (cur.anchor == TA_END) {
            ts << "(end anchor";
            if (cur.isVerticalText)
                ts << ", vertical";
            ts << ") ";
        } else if (cur.isVerticalText)
            ts << "(vertical) ";

        unsigned int totalOffset = 0;

        for (; boxIt != boxEnd; ++boxIt) {
            SVGInlineBoxCharacterRange& range = *boxIt;

            unsigned int offset = range.endOffset - range.startOffset;
            ASSERT(cur.start + totalOffset <= cur.end);
    
            totalOffset += offset;
      
            if (textBox != static_cast<SVGInlineTextBox*>(range.box)) {
                j++;
                continue;
            }
  
            FloatPoint topLeft = topLeftPositionOfCharacterRange(cur.start + totalOffset - offset, cur.start + totalOffset);

            ts << "text run " << j << " at (" << topLeft.x() << "," << topLeft.y() << ") ";
            ts << "startOffset " << range.startOffset << " endOffset " << range.endOffset;

            if (cur.isVerticalText)
                ts << " height " << cummulatedHeightOfInlineBoxCharacterRange(range);
            else
                ts << " width " << cummulatedWidthOfInlineBoxCharacterRange(range);

            if (textBox->direction() == RTL || textBox->m_dirOverride) {
                ts << (textBox->direction() == RTL ? " RTL" : " LTR");

                if (textBox->m_dirOverride)
                    ts << " override";
            }

            ts << ": " << quoteAndEscapeNonPrintables(String(textBox->textRenderer()->text()).substring(textBox->start() + range.startOffset, offset)) << "\n";

            j++;
        }

        i++;
    }
}

static inline void writeSVGInlineTextBoxes(TextStream& ts, const RenderText& text, int indent)
{
    for (InlineTextBox* box = text.firstTextBox(); box; box = box->nextTextBox())
        writeSVGInlineTextBox(ts, static_cast<SVGInlineTextBox*>(box), indent);
}

static void writeStandardPrefix(TextStream& ts, const RenderObject& object, int indent)
{
    writeIndent(ts, indent);
    ts << object.renderName();

    if (object.node())
        ts << " {" << object.node()->nodeName() << "}";
}

static void writeChildren(TextStream& ts, const RenderObject& object, int indent)
{
    for (RenderObject* child = object.firstChild(); child; child = child->nextSibling())
        write(ts, *child, indent + 1);
}

void writeSVGResource(TextStream& ts, const RenderObject& object, int indent)
{
    writeStandardPrefix(ts, object, indent);

    Element* element = static_cast<Element*>(object.node());
    const AtomicString& id = element->getIDAttribute();
    writeNameAndQuotedValue(ts, "id", id);    

    RenderSVGResource* resource = const_cast<RenderObject&>(object).toRenderSVGResource();
    if (resource->resourceType() == MaskerResourceType) {
        RenderSVGResourceMasker* masker = static_cast<RenderSVGResourceMasker*>(resource);
        ASSERT(masker);
        writeNameValuePair(ts, "maskUnits", masker->maskUnits());
        writeNameValuePair(ts, "maskContentUnits", masker->maskContentUnits());
    } else if (resource->resourceType() == ClipperResourceType) {
        RenderSVGResourceClipper* clipper = static_cast<RenderSVGResourceClipper*>(resource);
        ASSERT(clipper);
        writeNameValuePair(ts, "clipPathUnits", clipper->clipPathUnits());
    }

    // FIXME: Handle other RenderSVGResource* classes here, after converting them from SVGResource*.
    ts << "\n";
    writeChildren(ts, object, indent);
}

void writeSVGContainer(TextStream& ts, const RenderObject& container, int indent)
{
    writeStandardPrefix(ts, container, indent);
    writePositionAndStyle(ts, container);
    ts << "\n";
    writeResources(ts, container, indent);
    writeChildren(ts, container, indent);
}

void write(TextStream& ts, const RenderSVGRoot& root, int indent)
{
    writeStandardPrefix(ts, root, indent);
    ts << root << "\n";
    writeChildren(ts, root, indent);
}

void writeSVGText(TextStream& ts, const RenderBlock& text, int indent)
{
    writeStandardPrefix(ts, text, indent);
    writeRenderSVGTextBox(ts, text);
    ts << "\n";
    writeResources(ts, text, indent);
    writeChildren(ts, text, indent);
}

void writeSVGInlineText(TextStream& ts, const RenderText& text, int indent)
{
    writeStandardPrefix(ts, text, indent);

    // Why not just linesBoundingBox()?
    ts << " " << FloatRect(text.firstRunOrigin(), text.linesBoundingBox().size()) << "\n";
    writeResources(ts, text, indent);
    writeSVGInlineTextBoxes(ts, text, indent);
}

void writeSVGImage(TextStream& ts, const RenderImage& image, int indent)
{
    writeStandardPrefix(ts, image, indent);
    writePositionAndStyle(ts, image);
    ts << "\n";
    writeResources(ts, image, indent);
}

void write(TextStream& ts, const RenderPath& path, int indent)
{
    writeStandardPrefix(ts, path, indent);
    ts << path << "\n";
    writeResources(ts, path, indent);
}

void writeResources(TextStream& ts, const RenderObject& object, int indent)
{
    const RenderStyle* style = object.style();
    const SVGRenderStyle* svgStyle = style->svgStyle();

    if (!svgStyle->maskElement().isEmpty()) {
        if (RenderSVGResourceMasker* masker = getRenderSVGResourceById<RenderSVGResourceMasker>(object.document(), svgStyle->maskElement())) {
            writeIndent(ts, indent);
            ts << " ";
            writeNameAndQuotedValue(ts, "masker", svgStyle->maskElement());
            ts << " ";
            writeStandardPrefix(ts, *masker, 0);
            ts << " " << masker->resourceBoundingBox(object.objectBoundingBox()) << "\n";
        }
    }
    if (!svgStyle->clipPath().isEmpty()) {
        if (RenderSVGResourceClipper* clipper = getRenderSVGResourceById<RenderSVGResourceClipper>(object.document(), svgStyle->clipPath())) {
            writeIndent(ts, indent);
            ts << " ";
            writeNameAndQuotedValue(ts, "clipPath", svgStyle->clipPath());
            ts << " ";
            writeStandardPrefix(ts, *clipper, 0);
            ts << " " << clipper->resourceBoundingBox(object.objectBoundingBox()) << "\n";
        }
    }
    // FIXME: Handle other RenderSVGResource* classes here, after converting them from SVGResource*.
}

void writeRenderResources(TextStream& ts, Node* parent)
{
    ASSERT(parent);
    Node* node = parent;
    do {
        if (!node->isSVGElement())
            continue;
        SVGElement* svgElement = static_cast<SVGElement*>(node);
        if (!svgElement->isStyled())
            continue;

        SVGStyledElement* styled = static_cast<SVGStyledElement*>(svgElement);
        RefPtr<SVGResource> resource(styled->canvasResource(node->renderer()));
        if (!resource)
            continue;

        String elementId = svgElement->getAttribute(svgElement->idAttributeName());
        // FIXME: These names are lies!
        if (resource->isPaintServer()) {
            RefPtr<SVGPaintServer> paintServer = WTF::static_pointer_cast<SVGPaintServer>(resource);
            ts << "KRenderingPaintServer {id=\"" << elementId << "\" " << *paintServer << "}" << "\n";
        } else
            ts << "KCanvasResource {id=\"" << elementId << "\" " << *resource << "}" << "\n";
    } while ((node = node->traverseNextNode(parent)));
}

} // namespace WebCore

#endif // ENABLE(SVG)
