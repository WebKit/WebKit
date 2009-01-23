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

#ifndef JSNumberCell_h
#define JSNumberCell_h

#include "CallFrame.h"
#include "JSCell.h"
#include "JSImmediate.h"
#include "Collector.h"
#include "UString.h"
#include <stddef.h> // for size_t

namespace JSC {

    extern const double NaN;
    extern const double Inf;

    JSValuePtr jsNumberCell(ExecState*, double);

#if !USE(ALTERNATE_JSIMMEDIATE)

    class Identifier;
    class JSCell;
    class JSObject;
    class JSString;
    class PropertySlot;

    struct ClassInfo;
    struct Instruction;

    class JSNumberCell : public JSCell {
        friend class JIT;
        friend JSValuePtr jsNumberCell(JSGlobalData*, double);
        friend JSValuePtr jsNumberCell(ExecState*, double);
    public:
        double value() const { return m_value; }

        virtual JSValuePtr toPrimitive(ExecState*, PreferredPrimitiveType) const;
        virtual bool getPrimitiveNumber(ExecState*, double& number, JSValuePtr& value);
        virtual bool toBoolean(ExecState*) const;
        virtual double toNumber(ExecState*) const;
        virtual UString toString(ExecState*) const;
        virtual JSObject* toObject(ExecState*) const;

        virtual UString toThisString(ExecState*) const;
        virtual JSObject* toThisObject(ExecState*) const;
        virtual JSValuePtr getJSNumber();

        void* operator new(size_t size, ExecState* exec)
        {
    #ifdef JAVASCRIPTCORE_BUILDING_ALL_IN_ONE_FILE
            return exec->heap()->inlineAllocateNumber(size);
    #else
            return exec->heap()->allocateNumber(size);
    #endif
        }

        void* operator new(size_t size, JSGlobalData* globalData)
        {
    #ifdef JAVASCRIPTCORE_BUILDING_ALL_IN_ONE_FILE
            return globalData->heap.inlineAllocateNumber(size);
    #else
            return globalData->heap.allocateNumber(size);
    #endif
        }

        static PassRefPtr<Structure> createStructure(JSValuePtr proto) { return Structure::create(proto, TypeInfo(NumberType, NeedsThisConversion)); }

    private:
        JSNumberCell(JSGlobalData* globalData, double value)
            : JSCell(globalData->numberStructure.get())
            , m_value(value)
        {
        }

        JSNumberCell(ExecState* exec, double value)
            : JSCell(exec->globalData().numberStructure.get())
            , m_value(value)
        {
        }

        virtual bool getUInt32(uint32_t&) const;
        virtual bool getTruncatedInt32(int32_t&) const;
        virtual bool getTruncatedUInt32(uint32_t&) const;

        double m_value;
    };

    JSValuePtr jsNumberCell(JSGlobalData*, double);

    inline bool isNumberCell(JSValuePtr v)
    {
        return v.isCell() && v.asCell()->isNumber();
    }

    inline JSNumberCell* asNumberCell(JSValuePtr v)
    {
        ASSERT(isNumberCell(v));
        return static_cast<JSNumberCell*>(v.asCell());
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, double d)
    {
        JSValuePtr v = JSImmediate::from(d);
        return v ? v : jsNumberCell(exec, d);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, int i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, unsigned i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, unsigned long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, long long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, static_cast<double>(i));
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState* exec, unsigned long long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(exec, static_cast<double>(i));
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, double d)
    {
        JSValuePtr v = JSImmediate::from(d);
        return v ? v : jsNumberCell(globalData, d);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, int i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, unsigned i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, unsigned long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, long long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, static_cast<double>(i));
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData* globalData, unsigned long long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        return v ? v : jsNumberCell(globalData, static_cast<double>(i));
    }

    inline bool JSValuePtr::isDoubleNumber() const
    {
        return isNumberCell(asValue());
    }

    inline double JSValuePtr::getDoubleNumber() const
    {
        return asNumberCell(asValue())->value();
    }

    inline bool JSValuePtr::isNumber() const
    {
        return JSImmediate::isNumber(asValue()) || isDoubleNumber();
    }

    inline double JSValuePtr::uncheckedGetNumber() const
    {
        ASSERT(isNumber());
        return JSImmediate::isImmediate(asValue()) ? JSImmediate::toDouble(asValue()) : getDoubleNumber();
    }

#else

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState*, double d)
    {
        JSValuePtr v = JSImmediate::from(d);
        ASSERT(v);
        return v;
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState*, int i)
    {
        JSValuePtr v = JSImmediate::from(i);
        ASSERT(v);
        return v;
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState*, unsigned i)
    {
        JSValuePtr v = JSImmediate::from(i);
        ASSERT(v);
        return v;
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState*, long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        ASSERT(v);
        return v;
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState*, unsigned long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        ASSERT(v);
        return v;
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState*, long long i)
    {
        JSValuePtr v = JSImmediate::from(static_cast<double>(i));
        ASSERT(v);
        return v;
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState*, unsigned long long i)
    {
        JSValuePtr v = JSImmediate::from(static_cast<double>(i));
        ASSERT(v);
        return v;
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData*, double d)
    {
        JSValuePtr v = JSImmediate::from(d);
        ASSERT(v);
        return v;
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData*, int i)
    {
        JSValuePtr v = JSImmediate::from(i);
        ASSERT(v);
        return v;
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData*, unsigned i)
    {
        JSValuePtr v = JSImmediate::from(i);
        ASSERT(v);
        return v;
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData*, long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        ASSERT(v);
        return v;
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData*, unsigned long i)
    {
        JSValuePtr v = JSImmediate::from(i);
        ASSERT(v);
        return v;
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData*, long long i)
    {
        JSValuePtr v = JSImmediate::from(static_cast<double>(i));
        ASSERT(v);
        return v;
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData*, unsigned long long i)
    {
        JSValuePtr v = JSImmediate::from(static_cast<double>(i));
        ASSERT(v);
        return v;
    }

    inline bool JSValuePtr::isDoubleNumber() const
    {
        return JSImmediate::isDoubleNumber(asValue());
    }

    inline double JSValuePtr::getDoubleNumber() const
    {
        return JSImmediate::doubleValue(asValue());
    }

    inline bool JSValuePtr::isNumber() const
    {
        return JSImmediate::isNumber(asValue());
    }

    inline double JSValuePtr::uncheckedGetNumber() const
    {
        ASSERT(isNumber());
        return JSImmediate::toDouble(asValue());
    }

#endif

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState*, char i)
    {
        ASSERT(JSImmediate::from(i));
        return JSImmediate::from(i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState*, unsigned char i)
    {
        ASSERT(JSImmediate::from(i));
        return JSImmediate::from(i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState*, short i)
    {
        ASSERT(JSImmediate::from(i));
        return JSImmediate::from(i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(ExecState*, unsigned short i)
    {
        ASSERT(JSImmediate::from(i));
        return JSImmediate::from(i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData*, short i)
    {
        ASSERT(JSImmediate::from(i));
        return JSImmediate::from(i);
    }

    ALWAYS_INLINE JSValuePtr jsNumber(JSGlobalData*, unsigned short i)
    {
        ASSERT(JSImmediate::from(i));
        return JSImmediate::from(i);
    }

    inline JSValuePtr jsNaN(ExecState* exec)
    {
        return jsNumber(exec, NaN);
    }

    inline JSValuePtr jsNaN(JSGlobalData* globalData)
    {
        return jsNumber(globalData, NaN);
    }

    // --- JSValue inlines ----------------------------

    ALWAYS_INLINE JSValuePtr JSValuePtr::toJSNumber(ExecState* exec) const
    {
        return isNumber() ? asValue() : jsNumber(exec, this->toNumber(exec));
    }

    inline bool JSValuePtr::getNumber(double &result) const
    {
        if (isInt32Fast())
            result = getInt32Fast();
        else if (LIKELY(isDoubleNumber()))
            result = getDoubleNumber();
        else {
            ASSERT(!isNumber());
            return false;
        }
        return true;
    }

    inline bool JSValuePtr::numberToInt32(int32_t& arg)
    {
        if (isInt32Fast())
            arg = getInt32Fast();
        else if (LIKELY(isDoubleNumber()))
            arg = JSC::toInt32(getDoubleNumber());
        else {
            ASSERT(!isNumber());
            return false;
        }
        return true;
    }

    inline bool JSValuePtr::numberToUInt32(uint32_t& arg)
    {
        if (isUInt32Fast())
            arg = getUInt32Fast();
        else if (LIKELY(isDoubleNumber()))
            arg = JSC::toUInt32(getDoubleNumber());
        else if (isInt32Fast()) {
            // FIXME: I think this case can be merged with the uint case; toUInt32SlowCase
            // on a negative value is equivalent to simple static_casting.
            bool ignored;
            arg = toUInt32SlowCase(getInt32Fast(), ignored);
        } else {
            ASSERT(!isNumber());
            return false;
        }
        return true;
    }

} // namespace JSC

#endif // JSNumberCell_h
