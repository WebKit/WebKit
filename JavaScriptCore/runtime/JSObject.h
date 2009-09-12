/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "JSCell.h"
#include "JSNumberCell.h"
#include "MarkStack.h"
#include "PropertySlot.h"
#include "PutPropertySlot.h"
#include "ScopeChain.h"
#include "Structure.h"
#include "JSGlobalData.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

    inline JSCell* getJSFunction(JSGlobalData& globalData, JSValue value)
    {
        if (value.isCell() && (value.asCell()->vptr() == globalData.jsFunctionVPtr))
            return value.asCell();
        return 0;
    }
    
    class HashEntry;
    class InternalFunction;
    class PropertyDescriptor;
    class PropertyNameArray;
    class Structure;
    struct HashTable;

    // ECMA 262-3 8.6.1
    // Property attributes
    enum Attribute {
        None         = 0,
        ReadOnly     = 1 << 1,  // property can be only read, not written
        DontEnum     = 1 << 2,  // property doesn't appear in (for .. in ..)
        DontDelete   = 1 << 3,  // property can't be deleted
        Function     = 1 << 4,  // property is a function - only used by static hashtables
        Getter       = 1 << 5,  // property is a getter
        Setter       = 1 << 6   // property is a setter
    };

    typedef EncodedJSValue* PropertyStorage;
    typedef const EncodedJSValue* ConstPropertyStorage;

    class JSObject : public JSCell {
        friend class BatchedTransitionOptimizer;
        friend class JIT;
        friend class JSCell;

    public:
        explicit JSObject(PassRefPtr<Structure>);

        virtual void markChildren(MarkStack&);
        ALWAYS_INLINE void markChildrenDirect(MarkStack& markStack);

        // The inline virtual destructor cannot be the first virtual function declared
        // in the class as it results in the vtable being generated as a weak symbol
        virtual ~JSObject();

        JSValue prototype() const;
        void setPrototype(JSValue prototype);
        
        void setStructure(PassRefPtr<Structure>);
        Structure* inheritorID();

        virtual UString className() const;

        JSValue get(ExecState*, const Identifier& propertyName) const;
        JSValue get(ExecState*, unsigned propertyName) const;

        bool getPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        bool getPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
        bool getPropertyDescriptor(ExecState*, const Identifier& propertyName, PropertyDescriptor&);

        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
        virtual bool getOwnPropertyDescriptor(ExecState*, const Identifier&, PropertyDescriptor&);

        virtual void put(ExecState*, const Identifier& propertyName, JSValue value, PutPropertySlot&);
        virtual void put(ExecState*, unsigned propertyName, JSValue value);

        virtual void putWithAttributes(ExecState*, const Identifier& propertyName, JSValue value, unsigned attributes, bool checkReadOnly, PutPropertySlot& slot);
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
        bool getPropertySpecificValue(ExecState* exec, const Identifier& propertyName, JSCell*& specificFunction) const;

        // This get function only looks at the property map.
        JSValue getDirect(const Identifier& propertyName) const
        {
            size_t offset = m_structure->get(propertyName);
            return offset != WTF::notFound ? getDirectOffset(offset) : JSValue();
        }

        JSValue* getDirectLocation(const Identifier& propertyName)
        {
            size_t offset = m_structure->get(propertyName);
            return offset != WTF::notFound ? locationForOffset(offset) : 0;
        }

        JSValue* getDirectLocation(const Identifier& propertyName, unsigned& attributes)
        {
            JSCell* specificFunction;
            size_t offset = m_structure->get(propertyName, attributes, specificFunction);
            return offset != WTF::notFound ? locationForOffset(offset) : 0;
        }

        size_t offsetForLocation(JSValue* location) const
        {
            return location - reinterpret_cast<const JSValue*>(propertyStorage());
        }

        void transitionTo(Structure*);

        void removeDirect(const Identifier& propertyName);
        bool hasCustomProperties() { return !m_structure->isEmpty(); }
        bool hasGetterSetterProperties() { return m_structure->hasGetterSetterProperties(); }

        void putDirect(const Identifier& propertyName, JSValue value, unsigned attr, bool checkReadOnly, PutPropertySlot& slot);
        void putDirect(const Identifier& propertyName, JSValue value, unsigned attr = 0);

        void putDirectFunction(const Identifier& propertyName, JSCell* value, unsigned attr = 0);
        void putDirectFunction(const Identifier& propertyName, JSCell* value, unsigned attr, bool checkReadOnly, PutPropertySlot& slot);
        void putDirectFunction(ExecState* exec, InternalFunction* function, unsigned attr = 0);

        void putDirectWithoutTransition(const Identifier& propertyName, JSValue value, unsigned attr = 0);
        void putDirectFunctionWithoutTransition(const Identifier& propertyName, JSCell* value, unsigned attr = 0);
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

        static const size_t inlineStorageCapacity = sizeof(EncodedJSValue) == 2 * sizeof(void*) ? 4 : 3;
        static const size_t nonInlineBaseStorageCapacity = 16;

        static PassRefPtr<Structure> createStructure(JSValue prototype)
        {
            return Structure::create(prototype, TypeInfo(ObjectType, HasStandardGetOwnPropertySlot | HasDefaultMark | HasDefaultGetPropertyNames));
        }

    private:
        // Nobody should ever ask any of these questions on something already known to be a JSObject.
        using JSCell::isAPIValueWrapper;
        using JSCell::isGetterSetter;
        using JSCell::toObject;
        void getObject();
        void getString();
        void isObject();
        void isString();
#if USE(JSVALUE32)
        void isNumber();
#endif

        ConstPropertyStorage propertyStorage() const { return (isUsingInlineStorage() ? m_inlineStorage : m_externalStorage); }
        PropertyStorage propertyStorage() { return (isUsingInlineStorage() ? m_inlineStorage : m_externalStorage); }

        const JSValue* locationForOffset(size_t offset) const
        {
            return reinterpret_cast<const JSValue*>(&propertyStorage()[offset]);
        }

        JSValue* locationForOffset(size_t offset)
        {
            return reinterpret_cast<JSValue*>(&propertyStorage()[offset]);
        }

        void putDirectInternal(const Identifier& propertyName, JSValue value, unsigned attr, bool checkReadOnly, PutPropertySlot& slot, JSCell*);
        void putDirectInternal(JSGlobalData&, const Identifier& propertyName, JSValue value, unsigned attr, bool checkReadOnly, PutPropertySlot& slot);
        void putDirectInternal(JSGlobalData&, const Identifier& propertyName, JSValue value, unsigned attr = 0);

        bool inlineGetOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);

        const HashEntry* findPropertyHashEntry(ExecState*, const Identifier& propertyName) const;
        Structure* createInheritorID();

        union {
            PropertyStorage m_externalStorage;
            EncodedJSValue m_inlineStorage[inlineStorageCapacity];
        };

        RefPtr<Structure> m_inheritorID;
    };
    
JSObject* constructEmptyObject(ExecState*);

inline JSObject* asObject(JSCell* cell)
{
    ASSERT(cell->isObject());
    return static_cast<JSObject*>(cell);
}

inline JSObject* asObject(JSValue value)
{
    return asObject(value.asCell());
}

inline JSObject::JSObject(PassRefPtr<Structure> structure)
    : JSCell(structure.releaseRef()) // ~JSObject balances this ref()
{
    ASSERT(m_structure);
    ASSERT(m_structure->propertyStorageCapacity() == inlineStorageCapacity);
    ASSERT(m_structure->isEmpty());
    ASSERT(prototype().isNull() || Heap::heap(this) == Heap::heap(prototype()));
#if USE(JSVALUE64) || USE(JSVALUE32_64)
    ASSERT(OBJECT_OFFSETOF(JSObject, m_inlineStorage) % sizeof(double) == 0);
#endif
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

inline bool JSCell::inherits(const ClassInfo* info) const
{
    for (const ClassInfo* ci = classInfo(); ci; ci = ci->parentClass) {
        if (ci == info)
            return true;
    }
    return false;
}

// this method is here to be after the inline declaration of JSCell::inherits
inline bool JSValue::inherits(const ClassInfo* classInfo) const
{
    return isCell() && asCell()->inherits(classInfo);
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

inline void JSObject::putDirectInternal(const Identifier& propertyName, JSValue value, unsigned attributes, bool checkReadOnly, PutPropertySlot& slot, JSCell* specificFunction)
{
    ASSERT(value);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));

    if (m_structure->isDictionary()) {
        unsigned currentAttributes;
        JSCell* currentSpecificFunction;
        size_t offset = m_structure->get(propertyName, currentAttributes, currentSpecificFunction);
        if (offset != WTF::notFound) {
            if (currentSpecificFunction && (specificFunction != currentSpecificFunction))
                m_structure->despecifyDictionaryFunction(propertyName);
            if (checkReadOnly && currentAttributes & ReadOnly)
                return;
            putDirectOffset(offset, value);
            if (!specificFunction && !currentSpecificFunction)
                slot.setExistingProperty(this, offset);
            return;
        }

        size_t currentCapacity = m_structure->propertyStorageCapacity();
        offset = m_structure->addPropertyWithoutTransition(propertyName, attributes, specificFunction);
        if (currentCapacity != m_structure->propertyStorageCapacity())
            allocatePropertyStorage(currentCapacity, m_structure->propertyStorageCapacity());

        ASSERT(offset < m_structure->propertyStorageCapacity());
        putDirectOffset(offset, value);
        // See comment on setNewProperty call below.
        if (!specificFunction)
            slot.setNewProperty(this, offset);
        return;
    }

    size_t offset;
    size_t currentCapacity = m_structure->propertyStorageCapacity();
    if (RefPtr<Structure> structure = Structure::addPropertyTransitionToExistingStructure(m_structure, propertyName, attributes, specificFunction, offset)) {    
        if (currentCapacity != structure->propertyStorageCapacity())
            allocatePropertyStorage(currentCapacity, structure->propertyStorageCapacity());

        ASSERT(offset < structure->propertyStorageCapacity());
        setStructure(structure.release());
        putDirectOffset(offset, value);
        // See comment on setNewProperty call below.
        if (!specificFunction)
            slot.setNewProperty(this, offset);
        return;
    }

    unsigned currentAttributes;
    JSCell* currentSpecificFunction;
    offset = m_structure->get(propertyName, currentAttributes, currentSpecificFunction);
    if (offset != WTF::notFound) {
        if (checkReadOnly && currentAttributes & ReadOnly)
            return;

        if (currentSpecificFunction && (specificFunction != currentSpecificFunction)) {
            setStructure(Structure::despecifyFunctionTransition(m_structure, propertyName));
            putDirectOffset(offset, value);
            // Function transitions are not currently cachable, so leave the slot in an uncachable state.
            return;
        }
        putDirectOffset(offset, value);
        slot.setExistingProperty(this, offset);
        return;
    }

    // If we have a specific function, we may have got to this point if there is
    // already a transition with the correct property name and attributes, but
    // specialized to a different function.  In this case we just want to give up
    // and despecialize the transition.
    // In this case we clear the value of specificFunction which will result
    // in us adding a non-specific transition, and any subsequent lookup in
    // Structure::addPropertyTransitionToExistingStructure will just use that.
    if (specificFunction && m_structure->hasTransition(propertyName, attributes))
        specificFunction = 0;

    RefPtr<Structure> structure = Structure::addPropertyTransition(m_structure, propertyName, attributes, specificFunction, offset);

    if (currentCapacity != structure->propertyStorageCapacity())
        allocatePropertyStorage(currentCapacity, structure->propertyStorageCapacity());

    ASSERT(offset < structure->propertyStorageCapacity());
    setStructure(structure.release());
    putDirectOffset(offset, value);
    // Function transitions are not currently cachable, so leave the slot in an uncachable state.
    if (!specificFunction)
        slot.setNewProperty(this, offset);
}

inline void JSObject::putDirectInternal(JSGlobalData& globalData, const Identifier& propertyName, JSValue value, unsigned attributes, bool checkReadOnly, PutPropertySlot& slot)
{
    ASSERT(value);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));

    putDirectInternal(propertyName, value, attributes, checkReadOnly, slot, getJSFunction(globalData, value));
}

inline void JSObject::putDirectInternal(JSGlobalData& globalData, const Identifier& propertyName, JSValue value, unsigned attributes)
{
    PutPropertySlot slot;
    putDirectInternal(propertyName, value, attributes, false, slot, getJSFunction(globalData, value));
}

inline void JSObject::putDirect(const Identifier& propertyName, JSValue value, unsigned attributes, bool checkReadOnly, PutPropertySlot& slot)
{
    ASSERT(value);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));

    putDirectInternal(propertyName, value, attributes, checkReadOnly, slot, 0);
}

inline void JSObject::putDirect(const Identifier& propertyName, JSValue value, unsigned attributes)
{
    PutPropertySlot slot;
    putDirectInternal(propertyName, value, attributes, false, slot, 0);
}

inline void JSObject::putDirectFunction(const Identifier& propertyName, JSCell* value, unsigned attributes, bool checkReadOnly, PutPropertySlot& slot)
{
    putDirectInternal(propertyName, value, attributes, checkReadOnly, slot, value);
}

inline void JSObject::putDirectFunction(const Identifier& propertyName, JSCell* value, unsigned attr)
{
    PutPropertySlot slot;
    putDirectInternal(propertyName, value, attr, false, slot, value);
}

inline void JSObject::putDirectWithoutTransition(const Identifier& propertyName, JSValue value, unsigned attributes)
{
    size_t currentCapacity = m_structure->propertyStorageCapacity();
    size_t offset = m_structure->addPropertyWithoutTransition(propertyName, attributes, 0);
    if (currentCapacity != m_structure->propertyStorageCapacity())
        allocatePropertyStorage(currentCapacity, m_structure->propertyStorageCapacity());
    putDirectOffset(offset, value);
}

inline void JSObject::putDirectFunctionWithoutTransition(const Identifier& propertyName, JSCell* value, unsigned attributes)
{
    size_t currentCapacity = m_structure->propertyStorageCapacity();
    size_t offset = m_structure->addPropertyWithoutTransition(propertyName, attributes, value);
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
        JSObject* prototype = synthesizePrototype(exec);
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
        JSValue prototype = asObject(cell)->prototype();
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
        JSObject* prototype = synthesizePrototype(exec);
        if (!prototype->getPropertySlot(exec, propertyName, slot))
            return jsUndefined();
        return slot.getValue(exec, propertyName);
    }
    JSCell* cell = const_cast<JSCell*>(asCell());
    while (true) {
        if (cell->getOwnPropertySlot(exec, propertyName, slot))
            return slot.getValue(exec, propertyName);
        JSValue prototype = asObject(cell)->prototype();
        if (!prototype.isObject())
            return jsUndefined();
        cell = prototype.asCell();
    }
}

inline void JSValue::put(ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    if (UNLIKELY(!isCell())) {
        synthesizeObject(exec)->put(exec, propertyName, value, slot);
        return;
    }
    asCell()->put(exec, propertyName, value, slot);
}

inline void JSValue::put(ExecState* exec, unsigned propertyName, JSValue value)
{
    if (UNLIKELY(!isCell())) {
        synthesizeObject(exec)->put(exec, propertyName, value);
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

ALWAYS_INLINE void JSObject::markChildrenDirect(MarkStack& markStack)
{
    JSCell::markChildren(markStack);

    m_structure->markAggregate(markStack);
    
    PropertyStorage storage = propertyStorage();
    size_t storageSize = m_structure->propertyStorageSize();
    markStack.appendValues(reinterpret_cast<JSValue*>(storage), storageSize);
}

} // namespace JSC

#endif // JSObject_h
