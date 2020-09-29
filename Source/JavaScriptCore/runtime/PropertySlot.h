/*
 *  Copyright (C) 2005-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "DOMAnnotation.h"
#include "DisallowVMEntry.h"
#include "GetVM.h"
#include "JSCJSValue.h"
#include "PropertyName.h"
#include "PropertyOffset.h"
#include "ScopeOffset.h"
#include <wtf/Assertions.h>
#include <wtf/ForbidHeapAllocation.h>

namespace JSC {
class GetterSetter;
class JSObject;
class JSModuleEnvironment;

// ECMA 262-3 8.6.1
// Property attributes
enum class PropertyAttribute : unsigned {
    None              = 0,
    ReadOnly          = 1 << 1,  // property can be only read, not written
    DontEnum          = 1 << 2,  // property doesn't appear in (for .. in ..)
    DontDelete        = 1 << 3,  // property can't be deleted
    Accessor          = 1 << 4,  // property is a getter/setter
    CustomAccessor    = 1 << 5,
    CustomValue       = 1 << 6,
    CustomAccessorOrValue = CustomAccessor | CustomValue,
    AccessorOrCustomAccessorOrValue = Accessor | CustomAccessor | CustomValue,

    // Things that are used by static hashtables are not in the attributes byte in PropertyMapEntry.
    Function          = 1 << 8,  // property is a function - only used by static hashtables
    Builtin           = 1 << 9,  // property is a builtin function - only used by static hashtables
    ConstantInteger   = 1 << 10, // property is a constant integer - only used by static hashtables
    CellProperty      = 1 << 11, // property is a lazy property - only used by static hashtables
    ClassStructure    = 1 << 12, // property is a lazy class structure - only used by static hashtables
    PropertyCallback  = 1 << 13, // property that is a lazy property callback - only used by static hashtables
    DOMAttribute      = 1 << 14, // property is a simple DOM attribute - only used by static hashtables
    DOMJITAttribute   = 1 << 15, // property is a DOM JIT attribute - only used by static hashtables
    DOMJITFunction    = 1 << 16, // property is a DOM JIT function - only used by static hashtables
    BuiltinOrFunction = Builtin | Function, // helper only used by static hashtables
    BuiltinOrFunctionOrLazyProperty = Builtin | Function | CellProperty | ClassStructure | PropertyCallback, // helper only used by static hashtables
    BuiltinOrFunctionOrAccessorOrLazyProperty = Builtin | Function | Accessor | CellProperty | ClassStructure | PropertyCallback, // helper only used by static hashtables
    BuiltinOrFunctionOrAccessorOrLazyPropertyOrConstant = Builtin | Function | Accessor | CellProperty | ClassStructure | PropertyCallback | ConstantInteger // helper only used by static hashtables
};

static constexpr unsigned operator| (PropertyAttribute a, PropertyAttribute b) { return static_cast<unsigned>(a) | static_cast<unsigned>(b); }
static constexpr unsigned operator| (unsigned a, PropertyAttribute b) { return a | static_cast<unsigned>(b); }
static constexpr unsigned operator| (PropertyAttribute a, unsigned b) { return static_cast<unsigned>(a) | b; }
static constexpr unsigned operator&(unsigned a, PropertyAttribute b) { return a & static_cast<unsigned>(b); }
static constexpr bool operator<(PropertyAttribute a, PropertyAttribute b) { return static_cast<unsigned>(a) < static_cast<unsigned>(b); }
static constexpr unsigned operator~(PropertyAttribute a) { return ~static_cast<unsigned>(a); }
static constexpr bool operator<(PropertyAttribute a, unsigned b) { return static_cast<unsigned>(a) < b; }
static inline unsigned& operator|=(unsigned& a, PropertyAttribute b) { return a |= static_cast<unsigned>(b); }

enum CacheabilityType : uint8_t {
    CachingDisallowed,
    CachingAllowed
};

inline unsigned attributesForStructure(unsigned attributes)
{
    // The attributes that are used just for the static hashtable are at bit 8 and higher.
    return static_cast<uint8_t>(attributes);
}

class PropertySlot {

    // We rely on PropertySlot being stack allocated when used. This is needed
    // because we rely on some of its fields being a GC root. For example, it
    // may be the only thing that points to the CustomGetterSetter property it has.
    WTF_FORBID_HEAP_ALLOCATION;

    enum PropertyType : uint8_t {
        TypeUnset,
        TypeValue,
        TypeGetter,
        TypeCustom,
        TypeCustomAccessor,
    };

public:
    enum class InternalMethodType : uint8_t {
        Get, // [[Get]] internal method in the spec.
        HasProperty, // [[HasProperty]] internal method in the spec.
        GetOwnProperty, // [[GetOwnProperty]] internal method in the spec.
        VMInquiry, // Our VM is just poking around. When this is the InternalMethodType, getOwnPropertySlot is not allowed to do user observable actions.
    };

    enum class AdditionalDataType : uint8_t {
        None,
        DOMAttribute, // Annotated with DOMAttribute information.
        ModuleNamespace, // ModuleNamespaceObject's environment access.
    };

    explicit PropertySlot(const JSValue thisValue, InternalMethodType internalMethodType, VM* vmForInquiry = nullptr)
        : m_thisValue(thisValue)
        , m_internalMethodType(internalMethodType)
    {
        if (isVMInquiry())
            disallowVMEntry.emplace(*vmForInquiry);
    }

    // FIXME: Remove this slotBase / receiver behavior difference in custom values and custom accessors.
    // https://bugs.webkit.org/show_bug.cgi?id=158014
    using GetValueFunc = EncodedJSValue(JIT_OPERATION_ATTRIBUTES*)(JSGlobalObject*, EncodedJSValue thisValue, PropertyName);

    JSValue getValue(JSGlobalObject*, PropertyName) const;
    JSValue getValue(JSGlobalObject*, uint64_t propertyName) const;
    JSValue getPureResult() const;

    bool isCacheable() const { return isUnset() || m_cacheability == CachingAllowed; }
    bool isUnset() const { return m_propertyType == TypeUnset; }
    bool isValue() const { return m_propertyType == TypeValue; }
    bool isAccessor() const { return m_propertyType == TypeGetter; }
    bool isCustom() const { return m_propertyType == TypeCustom; }
    bool isCustomAccessor() const { return m_propertyType == TypeCustomAccessor; }
    bool isCacheableValue() const { return isCacheable() && isValue(); }
    bool isCacheableGetter() const { return isCacheable() && isAccessor(); }
    bool isCacheableCustom() const { return isCacheable() && isCustom(); }
    void setIsTaintedByOpaqueObject() { m_isTaintedByOpaqueObject = true; }
    bool isTaintedByOpaqueObject() const { return m_isTaintedByOpaqueObject; }

    InternalMethodType internalMethodType() const { return m_internalMethodType; }
    bool isVMInquiry() const { return m_internalMethodType == InternalMethodType::VMInquiry; }

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

    CustomGetterSetter* customGetterSetter() const
    {
        ASSERT(isCustomAccessor());
        return m_data.customAccessor.getterSetter;
    }

    JSObject* slotBase() const
    {
        return m_slotBase;
    }

    WatchpointSet* watchpointSet() const
    {
        return m_watchpointSet;
    }

    Optional<DOMAttributeAnnotation> domAttribute() const
    {
        if (m_additionalDataType == AdditionalDataType::DOMAttribute)
            return m_additionalData.domAttribute;
        return WTF::nullopt;
    }

    struct ModuleNamespaceSlot {
        JSModuleEnvironment* environment;
        unsigned scopeOffset;
    };

    Optional<ModuleNamespaceSlot> moduleNamespaceSlot() const
    {
        if (m_additionalDataType == AdditionalDataType::ModuleNamespace)
            return m_additionalData.moduleNamespaceSlot;
        return WTF::nullopt;
    }

    void setValue(JSObject* slotBase, unsigned attributes, JSValue value)
    {
        ASSERT(attributes == attributesForStructure(attributes));
        
        m_data.value = JSValue::encode(value);
        m_attributes = attributes;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeValue;

        ASSERT(m_cacheability == CachingDisallowed);
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

        m_cacheability = CachingAllowed;
    }

    void setValue(JSString*, unsigned attributes, JSValue value)
    {
        ASSERT(attributes == attributesForStructure(attributes));
        
        ASSERT(value);
        m_data.value = JSValue::encode(value);
        m_attributes = attributes;

        m_slotBase = nullptr;
        m_propertyType = TypeValue;

        ASSERT(m_cacheability == CachingDisallowed);
    }

    void setValueModuleNamespace(JSObject* slotBase, unsigned attributes, JSValue value, JSModuleEnvironment* environment, ScopeOffset scopeOffset)
    {
        setValue(slotBase, attributes, value);
        m_additionalDataType = AdditionalDataType::ModuleNamespace;
        m_additionalData.moduleNamespaceSlot.environment = environment;
        m_additionalData.moduleNamespaceSlot.scopeOffset = scopeOffset.offset();
    }

    void setCustom(JSObject* slotBase, unsigned attributes, GetValueFunc getValue)
    {
        ASSERT(attributes == attributesForStructure(attributes));
        
        ASSERT(getValue);
        assertIsCFunctionPtr(getValue);
        m_data.custom.getValue = getValue;
        m_attributes = attributes;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeCustom;
        ASSERT(m_cacheability == CachingDisallowed);
    }

    void setCustom(JSObject* slotBase, unsigned attributes, GetValueFunc getValue, DOMAttributeAnnotation domAttribute)
    {
        setCustom(slotBase, attributes, getValue);
        m_additionalDataType = AdditionalDataType::DOMAttribute;
        m_additionalData.domAttribute = domAttribute;
    }
    
    void setCacheableCustom(JSObject* slotBase, unsigned attributes, GetValueFunc getValue)
    {
        ASSERT(attributes == attributesForStructure(attributes));
        
        ASSERT(getValue);
        assertIsCFunctionPtr(getValue);
        m_data.custom.getValue = getValue;
        m_attributes = attributes;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeCustom;

        m_cacheability = CachingAllowed;
    }

    void setCacheableCustom(JSObject* slotBase, unsigned attributes, GetValueFunc getValue, DOMAttributeAnnotation domAttribute)
    {
        setCacheableCustom(slotBase, attributes, getValue);
        m_additionalDataType = AdditionalDataType::DOMAttribute;
        m_additionalData.domAttribute = domAttribute;
    }

    void setCustomGetterSetter(JSObject* slotBase, unsigned attributes, CustomGetterSetter* getterSetter)
    {
        ASSERT(attributes == attributesForStructure(attributes));
        ASSERT(attributes & PropertyAttribute::CustomAccessor);

        disableCaching();

        ASSERT(getterSetter);
        m_data.customAccessor.getterSetter = getterSetter;
        m_attributes = attributes;

        ASSERT(slotBase);
        m_slotBase = slotBase;
        m_propertyType = TypeCustomAccessor;

        ASSERT(m_cacheability == CachingDisallowed);
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

        ASSERT(m_cacheability == CachingDisallowed);
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

        m_cacheability = CachingAllowed;
    }

    JSValue thisValue() const
    {
        return m_thisValue;
    }

    void setThisValue(JSValue thisValue)
    {
        m_thisValue = thisValue;
    }

    void setUndefined()
    {
        m_data.value = JSValue::encode(jsUndefined());
        m_attributes = PropertyAttribute::ReadOnly | PropertyAttribute::DontDelete | PropertyAttribute::DontEnum;

        m_slotBase = nullptr;
        m_propertyType = TypeValue;
    }

    void setWatchpointSet(WatchpointSet& set)
    {
        m_watchpointSet = &set;
    }

private:
    JS_EXPORT_PRIVATE JSValue functionGetter(JSGlobalObject*) const;
    JS_EXPORT_PRIVATE JSValue customGetter(JSGlobalObject*, PropertyName) const;
    JS_EXPORT_PRIVATE JSValue customAccessorGetter(JSGlobalObject*, PropertyName) const;

    union {
        EncodedJSValue value;
        struct {
            GetterSetter* getterSetter;
        } getter;
        struct {
            GetValueFunc getValue;
        } custom;
        struct {
            CustomGetterSetter* getterSetter;
        } customAccessor;
    } m_data;

    unsigned m_attributes { 0 };
    PropertyOffset m_offset { invalidOffset };
    JSValue m_thisValue;
    JSObject* m_slotBase { nullptr };
    WatchpointSet* m_watchpointSet { nullptr };
    CacheabilityType m_cacheability { CachingDisallowed };
    PropertyType m_propertyType { TypeUnset };
    InternalMethodType m_internalMethodType;
    AdditionalDataType m_additionalDataType { AdditionalDataType::None };
    bool m_isTaintedByOpaqueObject { false };
public:
    Optional<DisallowVMEntry> disallowVMEntry;
private:
    union {
        DOMAttributeAnnotation domAttribute;
        ModuleNamespaceSlot moduleNamespaceSlot;
    } m_additionalData { { nullptr, nullptr } };
};

ALWAYS_INLINE JSValue PropertySlot::getValue(JSGlobalObject* globalObject, PropertyName propertyName) const
{
    if (m_propertyType == TypeValue)
        return JSValue::decode(m_data.value);
    if (m_propertyType == TypeGetter)
        return functionGetter(globalObject);
    if (m_propertyType == TypeCustomAccessor)
        return customAccessorGetter(globalObject, propertyName);
    return customGetter(globalObject, propertyName);
}

ALWAYS_INLINE JSValue PropertySlot::getValue(JSGlobalObject* globalObject, uint64_t propertyName) const
{
    VM& vm = getVM(globalObject);
    if (m_propertyType == TypeValue)
        return JSValue::decode(m_data.value);
    if (m_propertyType == TypeGetter)
        return functionGetter(globalObject);
    if (m_propertyType == TypeCustomAccessor)
        return customAccessorGetter(globalObject, Identifier::from(vm, propertyName));
    return customGetter(globalObject, Identifier::from(vm, propertyName));
}

} // namespace JSC
