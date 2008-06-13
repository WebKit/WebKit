// -*- c-basic-offset: 4 -*-
/*
 *  Copyright (C) 2005, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef KJS_PROPERTY_SLOT_H
#define KJS_PROPERTY_SLOT_H

#include "identifier.h"
#include "value.h"
#include <wtf/Assertions.h>

namespace KJS {

class ExecState;
class JSObject;

struct HashEntry;

#define KJS_VALUE_SLOT_MARKER 0
#define KJS_NUMERIC_PROPERTY_NAME_SLOT_MARKER reinterpret_cast<GetValueFunc>(1)

class PropertySlot {
public:
    PropertySlot()
    {
        clearBase();
    }

    explicit PropertySlot(JSValue* base)
        : m_slotBase(base)
    {
    }

    typedef JSValue* (*GetValueFunc)(ExecState*, const Identifier&, const PropertySlot&);
    typedef JSValue* (*GetValueNumericFunc)(ExecState*, unsigned index, const PropertySlot&);

    JSValue* getValue(ExecState* exec, const Identifier& propertyName) const
    {
        if (m_getValue == KJS_VALUE_SLOT_MARKER)
            return *m_data.valueSlot;
        ASSERT(m_getValue != KJS_NUMERIC_PROPERTY_NAME_SLOT_MARKER);
        return m_getValue(exec, propertyName, *this);
    }

    JSValue* getValue(ExecState* exec, unsigned propertyName) const
    {
        if (m_getValue == KJS_VALUE_SLOT_MARKER)
            return *m_data.valueSlot;
        if (m_getValue == KJS_NUMERIC_PROPERTY_NAME_SLOT_MARKER)
            return m_data.numericFunc(exec, propertyName, *this);
        return m_getValue(exec, Identifier::from(propertyName), *this);
    }

    void putValue(JSValue* value)
    { 
        ASSERT(m_getValue == KJS_VALUE_SLOT_MARKER);
        *m_data.valueSlot = value;
    }
    
    void setValueSlot(JSValue** valueSlot) 
    {
        ASSERT(valueSlot);
        m_getValue = KJS_VALUE_SLOT_MARKER;
        clearBase();
        m_data.valueSlot = valueSlot;
    }

    void setStaticEntry(JSValue* slotBase, const HashEntry* staticEntry, GetValueFunc getValue)
    {
        ASSERT(slotBase);
        ASSERT(staticEntry);
        ASSERT(getValue);
        m_getValue = getValue;
        m_slotBase = slotBase;
        m_data.staticEntry = staticEntry;
    }

    void setCustom(JSValue* slotBase, GetValueFunc getValue)
    {
        ASSERT(slotBase);
        ASSERT(getValue);
        m_getValue = getValue;
        m_slotBase = slotBase;
    }

    void setCustomIndex(JSValue* slotBase, unsigned index, GetValueFunc getValue)
    {
        ASSERT(slotBase);
        ASSERT(getValue);
        m_getValue = getValue;
        m_slotBase = slotBase;
        m_data.index = index;
    }
    
    void setCustomNumeric(JSValue* slotBase, GetValueNumericFunc getValue)
    {
        ASSERT(slotBase);
        ASSERT(getValue);
        m_slotBase = slotBase;
        m_getValue = KJS_NUMERIC_PROPERTY_NAME_SLOT_MARKER;
        m_data.numericFunc = getValue;
    }
    
    void setGetterSlot(JSObject* getterFunc)
    {
        ASSERT(getterFunc);
        m_getValue = functionGetter;
        m_data.getterFunc = getterFunc;
    }
    
    void setUndefined()
    {
        clearBase();
        m_getValue = undefinedGetter;
    }

    JSValue* slotBase() const { ASSERT(m_slotBase); return m_slotBase; }

    void setBase(JSValue* base)
    {
        ASSERT(m_slotBase);
        ASSERT(base);
        m_slotBase = base;
    }

    void clearBase()
    {
#ifndef NDEBUG
        m_slotBase = 0;
#endif
    }

    const HashEntry* staticEntry() const { return m_data.staticEntry; }
    unsigned index() const { return m_data.index; }

private:
    static JSValue* undefinedGetter(ExecState*, const Identifier&, const PropertySlot&);
    static JSValue* functionGetter(ExecState*, const Identifier&, const PropertySlot&);
    
    GetValueFunc m_getValue;

    JSValue* m_slotBase;
    union {
        JSObject* getterFunc;
        JSValue** valueSlot;
        const HashEntry* staticEntry;
        unsigned index;
        GetValueNumericFunc numericFunc;
    } m_data;
};

}

#endif // KJS_PROPERTY_SLOT_H
