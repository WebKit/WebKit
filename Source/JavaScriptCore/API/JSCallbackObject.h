/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef JSCallbackObject_h
#define JSCallbackObject_h

#include "JSObjectRef.h"
#include "JSValueRef.h"
#include "JSObject.h"
#include <wtf/PlatformCallingConventions.h>

namespace JSC {

struct JSCallbackObjectData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    JSCallbackObjectData(void* privateData, JSClassRef jsClass)
        : privateData(privateData)
        , jsClass(jsClass)
    {
        JSClassRetain(jsClass);
    }
    
    ~JSCallbackObjectData()
    {
        JSClassRelease(jsClass);
    }
    
    JSValue getPrivateProperty(const Identifier& propertyName) const
    {
        if (!m_privateProperties)
            return JSValue();
        return m_privateProperties->getPrivateProperty(propertyName);
    }
    
    void setPrivateProperty(VM& vm, JSCell* owner, const Identifier& propertyName, JSValue value)
    {
        if (!m_privateProperties)
            m_privateProperties = makeUnique<JSPrivatePropertyMap>();
        m_privateProperties->setPrivateProperty(vm, owner, propertyName, value);
    }
    
    void deletePrivateProperty(const Identifier& propertyName)
    {
        if (!m_privateProperties)
            return;
        m_privateProperties->deletePrivateProperty(propertyName);
    }

    DECLARE_VISIT_CHILDREN;

    template<typename Visitor>
    void visitChildren(Visitor& visitor)
    {
        JSPrivatePropertyMap* properties = m_privateProperties.get();
        if (!properties)
            return;
        properties->visitChildren(visitor);
    }

    void* privateData;
    JSClassRef jsClass;
    struct JSPrivatePropertyMap {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        JSValue getPrivateProperty(const Identifier& propertyName) const
        {
            PrivatePropertyMap::const_iterator location = m_propertyMap.find(propertyName.impl());
            if (location == m_propertyMap.end())
                return JSValue();
            return location->value.get();
        }
        
        void setPrivateProperty(VM& vm, JSCell* owner, const Identifier& propertyName, JSValue value)
        {
            Locker locker { m_lock };
            WriteBarrier<Unknown> empty;
            m_propertyMap.add(propertyName.impl(), empty).iterator->value.set(vm, owner, value);
        }
        
        void deletePrivateProperty(const Identifier& propertyName)
        {
            Locker locker { m_lock };
            m_propertyMap.remove(propertyName.impl());
        }

        template<typename Visitor>
        void visitChildren(Visitor& visitor)
        {
            Locker locker { m_lock };
            for (auto& pair : m_propertyMap) {
                if (pair.value)
                    visitor.append(pair.value);
            }
        }

    private:
        typedef HashMap<RefPtr<UniquedStringImpl>, WriteBarrier<Unknown>, IdentifierRepHash> PrivatePropertyMap;
        PrivatePropertyMap m_propertyMap;
        Lock m_lock;
    };
    std::unique_ptr<JSPrivatePropertyMap> m_privateProperties;
};

    
template <class Parent>
class JSCallbackObject final : public Parent {
public:
    using Base = Parent;
    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesGetOwnSpecialPropertyNames | OverridesGetCallData | OverridesPut | InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero | ImplementsHasInstance | ProhibitsPropertyCaching | GetOwnPropertySlotMayBeWrongAboutDontEnum;
    static_assert(!(StructureFlags & ImplementsDefaultHasInstance), "using customHasInstance");

    ~JSCallbackObject();

    static JSCallbackObject* create(JSGlobalObject* globalObject, Structure* structure, JSClassRef classRef, void* data)
    {
        VM& vm = getVM(globalObject);
        ASSERT_UNUSED(globalObject, !structure->globalObject() || structure->globalObject() == globalObject);
        JSCallbackObject* callbackObject = new (NotNull, allocateCell<JSCallbackObject>(vm.heap)) JSCallbackObject(globalObject, structure, classRef, data);
        callbackObject->finishCreation(globalObject);
        return callbackObject;
    }
    static JSCallbackObject<Parent>* create(VM&, JSClassRef, Structure*);

    static const bool needsDestruction;
    static void destroy(JSCell* cell)
    {
        static_cast<JSCallbackObject*>(cell)->JSCallbackObject::~JSCallbackObject();
    }

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return subspaceForImpl(vm, mode);
    }

    void setPrivate(void* data);
    void* getPrivate();

    // FIXME: We should fix the warnings for extern-template in JSObject template classes: https://bugs.webkit.org/show_bug.cgi?id=161979
    IGNORE_CLANG_WARNINGS_BEGIN("undefined-var-template")
    DECLARE_INFO;
    IGNORE_CLANG_WARNINGS_END

    JSClassRef classRef() const { return m_callbackObjectData->jsClass; }
    bool inherits(JSClassRef) const;

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);
    
    JSValue getPrivateProperty(const Identifier& propertyName) const
    {
        return m_callbackObjectData->getPrivateProperty(propertyName);
    }
    
    void setPrivateProperty(VM& vm, const Identifier& propertyName, JSValue value)
    {
        m_callbackObjectData->setPrivateProperty(vm, this, propertyName, value);
    }
    
    void deletePrivateProperty(const Identifier& propertyName)
    {
        m_callbackObjectData->deletePrivateProperty(propertyName);
    }

    using Parent::methodTable;

    static EncodedJSValue callImpl(JSGlobalObject*, CallFrame*);
    static EncodedJSValue constructImpl(JSGlobalObject*, CallFrame*);
    static EncodedJSValue staticFunctionGetterImpl(JSGlobalObject*, EncodedJSValue, PropertyName);
    static EncodedJSValue callbackGetterImpl(JSGlobalObject*, EncodedJSValue, PropertyName);
   
private:
    JSCallbackObject(JSGlobalObject*, Structure*, JSClassRef, void* data);
    JSCallbackObject(VM&, JSClassRef, Structure*);

    void finishCreation(JSGlobalObject*);
    void finishCreation(VM&);

    static IsoSubspace* subspaceForImpl(VM&, SubspaceAccess);
    static EncodedJSValue JSC_HOST_CALL_ATTRIBUTES customToPrimitive(JSGlobalObject*, CallFrame*);

    static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    static bool getOwnPropertySlotByIndex(JSObject*, JSGlobalObject*, unsigned propertyName, PropertySlot&);
    
    static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    static bool putByIndex(JSCell*, JSGlobalObject*, unsigned, JSValue, bool shouldThrow);

    static bool deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&);
    static bool deletePropertyByIndex(JSCell*, JSGlobalObject*, unsigned);

    static bool customHasInstance(JSObject*, JSGlobalObject*, JSValue);

    static void getOwnSpecialPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, DontEnumPropertiesMode);

    static CallData getConstructData(JSCell*);
    static CallData getCallData(JSCell*);

    DECLARE_VISIT_CHILDREN;

    void init(JSGlobalObject*);
 
    static JSCallbackObject* asCallbackObject(JSValue);
    static JSCallbackObject* asCallbackObject(EncodedJSValue);

    using RawNativeFunction = EncodedJSValue(JSC_HOST_CALL_ATTRIBUTES*)(JSGlobalObject*, CallFrame*);

    static RawNativeFunction getCallFunction();
    static RawNativeFunction getConstructFunction();

    static GetValueFunc getStaticFunctionGetter();
    static GetValueFunc getCallbackGetter();
 
    JSValue getStaticValue(JSGlobalObject*, PropertyName);

    std::unique_ptr<JSCallbackObjectData> m_callbackObjectData;
    const ClassInfo* m_classInfo { nullptr };
};

template <class Parent>
template<typename Visitor>
void JSCallbackObject<Parent>::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSCallbackObject* thisObject = jsCast<JSCallbackObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS((static_cast<Parent*>(thisObject)), JSCallbackObject<Parent>::info());
    Parent::visitChildren(thisObject, visitor);
    thisObject->m_callbackObjectData->visitChildren(visitor);
}

} // namespace JSC

// include the actual template class implementation
#include "JSCallbackObjectFunctions.h"

#endif // JSCallbackObject_h
