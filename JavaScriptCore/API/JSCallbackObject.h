/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

namespace JSC {

struct JSCallbackObjectData {
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
    
    void setPrivateProperty(const Identifier& propertyName, JSValue value)
    {
        if (!m_privateProperties)
            m_privateProperties.set(new JSPrivatePropertyMap);
        m_privateProperties->setPrivateProperty(propertyName, value);
    }
    
    void deletePrivateProperty(const Identifier& propertyName)
    {
        if (!m_privateProperties)
            return;
        m_privateProperties->deletePrivateProperty(propertyName);
    }

    void markChildren(MarkStack& markStack)
    {
        if (!m_privateProperties)
            return;
        m_privateProperties->markChildren(markStack);
    }

    void* privateData;
    JSClassRef jsClass;
    struct JSPrivatePropertyMap {
        JSValue getPrivateProperty(const Identifier& propertyName) const
        {
            PrivatePropertyMap::const_iterator location = m_propertyMap.find(propertyName.ustring().rep());
            if (location == m_propertyMap.end())
                return JSValue();
            return location->second;
        }
        
        void setPrivateProperty(const Identifier& propertyName, JSValue value)
        {
            m_propertyMap.set(propertyName.ustring().rep(), value);
        }
        
        void deletePrivateProperty(const Identifier& propertyName)
        {
            m_propertyMap.remove(propertyName.ustring().rep());
        }

        void markChildren(MarkStack& markStack)
        {
            for (PrivatePropertyMap::iterator ptr = m_propertyMap.begin(); ptr != m_propertyMap.end(); ++ptr) {
                if (ptr->second)
                    markStack.append(ptr->second);
            }
        }

    private:
        typedef HashMap<RefPtr<UString::Rep>, JSValue, IdentifierRepHash> PrivatePropertyMap;
        PrivatePropertyMap m_propertyMap;
    };
    OwnPtr<JSPrivatePropertyMap> m_privateProperties;
};

    
template <class Base>
class JSCallbackObject : public Base {
public:
    JSCallbackObject(ExecState*, NonNullPassRefPtr<Structure>, JSClassRef, void* data);
    JSCallbackObject(JSClassRef);
    virtual ~JSCallbackObject();

    void setPrivate(void* data);
    void* getPrivate();

    static const ClassInfo info;

    JSClassRef classRef() const { return m_callbackObjectData->jsClass; }
    bool inherits(JSClassRef) const;

    static PassRefPtr<Structure> createStructure(JSValue proto) 
    { 
        return Structure::create(proto, TypeInfo(ObjectType, StructureFlags), Base::AnonymousSlotCount); 
    }
    
    JSValue getPrivateProperty(const Identifier& propertyName) const
    {
        return m_callbackObjectData->getPrivateProperty(propertyName);
    }
    
    void setPrivateProperty(const Identifier& propertyName, JSValue value)
    {
        m_callbackObjectData->setPrivateProperty(propertyName, value);
    }
    
    void deletePrivateProperty(const Identifier& propertyName)
    {
        m_callbackObjectData->deletePrivateProperty(propertyName);
    }

protected:
    static const unsigned StructureFlags = OverridesGetOwnPropertySlot | ImplementsHasInstance | OverridesHasInstance | OverridesMarkChildren | OverridesGetPropertyNames | Base::StructureFlags;

private:
    virtual UString className() const;

    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual bool getOwnPropertySlot(ExecState*, unsigned, PropertySlot&);
    virtual bool getOwnPropertyDescriptor(ExecState*, const Identifier&, PropertyDescriptor&);
    
    virtual void put(ExecState*, const Identifier&, JSValue, PutPropertySlot&);

    virtual bool deleteProperty(ExecState*, const Identifier&);
    virtual bool deleteProperty(ExecState*, unsigned);

    virtual bool hasInstance(ExecState* exec, JSValue value, JSValue proto);

    virtual void getOwnPropertyNames(ExecState*, PropertyNameArray&, EnumerationMode mode = ExcludeDontEnumProperties);

    virtual double toNumber(ExecState*) const;
    virtual UString toString(ExecState*) const;

    virtual ConstructType getConstructData(ConstructData&);
    virtual CallType getCallData(CallData&);
    virtual const ClassInfo* classInfo() const { return &info; }

    virtual void markChildren(MarkStack& markStack)
    {
        Base::markChildren(markStack);
        m_callbackObjectData->markChildren(markStack);
    }

    void init(ExecState*);
 
    static JSCallbackObject* asCallbackObject(JSValue);
 
    static JSValue JSC_HOST_CALL call(ExecState*, JSObject* functionObject, JSValue thisValue, const ArgList&);
    static JSObject* construct(ExecState*, JSObject* constructor, const ArgList&);
   
    static JSValue staticValueGetter(ExecState*, JSValue, const Identifier&);
    static JSValue staticFunctionGetter(ExecState*, JSValue, const Identifier&);
    static JSValue callbackGetter(ExecState*, JSValue, const Identifier&);

    OwnPtr<JSCallbackObjectData> m_callbackObjectData;
};

} // namespace JSC

// include the actual template class implementation
#include "JSCallbackObjectFunctions.h"

#endif // JSCallbackObject_h
