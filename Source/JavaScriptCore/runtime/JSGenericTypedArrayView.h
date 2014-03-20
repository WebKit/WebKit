/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef JSGenericTypedArrayView_h
#define JSGenericTypedArrayView_h

#include "JSArrayBufferView.h"
#include "ToNativeFromValue.h"

namespace JSC {

JS_EXPORT_PRIVATE const ClassInfo* getInt8ArrayClassInfo();
JS_EXPORT_PRIVATE const ClassInfo* getInt16ArrayClassInfo();
JS_EXPORT_PRIVATE const ClassInfo* getInt32ArrayClassInfo();
JS_EXPORT_PRIVATE const ClassInfo* getUint8ArrayClassInfo();
JS_EXPORT_PRIVATE const ClassInfo* getUint8ClampedArrayClassInfo();
JS_EXPORT_PRIVATE const ClassInfo* getUint16ArrayClassInfo();
JS_EXPORT_PRIVATE const ClassInfo* getUint32ArrayClassInfo();
JS_EXPORT_PRIVATE const ClassInfo* getFloat32ArrayClassInfo();
JS_EXPORT_PRIVATE const ClassInfo* getFloat64ArrayClassInfo();

// A typed array view is our representation of a typed array object as seen
// from JavaScript. For example:
//
// var o = new Int8Array(100);
//
// Here, 'o' points to a JSGenericTypedArrayView<int8_t>.
//
// Views contain five fields:
//
//     Structure* S     // from JSCell
//     Butterfly* B     // from JSObject
//     ElementType* V
//     uint32_t L
//     TypedArrayMode M
//
// These fields take up a total of four pointer-width words. FIXME: Make
// it take less words!
//
// B is usually unused but may stored some additional "overflow" data for
// one of the modes. V always points to the base of the typed array's data,
// and may point to either GC-managed copied space, or data in the C heap;
// which of those things it points to is governed by the mode although for
// simple accesses to the view you can just read from the pointer either
// way. M specifies the mode of the view. L is the length, in units that
// depend on the view's type.

// The JSGenericTypedArrayView is templatized by an Adaptor that controls
// the element type and how it's converted; it should obey the following
// interface; I use int8_t as an example:
//
// struct Adaptor {
//     typedef int8_t Type;
//     typedef Int8Array ViewType;
//     typedef JSInt8Array JSViewType;
//     static int8_t toNativeFromInt32(int32_t);
//     static int8_t toNativeFromUint32(uint32_t);
//     static int8_t toNativeFromDouble(double);
//     static JSValue toJSValue(int8_t);
//     static double toDouble(int8_t);
//     template<T> static T::Type convertTo(uint8_t);
// };

template<typename Adaptor>
class JSGenericTypedArrayView : public JSArrayBufferView {
public:
    typedef JSArrayBufferView Base;
    static const unsigned elementSize = sizeof(typename Adaptor::Type);
    
protected:
    JSGenericTypedArrayView(VM&, ConstructionContext&);
    
public:
    static JSGenericTypedArrayView* create(ExecState*, Structure*, unsigned length);
    static JSGenericTypedArrayView* createUninitialized(ExecState*, Structure*, unsigned length);
    static JSGenericTypedArrayView* create(ExecState*, Structure*, PassRefPtr<ArrayBuffer>, unsigned byteOffset, unsigned length);
    static JSGenericTypedArrayView* create(VM&, Structure*, PassRefPtr<typename Adaptor::ViewType> impl);
    static JSGenericTypedArrayView* create(Structure*, JSGlobalObject*, PassRefPtr<typename Adaptor::ViewType> impl);
    
    unsigned byteLength() const { return m_length * sizeof(typename Adaptor::Type); }
    size_t byteSize() const { return sizeOf(m_length, sizeof(typename Adaptor::Type)); }
    
    const typename Adaptor::Type* typedVector() const
    {
        return static_cast<const typename Adaptor::Type*>(m_vector);
    }
    typename Adaptor::Type* typedVector()
    {
        return static_cast<typename Adaptor::Type*>(m_vector);
    }

    // These methods are meant to match indexed access methods that JSObject
    // supports - hence the slight redundancy.
    bool canGetIndexQuickly(unsigned i)
    {
        return i < m_length;
    }
    bool canSetIndexQuickly(unsigned i)
    {
        return i < m_length;
    }
    
    typename Adaptor::Type getIndexQuicklyAsNativeValue(unsigned i)
    {
        ASSERT(i < m_length);
        return typedVector()[i];
    }
    
    double getIndexQuicklyAsDouble(unsigned i)
    {
        return Adaptor::toDouble(getIndexQuicklyAsNativeValue(i));
    }
    
    JSValue getIndexQuickly(unsigned i)
    {
        return Adaptor::toJSValue(getIndexQuicklyAsNativeValue(i));
    }
    
    void setIndexQuicklyToNativeValue(unsigned i, typename Adaptor::Type value)
    {
        ASSERT(i < m_length);
        typedVector()[i] = value;
    }
    
    void setIndexQuicklyToDouble(unsigned i, double value)
    {
        setIndexQuicklyToNativeValue(i, toNativeFromValue<Adaptor>(value));
    }
    
    void setIndexQuickly(unsigned i, JSValue value)
    {
        setIndexQuicklyToNativeValue(i, toNativeFromValue<Adaptor>(value));
    }
    
    bool setIndex(ExecState* exec, unsigned i, JSValue jsValue)
    {
        typename Adaptor::Type value = toNativeFromValue<Adaptor>(exec, jsValue);
        if (exec->hadException())
            return false;

        if (i >= m_length)
            return false;

        setIndexQuicklyToNativeValue(i, value);
        return true;
    }
    
    bool canAccessRangeQuickly(unsigned offset, unsigned length)
    {
        return offset <= m_length
            && offset + length <= m_length
            // check overflow
            && offset + length >= offset;
    }
    
    // Like canSetQuickly, except: if it returns false, it will throw the
    // appropriate exception.
    bool validateRange(ExecState*, unsigned offset, unsigned length);
    
    // Returns true if successful, and false on error; if it returns false
    // then it will have thrown an exception.
    bool set(ExecState*, JSObject*, unsigned offset, unsigned length);
    
    PassRefPtr<typename Adaptor::ViewType> typedImpl()
    {
        return Adaptor::ViewType::create(buffer(), byteOffset(), length());
    }
    
    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(typeForTypedArrayType(Adaptor::typeValue), StructureFlags), info(), NonArray);
    }
    
    static const ClassInfo s_info; // This is never accessed directly, since that would break linkage on some compilers.
    
    static const ClassInfo* info()
    {
        switch (Adaptor::typeValue) {
        case TypeInt8:
            return getInt8ArrayClassInfo();
        case TypeInt16:
            return getInt16ArrayClassInfo();
        case TypeInt32:
            return getInt32ArrayClassInfo();
        case TypeUint8:
            return getUint8ArrayClassInfo();
        case TypeUint8Clamped:
            return getUint8ClampedArrayClassInfo();
        case TypeUint16:
            return getUint16ArrayClassInfo();
        case TypeUint32:
            return getUint32ArrayClassInfo();
        case TypeFloat32:
            return getFloat32ArrayClassInfo();
        case TypeFloat64:
            return getFloat64ArrayClassInfo();
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return 0;
        }
    }
    
    ArrayBuffer* existingBuffer();

    static const TypedArrayType TypedArrayStorageType = Adaptor::typeValue;
    
protected:
    friend struct TypedArrayClassInfos;
    
    static const unsigned StructureFlags = OverridesGetPropertyNames | OverridesGetOwnPropertySlot | InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero | Base::StructureFlags;

    static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
    static void put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);
    static bool defineOwnProperty(JSObject*, ExecState*, PropertyName, const PropertyDescriptor&, bool shouldThrow);
    static bool deleteProperty(JSCell*, ExecState*, PropertyName);

    static bool getOwnPropertySlotByIndex(JSObject*, ExecState*, unsigned propertyName, PropertySlot&);
    static void putByIndex(JSCell*, ExecState*, unsigned propertyName, JSValue, bool shouldThrow);
    static bool deletePropertyByIndex(JSCell*, ExecState*, unsigned propertyName);
    
    static void getOwnNonIndexPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
    static void getOwnPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
    
    static void visitChildren(JSCell*, SlotVisitor&);
    static void copyBackingStore(JSCell*, CopyVisitor&, CopyToken);

    // Allocates the full-on native buffer and moves data into the C heap if
    // necessary. Note that this never allocates in the GC heap.
    static ArrayBuffer* slowDownAndWasteMemory(JSArrayBufferView*);
    static PassRefPtr<ArrayBufferView> getTypedArrayImpl(JSArrayBufferView*);

private:
    // Returns true if successful, and false on error; it will throw on error.
    template<typename OtherAdaptor>
    bool setWithSpecificType(
        ExecState*, JSGenericTypedArrayView<OtherAdaptor>*,
        unsigned offset, unsigned length);
};

template<typename Adaptor>
inline PassRefPtr<typename Adaptor::ViewType> toNativeTypedView(JSValue value)
{
    typename Adaptor::JSViewType* wrapper = jsDynamicCast<typename Adaptor::JSViewType*>(value);
    if (!wrapper)
        return 0;
    return wrapper->typedImpl();
}

} // namespace JSC

#endif // JSGenericTypedArrayView_h

