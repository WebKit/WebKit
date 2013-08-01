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

    explicit PropertySlot(const JSValue thisValue)
        : m_propertyType(TypeUnset)
        , m_offset(invalidOffset)
        , m_thisValue(thisValue)
    {
    }

    typedef JSValue (*GetValueFunc)(ExecState*, JSValue slotBase, PropertyName);
    typedef JSValue (*GetIndexValueFunc)(ExecState*, JSValue slotBase, unsigned);

    JSValue getValue(ExecState*, PropertyName) const;
    JSValue getValue(ExecState*, unsigned propertyName) const;

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

    JSObject* slotBase() const
    {
        ASSERT(m_propertyType != TypeUnset);
        return m_slotBase;
    }

    void setValue(JSObject* slotBase, JSValue value)
    {
        ASSERT(value);
        m_data.value = JSValue::encode(value);

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeValue;
        m_offset = invalidOffset;
    }
    
    void setValue(JSObject* slotBase, JSValue value, PropertyOffset offset)
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

        m_slotBase = 0;
        m_propertyType = TypeValue;
        m_offset = invalidOffset;
    }

    void setCustom(JSObject* slotBase, GetValueFunc getValue)
    {
        ASSERT(getValue);
        m_data.custom.getValue = getValue;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeCustom;
        m_offset = invalidOffset;
    }
    
    void setCacheableCustom(JSObject* slotBase, GetValueFunc getValue)
    {
        ASSERT(getValue);
        m_data.custom.getValue = getValue;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeCustom;
        m_offset = !invalidOffset;
    }

    void setCustomIndex(JSObject* slotBase, unsigned index, GetIndexValueFunc getIndexValue)
    {
        ASSERT(getIndexValue);
        m_data.customIndex.getIndexValue = getIndexValue;
        m_data.customIndex.index = index;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeCustomIndex;
        m_offset = invalidOffset;
    }

    void setGetterSlot(JSObject* slotBase, GetterSetter* getterSetter)
    {
        ASSERT(getterSetter);
        m_data.getter.getterSetter = getterSetter;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeGetter;
        m_offset = invalidOffset;
    }

    void setCacheableGetterSlot(JSObject* slotBase, GetterSetter* getterSetter, PropertyOffset offset)
    {
        ASSERT(getterSetter);
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

private:
    JS_EXPORT_PRIVATE JSValue functionGetter(ExecState*) const;

    union {
        EncodedJSValue value;
        struct {
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

    PropertyType m_propertyType;
    PropertyOffset m_offset;
    JSValue m_thisValue;
    JSObject* m_slotBase;
};

} // namespace JSC

#endif // PropertySlot_h
