/*
 *  Copyright (C) 2005, 2007, 2008, 2015 Apple Inc. All rights reserved.
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

namespace JSC {

class ExecState;
class GetterSetter;
class JSObject;

// ECMA 262-3 8.6.1
// Property attributes
enum Attribute {
    None              = 0,
    ReadOnly          = 1 << 1,  // property can be only read, not written
    DontEnum          = 1 << 2,  // property doesn't appear in (for .. in ..)
    DontDelete        = 1 << 3,  // property can't be deleted
    Accessor          = 1 << 4,  // property is a getter/setter
    CustomAccessor    = 1 << 5,

    // Things that are used by static hashtables are not in the attributes byte in PropertyMapEntry.
    Function          = 1 << 8,  // property is a function - only used by static hashtables
    Builtin           = 1 << 9,  // property is a builtin function - only used by static hashtables
    ConstantInteger   = 1 << 10, // property is a constant integer - only used by static hashtables
    BuiltinOrFunction = Builtin | Function, // helper only used by static hashtables
    BuiltinOrFunctionOrAccessor = Builtin | Function | Accessor, // helper only used by static hashtables
    BuiltinOrFunctionOrAccessorOrConstant = Builtin | Function | Accessor | ConstantInteger, // helper only used by static hashtables
};

inline unsigned attributesForStructure(unsigned attributes)
{
    // The attributes that are used just for the static hashtable are at bit 8 and higher.
    return static_cast<uint8_t>(attributes);
}

class PropertySlot {
    enum PropertyType : uint8_t {
        TypeUnset,
        TypeValue,
        TypeGetter,
        TypeCustom
    };

    enum CacheabilityType : uint8_t {
        CachingDisallowed,
        CachingAllowed
    };

public:
    enum class InternalMethodType : uint8_t {
        Get, // [[Get]] internal method in the spec.
        HasProperty, // [[HasProperty]] internal method in the spec.
        GetOwnProperty, // [[GetOwnProperty]] internal method in the spec.
        VMInquiry, // Our VM is just poking around. When this is the InternalMethodType, getOwnPropertySlot is not allowed to do user observable actions.
    };

    explicit PropertySlot(const JSValue thisValue, InternalMethodType internalMethodType)
        : m_offset(invalidOffset)
        , m_thisValue(thisValue)
        , m_slotBase(nullptr)
        , m_watchpointSet(nullptr)
        , m_cacheability(CachingAllowed)
        , m_propertyType(TypeUnset)
        , m_internalMethodType(internalMethodType)
    {
    }

    typedef EncodedJSValue (*GetValueFunc)(ExecState*, EncodedJSValue thisValue, PropertyName);

    JSValue getValue(ExecState*, PropertyName) const;
    JSValue getValue(ExecState*, unsigned propertyName) const;

    bool isCacheable() const { return m_cacheability == CachingAllowed && m_offset != invalidOffset; }
    bool isUnset() const { return m_propertyType == TypeUnset; }
    bool isValue() const { return m_propertyType == TypeValue; }
    bool isAccessor() const { return m_propertyType == TypeGetter; }
    bool isCustom() const { return m_propertyType == TypeCustom; }
    bool isCacheableValue() const { return isCacheable() && isValue(); }
    bool isCacheableGetter() const { return isCacheable() && isAccessor(); }
    bool isCacheableCustom() const { return isCacheable() && isCustom(); }

    InternalMethodType internalMethodType() const { return m_internalMethodType; }

    void disableCaching()
    {
        m_cacheability = CachingDisallowed;
    }

    unsigned attributes() const { return m_attributes; }

    PropertyOffset cachedOffset() const
    {
        ASSERT(isCacheable());
        return m_offset;
    }

    GetterSetter* getterSetter() const
    {
        ASSERT(isAccessor());
        return m_data.getter.getterSetter;
    }

    GetValueFunc customGetter() const
    {
        ASSERT(isCacheableCustom());
        return m_data.custom.getValue;
    }

    JSObject* slotBase() const
    {
        return m_slotBase;
    }

    WatchpointSet* watchpointSet() const
    {
        return m_watchpointSet;
    }

    void setValue(JSObject* slotBase, unsigned attributes, JSValue value)
    {
        ASSERT(attributes == attributesForStructure(attributes));
        
        m_data.value = JSValue::encode(value);
        m_attributes = attributes;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeValue;
        m_offset = invalidOffset;
    }
    
    void setValue(JSObject* slotBase, unsigned attributes, JSValue value, PropertyOffset offset)
    {
        ASSERT(attributes == attributesForStructure(attributes));
        
        ASSERT(value);
        m_data.value = JSValue::encode(value);
        m_attributes = attributes;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeValue;
        m_offset = offset;
    }

    void setValue(JSString*, unsigned attributes, JSValue value)
    {
        ASSERT(attributes == attributesForStructure(attributes));
        
        ASSERT(value);
        m_data.value = JSValue::encode(value);
        m_attributes = attributes;

        m_slotBase = 0;
        m_propertyType = TypeValue;
        m_offset = invalidOffset;
    }

    void setCustom(JSObject* slotBase, unsigned attributes, GetValueFunc getValue)
    {
        ASSERT(attributes == attributesForStructure(attributes));
        
        ASSERT(getValue);
        m_data.custom.getValue = getValue;
        m_attributes = attributes;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeCustom;
        m_offset = invalidOffset;
    }
    
    void setCacheableCustom(JSObject* slotBase, unsigned attributes, GetValueFunc getValue)
    {
        ASSERT(attributes == attributesForStructure(attributes));
        
        ASSERT(getValue);
        m_data.custom.getValue = getValue;
        m_attributes = attributes;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeCustom;
        m_offset = !invalidOffset;
    }

    void setGetterSlot(JSObject* slotBase, unsigned attributes, GetterSetter* getterSetter)
    {
        ASSERT(attributes == attributesForStructure(attributes));
        
        ASSERT(getterSetter);
        m_data.getter.getterSetter = getterSetter;
        m_attributes = attributes;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeGetter;
        m_offset = invalidOffset;
    }

    void setCacheableGetterSlot(JSObject* slotBase, unsigned attributes, GetterSetter* getterSetter, PropertyOffset offset)
    {
        ASSERT(attributes == attributesForStructure(attributes));
        
        ASSERT(getterSetter);
        m_data.getter.getterSetter = getterSetter;
        m_attributes = attributes;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeGetter;
        m_offset = offset;
    }

    void setThisValue(JSValue thisValue)
    {
        m_thisValue = thisValue;
    }

    void setUndefined()
    {
        m_data.value = JSValue::encode(jsUndefined());
        m_attributes = ReadOnly | DontDelete | DontEnum;

        m_slotBase = 0;
        m_propertyType = TypeValue;
        m_offset = invalidOffset;
    }

    void setWatchpointSet(WatchpointSet& set)
    {
        m_watchpointSet = &set;
    }

private:
    JS_EXPORT_PRIVATE JSValue functionGetter(ExecState*) const;
    JS_EXPORT_PRIVATE JSValue customGetter(ExecState*, PropertyName) const;

    unsigned m_attributes;
    union {
        EncodedJSValue value;
        struct {
            GetterSetter* getterSetter;
        } getter;
        struct {
            GetValueFunc getValue;
        } custom;
    } m_data;

    PropertyOffset m_offset;
    JSValue m_thisValue;
    JSObject* m_slotBase;
    WatchpointSet* m_watchpointSet;
    CacheabilityType m_cacheability;
    PropertyType m_propertyType;
    InternalMethodType m_internalMethodType;
};

ALWAYS_INLINE JSValue PropertySlot::getValue(ExecState* exec, PropertyName propertyName) const
{
    if (m_propertyType == TypeValue)
        return JSValue::decode(m_data.value);
    if (m_propertyType == TypeGetter)
        return functionGetter(exec);
    return customGetter(exec, propertyName);
}

ALWAYS_INLINE JSValue PropertySlot::getValue(ExecState* exec, unsigned propertyName) const
{
    if (m_propertyType == TypeValue)
        return JSValue::decode(m_data.value);
    if (m_propertyType == TypeGetter)
        return functionGetter(exec);
    return customGetter(exec, Identifier::from(exec, propertyName));
}

} // namespace JSC

#endif // PropertySlot_h
