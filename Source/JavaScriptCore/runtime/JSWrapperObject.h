/*
 *  Copyright (C) 2006 Maks Orlovich
 *  Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef JSWrapperObject_h
#define JSWrapperObject_h

#include "JSObject.h"

namespace JSC {

    // This class is used as a base for classes such as String,
    // Number, Boolean and Date which are wrappers for primitive types.
    class JSWrapperObject : public JSObject {
    protected:
        explicit JSWrapperObject(NonNullPassRefPtr<Structure>);

    public:
        JSValue internalValue() const { return m_internalValue; }
        void setInternalValue(JSValue);

        static PassRefPtr<Structure> createStructure(JSValue prototype) 
        { 
            return Structure::create(prototype, TypeInfo(ObjectType, StructureFlags), AnonymousSlotCount);
        }

    protected:
        static const unsigned AnonymousSlotCount = 1 + JSObject::AnonymousSlotCount;

    private:
        virtual void markChildren(MarkStack&);
        
        JSValue m_internalValue;
    };

    inline JSWrapperObject::JSWrapperObject(NonNullPassRefPtr<Structure> structure)
        : JSObject(structure)
    {
        putAnonymousValue(0, jsNull());
    }

    inline void JSWrapperObject::setInternalValue(JSValue value)
    {
        ASSERT(value);
        ASSERT(!value.isObject());
        m_internalValue = value;
        putAnonymousValue(0, value);
    }

} // namespace JSC

#endif // JSWrapperObject_h
