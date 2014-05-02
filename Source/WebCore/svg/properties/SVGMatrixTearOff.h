/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGMatrixTearOff_h
#define SVGMatrixTearOff_h

#include "SVGPropertyTearOff.h"
#include "SVGTransform.h"

namespace WebCore {

class SVGMatrixTearOff : public SVGPropertyTearOff<SVGMatrix> {
public:
    // Used for non-animated POD types that are not associated with a SVGAnimatedProperty object, nor with a XML DOM attribute
    // and that contain a parent type that's exposed to the bindings via a SVGStaticPropertyTearOff object
    // (for example: SVGTransform::matrix).
    static PassRefPtr<SVGMatrixTearOff> create(SVGPropertyTearOff<SVGTransform>* parent, SVGMatrix& value)
    {
        RefPtr<SVGMatrixTearOff> result = adoptRef(new SVGMatrixTearOff(parent, value));
        parent->addChild(result->m_weakFactory.createWeakPtr());
        return result.release();
    }

    virtual void commitChange()
    {
        m_parent->propertyReference().updateMatrix();
        m_parent->commitChange();
    }

private:
    SVGMatrixTearOff(SVGPropertyTearOff<SVGTransform>* parent, SVGMatrix& value)
        : SVGPropertyTearOff<SVGMatrix>(0, UndefinedRole, value)
        , m_parent(parent)
        , m_weakFactory(this)
    {
    }

    RefPtr<SVGPropertyTearOff<SVGTransform>> m_parent;
    WeakPtrFactory<SVGPropertyTearOffBase> m_weakFactory;
};

}

#endif // SVGMatrixTearOff_h
