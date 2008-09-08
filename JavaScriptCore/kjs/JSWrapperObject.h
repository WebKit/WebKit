/*
 *  Copyright (C) 2006 Maks Orlovich
 *  Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef KJS_JSWrapperObject_h
#define KJS_JSWrapperObject_h

#include "JSObject.h"

namespace JSC {
    
    /** 
       This class is used as a base for classes such as String,
       Number, Boolean and Date which which are wrappers for primitive
       types. These classes stores the internal value, which is the
       actual value represented by the wrapper objects.
    */ 
    class JSWrapperObject : public JSObject {
    public:
        JSWrapperObject(JSObject* prototype);
        
        JSValue* internalValue() const;
        void setInternalValue(JSValue*);
        
        virtual void mark();
        
    private:
        JSValue* m_internalValue;
    };
    
    inline JSWrapperObject::JSWrapperObject(JSObject* prototype)
        : JSObject(prototype)
        , m_internalValue(0)
    {
    }
    
    inline JSValue* JSWrapperObject::internalValue() const
    {
        return m_internalValue;
    }
    
    inline void JSWrapperObject::setInternalValue(JSValue* value)
    {
        ASSERT(value);
        m_internalValue = value;
    }

} // namespace JSC

#endif // KJS_JSWrapperObject_h
