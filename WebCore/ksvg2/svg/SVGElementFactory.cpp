/*
 * This file is part of the SVG DOM implementation for KDE.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include "SVGElementFactory.h"

#include "SVGAElementImpl.h"
#include "SVGAnimateElementImpl.h"
#include "SVGAnimateColorElementImpl.h"
#include "SVGAnimateTransformElementImpl.h"
#include "SVGCircleElementImpl.h"
#include "SVGClipPathElementImpl.h"
#include "SVGCursorElementImpl.h"
#include "SVGDefsElementImpl.h"
#include "SVGDescElementImpl.h"
#include "SVGEllipseElementImpl.h"
#include "SVGFEBlendElementImpl.h"
#include "SVGFEComponentTransferElementImpl.h"
#include "SVGFECompositeElementImpl.h"
#include "SVGFEFloodElementImpl.h"
#include "SVGFEFuncAElementImpl.h"
#include "SVGFEFuncBElementImpl.h"
#include "SVGFEFuncGElementImpl.h"
#include "SVGFEFuncRElementImpl.h"
#include "SVGFEGaussianBlurElementImpl.h"
#include "SVGFEImageElementImpl.h"
#include "SVGFEMergeElementImpl.h"
#include "SVGFEMergeNodeElementImpl.h"
#include "SVGFEOffsetElementImpl.h"
#include "SVGFETileElementImpl.h"
#include "SVGFETurbulenceElementImpl.h"
#include "SVGFilterElementImpl.h"
#include "SVGGElementImpl.h"
#include "SVGImageElementImpl.h"
#include "SVGLineElementImpl.h"
#include "SVGLinearGradientElementImpl.h"
#include "SVGMarkerElementImpl.h"
#include "SVGPathElementImpl.h"
#include "SVGPatternElementImpl.h"
#include "SVGPolygonElementImpl.h"
#include "SVGPolylineElementImpl.h"
#include "SVGRadialGradientElementImpl.h"
#include "SVGRectElementImpl.h"
#include "SVGScriptElementImpl.h"
#include "SVGSetElementImpl.h"
#include "SVGStopElementImpl.h"
#include "SVGStyleElementImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGSwitchElementImpl.h"
#include "SVGSymbolElementImpl.h"
#include "SVGTextElementImpl.h"
#include "SVGTitleElementImpl.h"
#include "SVGTSpanElementImpl.h"
#include "SVGUseElementImpl.h"
#include "SVGViewElementImpl.h"


#include <kxmlcore/HashMap.h>

using namespace KDOM;
using namespace KSVG::SVGNames;

typedef KXMLCore::HashMap<DOMStringImpl *, void *, KXMLCore::PointerHash<DOMStringImpl *> > FunctionMap;
static FunctionMap *gFunctionMap = 0;

namespace KSVG
{

typedef SVGElementImpl *(*ConstructorFunc)(DocumentPtr *docPtr, bool createdByParser);

SVGElementImpl *aConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGAElementImpl(aTag, docPtr);
}

SVGElementImpl *animateConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGAnimateElementImpl(animateTag, docPtr);
}

SVGElementImpl *animatecolorConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGAnimateColorElementImpl(animatecolorTag, docPtr);
}

SVGElementImpl *animatetransformConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGAnimateTransformElementImpl(animatetransformTag, docPtr);
}

SVGElementImpl *circleConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGCircleElementImpl(circleTag, docPtr);
}

SVGElementImpl *clippathConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGClipPathElementImpl(clippathTag, docPtr);
}

SVGElementImpl *cursorConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGCursorElementImpl(cursorTag, docPtr);
}

SVGElementImpl *defsConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGDefsElementImpl(defsTag, docPtr);
}

SVGElementImpl *descConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGDescElementImpl(descTag, docPtr);
}

SVGElementImpl *ellipseConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGEllipseElementImpl(ellipseTag, docPtr);
}

SVGElementImpl *feblendConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFEBlendElementImpl(feblendTag, docPtr);
}

SVGElementImpl *fecomponenttransferConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFEComponentTransferElementImpl(fecomponenttransferTag, docPtr);
}

SVGElementImpl *fecompositeConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFECompositeElementImpl(fecompositeTag, docPtr);
}

SVGElementImpl *fefloodConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFEFloodElementImpl(fefloodTag, docPtr);
}

SVGElementImpl *fefuncaConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFEFuncAElementImpl(fefuncaTag, docPtr);
}

SVGElementImpl *fefuncbConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFEFuncBElementImpl(fefuncbTag, docPtr);
}

SVGElementImpl *fefuncgConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFEFuncGElementImpl(fefuncgTag, docPtr);
}

SVGElementImpl *fefuncrConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFEFuncRElementImpl(fefuncrTag, docPtr);
}

SVGElementImpl *fegaussianblurConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFEGaussianBlurElementImpl(fegaussianblurTag, docPtr);
}

SVGElementImpl *feimageConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFEImageElementImpl(feimageTag, docPtr);
}

SVGElementImpl *femergeConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFEMergeElementImpl(femergeTag, docPtr);
}

SVGElementImpl *femergenodeConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFEMergeNodeElementImpl(femergenodeTag, docPtr);
}

SVGElementImpl *feoffsetConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFEOffsetElementImpl(feoffsetTag, docPtr);
}

SVGElementImpl *fetileConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFETileElementImpl(fetileTag, docPtr);
}

SVGElementImpl *feturbulenceConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFETurbulenceElementImpl(feturbulenceTag, docPtr);
}

SVGElementImpl *filterConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGFilterElementImpl(filterTag, docPtr);
}

SVGElementImpl *gConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGGElementImpl(gTag, docPtr);
}

SVGElementImpl *imageConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGImageElementImpl(imageTag, docPtr);
}

SVGElementImpl *lineConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGLineElementImpl(lineTag, docPtr);
}

SVGElementImpl *lineargradientConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGLinearGradientElementImpl(lineargradientTag, docPtr);
}

SVGElementImpl *markerConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGMarkerElementImpl(markerTag, docPtr);
}

SVGElementImpl *pathConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGPathElementImpl(pathTag, docPtr);
}

SVGElementImpl *patternConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGPatternElementImpl(patternTag, docPtr);
}

SVGElementImpl *polygonConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGPolygonElementImpl(polygonTag, docPtr);
}

SVGElementImpl *polylineConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGPolylineElementImpl(polylineTag, docPtr);
}

SVGElementImpl *radialgradientConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGRadialGradientElementImpl(radialgradientTag, docPtr);
}

SVGElementImpl *rectConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGRectElementImpl(rectTag, docPtr);
}

SVGElementImpl *scriptConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGScriptElementImpl(scriptTag, docPtr);
}

SVGElementImpl *setConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGSetElementImpl(setTag, docPtr);
}

SVGElementImpl *stopConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGStopElementImpl(stopTag, docPtr);
}

SVGElementImpl *styleConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGStyleElementImpl(styleTag, docPtr);
}

SVGElementImpl *svgConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGSVGElementImpl(svgTag, docPtr);
}

SVGElementImpl *switchConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGSwitchElementImpl(switchTag, docPtr);
}

SVGElementImpl *symbolConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGSymbolElementImpl(symbolTag, docPtr);
}

SVGElementImpl *textConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGTextElementImpl(textTag, docPtr);
}

SVGElementImpl *titleConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGTitleElementImpl(titleTag, docPtr);
}

SVGElementImpl *tspanConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGTSpanElementImpl(tspanTag, docPtr);
}

SVGElementImpl *useConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGUseElementImpl(useTag, docPtr);
}

SVGElementImpl *viewConstructor(DocumentPtr *docPtr, bool createdByParser)
{
    return new SVGViewElementImpl(viewTag, docPtr);
}


static inline void createFunctionMapIfNecessary()
{
    if (gFunctionMap)
        return;
    // Create the table.
    gFunctionMap = new FunctionMap;
    
    // Populate it with constructor functions.
    gFunctionMap->set(aTag.localName().impl(), (void*)&aConstructor);
    gFunctionMap->set(animateTag.localName().impl(), (void*)&animateConstructor);
    gFunctionMap->set(animatecolorTag.localName().impl(), (void*)&animatecolorConstructor);
    gFunctionMap->set(animatetransformTag.localName().impl(), (void*)&animatetransformConstructor);
    gFunctionMap->set(circleTag.localName().impl(), (void*)&circleConstructor);
    gFunctionMap->set(clippathTag.localName().impl(), (void*)&clippathConstructor);
    gFunctionMap->set(cursorTag.localName().impl(), (void*)&cursorConstructor);
    gFunctionMap->set(defsTag.localName().impl(), (void*)&defsConstructor);
    gFunctionMap->set(descTag.localName().impl(), (void*)&descConstructor);
    gFunctionMap->set(ellipseTag.localName().impl(), (void*)&ellipseConstructor);
    gFunctionMap->set(feblendTag.localName().impl(), (void*)&feblendConstructor);
    gFunctionMap->set(fecomponenttransferTag.localName().impl(), (void*)&fecomponenttransferConstructor);
    gFunctionMap->set(fecompositeTag.localName().impl(), (void*)&fecompositeConstructor);
    gFunctionMap->set(fefloodTag.localName().impl(), (void*)&fefloodConstructor);
    gFunctionMap->set(fefuncaTag.localName().impl(), (void*)&fefuncaConstructor);
    gFunctionMap->set(fefuncbTag.localName().impl(), (void*)&fefuncbConstructor);
    gFunctionMap->set(fefuncgTag.localName().impl(), (void*)&fefuncgConstructor);
    gFunctionMap->set(fefuncrTag.localName().impl(), (void*)&fefuncrConstructor);
    gFunctionMap->set(fegaussianblurTag.localName().impl(), (void*)&fegaussianblurConstructor);
    gFunctionMap->set(feimageTag.localName().impl(), (void*)&feimageConstructor);
    gFunctionMap->set(femergeTag.localName().impl(), (void*)&femergeConstructor);
    gFunctionMap->set(femergenodeTag.localName().impl(), (void*)&femergenodeConstructor);
    gFunctionMap->set(feoffsetTag.localName().impl(), (void*)&feoffsetConstructor);
    gFunctionMap->set(fetileTag.localName().impl(), (void*)&fetileConstructor);
    gFunctionMap->set(feturbulenceTag.localName().impl(), (void*)&feturbulenceConstructor);
    gFunctionMap->set(filterTag.localName().impl(), (void*)&filterConstructor);
    gFunctionMap->set(gTag.localName().impl(), (void*)&gConstructor);
    gFunctionMap->set(imageTag.localName().impl(), (void*)&imageConstructor);
    gFunctionMap->set(lineTag.localName().impl(), (void*)&lineConstructor);
    gFunctionMap->set(lineargradientTag.localName().impl(), (void*)&lineargradientConstructor);
    gFunctionMap->set(markerTag.localName().impl(), (void*)&markerConstructor);
    gFunctionMap->set(pathTag.localName().impl(), (void*)&pathConstructor);
    gFunctionMap->set(patternTag.localName().impl(), (void*)&patternConstructor);
    gFunctionMap->set(polygonTag.localName().impl(), (void*)&polygonConstructor);
    gFunctionMap->set(polylineTag.localName().impl(), (void*)&polylineConstructor);
    gFunctionMap->set(radialgradientTag.localName().impl(), (void*)&radialgradientConstructor);
    gFunctionMap->set(rectTag.localName().impl(), (void*)&rectConstructor);
    gFunctionMap->set(scriptTag.localName().impl(), (void*)&scriptConstructor);
    gFunctionMap->set(setTag.localName().impl(), (void*)&setConstructor);
    gFunctionMap->set(stopTag.localName().impl(), (void*)&stopConstructor);
    gFunctionMap->set(styleTag.localName().impl(), (void*)&styleConstructor);
    gFunctionMap->set(svgTag.localName().impl(), (void*)&svgConstructor);
    gFunctionMap->set(switchTag.localName().impl(), (void*)&switchConstructor);
    gFunctionMap->set(symbolTag.localName().impl(), (void*)&symbolConstructor);
    gFunctionMap->set(textTag.localName().impl(), (void*)&textConstructor);
    gFunctionMap->set(titleTag.localName().impl(), (void*)&titleConstructor);
    gFunctionMap->set(tspanTag.localName().impl(), (void*)&tspanConstructor);
    gFunctionMap->set(useTag.localName().impl(), (void*)&useConstructor);
    gFunctionMap->set(viewTag.localName().impl(), (void*)&viewConstructor);
}

SVGElementImpl *SVGElementFactory::createSVGElement(const QualifiedName& qName, DocumentImpl* doc, bool createdByParser)
{
    if (!doc)
        return 0; // Don't allow elements to ever be made without having a doc.

    DocumentPtr *docPtr = doc->docPtr();
    createFunctionMapIfNecessary();
    void* result = gFunctionMap->get(qName.localName().impl());
    if (result) {
        ConstructorFunc func = (ConstructorFunc)result;
        return (func)(docPtr, createdByParser);
    }
    
    return 0;
}

}; // namespace

