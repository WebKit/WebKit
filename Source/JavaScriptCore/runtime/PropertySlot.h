/*
 *  Copyright (C) 2005, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef PropertySlot_h
#define PropertySlot_h

#include "JSCJSValue.h"
#include "PropertyName.h"
#include "PropertyOffset.h"
#include "Register.h"
#include <wtf/Assertions.h>
#include <wtf/NotFound.h>

namespace JSC {

class ExecState;
class GetterSetter;

class PropertySlot {
    enum PropertyType {
        TypeUnset,
        TypeValue,
        TypeGetter,
        TypeCustom,
        TypeCustomIndex
    };

public:
    PropertySlot()
        : m_propertyType(TypeUnset)
        , m_offset(invalidOffset)
    {
    }

    explicit PropertySlot(const JSValue base)
        : m_slotBase(base)
        , m_propertyType(TypeUnset)
        , m_offset(invalidOffset)
    {
    }

    typedef JSValue (*GetValueFunc)(ExecState*, JSValue slotBase, PropertyName);
    typedef JSValue (*GetIndexValueFunc)(ExecState*, JSValue slotBase, unsigned);

    JSValue getValue(ExecState* exec, PropertyName propertyName) const
    {
        if (m_propertyType == TypeValue)
            return JSValue::decode(m_data.value);
        if (m_propertyType == TypeCustomIndex)
            return m_data.customIndex.getIndexValue(exec, slotBase(), m_data.customIndex.index);
        if (m_propertyType == TypeGetter)
            return functionGetter(exec);
        return m_data.custom.getValue(exec, slotBase(), propertyName);
    }

    JSValue getValue(ExecState* exec, unsigned propertyName) const
    {
        if (m_propertyType == TypeValue)
            return JSValue::decode(m_data.value);
        if (m_propertyType == TypeCustomIndex)
            return m_data.customIndex.getIndexValue(exec, slotBase(), m_data.customIndex.index);
        if (m_propertyType == TypeGetter)
            return functionGetter(exec);
        return m_data.custom.getValue(exec, slotBase(), Identifier::from(exec, propertyName));
    }

    bool isCacheable() const { return m_offset != invalidOffset; }
    bool isCacheableValue() const { return isCacheable() && m_propertyType == TypeValue; }
    bool isCacheableGetter() const { return isCacheable() && m_propertyType == TypeGetter; }
    bool isCacheableCustom() const { return isCacheable() && m_propertyType == TypeCustom; }

    PropertyOffset cachedOffset() const
    {
        ASSERT(isCacheable());
        return m_offset;
    }

    GetValueFunc customGetter() const
    {
        ASSERT(isCacheableCustom());
        return m_data.custom.getValue;
    }

    void setValue(JSValue slotBase, JSValue value)
    {
        ASSERT(value);
        m_data.value = JSValue::encode(value);

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeValue;
        m_offset = invalidOffset;
    }
    
    void setValue(JSValue slotBase, JSValue value, PropertyOffset offset)
    {
        ASSERT(value);
        m_data.value = JSValue::encode(value);

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeValue;
        m_offset = offset;
    }

    void setValue(JSValue value)
    {
        ASSERT(value);
        m_data.value = JSValue::encode(value);

        m_slotBase = JSValue();
        m_propertyType = TypeValue;
        m_offset = invalidOffset;
    }

    void setCustom(JSValue slotBase, GetValueFunc getValue)
    {
        ASSERT(getValue);
        m_data.custom.getValue = getValue;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeCustom;
        m_offset = invalidOffset;
    }
    
    void setCacheableCustom(JSValue slotBase, GetValueFunc getValue)
    {
        ASSERT(getValue);
        m_data.custom.getValue = getValue;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeCustom;
        m_offset = !invalidOffset;
    }

    void setCustomIndex(JSValue slotBase, unsigned index, GetIndexValueFunc getIndexValue)
    {
        ASSERT(getIndexValue);
        m_data.customIndex.getIndexValue = getIndexValue;
        m_data.customIndex.index = index;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeCustomIndex;
        m_offset = invalidOffset;
    }

    void setGetterSlot(GetterSetter* getterSetter)
    {
        ASSERT(getterSetter);
        m_data.getter.thisValue = JSValue::encode(m_slotBase);
        m_data.getter.getterSetter = getterSetter;

        m_propertyType = TypeGetter;
        m_offset = invalidOffset;
    }

    void setCacheableGetterSlot(JSValue slotBase, GetterSetter* getterSetter, PropertyOffset offset)
    {
        ASSERT(getterSetter);
        m_data.getter.thisValue = JSValue::encode(m_slotBase);
        m_data.getter.getterSetter = getterSetter;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeGetter;
        m_offset = offset;
    }

    void setUndefined()
    {
        setValue(jsUndefined());
    }

    JSValue slotBase() const
    {
        return m_slotBase;
    }

private:
    JS_EXPORT_PRIVATE JSValue functionGetter(ExecState*) const;

    union {
        EncodedJSValue value;
        struct {
            EncodedJSValue thisValue;
            GetterSetter* getterSetter;
        } getter;
        struct {
            GetValueFunc getValue;
        } custom;
        struct {
            GetIndexValueFunc getIndexValue;
            unsigned index;
        } customIndex;
    } m_data;

    JSValue m_slotBase;
    PropertyType m_propertyType;
    PropertyOffset m_offset;
};

} // namespace JSC

#endif // PropertySlot_h
