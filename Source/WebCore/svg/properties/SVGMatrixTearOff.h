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

#pragma once

#include "SVGMatrix.h"
#include "SVGTransform.h"

namespace WebCore {

class SVGMatrixTearOff final : public SVGMatrix {
public:
    static Ref<SVGMatrixTearOff> create(SVGTransform& parent, SVGMatrixValue& value)
    {
        ASSERT_UNUSED(value, &parent.propertyReference().svgMatrix() == &value);
        Ref<SVGMatrixTearOff> result = adoptRef(*new SVGMatrixTearOff(parent));
        parent.addChild(result->m_weakFactory.createWeakPtr(result));
        return result;
    }

    SVGMatrixValue& propertyReference() final { return m_parent->propertyReference().svgMatrix(); }

    void setValue(SVGMatrixValue& value) final { m_parent->propertyReference().setMatrix(value); }

    void commitChange() final
    {
        m_parent->propertyReference().updateSVGMatrix();
        m_parent->commitChange();
    }

private:
    SVGMatrixTearOff(SVGTransform& parent)
        : SVGMatrix(nullptr)
        , m_parent(&parent)
    {
    }

    RefPtr<SVGTransform> m_parent;
    WeakPtrFactory<SVGPropertyTearOffBase> m_weakFactory;
};

} // namespace WebCore
