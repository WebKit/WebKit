/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef JSCell_h
#define JSCell_h

#include "Collector.h"
#include "JSImmediate.h"
#include "JSValue.h"
#include "MarkStack.h"
#include "Structure.h"
#include <wtf/Noncopyable.h>

namespace JSC {

    class JSCell : public NoncopyableCustomAllocated {
        friend class GetterSetter;
        friend class Heap;
        friend class JIT;
        friend class JSNumberCell;
        friend class JSObject;
        friend class JSPropertyNameIterator;
        friend class JSString;
        friend class JSValue;
        friend class JSAPIValueWrapper;
        friend struct VPtrSet;

    private:
        explicit JSCell(Structure*);
        JSCell(); // Only used for initializing Collector blocks.
        virtual ~JSCell();

    public:
        // Querying the type.
#if USE(JSVALUE32)
        bool isNumber() const;
#endif
        bool isString() const;
        bool isObject() const;
        virtual bool isGetterSetter() const;
        bool inherits(const ClassInfo*) const;
        virtual bool isAPIValueWrapper() const { return false; }

        Structure* structure() const;

        // Extracting the value.
        bool getString(UString&) const;
        UString getString() const; // null string if not a string
        JSObject* getObject(); // NULL if not an object
        const JSObject* getObject() const; // NULL if not an object
        
        virtual CallType getCallData(CallData&);
        virtual ConstructType getConstructData(ConstructData&);

        // Extracting integer values.
        // FIXME: remove these methods, can check isNumberCell in JSValue && then call asNumberCell::*.
        virtual bool getUInt32(uint32_t&) const;

        // Basic conversions.
        virtual JSValue toPrimitive(ExecState*, PreferredPrimitiveType) const;
        virtual bool getPrimitiveNumber(ExecState*, double& number, JSValue&);
        virtual bool toBoolean(ExecState*) const;
        virtual double toNumber(ExecState*) const;
        virtual UString toString(ExecState*) const;
        virtual JSObject* toObject(ExecState*) const;

        // Garbage collection.
        void* operator new(size_t, ExecState*);
        void* operator new(size_t, JSGlobalData*);
        void* operator new(size_t, void* placementNewDestination) { return placementNewDestination; }

        virtual void markChildren(MarkStack&);

        // Object operations, with the toObject operation included.
        virtual const ClassInfo* classInfo() const;
        virtual void put(ExecState*, const Identifier& propertyName, JSValue, PutPropertySlot&);
        virtual void put(ExecState*, unsigned propertyName, JSValue);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
        virtual bool deleteProperty(ExecState*, unsigned propertyName);

        virtual JSObject* toThisObject(ExecState*) const;
        virtual UString toThisString(ExecState*) const;
        virtual JSString* toThisJSString(ExecState*);
        virtual JSValue getJSNumber();
        void* vptr() { return *reinterpret_cast<void**>(this); }

    private:
        // Base implementation; for non-object classes implements getPropertySlot.
        bool fastGetOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
        
        Structure* m_structure;
    };

    inline JSCell::JSCell(Structure* structure)
        : m_structure(structure)
    {
    }

    // Only used for initializing Collector blocks.
    inline JSCell::JSCell()
    {
    }

    inline JSCell::~JSCell()
    {
    }

#if USE(JSVALUE32)
    inline bool JSCell::isNumber() const
    {
        return Heap::isNumber(const_cast<JSCell*>(this));
    }
#endif

    inline bool JSCell::isObject() const
    {
        return m_structure->typeInfo().type() == ObjectType;
    }

    inline bool JSCell::isString() const
    {
        return m_structure->typeInfo().type() == StringType;
    }

    inline Structure* JSCell::structure() const
    {
        return m_structure;
    }

    inline void JSCell::markChildren(MarkStack&)
    {
    }

    inline void* JSCell::operator new(size_t size, JSGlobalData* globalData)
    {
        return globalData->heap.allocate(size);
    }

    // --- JSValue inlines ----------------------------

    inline bool JSValue::isString() const
    {
        return isCell() && asCell()->isString();
    }

    inline bool JSValue::isGetterSetter() const
    {
        return isCell() && asCell()->isGetterSetter();
    }

    inline bool JSValue::isObject() const
    {
        return isCell() && asCell()->isObject();
    }

    inline bool JSValue::getString(UString& s) const
    {
        return isCell() && asCell()->getString(s);
    }

    inline UString JSValue::getString() const
    {
        return isCell() ? asCell()->getString() : UString();
    }

    inline JSObject* JSValue::getObject() const
    {
        return isCell() ? asCell()->getObject() : 0;
    }

    inline CallType JSValue::getCallData(CallData& callData)
    {
        return isCell() ? asCell()->getCallData(callData) : CallTypeNone;
    }

    inline ConstructType JSValue::getConstructData(ConstructData& constructData)
    {
        return isCell() ? asCell()->getConstructData(constructData) : ConstructTypeNone;
    }

    ALWAYS_INLINE bool JSValue::getUInt32(uint32_t& v) const
    {
        if (isInt32()) {
            int32_t i = asInt32();
            v = static_cast<uint32_t>(i);
            return i >= 0;
        }
        if (isDouble()) {
            double d = asDouble();
            v = static_cast<uint32_t>(d);
            return v == d;
        }
        return false;
    }

#if !USE(JSVALUE32_64)
    ALWAYS_INLINE JSCell* JSValue::asCell() const
    {
        ASSERT(isCell());
        return m_ptr;
    }
#endif // !USE(JSVALUE32_64)

    inline JSValue JSValue::toPrimitive(ExecState* exec, PreferredPrimitiveType preferredType) const
    {
        return isCell() ? asCell()->toPrimitive(exec, preferredType) : asValue();
    }

    inline bool JSValue::getPrimitiveNumber(ExecState* exec, double& number, JSValue& value)
    {
        if (isInt32()) {
            number = asInt32();
            value = *this;
            return true;
        }
        if (isDouble()) {
            number = asDouble();
            value = *this;
            return true;
        }
        if (isCell())
            return asCell()->getPrimitiveNumber(exec, number, value);
        if (isTrue()) {
            number = 1.0;
            value = *this;
            return true;
        }
        if (isFalse() || isNull()) {
            number = 0.0;
            value = *this;
            return true;
        }
        ASSERT(isUndefined());
        number = nonInlineNaN();
        value = *this;
        return true;
    }

    inline bool JSValue::toBoolean(ExecState* exec) const
    {
        if (isInt32())
            return asInt32() != 0;
        if (isDouble())
            return asDouble() > 0.0 || asDouble() < 0.0; // false for NaN
        if (isCell())
            return asCell()->toBoolean(exec);
        return isTrue(); // false, null, and undefined all convert to false.
    }

    ALWAYS_INLINE double JSValue::toNumber(ExecState* exec) const
    {
        if (isInt32())
            return asInt32();
        if (isDouble())
            return asDouble();
        if (isCell())
            return asCell()->toNumber(exec);
        if (isTrue())
            return 1.0;
        return isUndefined() ? nonInlineNaN() : 0; // null and false both convert to 0.
    }

    inline bool JSValue::needsThisConversion() const
    {
        if (UNLIKELY(!isCell()))
            return true;
        return asCell()->structure()->typeInfo().needsThisConversion();
    }

    inline UString JSValue::toThisString(ExecState* exec) const
    {
        return isCell() ? asCell()->toThisString(exec) : toString(exec);
    }

    inline JSValue JSValue::getJSNumber()
    {
        if (isInt32() || isDouble())
            return *this;
        if (isCell())
            return asCell()->getJSNumber();
        return JSValue();
    }

    inline JSObject* JSValue::toObject(ExecState* exec) const
    {
        return isCell() ? asCell()->toObject(exec) : toObjectSlowCase(exec);
    }

    inline JSObject* JSValue::toThisObject(ExecState* exec) const
    {
        return isCell() ? asCell()->toThisObject(exec) : toThisObjectSlowCase(exec);
    }

    ALWAYS_INLINE void MarkStack::append(JSCell* cell)
    {
        ASSERT(!m_isCheckingForDefaultMarkViolation);
        ASSERT(cell);
        if (Heap::isCellMarked(cell))
            return;
        Heap::markCell(cell);
        if (cell->structure()->typeInfo().type() >= CompoundType)
            m_values.append(cell);
    }

    ALWAYS_INLINE void MarkStack::append(JSValue value)
    {
        ASSERT(value);
        if (value.isCell())
            append(value.asCell());
    }

    inline Heap* Heap::heap(JSValue v)
    {
        if (!v.isCell())
            return 0;
        return heap(v.asCell());
    }

    inline Heap* Heap::heap(JSCell* c)
    {
        return cellBlock(c)->heap;
    }

} // namespace JSC

#endif // JSCell_h
