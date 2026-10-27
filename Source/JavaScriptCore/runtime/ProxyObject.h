/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/JSInternalFieldObjectImpl.h>

namespace JSC {

class ProxyObject final : public JSInternalFieldObjectImpl<2> {
public:
    using Base = JSInternalFieldObjectImpl<2>;

    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesGetOwnPropertyNames | OverridesGetPrototype | OverridesGetCallData | OverridesPut | OverridesIsExtensible | InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero | ProhibitsPropertyCaching;

    enum class Field : uint32_t {
        Target = 0,
        Handler,
    };
    static_assert(numberOfInternalFields == 2);
    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsNull(),
            jsUndefined(),
        } };
    }

    enum class HandlerTrap : uint8_t {
        Has = 0,
        Get,
        GetOwnPropertyDescriptor,
        OwnKeys,
        Set,
        Apply,
        DefineProperty,
        DeleteProperty,
        PreventExtensions,
        IsExtensible,
        SetPrototypeOf,
        GetPrototypeOf,
        Construct,
    };

    static constexpr unsigned numberOfCachedHandlerTrapsOffsets = static_cast<unsigned>(HandlerTrap::Construct) + 1;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.proxyObjectSpace<mode>();
    }

    static ProxyObject* create(JSGlobalObject* globalObject, JSValue target, JSValue handler)
    {
        VM& vm = getVM(globalObject);
        Structure* structure = ProxyObject::structureForTarget(globalObject, target);
        ProxyObject* proxy = new (NotNull, allocateCell<ProxyObject>(vm)) ProxyObject(vm, structure);
        proxy->finishCreation(vm, globalObject, target, handler);
        return proxy;
    }

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue, bool);

    DECLARE_EXPORT_INFO;

    DECLARE_VISIT_CHILDREN;

    JSObject* target() const { return uncheckedDowncast<JSObject>(internalField(Field::Target).get()); }
    JSValue handler() const { return internalField(Field::Handler).get(); }

    static void validateNegativeHasTrapResult(JSGlobalObject*, JSObject*, PropertyName);
    static void validatePositiveSetTrapResult(JSGlobalObject*, JSObject*, PropertyName, JSValue putValue);
    static void validateGetTrapResult(JSGlobalObject*, JSValue trapResult, JSObject*, PropertyName);

    static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    static bool putByIndex(JSCell*, JSGlobalObject*, unsigned propertyName, JSValue, bool shouldThrow);
    bool putByIndexCommon(JSGlobalObject*, JSValue thisValue, unsigned propertyName, JSValue putValue, bool shouldThrow);
    JSValue performGetPrototype(JSGlobalObject*);
    void revoke(VM&);
    bool LEGACY_NODELETE isRevoked() const;

    const WriteBarrier<Unknown>& internalField(Field field) const { return Base::internalField(static_cast<uint32_t>(field)); }
    WriteBarrier<Unknown>& internalField(Field field) { return Base::internalField(static_cast<uint32_t>(field)); }

    JSObject* getHandlerTrap(JSGlobalObject*, JSObject*, CallData&, const Identifier&, HandlerTrap);
    inline JSObject* callTargetIfKnownToHaveNoApplyTrap();
    bool LEGACY_NODELETE forwardsGetOwnPropertyNamesToTarget(DontEnumPropertiesMode);
    inline bool forwardsOwnPropertyDefinitionToTarget();

private:
    // A trap offset in one byte, so that a cache entry for every trap fits in ProxyObject's size class.
    class ProxyHandlerOffset {
    public:
        enum class State : uint8_t {
            HasCachedOffset,
            HasNoTrap,
            NeedsFullLookup,
        };

        constexpr ProxyHandlerOffset() = default;

        explicit constexpr ProxyHandlerOffset(PropertyOffset offset)
            : m_value(encode(offset))
        {
        }

        State state() const
        {
            if (m_value == hasNoTrapValue)
                return State::HasNoTrap;
            if (m_value == needsFullLookupValue)
                return State::NeedsFullLookup;
            return State::HasCachedOffset;
        }

        PropertyOffset offset() const
        {
            ASSERT(state() == State::HasCachedOffset);
            return m_value;
        }

    private:
        static constexpr uint8_t needsFullLookupValue = std::numeric_limits<uint8_t>::max();
        static constexpr uint8_t hasNoTrapValue = needsFullLookupValue - 1;

        static constexpr uint8_t encode(PropertyOffset offset)
        {
            if (!isValidOffset(offset))
                return hasNoTrapValue;
            if (static_cast<uint32_t>(offset) >= static_cast<uint32_t>(hasNoTrapValue))
                return needsFullLookupValue;
            return static_cast<uint8_t>(offset);
        }

        uint8_t m_value { needsFullLookupValue };
    };

    JS_EXPORT_PRIVATE ProxyObject(VM&, Structure*);
    JS_EXPORT_PRIVATE void finishCreation(VM&, JSGlobalObject*, JSValue target, JSValue handler);
    JS_EXPORT_PRIVATE static Structure* structureForTarget(JSGlobalObject*, JSValue target);

    bool isCallableProxy() const { return inlineTypeFlags() & ImplementsDefaultHasInstance; }
    bool isConstructibleProxy() const { return TypeInfo::perCellBit(inlineTypeFlags()); }

    bool isHandlerTrapsCacheValid(JSObject* handler);
    template<typename... Traps> bool isKnownToHaveNoHandlerTraps(Traps...);
    void LEGACY_NODELETE clearHandlerTrapsOffsetsCache();

    static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    static bool getOwnPropertySlotByIndex(JSObject*, JSGlobalObject*, unsigned propertyName, PropertySlot&);
    static CallData LEGACY_NODELETE getCallData(JSCell*);
    static CallData LEGACY_NODELETE getConstructData(JSCell*);
    static bool deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&);
    static bool deletePropertyByIndex(JSCell*, JSGlobalObject*, unsigned propertyName);
    static bool preventExtensions(JSObject*, JSGlobalObject*);
    static bool isExtensible(JSObject*, JSGlobalObject*);
    static bool defineOwnProperty(JSObject*, JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool shouldThrow);
    static void getOwnPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArrayBuilder&, DontEnumPropertiesMode);
    static bool setPrototype(JSObject*, JSGlobalObject*, JSValue prototype, bool shouldThrowIfCantSet);
    static JSValue getPrototype(JSObject*, JSGlobalObject*);

    bool getOwnPropertySlotCommon(JSGlobalObject*, PropertyName, PropertySlot&);
    bool performInternalMethodGetOwnProperty(JSGlobalObject*, PropertyName, PropertySlot&);
    bool performGet(JSGlobalObject*, PropertyName, PropertySlot&);
    bool performHasProperty(JSGlobalObject*, PropertyName, PropertySlot&);
    template <typename DefaultDeleteFunction>
    bool performDelete(JSGlobalObject*, PropertyName, DefaultDeleteFunction);
    template <typename PerformDefaultPutFunction>
    bool performPut(JSGlobalObject*, JSValue putValue, JSValue thisValue, PropertyName, PerformDefaultPutFunction, bool shouldThrow);
    bool performPreventExtensions(JSGlobalObject*);
    bool performIsExtensible(JSGlobalObject*);
    bool performDefineOwnProperty(JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool shouldThrow);
    void performGetOwnPropertyNames(JSGlobalObject*, PropertyNameArrayBuilder&);
    void performGetOwnEnumerablePropertyNames(JSGlobalObject*, PropertyNameArrayBuilder&);
    bool performSetPrototype(JSGlobalObject*, JSValue prototype, bool shouldThrowIfCantSet);

    WriteBarrierStructureID m_handlerStructureID;
    WriteBarrierStructureID m_handlerPrototypeStructureID;
    std::array<ProxyHandlerOffset, numberOfCachedHandlerTrapsOffsets> m_handlerTrapsOffsetsCache { };
};

ALWAYS_INLINE bool ProxyObject::isHandlerTrapsCacheValid(JSObject* handler)
{
    if (handler->structureID() != m_handlerStructureID.value())
        return false;
    ASSERT(handler->structure()->hasMonoProto());
    return asObject(handler->getPrototypeDirect())->structureID() == m_handlerPrototypeStructureID.value();
}

template<typename... Traps>
ALWAYS_INLINE bool ProxyObject::isKnownToHaveNoHandlerTraps(Traps... traps)
{
    JSValue handlerValue = handler();
    if (handlerValue.isNull())
        return false;
    if (!isHandlerTrapsCacheValid(asObject(handlerValue)))
        return false;
    return ((m_handlerTrapsOffsetsCache[static_cast<uint8_t>(traps)].state() == ProxyHandlerOffset::State::HasNoTrap) && ...);
}

// Returns the target when calling this proxy is exactly calling its target; decided without running user code.
inline JSObject* ProxyObject::callTargetIfKnownToHaveNoApplyTrap()
{
    if (!isCallableProxy())
        return nullptr;
    if (!isKnownToHaveNoHandlerTraps(HandlerTrap::Apply))
        return nullptr;
    return target();
}

// When true, this proxy's [[GetOwnProperty]] and [[DefineOwnProperty]] are exactly those of its target.
inline bool ProxyObject::forwardsOwnPropertyDefinitionToTarget()
{
    return isKnownToHaveNoHandlerTraps(HandlerTrap::GetOwnPropertyDescriptor, HandlerTrap::DefineProperty);
}

} // namespace JSC
