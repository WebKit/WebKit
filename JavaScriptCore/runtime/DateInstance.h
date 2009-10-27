/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef DateInstance_h
#define DateInstance_h

#include "JSWrapperObject.h"

namespace WTF {
    struct GregorianDateTime;
}

namespace JSC {

    class DateInstance : public JSWrapperObject {
    public:
        DateInstance(ExecState*, double);
        explicit DateInstance(ExecState*, NonNullPassRefPtr<Structure>);

        double internalNumber() const { return internalValue().uncheckedGetNumber(); }

        static JS_EXPORTDATA const ClassInfo info;

        bool getGregorianDateTime(ExecState*, bool outputIsUTC, WTF::GregorianDateTime&) const;

        static PassRefPtr<Structure> createStructure(JSValue prototype)
        {
            return Structure::create(prototype, TypeInfo(ObjectType, StructureFlags));
        }

    protected:
        static const unsigned StructureFlags = OverridesMarkChildren | JSWrapperObject::StructureFlags;

    private:
        virtual const ClassInfo* classInfo() const { return &info; }

        mutable RefPtr<DateInstanceData> m_data;
    };

    DateInstance* asDateInstance(JSValue);

    inline DateInstance* asDateInstance(JSValue value)
    {
        ASSERT(asObject(value)->inherits(&DateInstance::info));
        return static_cast<DateInstance*>(asObject(value));
    }

} // namespace JSC

#endif // DateInstance_h
