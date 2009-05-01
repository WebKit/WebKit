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

    typedef HashMap<unsigned, JSValue> SparseArrayValueMap;

    struct ArrayStorage {
        unsigned m_length;
        unsigned m_vectorLength;
        unsigned m_numValuesInVector;
        SparseArrayValueMap* m_sparseValueMap;
        void* lazyCreationData; // A JSArray subclass can use this to fill the vector lazily.
        JSValue m_vector[1];
    };

    class JSArray : public JSObject {
        friend class JIT;

    public:
        explicit JSArray(PassRefPtr<Structure>);
        JSArray(PassRefPtr<Structure>, unsigned initialLength);
        JSArray(PassRefPtr<Structure>, const ArgList& initialValues);
        virtual ~JSArray();

        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
        virtual void put(ExecState*, unsigned propertyName, JSValue); // FIXME: Make protected and add setItem.

        static JS_EXPORTDATA const ClassInfo info;

        unsigned length() const { return m_storage->m_length; }
        void setLength(unsigned); // OK to use on new arrays, but not if it might be a RegExpMatchArray.

        void sort(ExecState*);
        void sort(ExecState*, JSValue compareFunction, CallType, const CallData&);
        void sortNumeric(ExecState*, JSValue compareFunction, CallType, const CallData&);

        void push(ExecState*, JSValue);
        JSValue pop();

        bool canGetIndex(unsigned i) { return i < m_fastAccessCutoff; }
        JSValue getIndex(unsigned i)
        {
            ASSERT(canGetIndex(i));
            return m_storage->m_vector[i];
        }

        bool canSetIndex(unsigned i) { return i < m_fastAccessCutoff; }
        JSValue setIndex(unsigned i, JSValue v)
        {
            ASSERT(canSetIndex(i));
            return m_storage->m_vector[i] = v;
        }

        void fillArgList(ExecState*, MarkedArgumentBuffer&);
        void copyToRegisters(ExecState*, Register*, uint32_t);

        static PassRefPtr<Structure> createStructure(JSValue prototype)
        {
            return Structure::create(prototype, TypeInfo(ObjectType));
        }

    protected:
        virtual void put(ExecState*, const Identifier& propertyName, JSValue, PutPropertySlot&);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
        virtual bool deleteProperty(ExecState*, unsigned propertyName);
        virtual void getPropertyNames(ExecState*, PropertyNameArray&);
        virtual void mark();

        void* lazyCreationData();
        void setLazyCreationData(void*);

    private:
        virtual const ClassInfo* classInfo() const { return &info; }

        bool getOwnPropertySlotSlowCase(ExecState*, unsigned propertyName, PropertySlot&);
        void putSlowCase(ExecState*, unsigned propertyName, JSValue);

        bool increaseVectorLength(unsigned newLength);
        
        unsigned compactForSorting();

        enum ConsistencyCheckType { NormalConsistencyCheck, DestructorConsistencyCheck, SortConsistencyCheck };
        void checkConsistency(ConsistencyCheckType = NormalConsistencyCheck);

        unsigned m_fastAccessCutoff;
        ArrayStorage* m_storage;
    };

    JSArray* asArray(JSValue);

    JSArray* constructEmptyArray(ExecState*);
    JSArray* constructEmptyArray(ExecState*, unsigned initialLength);
    JSArray* constructArray(ExecState*, JSValue singleItemValue);
    JSArray* constructArray(ExecState*, const ArgList& values);

    inline JSArray* asArray(JSValue value)
    {
        ASSERT(asObject(value)->inherits(&JSArray::info));
        return static_cast<JSArray*>(asObject(value));
    }

    inline bool isJSArray(JSGlobalData* globalData, JSValue v) { return v.isCell() && v.asCell()->vptr() == globalData->jsArrayVPtr; }

} // namespace JSC

#endif // JSArray_h
