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
#include "CallFrame.h"
#include "JSNumberCell.h"
#include "PropertySlot.h"
#include "PutPropertySlot.h"
#include "ScopeChain.h"
#include "Structure.h"

namespace JSC {

    class InternalFunction;
    class PropertyNameArray;
    class Structure;
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

    typedef EncodedJSValue* PropertyStorage;
    typedef const EncodedJSValue* ConstPropertyStorage;

    class JSObject : public JSCell {
        friend class BatchedTransitionOptimizer;
        friend class JIT;
        friend class JSCell;

    public:
        explicit JSObject(PassRefPtr<Structure>);

        virtual void mark();

        // The inline virtual destructor cannot be the first virtual function declared
        // in the class as it results in the vtable being generated as a weak symbol
        virtual ~JSObject();

        bool inherits(const ClassInfo* classInfo) const { return JSCell::isObject(classInfo); }

        JSValue prototype() const;
        void setPrototype(JSValue prototype);
        
        void setStructure(PassRefPtr<Structure>);
        Structure* inheritorID();

        ConstPropertyStorage propertyStorage() const { return (isUsingInlineStorage() ? m_inlineStorage : m_externalStorage); }
        PropertyStorage propertyStorage() { return (isUsingInlineStorage() ? m_inlineStorage : m_externalStorage); }

        virtual UString className() const;

        JSValue get(ExecState*, const Identifier& propertyName) const;
        JSValue get(ExecState*, unsigned propertyName) const;

        bool getPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        bool getPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

        virtual void put(ExecState*, const Identifier& propertyName, JSValue value, PutPropertySlot&);
        virtual void put(ExecState*, unsigned propertyName, JSValue value);

        virtual void putWithAttributes(ExecState*, const Identifier& propertyName, JSValue value, unsigned attributes);
        virtual void putWithAttributes(ExecState*, unsigned propertyName, JSValue value, unsigned attributes);

        bool propertyIsEnumerable(ExecState*, const Identifier& propertyName) const;

        bool hasProperty(ExecState*, const Identifier& propertyName) const;
        bool hasProperty(ExecState*, unsigned propertyName) const;
        bool hasOwnProperty(ExecState*, const Identifier& propertyName) const;

        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
        virtual bool deleteProperty(ExecState*, unsigned propertyName);

        virtual JSValue defaultValue(ExecState*, PreferredPrimitiveType) const;

        virtual bool hasInstance(ExecState*, JSValue, JSValue prototypeProperty);

        virtual void getPropertyNames(ExecState*, PropertyNameArray&);

        virtual JSValue toPrimitive(ExecState*, PreferredPrimitiveType = NoPreference) const;
        virtual bool getPrimitiveNumber(ExecState*, double& number, JSValue& value);
        virtual bool toBoolean(ExecState*) const;
        virtual double toNumber(ExecState*) const;
        virtual UString toString(ExecState*) const;
        virtual JSObject* toObject(ExecState*) const;

        virtual JSObject* toThisObject(ExecState*) const;
        virtual JSObject* unwrappedObject();

        virtual bool getPropertyAttributes(ExecState*, const Identifier& propertyName, unsigned& attributes) const;

        // This get function only looks at the property map.
        JSValue getDirect(const Identifier& propertyName) const
        {
            size_t offset = m_structure->get(propertyName);
            return offset != WTF::notFound ? getDirectOffset(offset) : JSValue();
        }

        size_t getOffset(const Identifier& propertyName)
        {
            return m_structure->get(propertyName);
        }

        JSValue* getDirectLocation(const Identifier& propertyName)
        {
            size_t offset = m_structure->get(propertyName);
            return offset != WTF::notFound ? locationForOffset(offset) : 0;
        }

        JSValue* getDirectLocation(const Identifier& propertyName, unsigned& attributes)
        {
            size_t offset = m_structure->get(propertyName, attributes);
            return offset != WTF::notFound ? locationForOffset(offset) : 0;
        }

        size_t offsetForLocation(JSValue* location) const
        {
            return location - reinterpret_cast<const JSValue*>(propertyStorage());
        }

        const JSValue* locationForOffset(size_t offset) const
        {
            return reinterpret_cast<const JSValue*>(&propertyStorage()[offset]);
        }

        JSValue* locationForOffset(size_t offset)
        {
            return reinterpret_cast<JSValue*>(&propertyStorage()[offset]);
        }

        void transitionTo(Structure*);

        void removeDirect(const Identifier& propertyName);
        bool hasCustomProperties() { return !m_structure->isEmpty(); }
        bool hasGetterSetterProperties() { return m_structure->hasGetterSetterProperties(); }

        void putDirect(const Identifier& propertyName, JSValue value, unsigned attr = 0);
        void putDirect(const Identifier& propertyName, JSValue value, unsigned attr, bool checkReadOnly, PutPropertySlot& slot);
        void putDirectFunction(ExecState* exec, InternalFunction* function, unsigned attr = 0);
        void putDirectWithoutTransition(const Identifier& propertyName, JSValue value, unsigned attr = 0);
        void putDirectFunctionWithoutTransition(ExecState* exec, InternalFunction* function, unsigned attr = 0);

        // Fast access to known property offsets.
        JSValue getDirectOffset(size_t offset) const { return JSValue::decode(propertyStorage()[offset]); }
        void putDirectOffset(size_t offset, JSValue value) { propertyStorage()[offset] = JSValue::encode(value); }

        void fillGetterPropertySlot(PropertySlot&, JSValue* location);

        virtual void defineGetter(ExecState*, const Identifier& propertyName, JSObject* getterFunction);
        virtual void defineSetter(ExecState*, const Identifier& propertyName, JSObject* setterFunction);
        virtual JSValue lookupGetter(ExecState*, const Identifier& propertyName);
        virtual JSValue lookupSetter(ExecState*, const Identifier& propertyName);

        virtual bool isGlobalObject() const { return false; }
        virtual bool isVariableObject() const { return false; }
        virtual bool isActivationObject() const { return false; }
        virtual bool isWatchdogException() const { return false; }
        virtual bool isNotAnObjectErrorStub() const { return false; }

        void allocatePropertyStorage(size_t oldSize, size_t newSize);
        void allocatePropertyStorageInline(size_t oldSize, size_t newSize);
        bool isUsingInlineStorage() const { return m_structure->isUsingInlineStorage(); }

        static const size_t inlineStorageCapacity = 3;
        static const size_t nonInlineBaseStorageCapacity = 16;

        static PassRefPtr<Structure> createStructure(JSValue prototype)
        {
            return Structure::create(prototype, TypeInfo(ObjectType, HasStandardGetOwnPropertySlot));
        }

    protected:
        bool getOwnPropertySlotForWrite(ExecState*, const Identifier&, PropertySlot&, bool& slotIsWriteable);

    private:
        bool inlineGetOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);

        const HashEntry* findPropertyHashEntry(ExecState*, const Identifier& propertyName) const;
        Structure* createInheritorID();

        RefPtr<Structure> m_inheritorID;

        union {
            PropertyStorage m_externalStorage;
            EncodedJSValue m_inlineStorage[inlineStorageCapacity];
        };
    };

    JSObject* asObject(JSValue);

    JSObject* constructEmptyObject(ExecState*);

inline JSObject* asObject(JSValue value)
{
    ASSERT(asCell(value)->isObject());
    return static_cast<JSObject*>(asCell(value));
}

inline JSObject::JSObject(PassRefPtr<Structure> structure)
    : JSCell(structure.releaseRef()) // ~JSObject balances this ref()
{
    ASSERT(m_structure);
    ASSERT(m_structure->propertyStorageCapacity() == inlineStorageCapacity);
    ASSERT(m_structure->isEmpty());
    ASSERT(prototype().isNull() || Heap::heap(this) == Heap::heap(prototype()));
}

inline JSObject::~JSObject()
{
    ASSERT(m_structure);
    if (!isUsingInlineStorage())
        delete [] m_externalStorage;
    m_structure->deref();
}

inline JSValue JSObject::prototype() const
{
    return m_structure->storedPrototype();
}

inline void JSObject::setPrototype(JSValue prototype)
{
    ASSERT(prototype);
    RefPtr<Structure> newStructure = Structure::changePrototypeTransition(m_structure, prototype);
    setStructure(newStructure.release());
}

inline void JSObject::setStructure(PassRefPtr<Structure> structure)
{
    m_structure->deref();
    m_structure = structure.releaseRef(); // ~JSObject balances this ref()
}

inline Structure* JSObject::inheritorID()
{
    if (m_inheritorID)
        return m_inheritorID.get();
    return createInheritorID();
}

inline bool Structure::isUsingInlineStorage() const
{
    return (propertyStorageCapacity() == JSObject::inlineStorageCapacity);
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
    return isCell() && asCell()->isObject(classInfo);
}

ALWAYS_INLINE bool JSObject::inlineGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (JSValue* location = getDirectLocation(propertyName)) {
        if (m_structure->hasGetterSetterProperties() && location[0].isGetterSetter())
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
    if (JSValue* location = getDirectLocation(propertyName, attributes)) {
        if (m_structure->hasGetterSetterProperties() && location[0].isGetterSetter()) {
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
    if (structure()->typeInfo().hasStandardGetOwnPropertySlot())
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
        JSValue prototype = object->prototype();
        if (!prototype.isObject())
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
        JSValue prototype = object->prototype();
        if (!prototype.isObject())
            return false;
        object = asObject(prototype);
    }
}

inline JSValue JSObject::get(ExecState* exec, const Identifier& propertyName) const
{
    PropertySlot slot(this);
    if (const_cast<JSObject*>(this)->getPropertySlot(exec, propertyName, slot))
        return slot.getValue(exec, propertyName);
    
    return jsUndefined();
}

inline JSValue JSObject::get(ExecState* exec, unsigned propertyName) const
{
    PropertySlot slot(this);
    if (const_cast<JSObject*>(this)->getPropertySlot(exec, propertyName, slot))
        return slot.getValue(exec, propertyName);

    return jsUndefined();
}

inline void JSObject::putDirect(const Identifier& propertyName, JSValue value, unsigned attr)
{
    PutPropertySlot slot;
    putDirect(propertyName, value, attr, false, slot);
}

inline void JSObject::putDirect(const Identifier& propertyName, JSValue value, unsigned attributes, bool checkReadOnly, PutPropertySlot& slot)
{
    ASSERT(value);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));

    if (m_structure->isDictionary()) {
        unsigned currentAttributes;
        size_t offset = m_structure->get(propertyName, currentAttributes);
        if (offset != WTF::notFound) {
            if (checkReadOnly && currentAttributes & ReadOnly)
                return;
            putDirectOffset(offset, value);
            slot.setExistingProperty(this, offset);
            return;
        }

        size_t currentCapacity = m_structure->propertyStorageCapacity();
        offset = m_structure->addPropertyWithoutTransition(propertyName, attributes);
        if (currentCapacity != m_structure->propertyStorageCapacity())
            allocatePropertyStorage(currentCapacity, m_structure->propertyStorageCapacity());

        ASSERT(offset < m_structure->propertyStorageCapacity());
        putDirectOffset(offset, value);
        slot.setNewProperty(this, offset);
        return;
    }

    size_t offset;
    size_t currentCapacity = m_structure->propertyStorageCapacity();
    if (RefPtr<Structure> structure = Structure::addPropertyTransitionToExistingStructure(m_structure, propertyName, attributes, offset)) {
        if (currentCapacity != structure->propertyStorageCapacity())
            allocatePropertyStorage(currentCapacity, structure->propertyStorageCapacity());

        ASSERT(offset < structure->propertyStorageCapacity());
        setStructure(structure.release());
        putDirectOffset(offset, value);
        slot.setNewProperty(this, offset);
        slot.setWasTransition(true);
        return;
    }

    unsigned currentAttributes;
    offset = m_structure->get(propertyName, currentAttributes);
    if (offset != WTF::notFound) {
        if (checkReadOnly && currentAttributes & ReadOnly)
            return;
        putDirectOffset(offset, value);
        slot.setExistingProperty(this, offset);
        return;
    }

    RefPtr<Structure> structure = Structure::addPropertyTransition(m_structure, propertyName, attributes, offset);
    if (currentCapacity != structure->propertyStorageCapacity())
        allocatePropertyStorage(currentCapacity, structure->propertyStorageCapacity());

    ASSERT(offset < structure->propertyStorageCapacity());
    setStructure(structure.release());
    putDirectOffset(offset, value);
    slot.setNewProperty(this, offset);
    slot.setWasTransition(true);
}

inline void JSObject::putDirectWithoutTransition(const Identifier& propertyName, JSValue value, unsigned attributes)
{
    size_t currentCapacity = m_structure->propertyStorageCapacity();
    size_t offset = m_structure->addPropertyWithoutTransition(propertyName, attributes);
    if (currentCapacity != m_structure->propertyStorageCapacity())
        allocatePropertyStorage(currentCapacity, m_structure->propertyStorageCapacity());
    putDirectOffset(offset, value);
}

inline void JSObject::transitionTo(Structure* newStructure)
{
    if (m_structure->propertyStorageCapacity() != newStructure->propertyStorageCapacity())
        allocatePropertyStorage(m_structure->propertyStorageCapacity(), newStructure->propertyStorageCapacity());
    setStructure(newStructure);
}

inline JSValue JSObject::toPrimitive(ExecState* exec, PreferredPrimitiveType preferredType) const
{
    return defaultValue(exec, preferredType);
}

inline JSValue JSValue::get(ExecState* exec, const Identifier& propertyName) const
{
    PropertySlot slot(asValue());
    return get(exec, propertyName, slot);
}

inline JSValue JSValue::get(ExecState* exec, const Identifier& propertyName, PropertySlot& slot) const
{
    if (UNLIKELY(!isCell())) {
        JSObject* prototype = JSImmediate::prototype(asValue(), exec);
        if (propertyName == exec->propertyNames().underscoreProto)
            return prototype;
        if (!prototype->getPropertySlot(exec, propertyName, slot))
            return jsUndefined();
        return slot.getValue(exec, propertyName);
    }
    JSCell* cell = asCell();
    while (true) {
        if (cell->fastGetOwnPropertySlot(exec, propertyName, slot))
            return slot.getValue(exec, propertyName);
        ASSERT(cell->isObject());
        JSValue prototype = static_cast<JSObject*>(cell)->prototype();
        if (!prototype.isObject())
            return jsUndefined();
        cell = asObject(prototype);
    }
}

inline JSValue JSValue::get(ExecState* exec, unsigned propertyName) const
{
    PropertySlot slot(asValue());
    return get(exec, propertyName, slot);
}

inline JSValue JSValue::get(ExecState* exec, unsigned propertyName, PropertySlot& slot) const
{
    if (UNLIKELY(!isCell())) {
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
        JSValue prototype = static_cast<JSObject*>(cell)->prototype();
        if (!prototype.isObject())
            return jsUndefined();
        cell = prototype.asCell();
    }
}

inline void JSValue::put(ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    if (UNLIKELY(!isCell())) {
        JSImmediate::toObject(asValue(), exec)->put(exec, propertyName, value, slot);
        return;
    }
    asCell()->put(exec, propertyName, value, slot);
}

inline void JSValue::put(ExecState* exec, unsigned propertyName, JSValue value)
{
    if (UNLIKELY(!isCell())) {
        JSImmediate::toObject(asValue(), exec)->put(exec, propertyName, value);
        return;
    }
    asCell()->put(exec, propertyName, value);
}

ALWAYS_INLINE void JSObject::allocatePropertyStorageInline(size_t oldSize, size_t newSize)
{
    ASSERT(newSize > oldSize);

    // It's important that this function not rely on m_structure, since
    // we might be in the middle of a transition.
    bool wasInline = (oldSize == JSObject::inlineStorageCapacity);

    PropertyStorage oldPropertyStorage = (wasInline ? m_inlineStorage : m_externalStorage);
    PropertyStorage newPropertyStorage = new EncodedJSValue[newSize];

    for (unsigned i = 0; i < oldSize; ++i)
       newPropertyStorage[i] = oldPropertyStorage[i];

    if (!wasInline)
        delete [] oldPropertyStorage;

    m_externalStorage = newPropertyStorage;
}

} // namespace JSC

#endif // JSObject_h
