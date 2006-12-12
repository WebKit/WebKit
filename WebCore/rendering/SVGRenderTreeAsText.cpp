/*
 * Copyright (C) 2004, 2005 Apple Computer, Inc.  All rights reserved.
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
#ifdef SVG_SUPPORT
#include "SVGRenderTreeAsText.h"

#include "SVGResourceClipper.h"
#include "GraphicsTypes.h"
#include "HTMLNames.h"
#include "RenderTreeAsText.h"
#include "RenderSVGContainer.h"
#include "SVGPaintServerGradient.h"
#include "SVGPaintServerPattern.h"
#include "SVGPaintServerSolid.h"
#include "KCanvasRenderingStyle.h"
#include "SVGStyledElement.h"
#include <math.h>

namespace WebCore {

/** class + iomanip to help streaming list separators, i.e. ", " in string "a, b, c, d"
 * Can be used in cases where you don't know which item in the list is the first
 * one to be printed, but still want to avoid strings like ", b, c", works like 
 * DeprecatedStringList::join for streams
 */
class TextStreamSeparator
{
public:
    TextStreamSeparator(const DeprecatedString &s) : m_separator(s), m_needToSeparate(false) {}
private:
    friend TextStream& operator<<(TextStream& ts, TextStreamSeparator &sep);
    
private:
    DeprecatedString m_separator;
    bool m_needToSeparate;
};

TextStream& operator<<(TextStream& ts, TextStreamSeparator &sep)
{
    if (sep.m_needToSeparate)
        ts << sep.m_separator;
    else 
        sep.m_needToSeparate = true;
    return ts;
}

TextStream& operator<<(TextStream& ts, const IntPoint &p)
{
    return ts << "(" << p.x() << "," << p.y() << ")";
}

TextStream& operator<<(TextStream& ts, const IntRect &r)
{
    return ts << "at (" << r.x() << "," << r.y() << ") size " << r.width() << "x" << r.height();
}

bool hasFractions(double val)
{
    double epsilon = 0.0001;
    int ival = int(val);
    double dval = double(ival);    
    return (fabs(val-dval) > epsilon);
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

TextStream& operator<<(TextStream& ts, const FloatPoint &p)
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

TextStream& operator<<(TextStream& ts, const FloatSize &s)
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


TextStream& operator<<(TextStream& ts, const AffineTransform &m)
{
    if (m.isIdentity())
        ts << "identity";
    else {
        ts << "{m=((" << m.a() << "," << m.b() << ")(" << m.c() << "," << m.d() << "))";
        ts << " t=(" << m.e() << "," << m.f() << ")}";
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, const Color &c)
{
    return ts << c.name();
}

static void writeIndent(TextStream& ts, int indent)
{
    for (int i = 0; i != indent; ++i) {
        ts << "  ";
    }
}

//FIXME: This should be in KRenderingStyle.cpp
static TextStream& operator<<(TextStream& ts, const KCDashArray &a)
{
    ts << "{";
    KCDashArray::const_iterator end = a.end();
    for (KCDashArray::const_iterator it = a.begin(); it != end; ++it) {
        if (it != a.begin())
            ts << ", ";
        ts << *it;
    }
    ts << "}";
    return ts;
}

//FIXME: This should be in KRenderingStyle.cpp
static TextStream& operator<<(TextStream& ts, LineCap style)
{
    switch (style) {
    case ButtCap:
        ts << "BUTT"; break;
    case RoundCap:
        ts << "ROUND"; break;
    case SquareCap:
        ts << "SQUARE"; break;
    }
    return ts;
}

//FIXME: This should be in KRenderingStyle.cpp
static TextStream& operator<<(TextStream& ts, LineJoin style)
{
    switch (style) {
    case MiterJoin:
        ts << "MITER"; break;
    case RoundJoin:
        ts << "ROUND"; break;
    case BevelJoin:
        ts << "BEVEL"; break;
    }
    return ts;
}

#define DIFFERS_FROM_PARENT(path) (!parentStyle || (parentStyle->path != childStyle->path))
// avoids testing path if pred is false. This is used with tests that have side-effects
// for the parent object
#define DIFFERS_FROM_PARENT_AVOID_TEST_IF_FALSE(pred, path) (!parentStyle || ((!parentStyle->pred) || (parentStyle->path != childStyle->path)))

static void writeStyle(TextStream& ts, const RenderObject &object)
{
    const RenderStyle* style = object.style();
    const SVGRenderStyle* svgStyle = style->svgStyle();
    
    if (!object.localTransform().isIdentity())
        ts << " [transform=" << object.localTransform() << "]";
    if (svgStyle->imageRendering() != SVGRenderStyle::initialImageRendering())
        ts << " [image rendering=" << svgStyle->imageRendering() << "]";
    if (style->opacity() != RenderStyle::initialOpacity())
        ts << " [opacity=" << style->opacity() << "]";
    if (object.isRenderPath()) {
        const RenderPath& path = static_cast<const RenderPath&>(object);
        SVGPaintServer *strokePaintServer = KSVGPainterFactory::strokePaintServer(style, &path);
        if (strokePaintServer) {
            TextStreamSeparator s(" ");
            ts << " [stroke={";
            if (strokePaintServer)
                ts << s << *strokePaintServer;

            double dashOffset = KSVGPainterFactory::cssPrimitiveToLength(&path, svgStyle->strokeDashOffset(), 0.0);
            const KCDashArray& dashArray = KSVGPainterFactory::dashArrayFromRenderingStyle(style);
            double strokeWidth = KSVGPainterFactory::cssPrimitiveToLength(&path, svgStyle->strokeWidth(), 1.0);
            
            if (svgStyle->strokeOpacity() != 1.0f)
                ts << s << "[opacity=" << svgStyle->strokeOpacity() << "]";
            if (strokeWidth != 1.0f)
                ts << s << "[stroke width=" << strokeWidth << "]";
            if (svgStyle->strokeMiterLimit() != 4)
                ts << s << "[miter limit=" << svgStyle->strokeMiterLimit() << "]";
            if (svgStyle->capStyle() != 0)
                ts << s << "[line cap=" << svgStyle->capStyle() << "]";
            if (svgStyle->joinStyle() != 0)
                ts << s << "[line join=" << svgStyle->joinStyle() << "]";
            if (dashOffset != 0.0f)
                ts << s << "[dash offset=" << dashOffset << "]";
            if (!dashArray.isEmpty())
                ts << s << "[dash array=" << dashArray << "]";        
            ts << "}]";
        }
        SVGPaintServer *fillPaintServer = KSVGPainterFactory::fillPaintServer(style, &path);
        if (fillPaintServer) {
            TextStreamSeparator s(" ");
            ts << " [fill={";
            if (fillPaintServer)
                ts << s << *fillPaintServer;

            if (style->svgStyle()->fillOpacity() != 1.0f)
                ts << s << "[opacity=" << style->svgStyle()->fillOpacity() << "]";
            if (style->svgStyle()->fillRule() != RULE_NONZERO)
                ts << s << "[fill rule=" << style->svgStyle()->fillRule() << "]";
            ts << "}]";
        }
    }
    if (!svgStyle->clipPath().isEmpty())
        ts << " [clip path=\"" << svgStyle->clipPath() << "\"]";
    if (!svgStyle->startMarker().isEmpty())
        ts << " [start marker=" << svgStyle->startMarker() << "]";
    if (!svgStyle->midMarker().isEmpty())
        ts << " [middle marker=" << svgStyle->midMarker() << "]";
    if (!svgStyle->endMarker().isEmpty())
        ts << " [end marker=" << svgStyle->endMarker() << "]";
    if (!svgStyle->filter().isEmpty())
        ts << " [filter=" << svgStyle->filter() << "]";
}
#undef DIFFERS_FROM_PARENT
#undef DIFFERS_FROM_PARENT_AVOID_TEST_IF_FALSE

static TextStream& operator<<(TextStream& ts, const RenderPath &o)
{
    ts << " " << o.absoluteTransform().mapRect(o.relativeBBox());
    
    writeStyle(ts, o);
    
    ts << " [data=\"" << o.path().debugString() << "\"]";
    
    return ts;
}

static TextStream& operator<<(TextStream& ts, const RenderSVGContainer &o)
{
    ts << " " << o.absoluteTransform().mapRect(o.relativeBBox());
    
    writeStyle(ts, o);
    
    return ts;
}

static DeprecatedString getTagName(void *node)
{
    SVGStyledElement *elem = static_cast<SVGStyledElement*>(node);
    if (elem)
        return String(elem->nodeName()).deprecatedString();
    return DeprecatedString();
}

void write(TextStream& ts, const RenderSVGContainer &container, int indent)
{
    writeIndent(ts, indent);
    ts << container.renderName();
    
    if (container.element()) {
        DeprecatedString tagName = getTagName(container.element());
        if (!tagName.isEmpty())
            ts << " {" << tagName << "}";
    }
    
    ts << container << endl;
    
    for (RenderObject *child = container.firstChild(); child != NULL; child = child->nextSibling())
        write(ts, *child, indent + 1);
}

void write(TextStream& ts, const RenderPath &path, int indent)
{
    writeIndent(ts, indent);
    ts << path.renderName();
    
    if (path.element()) {
        DeprecatedString tagName = getTagName(path.element());
        if (!tagName.isEmpty())
            ts << " {" << tagName << "}";
    }
    
    ts << path << endl;
}

void writeRenderResources(TextStream& ts, Node *parent)
{
    ASSERT(parent);
    Node *node = parent;
    do {
        if (!node->isSVGElement())
            continue;
        SVGElement *svgElement = static_cast<SVGElement*>(node);
        if (!svgElement->isStyled())
            continue;

        SVGStyledElement *styled = static_cast<SVGStyledElement*>(svgElement);
        RefPtr<SVGResource> resource(styled->canvasResource());
        if (!resource)
            continue;
        
        DeprecatedString elementId = svgElement->getAttribute(HTMLNames::idAttr).deprecatedString();
        if (resource->isPaintServer()) {
            RefPtr<SVGPaintServer> paintServer = WTF::static_pointer_cast<SVGPaintServer>(resource);
            ts << "KRenderingPaintServer {id=\"" << elementId << "\" " << *paintServer << "}" << endl;
        } else
            ts << "KCanvasResource {id=\"" << elementId << "\" " << *resource << "}" << endl;
    } while ((node = node->traverseNextNode(parent)));
}

}

#endif // SVG_SUPPORT
