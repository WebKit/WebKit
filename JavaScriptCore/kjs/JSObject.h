/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef JSObject_h
#define JSObject_h

#include "ArgList.h"
#include "ClassInfo.h"
#include "CommonIdentifiers.h"
#include "ExecState.h"
#include "JSNumberCell.h"
#include "PropertyMap.h"
#include "PropertySlot.h"
#include "PutPropertySlot.h"
#include "ScopeChain.h"
#include "StructureID.h"

namespace KJS {

    class InternalFunction;
    class PropertyNameArray;
    class StructureID;
    struct HashEntry;
    struct HashTable;

    // ECMA 262-3 8.6.1
    // Property attributes
    enum Attribute {
        None         = 0,
        ReadOnly     = 1 << 1,  // property can be only read, not written
        DontEnum     = 1 << 2,  // property doesn't appear in (for .. in ..)
        DontDelete   = 1 << 3,  // property can't be deleted
        Function     = 1 << 4,  // property is a function - only used by static hashtables
    };

    class JSObject : public JSCell {
        friend class BatchedTransitionOptimizer;

    public:
        JSObject(PassRefPtr<StructureID>);
        JSObject(JSObject* prototype);
        virtual ~JSObject();

        virtual void mark();

        bool inherits(const ClassInfo* classInfo) const { return JSCell::isObject(classInfo); }

        JSValue* prototype() const;
        void setPrototype(JSValue* prototype);
        
        StructureID* inheritorID();

        virtual UString className() const;

        JSValue* get(ExecState*, const Identifier& propertyName) const;
        JSValue* get(ExecState*, unsigned propertyName) const;

        bool getPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        bool getPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

        virtual void put(ExecState*, const Identifier& propertyName, JSValue* value, PutPropertySlot&);
        virtual void put(ExecState*, unsigned propertyName, JSValue* value);

        virtual void putWithAttributes(ExecState*, const Identifier& propertyName, JSValue* value, unsigned attributes);
        virtual void putWithAttributes(ExecState*, unsigned propertyName, JSValue* value, unsigned attributes);

        bool propertyIsEnumerable(ExecState*, const Identifier& propertyName) const;

        bool hasProperty(ExecState*, const Identifier& propertyName) const;
        bool hasProperty(ExecState*, unsigned propertyName) const;
        bool hasOwnProperty(ExecState*, const Identifier& propertyName) const;

        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
        virtual bool deleteProperty(ExecState*, unsigned propertyName);

        virtual JSValue* defaultValue(ExecState*, PreferredPrimitiveType) const;

        virtual bool implementsHasInstance() const;
        virtual bool hasInstance(ExecState*, JSValue*);

        virtual void getPropertyNames(ExecState*, PropertyNameArray&);

        virtual JSValue* toPrimitive(ExecState*, PreferredPrimitiveType = NoPreference) const;
        virtual bool getPrimitiveNumber(ExecState*, double& number, JSValue*& value);
        virtual bool toBoolean(ExecState*) const;
        virtual double toNumber(ExecState*) const;
        virtual UString toString(ExecState*) const;
        virtual JSObject* toObject(ExecState*) const;

        virtual JSObject* toThisObject(ExecState*) const;
        virtual JSGlobalObject* toGlobalObject(ExecState*) const;

        virtual bool getPropertyAttributes(ExecState*, const Identifier& propertyName, unsigned& attributes) const;

        // This get function only looks at the property map.
        JSValue* getDirect(const Identifier& propertyName) const { return m_propertyMap.get(propertyName); }
        JSValue** getDirectLocation(const Identifier& propertyName) { return m_propertyMap.getLocation(propertyName); }
        JSValue** getDirectLocation(const Identifier& propertyName, bool& isWriteable) { return m_propertyMap.getLocation(propertyName, isWriteable); }
        size_t offsetForLocation(JSValue** location) { return m_propertyMap.offsetForLocation(location); }
        void removeDirect(const Identifier& propertyName);
        bool hasCustomProperties() { return !m_propertyMap.isEmpty(); }
        bool hasGetterSetterProperties() { return m_propertyMap.hasGetterSetterProperties(); }

        void putDirect(const Identifier& propertyName, JSValue* value, unsigned attr = 0);
        void putDirect(const Identifier& propertyName, JSValue* value, unsigned attr, bool checkReadOnly, PutPropertySlot& slot);
        void putDirectFunction(ExecState* exec, InternalFunction* function, unsigned attr = 0);

        // Fast access to known property offsets.
        JSValue* getDirectOffset(size_t offset) { return m_propertyMap.getOffset(offset); }
        void putDirectOffset(size_t offset, JSValue* v) { m_propertyMap.putOffset(offset, v); }

        void fillGetterPropertySlot(PropertySlot&, JSValue** location);

        virtual void defineGetter(ExecState*, const Identifier& propertyName, JSObject* getterFunction);
        virtual void defineSetter(ExecState*, const Identifier& propertyName, JSObject* setterFunction);
        virtual JSValue* lookupGetter(ExecState*, const Identifier& propertyName);
        virtual JSValue* lookupSetter(ExecState*, const Identifier& propertyName);

        virtual bool isActivationObject() const { return false; }
        virtual bool isGlobalObject() const { return false; }
        virtual bool isVariableObject() const { return false; }
        virtual bool isWatchdogException() const { return false; }
        virtual bool isNotAnObjectErrorStub() const { return false; }

    protected:
        bool getOwnPropertySlotForWrite(ExecState*, const Identifier&, PropertySlot&, bool& slotIsWriteable);

    private:
        virtual bool isObject() const;

        const HashEntry* findPropertyHashEntry(ExecState*, const Identifier& propertyName) const;
        void setStructureID(PassRefPtr<StructureID>);
        StructureID* createInheritorID();

        PropertyMap m_propertyMap;
        RefPtr<StructureID> m_inheritorID;
    };

  JSObject* constructEmptyObject(ExecState*);

inline JSObject::JSObject(JSObject* prototype)
    : JSCell(prototype->inheritorID())
{
    ASSERT(m_structureID);
    ASSERT(this->prototype());
    ASSERT(this->prototype()->isNull() || Heap::heap(this) == Heap::heap(this->prototype()));
    m_structureID->ref(); // ~JSObject balances this ref()
}

inline JSObject::JSObject(PassRefPtr<StructureID> structureID)
    : JSCell(structureID.releaseRef()) // ~JSObject balances this ref()
{
    ASSERT(m_structureID);
}

inline JSObject::~JSObject()
{
    ASSERT(m_structureID);
    m_structureID->deref();
}

inline JSValue* JSObject::prototype() const
{
    return m_structureID->prototype();
}

inline void JSObject::setPrototype(JSValue* prototype)
{
    ASSERT(prototype);
    RefPtr<StructureID> newStructureID = StructureID::changePrototypeTransition(m_structureID, prototype);
    setStructureID(newStructureID.release());
}

inline void JSObject::setStructureID(PassRefPtr<StructureID> structureID)
{
    m_structureID->deref();
    m_structureID = structureID.releaseRef(); // ~JSObject balances this ref()
}

inline StructureID* JSObject::inheritorID()
{
    if (m_inheritorID)
        return m_inheritorID.get();
    return createInheritorID();
}

inline bool JSCell::isObject(const ClassInfo* info) const
{
    for (const ClassInfo* ci = classInfo(); ci; ci = ci->parentClass) {
        if (ci == info)
            return true;
    }
    return false;
}

// this method is here to be after the inline declaration of JSCell::isObject
inline bool JSValue::isObject(const ClassInfo* classInfo) const
{
    return !JSImmediate::isImmediate(this) && asCell()->isObject(classInfo);
}

inline JSValue* JSObject::get(ExecState* exec, const Identifier& propertyName) const
{
    PropertySlot slot(const_cast<JSObject*>(this));
    if (const_cast<JSObject*>(this)->getPropertySlot(exec, propertyName, slot))
        return slot.getValue(exec, propertyName);
    
    return jsUndefined();
}

inline JSValue* JSObject::get(ExecState* exec, unsigned propertyName) const
{
    PropertySlot slot(const_cast<JSObject*>(this));
    if (const_cast<JSObject*>(this)->getPropertySlot(exec, propertyName, slot))
        return slot.getValue(exec, propertyName);

    return jsUndefined();
}

// It may seem crazy to inline a function this large but it makes a big difference
// since this is function very hot in variable lookup
inline bool JSObject::getPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    JSObject* object = this;
    while (true) {
        if (object->getOwnPropertySlot(exec, propertyName, slot))
            return true;

        JSValue* prototype = object->prototype();
        if (!prototype->isObject())
            return false;

        object = static_cast<JSObject*>(prototype);
    }
}

inline bool JSObject::getPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    JSObject* object = this;

    while (true) {
        if (object->getOwnPropertySlot(exec, propertyName, slot))
            return true;

        JSValue* prototype = object->prototype();
        if (!prototype->isObject())
            break;

        object = static_cast<JSObject*>(prototype);
    }

    return false;
}

// It may seem crazy to inline a function this large, especially a virtual function,
// but it makes a big difference to property lookup that derived classes can inline their
// base class call to this.
ALWAYS_INLINE bool JSObject::getOwnPropertySlotForWrite(ExecState* exec, const Identifier& propertyName, PropertySlot& slot, bool& slotIsWriteable)
{
    if (JSValue** location = getDirectLocation(propertyName, slotIsWriteable)) {
        if (m_propertyMap.hasGetterSetterProperties() && location[0]->isGetterSetter()) {
            slotIsWriteable = false;
            fillGetterPropertySlot(slot, location);
        } else
            slot.setValueSlot(this, location, offsetForLocation(location));
        return true;
    }

    // non-standard Netscape extension
    if (propertyName == exec->propertyNames().underscoreProto) {
        slot.setValue(prototype());
        slotIsWriteable = false;
        return true;
    }

    return false;
}

// It may seem crazy to inline a function this large, especially a virtual function,
// but it makes a big difference to property lookup that derived classes can inline their
// base class call to this.
ALWAYS_INLINE bool JSObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (JSValue** location = getDirectLocation(propertyName)) {
        if (m_propertyMap.hasGetterSetterProperties() && location[0]->isGetterSetter())
            fillGetterPropertySlot(slot, location);
        else
            slot.setValueSlot(this, location, offsetForLocation(location));
        return true;
    }

    // non-standard Netscape extension
    if (propertyName == exec->propertyNames().underscoreProto) {
        slot.setValue(prototype());
        return true;
    }

    return false;
}

inline void JSObject::putDirect(const Identifier& propertyName, JSValue* value, unsigned attr)
{
    PutPropertySlot slot;
    putDirect(propertyName, value, attr, false, slot);
}

inline void JSObject::putDirect(const Identifier& propertyName, JSValue* value, unsigned attr, bool checkReadOnly, PutPropertySlot& slot)
{
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));
    m_propertyMap.put(propertyName, value, attr, checkReadOnly, this, slot);
    if (slot.type() == PutPropertySlot::NewProperty) {
        if (!m_structureID->isDictionary()) {
            RefPtr<StructureID> structureID = StructureID::addPropertyTransition(m_structureID, propertyName);
            setStructureID(structureID.release());
        }
    }
}

inline JSValue* JSObject::toPrimitive(ExecState* exec, PreferredPrimitiveType preferredType) const
{
    return defaultValue(exec, preferredType);
}

inline JSValue* JSValue::get(ExecState* exec, const Identifier& propertyName) const
{
    PropertySlot slot(const_cast<JSValue*>(this));
    return get(exec, propertyName, slot);
}

inline JSValue* JSValue::get(ExecState* exec, const Identifier& propertyName, PropertySlot& slot) const
{
    if (UNLIKELY(JSImmediate::isImmediate(this))) {
        JSObject* prototype = JSImmediate::prototype(this, exec);
        if (!prototype->getPropertySlot(exec, propertyName, slot))
            return jsUndefined();
        return slot.getValue(exec, propertyName);
    }
    JSCell* cell = static_cast<JSCell*>(const_cast<JSValue*>(this));
    while (true) {
        if (cell->getOwnPropertySlot(exec, propertyName, slot))
            return slot.getValue(exec, propertyName);
        ASSERT(cell->isObject());
        JSValue* prototype = static_cast<JSObject*>(cell)->prototype();
        if (!prototype->isObject())
            return jsUndefined();
        cell = static_cast<JSCell*>(prototype);
    }
}

inline JSValue* JSValue::get(ExecState* exec, unsigned propertyName) const
{
    PropertySlot slot(const_cast<JSValue*>(this));
    return get(exec, propertyName, slot);
}

inline JSValue* JSValue::get(ExecState* exec, unsigned propertyName, PropertySlot& slot) const
{
    if (UNLIKELY(JSImmediate::isImmediate(this))) {
        JSObject* prototype = JSImmediate::prototype(this, exec);
        if (!prototype->getPropertySlot(exec, propertyName, slot))
            return jsUndefined();
        return slot.getValue(exec, propertyName);
    }
    JSCell* cell = const_cast<JSCell*>(asCell());
    while (true) {
        if (cell->getOwnPropertySlot(exec, propertyName, slot))
            return slot.getValue(exec, propertyName);
        ASSERT(cell->isObject());
        JSValue* prototype = static_cast<JSObject*>(cell)->prototype();
        if (!prototype->isObject())
            return jsUndefined();
        cell = static_cast<JSCell*>(prototype);
    }
}

inline void JSValue::put(ExecState* exec, const Identifier& propertyName, JSValue* value, PutPropertySlot& slot)
{
    if (UNLIKELY(JSImmediate::isImmediate(this))) {
        JSImmediate::toObject(this, exec)->put(exec, propertyName, value, slot);
        return;
    }
    asCell()->put(exec, propertyName, value, slot);
}

inline void JSValue::put(ExecState* exec, unsigned propertyName, JSValue* value)
{
    if (UNLIKELY(JSImmediate::isImmediate(this))) {
        JSImmediate::toObject(this, exec)->put(exec, propertyName, value);
        return;
    }
    asCell()->put(exec, propertyName, value);
}

} // namespace KJS

#endif // JSObject_h
