/*
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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

#pragma once

#include "JSArrayBufferView.h"
#include "ThrowScope.h"
#include "ToNativeFromValue.h"
#include <wtf/CheckedArithmetic.h>

namespace JSC {

#define JSC_DECLARE_CLASS_INFO_FUNCTION(type) \
    JS_EXPORT_PRIVATE const ClassInfo* get##type##ArrayClassInfo(); \
    JS_EXPORT_PRIVATE const ClassInfo* getResizableOrGrowableShared##type##ArrayClassInfo();
FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(JSC_DECLARE_CLASS_INFO_FUNCTION)
#undef JSC_DECLARE_CLASS_INFO_FUNCTION

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
//     size_t L
//     TypedArrayMode M
//
// These fields take up a total of five pointer-width words. FIXME: Make
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
//     static int8_t toNativeFromUndefined();
//     static JSValue toJSValue(int8_t);
//     template<T> static T::Type convertTo(uint8_t);
// };

template<typename PassedAdaptor>
class JSGenericTypedArrayView : public JSArrayBufferView {
public:
    using Base = JSArrayBufferView;
    using Adaptor = PassedAdaptor;
    using ElementType = typename Adaptor::Type;
    static constexpr TypedArrayContentType contentType = Adaptor::contentType;

    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesGetOwnPropertyNames | OverridesPut | InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero;

    static constexpr unsigned elementSize = sizeof(typename Adaptor::Type);

    static JSGenericTypedArrayView* create(JSGlobalObject*, Structure*, size_t length);
    static JSGenericTypedArrayView* createWithFastVector(JSGlobalObject*, Structure*, size_t length, void* vector);
    static JSGenericTypedArrayView* createUninitialized(JSGlobalObject*, Structure*, size_t length);
    static JSGenericTypedArrayView* create(JSGlobalObject*, Structure*, RefPtr<ArrayBuffer>&&, size_t byteOffset, std::optional<size_t> length);
    static JSGenericTypedArrayView* create(VM&, Structure*, RefPtr<typename Adaptor::ViewType>&& impl);
    static JSGenericTypedArrayView* create(Structure*, JSGlobalObject*, RefPtr<typename Adaptor::ViewType>&& impl);
    
    size_t byteLength() const
    {
        // https://tc39.es/proposal-resizablearraybuffer/#sec-get-%typedarray%.prototype.bytelength
        if (LIKELY(canUseRawFieldsDirectly()))
            return byteLengthRaw();
        IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
        return integerIndexedObjectByteLength(const_cast<JSGenericTypedArrayView*>(this), getter);
    }

    size_t byteLengthRaw() const { return lengthRaw() << logElementSize(Adaptor::typeValue); }
    
    const typename Adaptor::Type* typedVector() const
    {
        return bitwise_cast<const typename Adaptor::Type*>(vector());
    }
    typename Adaptor::Type* typedVector()
    {
        return bitwise_cast<typename Adaptor::Type*>(vector());
    }

    bool inBounds(size_t i) const
    {
        if (LIKELY(canUseRawFieldsDirectly()))
            return i < lengthRaw();
        size_t bufferByteLength = const_cast<JSGenericTypedArrayView*>(this)->existingBufferInButterfly()->byteLength();
        size_t byteOffset = const_cast<JSGenericTypedArrayView*>(this)->byteOffsetRaw();
        size_t byteLength = byteLengthRaw() + byteOffset; // Keep in mind that byteLengthRaw returns 0 for AutoLength TypedArray.
        if (byteLength > bufferByteLength)
            return false;
        if (isAutoLength()) {
            constexpr size_t logSize = logElementSize(Adaptor::typeValue);
            size_t remainingLength = bufferByteLength - byteOffset;
            return i < (remainingLength >> logSize);
        }
        return i < lengthRaw();
    }

    // These methods are meant to match indexed access methods that JSObject
    // supports - hence the slight redundancy.
    bool canGetIndexQuickly(size_t i) const
    {
        return inBounds(i) && Adaptor::canConvertToJSQuickly;
    }
    bool canSetIndexQuickly(size_t i, JSValue value) const
    {
        return inBounds(i) && value.isNumber() && Adaptor::canConvertToJSQuickly;
    }
    
    typename Adaptor::Type getIndexQuicklyAsNativeValue(size_t i) const
    {
        ASSERT(inBounds(i));
        return typedVector()[i];
    }
    
    JSValue getIndexQuickly(size_t i) const
    {
        return Adaptor::toJSValue(nullptr, getIndexQuicklyAsNativeValue(i));
    }
    
    void setIndexQuicklyToNativeValue(size_t i, typename Adaptor::Type value)
    {
        ASSERT(inBounds(i));
        typedVector()[i] = value;
    }
    
    void setIndexQuickly(size_t i, JSValue value)
    {
        ASSERT(!value.isObject());
        setIndexQuicklyToNativeValue(i, toNativeFromValue<Adaptor>(value));
    }
    
    bool setIndex(JSGlobalObject* globalObject, size_t i, JSValue jsValue)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        typename Adaptor::Type value = toNativeFromValue<Adaptor>(globalObject, jsValue);
        RETURN_IF_EXCEPTION(scope, false);

        if (isDetached())
            return true;

        if (!inBounds(i))
            return false;

        setIndexQuicklyToNativeValue(i, value);
        return true;
    }

    static ElementType toAdaptorNativeFromValue(JSGlobalObject* globalObject, JSValue jsValue)
    {
        return toNativeFromValue<Adaptor>(globalObject, jsValue);
    }

    static std::optional<ElementType> toAdaptorNativeFromValueWithoutCoercion(JSValue jsValue)
    {
        return toNativeFromValueWithoutCoercion<Adaptor>(jsValue);
    }

    bool sort()
    {
        RELEASE_ASSERT(!isDetached());
        switch (Adaptor::typeValue) {
        case TypeFloat32:
            return sortFloat<int32_t>();
        case TypeFloat64:
            return sortFloat<int64_t>();
        default: {
            IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
            auto lengthValue = integerIndexedObjectLength(this, getter);
            if (!lengthValue)
                return false;

            size_t length = lengthValue.value();
            ElementType* array = typedVector();
            std::sort(array, array + length);
            return true;
        }
        }
    }

    bool canAccessRangeQuickly(size_t offset, size_t length)
    {
        return isSumSmallerThanOrEqual(offset, length, this->length());
    }
    
    // Like canSetQuickly, except: if it returns false, it will throw the
    // appropriate exception.
    bool validateRange(JSGlobalObject*, size_t offset, size_t length);

    // Returns true if successful, and false on error; if it returns false
    // then it will have thrown an exception.
    bool setFromTypedArray(JSGlobalObject*, size_t offset, JSArrayBufferView*, size_t objectOffset, size_t length, CopyType);
    bool setFromArrayLike(JSGlobalObject*, size_t offset, JSObject*, size_t objectOffset, size_t length);
    bool setFromArrayLike(JSGlobalObject*, size_t offset, JSValue source);
    
    RefPtr<typename Adaptor::ViewType> possiblySharedTypedImpl();
    RefPtr<typename Adaptor::ViewType> unsharedTypedImpl();

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(typeForTypedArrayType(Adaptor::typeValue), StructureFlags), info(), NonArray);
    }
    
    static const ClassInfo s_info; // This is never accessed directly, since that would break linkage on some compilers.
    
    static const ClassInfo* info()
    {
        switch (Adaptor::typeValue) {
#define JSC_GET_CLASS_INFO(type) \
        case Type##type: return get##type##ArrayClassInfo();
            FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(JSC_GET_CLASS_INFO)
#undef JSC_GET_CLASS_INFO
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return nullptr;
        }
    }

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        switch (Adaptor::typeValue) {
        case TypeInt8:
            return vm.int8ArraySpace<mode>();
        case TypeInt16:
            return vm.int16ArraySpace<mode>();
        case TypeInt32:
            return vm.int32ArraySpace<mode>();
        case TypeUint8:
            return vm.uint8ArraySpace<mode>();
        case TypeUint8Clamped:
            return vm.uint8ClampedArraySpace<mode>();
        case TypeUint16:
            return vm.uint16ArraySpace<mode>();
        case TypeUint32:
            return vm.uint32ArraySpace<mode>();
        case TypeFloat32:
            return vm.float32ArraySpace<mode>();
        case TypeFloat64:
            return vm.float64ArraySpace<mode>();
        case TypeBigInt64:
            return vm.bigInt64ArraySpace<mode>();
        case TypeBigUint64:
            return vm.bigUint64ArraySpace<mode>();
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return nullptr;
        }
    }
    
    ArrayBuffer* existingBuffer();

    static constexpr TypedArrayType TypedArrayStorageType = Adaptor::typeValue;

    // This is the default DOM unwrapping. It calls toUnsharedNativeTypedView().
    static RefPtr<typename Adaptor::ViewType> toWrapped(VM&, JSValue);
    // [AllowShared] annotation allows accepting TypedArray originated from SharedArrayBuffer.
    static RefPtr<typename Adaptor::ViewType> toWrappedAllowShared(VM&, JSValue);
    
protected:
    friend struct TypedArrayClassInfos;

    JSGenericTypedArrayView(VM&, ConstructionContext&);

    static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    static bool defineOwnProperty(JSObject*, JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool shouldThrow);
    static bool deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&);

    static bool getOwnPropertySlotByIndex(JSObject*, JSGlobalObject*, unsigned propertyName, PropertySlot&);
    static bool putByIndex(JSCell*, JSGlobalObject*, unsigned propertyName, JSValue, bool shouldThrow);
    static bool deletePropertyByIndex(JSCell*, JSGlobalObject*, unsigned propertyName);
    
    static void getOwnPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, DontEnumPropertiesMode);

    static size_t estimatedSize(JSCell*, VM&);
    DECLARE_VISIT_CHILDREN;

    // Returns true if successful, and false on error; it will throw on error.
    template<typename OtherAdaptor>
    bool setWithSpecificType(
        JSGlobalObject*, size_t offset, JSGenericTypedArrayView<OtherAdaptor>*,
        size_t objectOffset, size_t length, CopyType);

    // The ECMA 6 spec states that floating point Typed Arrays should have the following ordering:
    //
    // -Inifinity < negative finite numbers < -0.0 < 0.0 < positive finite numbers < Infinity < NaN
    // Note: regardless of the sign or exact representation of a NaN it is greater than all other values.
    //
    // An interesting fact about IEEE 754 floating point numbers is that have an adjacent representation
    // i.e. for any finite floating point x there does not exist a finite floating point y such that
    // ((float) ((int) x + 1)) > y > x (where int represents a signed bit integer with the same number
    // of bits as float). Thus, if we have an array of floating points if we view it as an
    // array of signed bit integers it will sort in the format we desire. Note, denormal
    // numbers fit this property as they are floating point numbers with a exponent field of all
    // zeros so they will be closer to the signed zeros than any normalized number.
    //
    // All the processors we support, however, use twos complement. Fortunately, if you compare a signed
    // bit number as if it were twos complement the result will be correct assuming both numbers are not
    // negative. e.g.
    //
    //    - <=> - = reversed (-30 > -20 = true)
    //    + <=> + = ordered (30 > 20 = true)
    //    - <=> + = ordered (-30 > 20 = false)
    //    + <=> - = ordered (30 > -20 = true)
    //
    // For NaN, we normalize the NaN to a peticular representation; the sign bit is 0, all exponential bits
    // are 1 and only the MSB of the mantissa is 1. So, NaN is recognized as the largest integral numbers.

    template<typename IntegralType>
    bool sortFloat()
    {
        // FIXME: Need to get m_length once.
        ASSERT(sizeof(IntegralType) == sizeof(ElementType));

        IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
        auto lengthValue = integerIndexedObjectLength(this, getter);
        if (!lengthValue)
            return false;

        size_t length = lengthValue.value();

        auto purifyArray = [&]() {
            ElementType* array = typedVector();
            for (size_t i = 0; i < length; i++)
                array[i] = purifyNaN(array[i]);
        };

        // Since there might be another view that sets the bits of
        // our floats to NaNs with negative sign bits we need to
        // purify the array.
        // We use a separate function here to avoid the strict aliasing rule.
        // We could use a union but ASAN seems to frown upon that.
        purifyArray();

        IntegralType* array = reinterpret_cast_ptr<IntegralType*>(typedVector());
        std::sort(array, array + length, [] (IntegralType a, IntegralType b) {
            if (a >= 0 || b >= 0)
                return a < b;
            return a > b;
        });

        return true;
    }
};

template<typename PassedAdaptor>
class JSGenericResizableOrGrowableSharedTypedArrayView final : public JSGenericTypedArrayView<PassedAdaptor> {
public:
    using Base = JSGenericTypedArrayView<PassedAdaptor>;
    using Base::StructureFlags;

    static constexpr bool isResizableOrGrowableSharedTypedArray = true;

    static const ClassInfo s_info; // This is never accessed directly, since that would break linkage on some compilers.

    static const ClassInfo* info()
    {
        switch (Base::Adaptor::typeValue) {
#define JSC_GET_CLASS_INFO(type) \
        case Type##type: return getResizableOrGrowableShared##type##ArrayClassInfo();
            FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(JSC_GET_CLASS_INFO)
#undef JSC_GET_CLASS_INFO
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return nullptr;
        }
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(typeForTypedArrayType(Base::Adaptor::typeValue), StructureFlags), info(), NonArray);
    }
};

template<typename Adaptor>
inline RefPtr<typename Adaptor::ViewType> toPossiblySharedNativeTypedView(VM&, JSValue value)
{
    typename Adaptor::JSViewType* wrapper = jsDynamicCast<typename Adaptor::JSViewType*>(value);
    if (!wrapper)
        return nullptr;
    return wrapper->possiblySharedTypedImpl();
}

template<typename Adaptor>
inline RefPtr<typename Adaptor::ViewType> toUnsharedNativeTypedView(VM& vm, JSValue value)
{
    RefPtr<typename Adaptor::ViewType> result = toPossiblySharedNativeTypedView<Adaptor>(vm, value);
    if (!result || result->isShared())
        return nullptr;
    return result;
}

template<typename Adaptor>
RefPtr<typename Adaptor::ViewType> JSGenericTypedArrayView<Adaptor>::toWrapped(VM& vm, JSValue value)
{
    auto result = JSC::toUnsharedNativeTypedView<Adaptor>(vm, value);
    if (!result || result->isResizableOrGrowableShared())
        return nullptr;
    return result;
}

template<typename Adaptor>
RefPtr<typename Adaptor::ViewType> JSGenericTypedArrayView<Adaptor>::toWrappedAllowShared(VM& vm, JSValue value)
{
    auto result = JSC::toPossiblySharedNativeTypedView<Adaptor>(vm, value);
    if (!result || result->isResizableOrGrowableShared())
        return nullptr;
    return result;
}


} // namespace JSC
