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

#define CHECK_ARRAY_CONSISTENCY 0

namespace JSC {

    class JSArray;
    class LLIntOffsetsExtractor;

    struct SparseArrayEntry : public WriteBarrier<Unknown> {
        typedef WriteBarrier<Unknown> Base;

        SparseArrayEntry() : attributes(0) {}

        JSValue get(ExecState*, JSArray*) const;
        void get(PropertySlot&) const;
        void get(PropertyDescriptor&) const;
        JSValue getNonSparseMode() const;

        unsigned attributes;
    };

    class SparseArrayValueMap {
        typedef HashMap<uint64_t, SparseArrayEntry, WTF::IntHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t> > Map;

        enum Flags {
            Normal = 0,
            SparseMode = 1,
            LengthIsReadOnly = 2,
        };

    public:
        typedef Map::iterator iterator;
        typedef Map::const_iterator const_iterator;
        typedef Map::AddResult AddResult;

        SparseArrayValueMap()
            : m_flags(Normal)
            , m_reportedCapacity(0)
        {
        }

        void visitChildren(SlotVisitor&);

        bool sparseMode()
        {
            return m_flags & SparseMode;
        }

        void setSparseMode()
        {
            m_flags = static_cast<Flags>(m_flags | SparseMode);
        }

        bool lengthIsReadOnly()
        {
            return m_flags & LengthIsReadOnly;
        }

        void setLengthIsReadOnly()
        {
            m_flags = static_cast<Flags>(m_flags | LengthIsReadOnly);
        }

        // These methods may mutate the contents of the map
        void put(ExecState*, JSArray*, unsigned, JSValue, bool shouldThrow);
        bool putDirect(ExecState*, JSArray*, unsigned, JSValue, bool shouldThrow);
        AddResult add(JSArray*, unsigned);
        iterator find(unsigned i) { return m_map.find(i); }
        // This should ASSERT the remove is valid (check the result of the find).
        void remove(iterator it) { m_map.remove(it); }
        void remove(unsigned i) { m_map.remove(i); }

        // These methods do not mutate the contents of the map.
        iterator notFound() { return m_map.end(); }
        bool isEmpty() const { return m_map.isEmpty(); }
        bool contains(unsigned i) const { return m_map.contains(i); }
        size_t size() const { return m_map.size(); }
        // Only allow const begin/end iteration.
        const_iterator begin() const { return m_map.begin(); }
        const_iterator end() const { return m_map.end(); }

    private:
        Map m_map;
        Flags m_flags;
        size_t m_reportedCapacity;
    };

    // This struct holds the actual data values of an array.  A JSArray object points to it's contained ArrayStorage
    // struct by pointing to m_vector.  To access the contained ArrayStorage struct, use the getStorage() and 
    // setStorage() methods.  It is important to note that there may be space before the ArrayStorage that 
    // is used to quick unshift / shift operation.  The actual allocated pointer is available by using:
    //     getStorage() - m_indexBias * sizeof(JSValue)
    struct ArrayStorage {
        unsigned m_length; // The "length" property on the array
        unsigned m_numValuesInVector;
        void* m_allocBase; // Pointer to base address returned by malloc().  Keeping this pointer does eliminate false positives from the leak detector.
#if CHECK_ARRAY_CONSISTENCY
        // Needs to be a uintptr_t for alignment purposes.
        uintptr_t m_initializationIndex;
        uintptr_t m_inCompactInitialization;
#else
        uintptr_t m_padding;
#endif
        WriteBarrier<Unknown> m_vector[1];

        static ptrdiff_t lengthOffset() { return OBJECT_OFFSETOF(ArrayStorage, m_length); }
        static ptrdiff_t numValuesInVectorOffset() { return OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector); }
        static ptrdiff_t allocBaseOffset() { return OBJECT_OFFSETOF(ArrayStorage, m_allocBase); }
        static ptrdiff_t vectorOffset() { return OBJECT_OFFSETOF(ArrayStorage, m_vector); }
    };

    class JSArray : public JSNonFinalObject {
        friend class LLIntOffsetsExtractor;
        friend class Walker;
        friend class JIT;

    protected:
        explicit JSArray(JSGlobalData& globalData, Structure* structure)
            : JSNonFinalObject(globalData, structure)
            , m_indexBias(0)
            , m_storage(0)
            , m_sparseValueMap(0)
        {
        }

        JS_EXPORT_PRIVATE void finishCreation(JSGlobalData&, unsigned initialLength = 0);
        JS_EXPORT_PRIVATE JSArray* tryFinishCreationUninitialized(JSGlobalData&, unsigned initialLength);
    
    public:
        typedef JSNonFinalObject Base;

        static void finalize(JSCell*);

        static JSArray* create(JSGlobalData&, Structure*, unsigned initialLength = 0);

        // tryCreateUninitialized is used for fast construction of arrays whose size and
        // contents are known at time of creation. Clients of this interface must:
        //   - null-check the result (indicating out of memory, or otherwise unable to allocate vector).
        //   - call 'initializeIndex' for all properties in sequence, for 0 <= i < initialLength.
        //   - called 'completeInitialization' after all properties have been initialized.
        static JSArray* tryCreateUninitialized(JSGlobalData&, Structure*, unsigned initialLength);

        JS_EXPORT_PRIVATE static bool defineOwnProperty(JSObject*, ExecState*, const Identifier&, PropertyDescriptor&, bool throwException);

        static bool getOwnPropertySlot(JSCell*, ExecState*, const Identifier&, PropertySlot&);
        JS_EXPORT_PRIVATE static bool getOwnPropertySlotByIndex(JSCell*, ExecState*, unsigned propertyName, PropertySlot&);
        static bool getOwnPropertyDescriptor(JSObject*, ExecState*, const Identifier&, PropertyDescriptor&);
        static void putByIndex(JSCell*, ExecState*, unsigned propertyName, JSValue, bool shouldThrow);
        // This is similar to the JSObject::putDirect* methods:
        //  - the prototype chain is not consulted
        //  - accessors are not called.
        // This method creates a property with attributes writable, enumerable and configurable all set to true.
        bool putDirectIndex(ExecState* exec, unsigned propertyName, JSValue value, bool shouldThrow = true)
        {
            if (canSetIndex(propertyName)) {
                setIndex(exec->globalData(), propertyName, value);
                return true;
            }
            return putDirectIndexBeyondVectorLength(exec, propertyName, value, shouldThrow);
        }

        static JS_EXPORTDATA const ClassInfo s_info;
        
        unsigned length() const { return m_storage->m_length; }
        // OK to use on new arrays, but not if it might be a RegExpMatchArray.
        bool setLength(ExecState*, unsigned, bool throwException = false);

        void sort(ExecState*);
        void sort(ExecState*, JSValue compareFunction, CallType, const CallData&);
        void sortNumeric(ExecState*, JSValue compareFunction, CallType, const CallData&);

        void push(ExecState*, JSValue);
        JSValue pop(ExecState*);

        bool shiftCount(ExecState*, unsigned count);
        bool unshiftCount(ExecState*, unsigned count);

        bool canGetIndex(unsigned i) { return i < m_vectorLength && m_storage->m_vector[i]; }
        JSValue getIndex(unsigned i)
        {
            ASSERT(canGetIndex(i));
            return m_storage->m_vector[i].get();
        }

        bool canSetIndex(unsigned i) { return i < m_vectorLength; }
        void setIndex(JSGlobalData& globalData, unsigned i, JSValue v)
        {
            ASSERT(canSetIndex(i));
            
            WriteBarrier<Unknown>& x = m_storage->m_vector[i];
            if (!x) {
                ArrayStorage *storage = m_storage;
                ++storage->m_numValuesInVector;
                if (i >= storage->m_length)
                    storage->m_length = i + 1;
            }
            x.set(globalData, this, v);
        }
        
        inline void initializeIndex(JSGlobalData& globalData, unsigned i, JSValue v)
        {
            ASSERT(canSetIndex(i));
            ArrayStorage *storage = m_storage;
#if CHECK_ARRAY_CONSISTENCY
            ASSERT(storage->m_inCompactInitialization);
            // Check that we are initializing the next index in sequence.
            ASSERT(i == storage->m_initializationIndex);
            // tryCreateUninitialized set m_numValuesInVector to the initialLength,
            // check we do not try to initialize more than this number of properties.
            ASSERT(storage->m_initializationIndex < storage->m_numValuesInVector);
            storage->m_initializationIndex++;
#endif
            ASSERT(i < storage->m_length);
            ASSERT(i < storage->m_numValuesInVector);
            storage->m_vector[i].set(globalData, this, v);
        }

        inline void completeInitialization(unsigned newLength)
        {
            // Check that we have initialized as meny properties as we think we have.
            ASSERT_UNUSED(newLength, newLength == m_storage->m_length);
#if CHECK_ARRAY_CONSISTENCY
            // Check that the number of propreties initialized matches the initialLength.
            ASSERT(m_storage->m_initializationIndex == m_storage->m_numValuesInVector);
            ASSERT(m_storage->m_inCompactInitialization);
            m_storage->m_inCompactInitialization = false;
#endif
        }

        bool inSparseMode()
        {
            SparseArrayValueMap* map = m_sparseValueMap;
            return map && map->sparseMode();
        }

        void fillArgList(ExecState*, MarkedArgumentBuffer&);
        void copyToArguments(ExecState*, CallFrame*, uint32_t length);

        static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue prototype)
        {
            return Structure::create(globalData, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), &s_info);
        }
        
        static ptrdiff_t storageOffset()
        {
            return OBJECT_OFFSETOF(JSArray, m_storage);
        }

        static ptrdiff_t vectorLengthOffset()
        {
            return OBJECT_OFFSETOF(JSArray, m_vectorLength);
        }

        JS_EXPORT_PRIVATE static void visitChildren(JSCell*, SlotVisitor&);

        void enterDictionaryMode(JSGlobalData&);

    protected:
        static const unsigned StructureFlags = OverridesGetOwnPropertySlot | OverridesVisitChildren | OverridesGetPropertyNames | JSObject::StructureFlags;
        static void put(JSCell*, ExecState*, const Identifier& propertyName, JSValue, PutPropertySlot&);

        static bool deleteProperty(JSCell*, ExecState*, const Identifier& propertyName);
        static bool deletePropertyByIndex(JSCell*, ExecState*, unsigned propertyName);
        static void getOwnPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);

        JS_EXPORT_PRIVATE void* subclassData() const;
        JS_EXPORT_PRIVATE void setSubclassData(void*);

    private:
        static size_t storageSize(unsigned vectorLength);
        bool isLengthWritable()
        {
            SparseArrayValueMap* map = m_sparseValueMap;
            return !map || !map->lengthIsReadOnly();
        }

        void setLengthWritable(ExecState*, bool writable);
        void putDescriptor(ExecState*, SparseArrayEntry*, PropertyDescriptor&, PropertyDescriptor& old);
        bool defineOwnNumericProperty(ExecState*, unsigned, PropertyDescriptor&, bool throwException);
        void allocateSparseMap(JSGlobalData&);
        void deallocateSparseMap();

        bool getOwnPropertySlotSlowCase(ExecState*, unsigned propertyName, PropertySlot&);
        void putByIndexBeyondVectorLength(ExecState*, unsigned propertyName, JSValue, bool shouldThrow);
        JS_EXPORT_PRIVATE bool putDirectIndexBeyondVectorLength(ExecState*, unsigned propertyName, JSValue, bool shouldThrow);

        unsigned getNewVectorLength(unsigned desiredLength);
        bool increaseVectorLength(JSGlobalData&, unsigned newLength);
        bool unshiftCountSlowCase(JSGlobalData&, unsigned count);
        
        unsigned compactForSorting(JSGlobalData&);

        enum ConsistencyCheckType { NormalConsistencyCheck, DestructorConsistencyCheck, SortConsistencyCheck };
        void checkConsistency(ConsistencyCheckType = NormalConsistencyCheck);

        unsigned m_vectorLength; // The valid length of m_vector
        unsigned m_indexBias; // The number of JSValue sized blocks before ArrayStorage.
        ArrayStorage *m_storage;

        // FIXME: Maybe SparseArrayValueMap should be put into its own JSCell?
        SparseArrayValueMap* m_sparseValueMap;

        static ptrdiff_t sparseValueMapOffset() { return OBJECT_OFFSETOF(JSArray, m_sparseValueMap); }
        static ptrdiff_t indexBiasOffset() { return OBJECT_OFFSETOF(JSArray, m_indexBias); }
    };

    inline JSArray* JSArray::create(JSGlobalData& globalData, Structure* structure, unsigned initialLength)
    {
        JSArray* array = new (NotNull, allocateCell<JSArray>(globalData.heap)) JSArray(globalData, structure);
        array->finishCreation(globalData, initialLength);
        return array;
    }

    inline JSArray* JSArray::tryCreateUninitialized(JSGlobalData& globalData, Structure* structure, unsigned initialLength)
    {
        JSArray* array = new (NotNull, allocateCell<JSArray>(globalData.heap)) JSArray(globalData, structure);
        return array->tryFinishCreationUninitialized(globalData, initialLength);
    }

    JSArray* asArray(JSValue);

    inline JSArray* asArray(JSCell* cell)
    {
        ASSERT(cell->inherits(&JSArray::s_info));
        return jsCast<JSArray*>(cell);
    }

    inline JSArray* asArray(JSValue value)
    {
        return asArray(value.asCell());
    }

    inline bool isJSArray(JSCell* cell) { return cell->classInfo() == &JSArray::s_info; }
    inline bool isJSArray(JSValue v) { return v.isCell() && isJSArray(v.asCell()); }

    // Rule from ECMA 15.2 about what an array index is.
    // Must exactly match string form of an unsigned integer, and be less than 2^32 - 1.
    inline unsigned Identifier::toArrayIndex(bool& ok) const
    {
        unsigned i = toUInt32(ok);
        if (ok && i >= 0xFFFFFFFFU)
            ok = false;
        return i;
    }

// The definition of MAX_STORAGE_VECTOR_LENGTH is dependant on the definition storageSize
// function below - the MAX_STORAGE_VECTOR_LENGTH limit is defined such that the storage
// size calculation cannot overflow.  (sizeof(ArrayStorage) - sizeof(WriteBarrier<Unknown>)) +
// (vectorLength * sizeof(WriteBarrier<Unknown>)) must be <= 0xFFFFFFFFU (which is maximum value of size_t).
#define MAX_STORAGE_VECTOR_LENGTH static_cast<unsigned>((0xFFFFFFFFU - (sizeof(ArrayStorage) - sizeof(WriteBarrier<Unknown>))) / sizeof(WriteBarrier<Unknown>))

// These values have to be macros to be used in max() and min() without introducing
// a PIC branch in Mach-O binaries, see <rdar://problem/5971391>.
#define MIN_SPARSE_ARRAY_INDEX 10000U
#define MAX_STORAGE_VECTOR_INDEX (MAX_STORAGE_VECTOR_LENGTH - 1)
    inline size_t JSArray::storageSize(unsigned vectorLength)
    {
        ASSERT(vectorLength <= MAX_STORAGE_VECTOR_LENGTH);
    
        // MAX_STORAGE_VECTOR_LENGTH is defined such that provided (vectorLength <= MAX_STORAGE_VECTOR_LENGTH)
        // - as asserted above - the following calculation cannot overflow.
        size_t size = (sizeof(ArrayStorage) - sizeof(WriteBarrier<Unknown>)) + (vectorLength * sizeof(WriteBarrier<Unknown>));
        // Assertion to detect integer overflow in previous calculation (should not be possible, provided that
        // MAX_STORAGE_VECTOR_LENGTH is correctly defined).
        ASSERT(((size - (sizeof(ArrayStorage) - sizeof(WriteBarrier<Unknown>))) / sizeof(WriteBarrier<Unknown>) == vectorLength) && (size >= (sizeof(ArrayStorage) - sizeof(WriteBarrier<Unknown>))));
    
        return size;
    }
    
    } // namespace JSC

#endif // JSArray_h
