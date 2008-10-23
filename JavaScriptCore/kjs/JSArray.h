/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef JSArray_h
#define JSArray_h

#include "JSObject.h"

namespace JSC {

    typedef HashMap<unsigned, JSValuePtr> SparseArrayValueMap;

    struct ArrayStorage {
        unsigned m_length;
        unsigned m_vectorLength;
        unsigned m_numValuesInVector;
        SparseArrayValueMap* m_sparseValueMap;
        void* lazyCreationData; // A JSArray subclass can use this to fill the vector lazily.
        JSValuePtr m_vector[1];
    };

    class JSArray : public JSObject {
        friend class CTI;

    public:
        explicit JSArray(PassRefPtr<StructureID>);
        JSArray(PassRefPtr<StructureID>, unsigned initialLength);
        JSArray(ExecState*, PassRefPtr<StructureID>, const ArgList& initialValues);
        virtual ~JSArray();

        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
        virtual void put(ExecState*, unsigned propertyName, JSValuePtr); // FIXME: Make protected and add setItem.

        static const ClassInfo info;

        unsigned length() const { return m_storage->m_length; }
        void setLength(unsigned); // OK to use on new arrays, but not if it might be a RegExpMatchArray.

        void sort(ExecState*);
        void sort(ExecState*, JSValuePtr compareFunction, CallType, const CallData&);

        void push(ExecState*, JSValuePtr);
        JSValuePtr pop();

        bool canGetIndex(unsigned i) { return i < m_fastAccessCutoff; }
        JSValuePtr getIndex(unsigned i)
        {
            ASSERT(canGetIndex(i));
            return m_storage->m_vector[i];
        }

        bool canSetIndex(unsigned i) { return i < m_fastAccessCutoff; }
        JSValuePtr setIndex(unsigned i, JSValuePtr v)
        {
            ASSERT(canSetIndex(i));
            return m_storage->m_vector[i] = v;
        }

        void fillArgList(ExecState*, ArgList&);

        static PassRefPtr<StructureID> createStructureID(JSValuePtr prototype)
        {
            return StructureID::create(prototype, TypeInfo(ObjectType));
        }

    protected:
        virtual void put(ExecState*, const Identifier& propertyName, JSValuePtr, PutPropertySlot&);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
        virtual bool deleteProperty(ExecState*, unsigned propertyName);
        virtual void getPropertyNames(ExecState*, PropertyNameArray&);
        virtual void mark();

        void* lazyCreationData();
        void setLazyCreationData(void*);

    private:
        virtual const ClassInfo* classInfo() const { return &info; }

        bool getOwnPropertySlotSlowCase(ExecState*, unsigned propertyName, PropertySlot&);
        void putSlowCase(ExecState*, unsigned propertyName, JSValuePtr);

        bool increaseVectorLength(unsigned newLength);
        
        unsigned compactForSorting();

        enum ConsistencyCheckType { NormalConsistencyCheck, DestructorConsistencyCheck, SortConsistencyCheck };
        void checkConsistency(ConsistencyCheckType = NormalConsistencyCheck);

        unsigned m_fastAccessCutoff;
        ArrayStorage* m_storage;
    };

    JSArray* asArray(JSValuePtr);

    JSArray* constructEmptyArray(ExecState*);
    JSArray* constructEmptyArray(ExecState*, unsigned initialLength);
    JSArray* constructArray(ExecState*, JSValuePtr singleItemValue);
    JSArray* constructArray(ExecState*, const ArgList& values);

    inline JSArray* asArray(JSValuePtr value)
    {
        ASSERT(asObject(value)->inherits(&JSArray::info));
        return static_cast<JSArray*>(asObject(value));
    }

} // namespace JSC

#endif // JSArray_h
