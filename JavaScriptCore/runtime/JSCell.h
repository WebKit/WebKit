/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2007, 2008 Apple Inc. All rights reserved.
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

#include <wtf/Noncopyable.h>
#include "Structure.h"
#include "JSValue.h"
#include "JSImmediate.h"
#include "Collector.h"

namespace JSC {

    class JSCell : Noncopyable {
        friend class GetterSetter;
        friend class Heap;
        friend class JIT;
        friend class JSNumberCell;
        friend class JSObject;
        friend class JSPropertyNameIterator;
        friend class JSString;
        friend class JSValue;
        friend class VPtrSet;

    private:
        explicit JSCell(Structure*);
        virtual ~JSCell();

    public:
        // Querying the type.
        bool isNumber() const;
        bool isString() const;
        bool isObject() const;
        virtual bool isGetterSetter() const;
        virtual bool isObject(const ClassInfo*) const;

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
        virtual bool getTruncatedInt32(int32_t&) const;
        virtual bool getTruncatedUInt32(uint32_t&) const;

        // Basic conversions.
        virtual JSValue toPrimitive(ExecState*, PreferredPrimitiveType) const = 0;
        virtual bool getPrimitiveNumber(ExecState*, double& number, JSValue&) = 0;
        virtual bool toBoolean(ExecState*) const = 0;
        virtual double toNumber(ExecState*) const = 0;
        virtual UString toString(ExecState*) const = 0;
        virtual JSObject* toObject(ExecState*) const = 0;

        // Garbage collection.
        void* operator new(size_t, ExecState*);
        void* operator new(size_t, JSGlobalData*);
        void* operator new(size_t, void* placementNewDestination) { return placementNewDestination; }
        virtual void mark();
        bool marked() const;

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

    JSCell* asCell(JSValue);

    inline JSCell* asCell(JSValue value)
    {
        return value.asCell();
    }

    inline JSCell::JSCell(Structure* structure)
        : m_structure(structure)
    {
    }

    inline JSCell::~JSCell()
    {
    }

    inline bool JSCell::isNumber() const
    {
        return Heap::isNumber(const_cast<JSCell*>(this));
    }

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

    inline bool JSCell::marked() const
    {
        return Heap::isCellMarked(this);
    }

    inline void JSCell::mark()
    {
        return Heap::markCell(this);
    }

    ALWAYS_INLINE JSCell* JSValue::asCell() const
    {
        ASSERT(isCell());
        return m_ptr;
    }

    inline void* JSCell::operator new(size_t size, JSGlobalData* globalData)
    {
#ifdef JAVASCRIPTCORE_BUILDING_ALL_IN_ONE_FILE
        return globalData->heap.inlineAllocate(size);
#else
        return globalData->heap.allocate(size);
#endif
    }

    // --- JSValue inlines ----------------------------

    inline bool JSValue::isString() const
    {
        return !JSImmediate::isImmediate(asValue()) && asCell()->isString();
    }

    inline bool JSValue::isGetterSetter() const
    {
        return !JSImmediate::isImmediate(asValue()) && asCell()->isGetterSetter();
    }

    inline bool JSValue::isObject() const
    {
        return !JSImmediate::isImmediate(asValue()) && asCell()->isObject();
    }

    inline bool JSValue::getString(UString& s) const
    {
        return !JSImmediate::isImmediate(asValue()) && asCell()->getString(s);
    }

    inline UString JSValue::getString() const
    {
        return JSImmediate::isImmediate(asValue()) ? UString() : asCell()->getString();
    }

    inline JSObject* JSValue::getObject() const
    {
        return JSImmediate::isImmediate(asValue()) ? 0 : asCell()->getObject();
    }

    inline CallType JSValue::getCallData(CallData& callData)
    {
        return JSImmediate::isImmediate(asValue()) ? CallTypeNone : asCell()->getCallData(callData);
    }

    inline ConstructType JSValue::getConstructData(ConstructData& constructData)
    {
        return JSImmediate::isImmediate(asValue()) ? ConstructTypeNone : asCell()->getConstructData(constructData);
    }

    ALWAYS_INLINE bool JSValue::getUInt32(uint32_t& v) const
    {
        return JSImmediate::isImmediate(asValue()) ? JSImmediate::getUInt32(asValue(), v) : asCell()->getUInt32(v);
    }

    ALWAYS_INLINE bool JSValue::getTruncatedInt32(int32_t& v) const
    {
        return JSImmediate::isImmediate(asValue()) ? JSImmediate::getTruncatedInt32(asValue(), v) : asCell()->getTruncatedInt32(v);
    }

    inline bool JSValue::getTruncatedUInt32(uint32_t& v) const
    {
        return JSImmediate::isImmediate(asValue()) ? JSImmediate::getTruncatedUInt32(asValue(), v) : asCell()->getTruncatedUInt32(v);
    }

    inline void JSValue::mark()
    {
        asCell()->mark(); // callers should check !marked() before calling mark(), so this should only be called with cells
    }

    inline bool JSValue::marked() const
    {
        return JSImmediate::isImmediate(asValue()) || asCell()->marked();
    }

    inline JSValue JSValue::toPrimitive(ExecState* exec, PreferredPrimitiveType preferredType) const
    {
        return JSImmediate::isImmediate(asValue()) ? asValue() : asCell()->toPrimitive(exec, preferredType);
    }

    inline bool JSValue::getPrimitiveNumber(ExecState* exec, double& number, JSValue& value)
    {
        if (JSImmediate::isImmediate(asValue())) {
            number = JSImmediate::toDouble(asValue());
            value = asValue();
            return true;
        }
        return asCell()->getPrimitiveNumber(exec, number, value);
    }

    inline bool JSValue::toBoolean(ExecState* exec) const
    {
        return JSImmediate::isImmediate(asValue()) ? JSImmediate::toBoolean(asValue()) : asCell()->toBoolean(exec);
    }

    ALWAYS_INLINE double JSValue::toNumber(ExecState* exec) const
    {
        return JSImmediate::isImmediate(asValue()) ? JSImmediate::toDouble(asValue()) : asCell()->toNumber(exec);
    }

    inline UString JSValue::toString(ExecState* exec) const
    {
        return JSImmediate::isImmediate(asValue()) ? JSImmediate::toString(asValue()) : asCell()->toString(exec);
    }

    inline JSObject* JSValue::toObject(ExecState* exec) const
    {
        return JSImmediate::isImmediate(asValue()) ? JSImmediate::toObject(asValue(), exec) : asCell()->toObject(exec);
    }

    inline JSObject* JSValue::toThisObject(ExecState* exec) const
    {
        if (UNLIKELY(JSImmediate::isImmediate(asValue())))
            return JSImmediate::toThisObject(asValue(), exec);
        return asCell()->toThisObject(exec);
    }

    inline bool JSValue::needsThisConversion() const
    {
        if (UNLIKELY(JSImmediate::isImmediate(asValue())))
            return true;
        return asCell()->structure()->typeInfo().needsThisConversion();
    }

    inline UString JSValue::toThisString(ExecState* exec) const
    {
        return JSImmediate::isImmediate(asValue()) ? JSImmediate::toString(asValue()) : asCell()->toThisString(exec);
    }

    inline JSValue JSValue::getJSNumber()
    {
        return JSImmediate::isNumber(asValue()) ? asValue() : JSImmediate::isImmediate(asValue()) ? JSValue() : asCell()->getJSNumber();
    }

} // namespace JSC

#endif // JSCell_h
