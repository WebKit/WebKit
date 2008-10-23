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

namespace JSC {

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
        friend class CTI;
        friend class JSCell;

    public:
        explicit JSObject(PassRefPtr<StructureID>);

        virtual void mark();

        // The inline virtual destructor cannot be the first virtual function declared
        // in the class as it results in the vtable being generated as a weak symbol
        virtual ~JSObject();

        bool inherits(const ClassInfo* classInfo) const { return JSCell::isObject(classInfo); }

        JSValuePtr prototype() const;
        void setPrototype(JSValuePtr prototype);
        
        void setStructureID(PassRefPtr<StructureID>);
        StructureID* inheritorID();

        PropertyStorage& propertyStorage() { return m_propertyStorage; }

        virtual UString className() const;

        JSValuePtr get(ExecState*, const Identifier& propertyName) const;
        JSValuePtr get(ExecState*, unsigned propertyName) const;

        bool getPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        bool getPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

        virtual void put(ExecState*, const Identifier& propertyName, JSValuePtr value, PutPropertySlot&);
        virtual void put(ExecState*, unsigned propertyName, JSValuePtr value);

        virtual void putWithAttributes(ExecState*, const Identifier& propertyName, JSValuePtr value, unsigned attributes);
        virtual void putWithAttributes(ExecState*, unsigned propertyName, JSValuePtr value, unsigned attributes);

        bool propertyIsEnumerable(ExecState*, const Identifier& propertyName) const;

        bool hasProperty(ExecState*, const Identifier& propertyName) const;
        bool hasProperty(ExecState*, unsigned propertyName) const;
        bool hasOwnProperty(ExecState*, const Identifier& propertyName) const;

        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
        virtual bool deleteProperty(ExecState*, unsigned propertyName);

        virtual JSValuePtr defaultValue(ExecState*, PreferredPrimitiveType) const;

        virtual bool hasInstance(ExecState*, JSValuePtr, JSValuePtr prototypeProperty);

        virtual void getPropertyNames(ExecState*, PropertyNameArray&);

        virtual JSValuePtr toPrimitive(ExecState*, PreferredPrimitiveType = NoPreference) const;
        virtual bool getPrimitiveNumber(ExecState*, double& number, JSValuePtr& value);
        virtual bool toBoolean(ExecState*) const;
        virtual double toNumber(ExecState*) const;
        virtual UString toString(ExecState*) const;
        virtual JSObject* toObject(ExecState*) const;

        virtual JSObject* toThisObject(ExecState*) const;
        virtual JSGlobalObject* toGlobalObject(ExecState*) const;

        virtual bool getPropertyAttributes(ExecState*, const Identifier& propertyName, unsigned& attributes) const;

        // This get function only looks at the property map.
        JSValuePtr getDirect(const Identifier& propertyName) const
        {
            size_t offset = m_structureID->propertyMap().get(propertyName);
            return offset != WTF::notFound ? m_propertyStorage[offset] : noValue();
        }

        JSValuePtr* getDirectLocation(const Identifier& propertyName)
        {
            size_t offset = m_structureID->propertyMap().get(propertyName);
            return offset != WTF::notFound ? locationForOffset(offset) : 0;
        }

        JSValuePtr* getDirectLocation(const Identifier& propertyName, unsigned& attributes)
        {
            size_t offset = m_structureID->propertyMap().get(propertyName, attributes);
            return offset != WTF::notFound ? locationForOffset(offset) : 0;
        }

        size_t offsetForLocation(JSValuePtr* location)
        {
            return location - m_propertyStorage;
        }

        JSValuePtr* locationForOffset(size_t offset)
        {
            return &m_propertyStorage[offset];
        }

        void transitionTo(StructureID*);

        void removeDirect(const Identifier& propertyName);
        bool hasCustomProperties() { return !m_structureID->propertyMap().isEmpty(); }
        bool hasGetterSetterProperties() { return m_structureID->hasGetterSetterProperties(); }

        void putDirect(const Identifier& propertyName, JSValuePtr value, unsigned attr = 0);
        void putDirect(const Identifier& propertyName, JSValuePtr value, unsigned attr, bool checkReadOnly, PutPropertySlot& slot);
        void putDirectFunction(ExecState* exec, InternalFunction* function, unsigned attr = 0);
        void putDirectWithoutTransition(const Identifier& propertyName, JSValuePtr value, unsigned attr = 0);
        void putDirectFunctionWithoutTransition(ExecState* exec, InternalFunction* function, unsigned attr = 0);

        // Fast access to known property offsets.
        JSValuePtr getDirectOffset(size_t offset) { return m_propertyStorage[offset]; }
        void putDirectOffset(size_t offset, JSValuePtr value) { m_propertyStorage[offset] = value; }

        void fillGetterPropertySlot(PropertySlot&, JSValuePtr* location);

        virtual void defineGetter(ExecState*, const Identifier& propertyName, JSObject* getterFunction);
        virtual void defineSetter(ExecState*, const Identifier& propertyName, JSObject* setterFunction);
        virtual JSValuePtr lookupGetter(ExecState*, const Identifier& propertyName);
        virtual JSValuePtr lookupSetter(ExecState*, const Identifier& propertyName);

        virtual bool isGlobalObject() const { return false; }
        virtual bool isVariableObject() const { return false; }
        virtual bool isWatchdogException() const { return false; }
        virtual bool isNotAnObjectErrorStub() const { return false; }

        void allocatePropertyStorage(size_t oldSize, size_t newSize);
        void allocatePropertyStorageInline(size_t oldSize, size_t newSize);
        bool usingInlineStorage() const { return m_propertyStorage == m_inlineStorage; }

        static const size_t inlineStorageCapacity = 2;
        static const size_t nonInlineBaseStorageCapacity = 16;

        static PassRefPtr<StructureID> createStructureID(JSValuePtr prototype)
        {
            return StructureID::create(prototype, TypeInfo(ObjectType, HasStandardGetOwnPropertySlot));
        }

    protected:
        bool getOwnPropertySlotForWrite(ExecState*, const Identifier&, PropertySlot&, bool& slotIsWriteable);

    private:
        bool inlineGetOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);

        const HashEntry* findPropertyHashEntry(ExecState*, const Identifier& propertyName) const;
        StructureID* createInheritorID();

        RefPtr<StructureID> m_inheritorID;

        PropertyStorage m_propertyStorage;        
        JSValuePtr m_inlineStorage[inlineStorageCapacity];
    };

    JSObject* asObject(JSValuePtr);

    JSObject* constructEmptyObject(ExecState*);

inline JSObject* asObject(JSValuePtr value)
{
    ASSERT(asCell(value)->isObject());
    return static_cast<JSObject*>(asCell(value));
}

inline JSObject::JSObject(PassRefPtr<StructureID> structureID)
    : JSCell(structureID.releaseRef()) // ~JSObject balances this ref()
    , m_propertyStorage(m_inlineStorage)
{
    ASSERT(m_structureID);
    ASSERT(m_structureID->propertyStorageCapacity() == inlineStorageCapacity);
    ASSERT(m_structureID->propertyMap().isEmpty());
    ASSERT(prototype()->isNull() || Heap::heap(this) == Heap::heap(prototype()));
}

inline JSObject::~JSObject()
{
    ASSERT(m_structureID);
    if (m_propertyStorage != m_inlineStorage)
        delete [] m_propertyStorage;
    m_structureID->deref();
}

inline JSValuePtr JSObject::prototype() const
{
    return m_structureID->storedPrototype();
}

inline void JSObject::setPrototype(JSValuePtr prototype)
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
    return !JSImmediate::isImmediate(asValue()) && asCell()->isObject(classInfo);
}

ALWAYS_INLINE bool JSObject::inlineGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (JSValuePtr* location = getDirectLocation(propertyName)) {
        if (m_structureID->hasGetterSetterProperties() && location[0]->isGetterSetter())
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

ALWAYS_INLINE bool JSObject::getOwnPropertySlotForWrite(ExecState* exec, const Identifier& propertyName, PropertySlot& slot, bool& slotIsWriteable)
{
    unsigned attributes;
    if (JSValuePtr* location = getDirectLocation(propertyName, attributes)) {
        if (m_structureID->hasGetterSetterProperties() && location[0]->isGetterSetter()) {
            slotIsWriteable = false;
            fillGetterPropertySlot(slot, location);
        } else {
            slotIsWriteable = !(attributes & ReadOnly);
            slot.setValueSlot(this, location, offsetForLocation(location));
        }
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
    return inlineGetOwnPropertySlot(exec, propertyName, slot);
}

ALWAYS_INLINE bool JSCell::fastGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (structureID()->typeInfo().hasStandardGetOwnPropertySlot())
        return asObject(this)->inlineGetOwnPropertySlot(exec, propertyName, slot);
    return getOwnPropertySlot(exec, propertyName, slot);
}

// It may seem crazy to inline a function this large but it makes a big difference
// since this is function very hot in variable lookup
inline bool JSObject::getPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    JSObject* object = this;
    while (true) {
        if (object->fastGetOwnPropertySlot(exec, propertyName, slot))
            return true;
        JSValuePtr prototype = object->prototype();
        if (!prototype->isObject())
            return false;
        object = asObject(prototype);
    }
}

inline bool JSObject::getPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    JSObject* object = this;
    while (true) {
        if (object->getOwnPropertySlot(exec, propertyName, slot))
            return true;
        JSValuePtr prototype = object->prototype();
        if (!prototype->isObject())
            return false;
        object = asObject(prototype);
    }
}

inline JSValuePtr JSObject::get(ExecState* exec, const Identifier& propertyName) const
{
    PropertySlot slot(this);
    if (const_cast<JSObject*>(this)->getPropertySlot(exec, propertyName, slot))
        return slot.getValue(exec, propertyName);
    
    return jsUndefined();
}

inline JSValuePtr JSObject::get(ExecState* exec, unsigned propertyName) const
{
    PropertySlot slot(this);
    if (const_cast<JSObject*>(this)->getPropertySlot(exec, propertyName, slot))
        return slot.getValue(exec, propertyName);

    return jsUndefined();
}

inline void JSObject::putDirect(const Identifier& propertyName, JSValuePtr value, unsigned attr)
{
    PutPropertySlot slot;
    putDirect(propertyName, value, attr, false, slot);
}

inline void JSObject::putDirect(const Identifier& propertyName, JSValuePtr value, unsigned attributes, bool checkReadOnly, PutPropertySlot& slot)
{
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));

    if (m_structureID->isDictionary()) {
        unsigned currentAttributes;
        size_t offset = m_structureID->propertyMap().get(propertyName, currentAttributes);
        if (offset != WTF::notFound) {
            if (checkReadOnly && currentAttributes & ReadOnly)
                return;
            m_propertyStorage[offset] = value;
            slot.setExistingProperty(this, offset);
            return;
        }

        size_t currentCapacity = m_structureID->propertyStorageCapacity();
        offset = m_structureID->propertyMap().put(propertyName, attributes);
        if (m_structureID->propertyMap().storageSize() > m_structureID->propertyStorageCapacity()) {
            m_structureID->growPropertyStorageCapacity();
            allocatePropertyStorage(currentCapacity, m_structureID->propertyStorageCapacity());
        }

        m_propertyStorage[offset] = value;
        slot.setNewProperty(this, offset);
        m_structureID->clearEnumerationCache();
        return;
    }

    unsigned currentAttributes;
    size_t offset = m_structureID->propertyMap().get(propertyName, currentAttributes);
    if (offset != WTF::notFound) {
        if (checkReadOnly && currentAttributes & ReadOnly)
            return;
        m_propertyStorage[offset] = value;
        slot.setExistingProperty(this, offset);
        return;
    }

    size_t currentCapacity = m_structureID->propertyStorageCapacity();
    RefPtr<StructureID> structureID = StructureID::addPropertyTransition(m_structureID, propertyName, attributes, offset);
    if (currentCapacity != structureID->propertyStorageCapacity())
        allocatePropertyStorage(currentCapacity, structureID->propertyStorageCapacity());

    ASSERT(offset < structureID->propertyStorageCapacity());
    m_propertyStorage[offset] = value;
    slot.setNewProperty(this, offset);
    slot.setWasTransition(true);
    setStructureID(structureID.release());
}

inline void JSObject::putDirectWithoutTransition(const Identifier& propertyName, JSValuePtr value, unsigned attributes)
{
    size_t currentCapacity = m_structureID->propertyStorageCapacity();
    size_t offset = m_structureID->addPropertyWithoutTransition(propertyName, attributes);
    if (currentCapacity != m_structureID->propertyStorageCapacity())
        allocatePropertyStorage(currentCapacity, m_structureID->propertyStorageCapacity());
    m_propertyStorage[offset] = value;
}

inline void JSObject::transitionTo(StructureID* newStructureID)
{
    if (m_structureID->propertyStorageCapacity() != newStructureID->propertyStorageCapacity())
        allocatePropertyStorage(m_structureID->propertyStorageCapacity(), newStructureID->propertyStorageCapacity());
    setStructureID(newStructureID);
}

inline JSValuePtr JSObject::toPrimitive(ExecState* exec, PreferredPrimitiveType preferredType) const
{
    return defaultValue(exec, preferredType);
}

inline JSValuePtr JSValue::get(ExecState* exec, const Identifier& propertyName) const
{
    PropertySlot slot(this);
    return get(exec, propertyName, slot);
}

inline JSValuePtr JSValue::get(ExecState* exec, const Identifier& propertyName, PropertySlot& slot) const
{
    if (UNLIKELY(JSImmediate::isImmediate(asValue()))) {
        JSObject* prototype = JSImmediate::prototype(asValue(), exec);
        if (!prototype->getPropertySlot(exec, propertyName, slot))
            return jsUndefined();
        return slot.getValue(exec, propertyName);
    }
    JSCell* cell = asCell();
    while (true) {
        if (cell->fastGetOwnPropertySlot(exec, propertyName, slot))
            return slot.getValue(exec, propertyName);
        ASSERT(cell->isObject());
        JSValuePtr prototype = static_cast<JSObject*>(cell)->prototype();
        if (!prototype->isObject())
            return jsUndefined();
        cell = asObject(prototype);
    }
}

inline JSValuePtr JSValue::get(ExecState* exec, unsigned propertyName) const
{
    PropertySlot slot(this);
    return get(exec, propertyName, slot);
}

inline JSValuePtr JSValue::get(ExecState* exec, unsigned propertyName, PropertySlot& slot) const
{
    if (UNLIKELY(JSImmediate::isImmediate(asValue()))) {
        JSObject* prototype = JSImmediate::prototype(asValue(), exec);
        if (!prototype->getPropertySlot(exec, propertyName, slot))
            return jsUndefined();
        return slot.getValue(exec, propertyName);
    }
    JSCell* cell = const_cast<JSCell*>(asCell());
    while (true) {
        if (cell->getOwnPropertySlot(exec, propertyName, slot))
            return slot.getValue(exec, propertyName);
        ASSERT(cell->isObject());
        JSValuePtr prototype = static_cast<JSObject*>(cell)->prototype();
        if (!prototype->isObject())
            return jsUndefined();
        cell = prototype->asCell();
    }
}

inline void JSValue::put(ExecState* exec, const Identifier& propertyName, JSValuePtr value, PutPropertySlot& slot)
{
    if (UNLIKELY(JSImmediate::isImmediate(asValue()))) {
        JSImmediate::toObject(asValue(), exec)->put(exec, propertyName, value, slot);
        return;
    }
    asCell()->put(exec, propertyName, value, slot);
}

inline void JSValue::put(ExecState* exec, unsigned propertyName, JSValuePtr value)
{
    if (UNLIKELY(JSImmediate::isImmediate(asValue()))) {
        JSImmediate::toObject(asValue(), exec)->put(exec, propertyName, value);
        return;
    }
    asCell()->put(exec, propertyName, value);
}

ALWAYS_INLINE void JSObject::allocatePropertyStorageInline(size_t oldSize, size_t newSize)
{
    ASSERT(newSize > oldSize);

    JSValuePtr* oldPropertyStorage = m_propertyStorage;
    m_propertyStorage = new JSValuePtr[newSize];

    for (unsigned i = 0; i < oldSize; ++i)
        m_propertyStorage[i] = oldPropertyStorage[i];

    if (oldPropertyStorage != m_inlineStorage)
        delete [] oldPropertyStorage;
}

} // namespace JSC

#endif // JSObject_h
