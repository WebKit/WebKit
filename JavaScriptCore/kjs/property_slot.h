// -*- c-basic-offset: 4 -*-
/*
 *  Copyright (C) 2005, 2007 Apple Inc. All rights reserved.
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
    typedef JSValue* (*GetValueFunc)(ExecState*, JSObject* originalObject, const Identifier&, const PropertySlot&);
    typedef JSValue* (*GetValueNumericFunc)(ExecState*, JSObject* originalObject, unsigned index, const PropertySlot&);

    JSValue* getValue(ExecState* exec, JSObject* originalObject, const Identifier& propertyName) const
    { 
        if (m_getValue == KJS_VALUE_SLOT_MARKER)
            return *m_data.valueSlot;
        ASSERT(m_getValue != KJS_NUMERIC_PROPERTY_NAME_SLOT_MARKER);
        return m_getValue(exec, originalObject, propertyName, *this); 
    }

    JSValue* getValue(ExecState* exec, JSObject* originalObject, unsigned propertyName) const
    { 
        if (m_getValue == KJS_VALUE_SLOT_MARKER)
            return *m_data.valueSlot;
        if (m_getValue == KJS_NUMERIC_PROPERTY_NAME_SLOT_MARKER)
            return m_data.numericFunc(exec, originalObject, propertyName, *this);
        return m_getValue(exec, originalObject, Identifier::from(propertyName), *this); 
    }
    
    void setValueSlot(JSObject* slotBase, JSValue** valueSlot) 
    {
        m_getValue = KJS_VALUE_SLOT_MARKER;
        m_slotBase = slotBase;
        m_data.valueSlot = valueSlot;
    }

    void setStaticEntry(JSObject* slotBase, const HashEntry* staticEntry, GetValueFunc getValue)
    {
        ASSERT(getValue);
        m_getValue = getValue;
        m_slotBase = slotBase;
        m_data.staticEntry = staticEntry;
    }

    void setCustom(JSObject* slotBase, GetValueFunc getValue)
    {
        ASSERT(getValue);
        m_getValue = getValue;
        m_slotBase = slotBase;
    }

    void setCustomIndex(JSObject* slotBase, unsigned index, GetValueFunc getValue)
    {
        ASSERT(getValue);
        m_getValue = getValue;
        m_slotBase = slotBase;
        m_data.index = index;
    }
    
    void setCustomNumeric(JSObject* slotBase, GetValueNumericFunc getValue)
    {
        ASSERT(getValue);
        m_slotBase = slotBase;
        m_getValue = KJS_NUMERIC_PROPERTY_NAME_SLOT_MARKER;
        m_data.numericFunc = getValue;
    }
    
    void setGetterSlot(JSObject* slotBase, JSObject* getterFunc)
    {
        m_getValue = functionGetter;
        m_slotBase = slotBase;
        m_data.getterFunc = getterFunc;
    }
    
    void setUndefined(JSObject *slotBase)
    {
        m_slotBase = slotBase;
        m_getValue = undefinedGetter;
    }

    void setUngettable(JSObject* slotBase) // Used to signal that you have a property, but trying to get it at this time is an error.
    {
        m_slotBase = slotBase;
        m_getValue = ungettableGetter;
    }

    JSObject* slotBase() const { return m_slotBase; }

    const HashEntry* staticEntry() const { return m_data.staticEntry; }
    unsigned index() const { return m_data.index; }

private:
    static JSValue* undefinedGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    static JSValue* ungettableGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    static JSValue* functionGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    
    GetValueFunc m_getValue;

    JSObject* m_slotBase;
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
