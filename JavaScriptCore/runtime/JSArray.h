/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
        
        inline void markChildrenDirect(MarkStack& markStack);

    protected:
        virtual void put(ExecState*, const Identifier& propertyName, JSValue, PutPropertySlot&);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
        virtual bool deleteProperty(ExecState*, unsigned propertyName);
        virtual void getPropertyNames(ExecState*, PropertyNameArray&);
        virtual void markChildren(MarkStack&);

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

    void JSArray::markChildrenDirect(MarkStack& markStack) {
        JSObject::markChildrenDirect(markStack);
        
        ArrayStorage* storage = m_storage;
        
        unsigned usedVectorLength = std::min(storage->m_length, storage->m_vectorLength);
        markStack.appendValues(storage->m_vector, usedVectorLength, MayContainNullValues);
        
        if (SparseArrayValueMap* map = storage->m_sparseValueMap) {
            SparseArrayValueMap::iterator end = map->end();
            for (SparseArrayValueMap::iterator it = map->begin(); it != end; ++it)
                markStack.append(it->second);
        }
    }

    inline void MarkStack::drain()
    {
        while (!m_markSets.isEmpty() || !m_values.isEmpty()) {
            while (!m_markSets.isEmpty() && m_values.size() < 50) {
                ASSERT(!m_markSets.isEmpty());
                MarkSet& current = m_markSets.last();
                ASSERT(current.m_values);
                JSValue* end = current.m_end;
                ASSERT(current.m_values);
                ASSERT(current.m_values != end);
            findNextUnmarkedNullValue:
                ASSERT(current.m_values != end);
                JSValue v = *current.m_values;
                current.m_values++;
                
                if (!v || v.marked()) {
                    if (current.m_values == end) {
                        m_markSets.removeLast();
                        continue;
                    }
                    goto findNextUnmarkedNullValue;
                }
                
                JSCell* currentCell = v.asCell();
                currentCell->markCellDirect();
                if (currentCell->structure()->typeInfo().type() < CompoundType) {
                    if (current.m_values == end) {
                        m_markSets.removeLast();
                        continue;
                    }
                    goto findNextUnmarkedNullValue;
                }
                
                if (current.m_values == end)
                    m_markSets.removeLast();

                if (currentCell->structure()->typeInfo().hasDefaultMark())
                    static_cast<JSObject*>(currentCell)->markChildrenDirect(*this);
                else if (currentCell->vptr() == m_jsArrayVPtr)
                    static_cast<JSArray*>(currentCell)->markChildrenDirect(*this);
                else
                    currentCell->markChildren(*this);
            }
            while (!m_values.isEmpty()) {
                JSCell* current = m_values.removeLast();
                ASSERT(current->marked());
                if (current->structure()->typeInfo().hasDefaultMark())
                    static_cast<JSObject*>(current)->markChildrenDirect(*this);
                else if (current->vptr() == m_jsArrayVPtr)
                    static_cast<JSArray*>(current)->markChildrenDirect(*this);
                else
                    current->markChildren(*this);
            }
        }
    }
    
} // namespace JSC

#endif // JSArray_h
