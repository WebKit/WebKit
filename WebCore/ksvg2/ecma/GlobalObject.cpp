/*
    Copyright (C) 2002 David Faure <faure@kde.org>
                  2004, 2005 Nikolas Zimmermann <wildfox@kde.org>

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

#include <kdom/impl/NodeImpl.h>
#include <kdom/impl/DocumentImpl.h>

#include "GlobalObject.h"
#include "Constructors.h"
#include "SVGDocumentImpl.h"

#include <ksvg2/data/EcmaConstants.h>
#include <ksvg2/data/GlobalObject.lut.h>
using namespace KSVG;

/*
@begin GlobalObject::s_hashTable 13
# Constructors
 SVGException    GlobalObjectConstants::SVGException DontDelete|Function 1
 SVGLength        GlobalObjectConstants::SVGLength DontDelete|Function 1
 SVGAngle        GlobalObjectConstants::SVGAngle DontDelete|Function 1
 SVGColor        GlobalObjectConstants::SVGColor DontDelete|Function 1
 SVGUnitTypes    GlobalObjectConstants::SVGUnitTypes DontDelete|Function 1
 SVGTransform    GlobalObjectConstants::SVGTransform DontDelete|Function 1
 SVGPaint        GlobalObjectConstants::SVGPaint DontDelete|Function 1
 SVGGradientElement        GlobalObjectConstants::SVGGradientElement DontDelete|Function 1
 SVGPreserveAspectRatio        GlobalObjectConstants::SVGPreserveAspectRatio DontDelete|Function 1
 SVGZoomAndPan        GlobalObjectConstants::SVGZoomAndPan DontDelete|Function 1
 SVGMarkerElement    GlobalObjectConstants::SVGMarkerElement DontDelete|Function 1
@end
*/

GlobalObject::GlobalObject(KDOM::DocumentImpl *doc) : KDOM::GlobalObject(doc)
{
}

GlobalObject::~GlobalObject()
{
}

void GlobalObject::afterTimeout() const
{
    if(doc())
        doc()->updateRendering();
}

KJS::ValueImp *GlobalObject::get(KJS::ExecState *exec, const KJS::Identifier &p) const
{
    kdDebug(26004) << "KSVG::GlobalObject (" << this << ")::get " << p.qstring() << endl;

    KJS::ValueImp *ret = KDOM::GlobalObject::get(exec, p);
    if(ret->type() != KJS::UndefinedType)
        return ret;

    const KJS::HashEntry *entry = KJS::Lookup::findEntry(&GlobalObject::s_hashTable, p);
    if(entry)
    {
        switch(entry->value)
        {
            case GlobalObjectConstants::SVGException:
                return getSVGExceptionConstructor(exec);
            case GlobalObjectConstants::SVGLength:
                return getSVGLengthConstructor(exec);
            case GlobalObjectConstants::SVGAngle:
                return getSVGAngleConstructor(exec);
            case GlobalObjectConstants::SVGColor:
                return getSVGColorConstructor(exec);
            case GlobalObjectConstants::SVGPaint:
                return getSVGPaintConstructor(exec);
            case GlobalObjectConstants::SVGUnitTypes:
                return getSVGUnitTypesConstructor(exec);
            case GlobalObjectConstants::SVGTransform:
                return getSVGTransformConstructor(exec);
            case GlobalObjectConstants::SVGGradientElement:
                return getSVGGradientElementConstructor(exec);
            case GlobalObjectConstants::SVGPreserveAspectRatio:
                return getSVGPreserveAspectRatioConstructor(exec);
            case GlobalObjectConstants::SVGZoomAndPan:
                return getSVGZoomAndPanConstructor(exec);
            case GlobalObjectConstants::SVGMarkerElement:
                return getSVGMarkerElementConstructor(exec);
        }
    }

    // This isn't necessarily a bug. Some code uses if(!window.blah) window.blah=1
    // But it can also mean something isn't loaded or implemented...
    kdDebug(26004) << "GlobalObject::get property not found: " << p.qstring() << endl;
    return KJS::Undefined();
}

// vim:ts=4:noet
