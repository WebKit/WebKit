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

#ifndef GetterSetter_h
#define GetterSetter_h

#include "JSCell.h"

#include "CallFrame.h"

namespace JSC {

    class JSObject;

    // This is an internal value object which stores getter and setter functions
    // for a property.
    class GetterSetter : public JSCell {
    public:
        GetterSetter(ExecState* exec)
            : JSCell(exec->globalData().getterSetterStructure.get())
            , m_getter(0)
            , m_setter(0)
        {
        }

        virtual void markChildren(MarkStack&);

        JSObject* getter() const { return m_getter; }
        void setGetter(JSObject* getter) { m_getter = getter; }
        JSObject* setter() const { return m_setter; }
        void setSetter(JSObject* setter) { m_setter = setter; }
        static PassRefPtr<Structure> createStructure(JSValue prototype)
        {
            return Structure::create(prototype, TypeInfo(GetterSetterType, OverridesMarkChildren), AnonymousSlotCount);
        }
    private:
        virtual bool isGetterSetter() const;

        JSObject* m_getter;
        JSObject* m_setter;  
    };

    GetterSetter* asGetterSetter(JSValue);

    inline GetterSetter* asGetterSetter(JSValue value)
    {
        ASSERT(asCell(value)->isGetterSetter());
        return static_cast<GetterSetter*>(asCell(value));
    }


} // namespace JSC

#endif // GetterSetter_h
