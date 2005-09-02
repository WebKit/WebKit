/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml css code by:
    Copyright(C) 1999-2003 Lars Knoll(knoll@kde.org)
             (C) 2003 Apple Computer, Inc.
             (C) 2004 Allan Sandfeld Jensen(kde@carewolf.com)
             (C) 2004 Germain Garand(germain@ebooksfrance.org)

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <kurl.h>
#include <kstandarddirs.h>

#include <qfile.h>
#include <qpaintdevice.h>

#include <kdom/core/ElementImpl.h>
#include <kdom/core/CDFInterface.h>
#include <kdom/core/DocumentImpl.h>
#include <kdom/css/cssvalues.h>
#include <kdom/css/CSSRuleImpl.h>
#include <kdom/css/CSSValueImpl.h>
#include <kdom/css/cssproperties.h>
#include <kdom/css/MediaListImpl.h>
#include <kdom/css/CSSRuleListImpl.h>
#include <kdom/css/CSSStyleRuleImpl.h>
#include <kdom/css/CSSMediaRuleImpl.h>
#include <kdom/css/CSSValueListImpl.h>
#include <kdom/css/CSSStyleSheetImpl.h>
#include <kdom/css/StyleSheetListImpl.h>
#include <kdom/css/CSSPrimitiveValueImpl.h>
#include <kdom/css/CSSStyleDeclarationImpl.h>

#include "ksvg.h"
#include "svgtags.h"
#include <ksvg2/css/cssvalues.h>
#include "SVGColorImpl.h"
#include "SVGPaintImpl.h"
#include <ksvg2/css/cssproperties.h>
#include "SVGRenderStyle.h"
#include "SVGRenderStyleDefs.h"
#include "SVGCSSStyleSelector.h"
#include "SVGCSSStyleSheetImpl.h"
#include "SVGStyledElementImpl.h"

#include <stdlib.h>

using namespace KSVG;

#define HANDLE_INHERIT(prop, Prop) \
if(isInherit) \
{\
    svgstyle->set##Prop(static_cast<SVGRenderStyle *>(parentStyle)->prop());\
    return;\
}

#define HANDLE_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_INHERIT(prop, Prop) \
else if(isInitial) \
    svgstyle->set##Prop(SVGRenderStyle::initial##Prop());

#define HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(prop, Prop, Value) \
HANDLE_INHERIT(prop, Prop) \
else if(isInitial) \
    svgstyle->set##Prop(SVGRenderStyle::initial##Value());

#define HANDLE_INHERIT_COND(propID, prop, Prop) \
if(id == propID) \
{\
    svgstyle->set##Prop(parentStyle->prop());\
    return;\
}

#define HANDLE_INITIAL_COND(propID, Prop) \
if(id == propID) \
{\
    svgstyle->set##Prop(SVGRenderStyle::initial##Prop());\
    return;\
}

#define HANDLE_INITIAL_COND_WITH_VALUE(propID, Prop, Value) \
if(id == propID) \
{\
    svgstyle->set##Prop(SVGRenderStyle::initial##Value());\
    return;\
}

KDOM::CSSStyleSheetImpl *SVGCSSStyleSelector::s_defaultSheet = 0;
KDOM::CSSStyleSelectorList *SVGCSSStyleSelector::s_defaultStyle = 0;
KDOM::CSSStyleSelectorList *SVGCSSStyleSelector::s_defaultPrintStyle = 0;

SVGCSSStyleSelector::SVGCSSStyleSelector(KDOM::DocumentImpl *doc, const QString &userStyleSheet, KDOM::StyleSheetListImpl *styleSheets, const KURL &url, bool strictParsing)
: KDOM::CSSStyleSelector(doc, userStyleSheet, styleSheets, url, strictParsing)
{
    if(!s_defaultStyle)
        loadDefaultStyle(doc);
        
    defaultStyle = s_defaultStyle;
    defaultPrintStyle = s_defaultPrintStyle;
    defaultQuirksStyle = 0;
    
    buildLists();
}

SVGCSSStyleSelector::SVGCSSStyleSelector(KDOM::CSSStyleSheetImpl *sheet) : KDOM::CSSStyleSelector(sheet)
{
    if(!s_defaultStyle)
        loadDefaultStyle(0);
        
    defaultStyle = s_defaultStyle;
    defaultPrintStyle = s_defaultPrintStyle;
    defaultQuirksStyle = 0;
    
    buildLists();
}

SVGCSSStyleSelector::~SVGCSSStyleSelector()
{
}

void SVGCSSStyleSelector::loadDefaultStyle(KDOM::DocumentImpl *doc)
{
    QFile f(locate("data", QString::fromLatin1("ksvg2/svg.css")));
    f.open(IO_ReadOnly);

    Q3CString file(f.size() + 1);
    int readbytes = f.readBlock(file.data(), f.size());
    f.close();
    if(readbytes >= 0)
        file[readbytes] = '\0';

    QString style = QString::fromLatin1(file.data());
    KDOM::DOMString str(style);

    s_defaultSheet = new SVGCSSStyleSheetImpl(doc);
    s_defaultSheet->parseString(str.handle());

    // Collect only strict-mode rules.
    s_defaultStyle = new KDOM::CSSStyleSelectorList();
    s_defaultStyle->append(s_defaultSheet, KDOM::DOMString("screen").handle());

    s_defaultPrintStyle = new KDOM::CSSStyleSelectorList();
    s_defaultPrintStyle->append(s_defaultSheet, KDOM::DOMString("print").handle());
}

unsigned int SVGCSSStyleSelector::addExtraDeclarations(KDOM::ElementImpl *e, unsigned int numProps)
{
    SVGStyledElementImpl *se = static_cast<SVGStyledElementImpl *>(e);
    if(!se)
        return numProps;

    KDOM::CSSStyleDeclarationImpl *decl = se->pa();
    if(!decl)
        return numProps;

    Q3PtrList<KDOM::CSSProperty>* values = decl ? decl->values() : 0;
    if(!values)
        return numProps;

    int totalLen = values ? values->count() : 0;

    if(presentationAttrs.size() <(uint)totalLen)
        presentationAttrs.resize(totalLen + 1);

    if(numProps + totalLen >= propsToApplySize)
    {
        propsToApplySize += propsToApplySize;
        propsToApply = (KDOM::CSSOrderedProperty **) realloc(propsToApply, propsToApplySize * sizeof(KDOM::CSSOrderedProperty *));
    }

    KDOM::CSSOrderedProperty *array = (KDOM::CSSOrderedProperty *) presentationAttrs.data();
    for(int i = 0; i < totalLen; i++)
    {
        KDOM::CSSProperty *prop = values->at(i);
        KDOM::Source source = KDOM::Author;

        if(prop->m_nonCSSHint)
            source = KDOM::NonCSSHint;

        bool first = (decl->interface() ? decl->interface()->cssPropertyApplyFirst(prop->m_id) : false);
        array->prop = prop;
        array->pseudoId = KDOM::RenderStyle::NOPSEUDO;
        array->selector = 0;
        array->position = i;
        array->priority =(!first << 30) |(source << 24);
        propsToApply[numProps++] = array++;
    }

    return numProps;
}

void SVGCSSStyleSelector::applyRule(int id, KDOM::CSSValueImpl *value)
{
    if(id < SVGCSS_PROP_MIN && id != CSS_PROP_COLOR)
    {
        KDOM::CSSStyleSelector::applyRule(id, value);
        return;
    }

    KDOM::CSSPrimitiveValueImpl *primitiveValue = 0;
    if(value->isPrimitiveValue())
        primitiveValue = static_cast<KDOM::CSSPrimitiveValueImpl *>(value);

    KDOM::Length l;

    bool isInherit = (parentNode && value->cssValueType() == KDOM::CSS_INHERIT);
    bool isInitial = (value->cssValueType() == KDOM::CSS_INITIAL) ||
                      (!parentNode && value->cssValueType() == KDOM::CSS_INHERIT);

    SVGRenderStyle *svgstyle = static_cast<SVGRenderStyle *>(style);

    // What follows is a list that maps the CSS properties into their
    // corresponding front-end RenderStyle values. Shorthands(e.g. border,
    // background) occur in this list as well and are only hit when mapping
    // "inherit" or "initial" into front-end values.
    switch(id)
    {
        // ident only properties
        case SVGCSS_PROP_ALIGNMENT_BASELINE:
        {
            HANDLE_INHERIT_AND_INITIAL(alignmentBaseline, AlignmentBaseline)
            if(!primitiveValue)
                break;

            switch(primitiveValue->getIdent())
            {
                case CSS_VAL_AUTO:
                    svgstyle->setAlignmentBaseline(AB_AUTO);
                    break;
                case SVGCSS_VAL_BASELINE:
                    svgstyle->setAlignmentBaseline(AB_BASELINE);
                    break;
                case SVGCSS_VAL_BEFORE_EDGE:
                    svgstyle->setAlignmentBaseline(AB_BEFORE_EDGE);
                    break;
                case SVGCSS_VAL_TEXT_BEFORE_EDGE:
                    svgstyle->setAlignmentBaseline(AB_TEXT_BEFORE_EDGE);
                    break;
                case CSS_VAL_MIDDLE:
                    svgstyle->setAlignmentBaseline(AB_MIDDLE);
                    break;
                case SVGCSS_VAL_CENTRAL:
                    svgstyle->setAlignmentBaseline(AB_CENTRAL);
                    break;
                case SVGCSS_VAL_AFTER_EDGE:
                    svgstyle->setAlignmentBaseline(AB_AFTER_EDGE);
                    break;
                case SVGCSS_VAL_TEXT_AFTER_EDGE:
                    svgstyle->setAlignmentBaseline(AB_TEXT_AFTER_EDGE);
                    break;
                case SVGCSS_VAL_IDEOGRAPHIC:
                    svgstyle->setAlignmentBaseline(AB_IDEOGRAPHIC);
                    break;
                case SVGCSS_VAL_ALPHABETIC:
                    svgstyle->setAlignmentBaseline(AB_ALPHABETIC);
                    break;
                case SVGCSS_VAL_HANGING:
                    svgstyle->setAlignmentBaseline(AB_HANGING);
                    break;
                case SVGCSS_VAL_MATHEMATICAL:
                    svgstyle->setAlignmentBaseline(AB_MATHEMATICAL);
                    break;
                default:
                    return;
            }

            break;
        }
        case SVGCSS_PROP_POINTER_EVENTS:
        {
            HANDLE_INHERIT_AND_INITIAL(pointerEvents, PointerEvents)
            if(!primitiveValue)
                break;
                
            switch(primitiveValue->getIdent())
            {
                case SVGCSS_VAL_ALL:
                    svgstyle->setPointerEvents(PE_ALL);
                    break;
                case CSS_VAL_NONE:
                    svgstyle->setPointerEvents(PE_NONE);
                    break;
                case SVGCSS_VAL_VISIBLEPAINTED:
                    svgstyle->setPointerEvents(PE_VISIBLE_PAINTED);
                    break;
                case SVGCSS_VAL_VISIBLEFILL:
                    svgstyle->setPointerEvents(PE_VISIBLE_FILL);
                    break;
                case SVGCSS_VAL_VISIBLESTROKE:
                    svgstyle->setPointerEvents(PE_VISIBLE_STROKE);
                    break;
                case CSS_VAL_VISIBLE:
                    svgstyle->setPointerEvents(PE_VISIBLE);
                    break;
                case SVGCSS_VAL_PAINTED:
                    svgstyle->setPointerEvents(PE_PAINTED);
                    break;
                case SVGCSS_VAL_FILL:
                    svgstyle->setPointerEvents(PE_FILL);
                    break;
                case SVGCSS_VAL_STROKE:
                    svgstyle->setPointerEvents(PE_STROKE);
                default:
                    return;
            }

            break;
        }
        case SVGCSS_PROP_DOMINANT_BASELINE:
        {
            HANDLE_INHERIT_AND_INITIAL(dominantBaseline, DominantBaseline)
            if(!primitiveValue)
                break;
    
            switch(primitiveValue->getIdent())
            {
                case CSS_VAL_AUTO:
                    svgstyle->setDominantBaseline(DB_AUTO);
                    break;
                case SVGCSS_VAL_USE_SCRIPT:
                    svgstyle->setDominantBaseline(DB_USE_SCRIPT);
                    break;
                case SVGCSS_VAL_NO_CHANGE:
                    svgstyle->setDominantBaseline(DB_NO_CHANGE);
                    break;
                case SVGCSS_VAL_RESET_SIZE:
                    svgstyle->setDominantBaseline(DB_RESET_SIZE);
                    break;
                case SVGCSS_VAL_IDEOGRAPHIC:
                    svgstyle->setDominantBaseline(DB_IDEOGRAPHIC);
                    break;
                case SVGCSS_VAL_ALPHABETIC:
                    svgstyle->setDominantBaseline(DB_ALPHABETIC);
                    break;
                case SVGCSS_VAL_HANGING:
                    svgstyle->setDominantBaseline(DB_HANGING);
                    break;
                case SVGCSS_VAL_MATHEMATICAL:
                    svgstyle->setDominantBaseline(DB_MATHEMATICAL);
                    break;
                case SVGCSS_VAL_CENTRAL:
                    svgstyle->setDominantBaseline(DB_CENTRAL);
                    break;
                case CSS_VAL_MIDDLE:
                    svgstyle->setDominantBaseline(DB_MIDDLE);
                    break;
                case SVGCSS_VAL_TEXT_AFTER_EDGE:
                    svgstyle->setDominantBaseline(DB_TEXT_AFTER_EDGE);
                    break;
                case SVGCSS_VAL_TEXT_BEFORE_EDGE:
                    svgstyle->setDominantBaseline(DB_TEXT_BEFORE_EDGE);
                    break;
                default:
                    return;
            }
    
            break;
        }
        case SVGCSS_PROP_COLOR_INTERPOLATION:
        {
            HANDLE_INHERIT_AND_INITIAL(colorInterpolation, ColorInterpolation)
            if(!primitiveValue)
                return;
                
            switch(primitiveValue->getIdent())
            {
                case SVGCSS_VAL_SRGB:
                    svgstyle->setColorInterpolation(CI_SRGB);
                    break;
                case SVGCSS_VAL_LINEARRGB:
                    svgstyle->setColorInterpolation(CI_LINEARRGB);
                    break;
                case CSS_VAL_AUTO:
                    svgstyle->setColorInterpolation(CI_AUTO);
                default:
                    return;
            }
    
            break;
        }
        case SVGCSS_PROP_COLOR_INTERPOLATION_FILTERS:
        {
            HANDLE_INHERIT_AND_INITIAL(colorInterpolationFilters, ColorInterpolationFilters)
            if(!primitiveValue)
                return;
                
            switch(primitiveValue->getIdent())
            {
                case SVGCSS_VAL_SRGB:
                    svgstyle->setColorInterpolationFilters(CI_SRGB);
                    break;
                case SVGCSS_VAL_LINEARRGB:
                    svgstyle->setColorInterpolationFilters(CI_LINEARRGB);
                    break;
                case CSS_VAL_AUTO:
                    svgstyle->setColorInterpolationFilters(CI_AUTO);
                default:
                    return;
            }
            
            break;
        }
        case SVGCSS_PROP_CLIP_RULE:
        {
            HANDLE_INHERIT_AND_INITIAL(clipRule, ClipRule)
            if(!primitiveValue)
                break;
                
            switch(primitiveValue->getIdent())
            {
                case SVGCSS_VAL_NONZERO:
                    svgstyle->setClipRule(WR_NONZERO);
                    break;
                case SVGCSS_VAL_EVENODD:
                    svgstyle->setClipRule(WR_EVENODD);
                    break;
                default:
                    break;
            }
        
            break;
        }
        case SVGCSS_PROP_FILL_RULE:
        {
            HANDLE_INHERIT_AND_INITIAL(fillRule, FillRule)
            if(!primitiveValue)
                break;
            
            switch(primitiveValue->getIdent())
            {
                case SVGCSS_VAL_NONZERO:
                    svgstyle->setFillRule(WR_NONZERO);
                    break;
                case SVGCSS_VAL_EVENODD:
                    svgstyle->setFillRule(WR_EVENODD);
                default:
                    return;
            }
        
            break;
        }
        case SVGCSS_PROP_STROKE_LINEJOIN:
        {
            HANDLE_INHERIT_AND_INITIAL(joinStyle, JoinStyle)

            if(!primitiveValue)
                break;
                
            switch(primitiveValue->getIdent())
            {
                case SVGCSS_VAL_MITER:
                    svgstyle->setJoinStyle(JS_MITER);
                    break;
                case SVGCSS_VAL_ROUND:
                    svgstyle->setJoinStyle(JS_ROUND);
                    break;
                case SVGCSS_VAL_BEVEL:
                    svgstyle->setJoinStyle(JS_BEVEL);
                default:
                    return;
            }
            
            break;
        }
        case SVGCSS_PROP_IMAGE_RENDERING:
        {
            HANDLE_INHERIT_AND_INITIAL(imageRendering, ImageRendering)
            if(!primitiveValue)
                return;
            
            switch(primitiveValue->getIdent())
            {
                case SVGCSS_VAL_OPTIMIZESPEED:
                    svgstyle->setImageRendering(IR_OPTIMIZESPEED);
                default:
                    return;
            }
    
            break;
        }
        // end of ident only properties
        case SVGCSS_PROP_FILL:
        {
            HANDLE_INHERIT_AND_INITIAL(fillPaint, FillPaint)
            if(!primitiveValue && value)
            {
                SVGPaintImpl *paint = static_cast<SVGPaintImpl *>(value);
                if(paint)
                    svgstyle->setFillPaint(paint);
            }
            
            break;
        }
        case SVGCSS_PROP_STROKE:
        {
            HANDLE_INHERIT_AND_INITIAL(strokePaint, StrokePaint)
            if(!primitiveValue && value)
            {
                SVGPaintImpl *paint = static_cast<SVGPaintImpl *>(value);
                if(paint)
                    svgstyle->setStrokePaint(paint);
            }
            
            break;
        }
        case SVGCSS_PROP_STROKE_WIDTH:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeWidth, StrokeWidth)
            if(!primitiveValue)
                return;
        
            svgstyle->setStrokeWidth(primitiveValue);
            break;
        }
        case SVGCSS_PROP_STROKE_DASHARRAY:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeDashArray, StrokeDashArray)
            if(!primitiveValue && value)
            {
                KDOM::CSSValueListImpl *dashes = static_cast<KDOM::CSSValueListImpl *>(value);
                if(dashes)
                    svgstyle->setStrokeDashArray(dashes);
            }
        
            break;
        }
        case SVGCSS_PROP_STROKE_DASHOFFSET:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeDashOffset, StrokeDashOffset)
            if(!primitiveValue)
                return;

            svgstyle->setStrokeDashOffset(primitiveValue);
            break;
        }
        case SVGCSS_PROP_OPACITY:
        {
            HANDLE_INHERIT_AND_INITIAL(opacity, Opacity)
            if(!primitiveValue)
                return;
        
            float f = 0.0;    
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue(KDOM::CSS_PERCENTAGE) / 100.;
            else if(type == KDOM::CSS_NUMBER)
                f = primitiveValue->getFloatValue(KDOM::CSS_NUMBER);
            else
                return;

            svgstyle->setOpacity(f);
            break;
        }
        case SVGCSS_PROP_FILL_OPACITY:
        {
            HANDLE_INHERIT_AND_INITIAL(fillOpacity, FillOpacity)
            if(!primitiveValue)
                return;
        
            float f = 0.0;    
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue(KDOM::CSS_PERCENTAGE) / 100.;
            else if(type == KDOM::CSS_NUMBER)
                f = primitiveValue->getFloatValue(KDOM::CSS_NUMBER);
            else
                return;

            svgstyle->setFillOpacity(f);
            break;
        }
        case SVGCSS_PROP_STROKE_OPACITY:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeOpacity, StrokeOpacity)
            if(!primitiveValue)
                return;
        
            float f = 0.0;    
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue(KDOM::CSS_PERCENTAGE) / 100.;
            else if(type == KDOM::CSS_NUMBER)
                f = primitiveValue->getFloatValue(KDOM::CSS_NUMBER);
            else
                return;

            svgstyle->setStrokeOpacity(f);
            break;
        }
        case SVGCSS_PROP_STOP_OPACITY:
        {
            HANDLE_INHERIT_AND_INITIAL(stopOpacity, StopOpacity)
            if(!primitiveValue)
                return;
        
            float f = 0.0;    
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue(KDOM::CSS_PERCENTAGE) / 100.;
            else if(type == KDOM::CSS_NUMBER)
                f = primitiveValue->getFloatValue(KDOM::CSS_NUMBER);
            else
                return;

            svgstyle->setStopOpacity(f);
            break;
        }
        case SVGCSS_PROP_MARKER_START:
        {
            HANDLE_INHERIT_AND_INITIAL(startMarker, StartMarker)
            if(!primitiveValue)
                return;

            QString s;
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSS_URI)
                s = KDOM::DOMString(primitiveValue->getDOMStringValue()).string();
            else
                return;

            svgstyle->setStartMarker(s);
            break;
        }
        case SVGCSS_PROP_MARKER_MID:
        {
            HANDLE_INHERIT_AND_INITIAL(midMarker, MidMarker)
            if(!primitiveValue)
                return;

            QString s;
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSS_URI)
                s = KDOM::DOMString(primitiveValue->getDOMStringValue()).string();
            else
                return;

            svgstyle->setMidMarker(s);
            break;
        }
        case SVGCSS_PROP_MARKER_END:
        {
            HANDLE_INHERIT_AND_INITIAL(endMarker, EndMarker)
            if(!primitiveValue)
                return;

            QString s;
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSS_URI)
                s = KDOM::DOMString(primitiveValue->getDOMStringValue()).string();
            else
                return;

            svgstyle->setEndMarker(s);
            break;
        }
        case SVGCSS_PROP_STROKE_LINECAP:
        {
            HANDLE_INHERIT_AND_INITIAL(capStyle, CapStyle)
            if(primitiveValue)
                svgstyle->setCapStyle((ECapStyle)(primitiveValue->getIdent() - SVGCSS_VAL_GEOMETRICPRECISION));

            break;
        }
        case SVGCSS_PROP_STROKE_MITERLIMIT:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeMiterLimit, StrokeMiterLimit)
            if(!primitiveValue)
                return;

            float f = 0.0;
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSS_NUMBER)
                f = primitiveValue->getFloatValue(KDOM::CSS_NUMBER);
            else
                return;

            svgstyle->setStrokeMiterLimit(qRound(f));
            break;
        }
        case SVGCSS_PROP_FILTER:
        {
            HANDLE_INHERIT_AND_INITIAL(filter, Filter)
            if(!primitiveValue)
                return;

            QString s;
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSS_URI)
                s = KDOM::DOMString(primitiveValue->getDOMStringValue()).string();
            else
                return;
            svgstyle->setFilter(s);
            break;
        }
        case SVGCSS_PROP_CLIP_PATH:
        {
            HANDLE_INHERIT_AND_INITIAL(clipPath, ClipPath)
            if(!primitiveValue)
                return;

            QString s;
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSS_URI)
                s = KDOM::DOMString(primitiveValue->getDOMStringValue()).string();
            else
                return;

            svgstyle->setClipPath(s);
            break;
        }
        case SVGCSS_PROP_TEXT_ANCHOR:
        {
            HANDLE_INHERIT_AND_INITIAL(textAnchor, TextAnchor)
            if(primitiveValue)
                svgstyle->setTextAnchor((ETextAnchor)(primitiveValue->getIdent() - SVGCSS_VAL_RESET_SIZE));
                
            break;
        }
        case CSS_PROP_COLOR: // colors || inherit
        {
            QColor col;
            if(isInherit)
            {
                HANDLE_INHERIT_COND(CSS_PROP_COLOR, color, Color)
                return;
            }
            else if(isInitial)
                col = KDOM::RenderStyle::initialColor();
            else
            {
                SVGColorImpl *c = static_cast<SVGColorImpl *>(value);
                if(!c)
                    return KDOM::CSSStyleSelector::applyRule(id, value);
                
                col = c->color();
            }
        
            svgstyle->setColor(col);
            break;
        }
        case SVGCSS_PROP_STOP_COLOR:
        {
            QColor col;
            if(isInherit)
            {
                HANDLE_INHERIT_COND(SVGCSS_PROP_STOP_COLOR, color, Color)
                return;
            }
            else if(isInitial)
                col = SVGRenderStyle::initialStopColor();
            else
            {
                SVGColorImpl *c = static_cast<SVGColorImpl *>(value);
                if(!c)
                    return KDOM::CSSStyleSelector::applyRule(id, value);

                if(c->colorType() == SVG_COLORTYPE_CURRENTCOLOR)
                    col = svgstyle->color();
                else
                    col = c->color();
            }

            svgstyle->setStopColor(col);
            break;
        }
        case SVGCSS_PROP_FLOOD_OPACITY:
        {
            HANDLE_INHERIT_AND_INITIAL(floodOpacity, FloodOpacity)
            if(!primitiveValue)
                return;

            float f = 0.0;    
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue(KDOM::CSS_PERCENTAGE) / 100.;
            else if(type == KDOM::CSS_NUMBER)
                f = primitiveValue->getFloatValue(KDOM::CSS_NUMBER);
            else
                return;

            svgstyle->setFloodOpacity(f);
            break;
        }
        case SVGCSS_PROP_FLOOD_COLOR:
        {
            QColor col;
            if(isInitial)
                col = SVGRenderStyle::initialStopColor();
            else
            {
                SVGColorImpl *c = static_cast<SVGColorImpl *>(value);
                if(!c)
                    return KDOM::CSSStyleSelector::applyRule(id, value);

                if(c->colorType() == SVG_COLORTYPE_CURRENTCOLOR)
                    col = svgstyle->color();
                else
                    col = c->color();
            }

            svgstyle->setFloodColor(col);
            break;
        }
        default:
        {
            kdDebug() << "RULE UNIMPLEMENTED! FIX ME :) id=" << id << endl;
            return;
        }
    }
}

// vim:ts=4:noet
