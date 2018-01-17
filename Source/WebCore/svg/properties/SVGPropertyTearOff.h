/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "ExceptionOr.h"
#include "SVGAnimatedProperty.h"
#include "SVGProperty.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class SVGElement;

class SVGPropertyTearOffBase : public SVGProperty {
public:
    virtual void detachWrapper() = 0;
};

template<typename T>
class SVGPropertyTearOff : public SVGPropertyTearOffBase {
public:
    using PropertyType = T;
    using Self = SVGPropertyTearOff<PropertyType>;

    // Used for child types (baseVal/animVal) of a SVGAnimated* property (for example: SVGAnimatedLength::baseVal()).
    // Also used for list tear offs (for example: text.x.baseVal.getItem(0)).
    static Ref<Self> create(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, PropertyType& value)
    {
        return adoptRef(*new Self(animatedProperty, role, value));
    }

    // Used for non-animated POD types (for example: SVGSVGElement::createSVGLength()).
    static Ref<Self> create(const PropertyType& initialValue)
    {
        return adoptRef(*new Self(initialValue));
    }

    static Ref<Self> create(const PropertyType* initialValue)
    {
        return adoptRef(*new Self(initialValue));
    }

    template<typename U> static ExceptionOr<Ref<Self>> create(ExceptionOr<U>&& initialValue)
    {
        if (initialValue.hasException())
            return initialValue.releaseException();
        return create(initialValue.releaseReturnValue());
    }

    virtual PropertyType& propertyReference() { return *m_value; }
    SVGAnimatedProperty* animatedProperty() const { return m_animatedProperty.get(); }

    virtual void setValue(PropertyType& value)
    {
        if (m_valueIsCopy) {
            detachChildren();
            delete m_value;
        }
        m_valueIsCopy = false;
        m_value = &value;
    }

    void setAnimatedProperty(SVGAnimatedProperty* animatedProperty)
    {
        m_animatedProperty = animatedProperty;
    }

    SVGElement* contextElement() const
    {
        if (!m_animatedProperty || m_valueIsCopy)
            return nullptr;
        return m_animatedProperty->contextElement();
    }

    void addChild(WeakPtr<SVGPropertyTearOffBase> child)
    {
        m_childTearOffs.append(child);
    }

    void detachWrapper() override
    {
        if (m_valueIsCopy)
            return;

        detachChildren();

        // Switch from a live value, to a non-live value.
        // For example: <text x="50"/>
        // var item = text.x.baseVal.getItem(0);
        // text.setAttribute("x", "100");
        // item.value still has to report '50' and it has to be possible to modify 'item'
        // w/o changing the "new item" (with x=100) in the text element.
        // Whenever the XML DOM modifies the "x" attribute, all existing wrappers are detached, using this function.
        m_value = new PropertyType(*m_value);
        m_valueIsCopy = true;
        m_animatedProperty = nullptr;
    }

    void commitChange() override
    {
        if (!m_animatedProperty || m_valueIsCopy)
            return;
        m_animatedProperty->commitChange();
    }

    bool isReadOnly() const override
    {
        if (m_role == AnimValRole)
            return true;
        if (m_animatedProperty && m_animatedProperty->isReadOnly())
            return true;
        return false;
    }

    WeakPtr<SVGPropertyTearOff> createWeakPtr() const
    {
        return m_weakPtrFactory.createWeakPtr(*const_cast<SVGPropertyTearOff*>(this));
    }

protected:
    SVGPropertyTearOff(SVGAnimatedProperty* animatedProperty, SVGPropertyRole role, PropertyType& value)
        : m_animatedProperty(animatedProperty)
        , m_role(role)
        , m_value(&value)
        , m_valueIsCopy(false)
    {
    }

    SVGPropertyTearOff(const PropertyType& initialValue)
        : SVGPropertyTearOff(&initialValue)
    {
    }

    SVGPropertyTearOff(const PropertyType* initialValue)
        : m_animatedProperty(nullptr)
        , m_role(UndefinedRole)
        , m_value(initialValue ? new PropertyType(*initialValue) : nullptr)
        , m_valueIsCopy(true)
    {
    }

    virtual ~SVGPropertyTearOff()
    {
        if (m_valueIsCopy) {
            detachChildren();
            delete m_value;
        }
    }

    void detachChildren()
    {
        for (const auto& childTearOff : m_childTearOffs) {
            if (childTearOff.get())
                childTearOff.get()->detachWrapper();
        }
        m_childTearOffs.clear();
    }

    RefPtr<SVGAnimatedProperty> m_animatedProperty;
    SVGPropertyRole m_role;
    PropertyType* m_value;
    Vector<WeakPtr<SVGPropertyTearOffBase>> m_childTearOffs;
    WeakPtrFactory<SVGPropertyTearOff> m_weakPtrFactory;
    bool m_valueIsCopy;
};

}
