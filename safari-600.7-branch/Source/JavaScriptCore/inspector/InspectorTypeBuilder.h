/*
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 The Chromium Authors. All rights reserved.
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

#ifndef InspectorTypeBuilder_h
#define InspectorTypeBuilder_h

#if ENABLE(INSPECTOR)

#include "InspectorValues.h"
#include <wtf/Assertions.h>
#include <wtf/PassRefPtr.h>

namespace Inspector {

namespace TypeBuilder {

template<typename T>
class OptOutput {
public:
    OptOutput() : m_assigned(false) { }

    void operator=(T value)
    {
        m_value = value;
        m_assigned = true;
    }

    bool isAssigned() const { return m_assigned; }

    T getValue()
    {
        ASSERT(isAssigned());
        return m_value;
    }

private:
    T m_value;
    bool m_assigned;

    WTF_MAKE_NONCOPYABLE(OptOutput);
};


// A small transient wrapper around int type, that can be used as a funciton parameter type
// cleverly disallowing C++ implicit casts from float or double.
class ExactlyInt {
public:
    template<typename T>
    ExactlyInt(T t) : m_value(cast_to_int<T>(t)) { }
    ExactlyInt() { }

    operator int() { return m_value; }

private:
    int m_value;

    template<typename T>
    static int cast_to_int(T) { return T::default_case_cast_is_not_supported(); }
};

template<>
inline int ExactlyInt::cast_to_int<int>(int i) { return i; }

template<>
inline int ExactlyInt::cast_to_int<unsigned int>(unsigned int i) { return i; }

#if !ASSERT_DISABLED
class RuntimeCastHelper {
public:
    template<InspectorValue::Type TYPE>
    static void assertType(InspectorValue* value)
    {
        ASSERT(value->type() == TYPE);
    }

    static void assertAny(InspectorValue*)
    {
    }

    static void assertInt(InspectorValue* value)
    {
        double v;
        bool castRes = value->asNumber(&v);
        ASSERT_UNUSED(castRes, castRes);
        ASSERT(static_cast<double>(static_cast<int>(v)) == v);
    }
};
#endif


// This class provides "Traits" type for the input type T. It is programmed using C++ template specialization
// technique. By default it simply takes "ItemTraits" type from T, but it doesn't work with the base types.
template<typename T>
struct ArrayItemHelper {
    typedef typename T::ItemTraits Traits;
};

template<typename T>
class Array : public InspectorArrayBase {
private:
    Array() { }

    InspectorArray* openAccessors()
    {
        COMPILE_ASSERT(sizeof(InspectorArray) == sizeof(Array<T>), cannot_cast);
        return static_cast<InspectorArray*>(static_cast<InspectorArrayBase*>(this));
    }

public:
    void addItem(PassRefPtr<T> value)
    {
        ArrayItemHelper<T>::Traits::pushRefPtr(this->openAccessors(), value);
    }

    void addItem(T value)
    {
        ArrayItemHelper<T>::Traits::pushRaw(this->openAccessors(), value);
    }

    static PassRefPtr<Array<T>> create()
    {
        return adoptRef(new Array<T>());
    }

    static PassRefPtr<Array<T>> runtimeCast(PassRefPtr<InspectorValue> value)
    {
        RefPtr<InspectorArray> array;
        bool castRes = value->asArray(&array);
        ASSERT_UNUSED(castRes, castRes);
#if !ASSERT_DISABLED
        assertCorrectValue(array.get());
#endif // !ASSERT_DISABLED
        COMPILE_ASSERT(sizeof(Array<T>) == sizeof(InspectorArray), type_cast_problem);
        return static_cast<Array<T>*>(static_cast<InspectorArrayBase*>(array.get()));
    }

#if !ASSERT_DISABLED
    static void assertCorrectValue(InspectorValue* value)
    {
        RefPtr<InspectorArray> array;
        bool castRes = value->asArray(&array);
        ASSERT_UNUSED(castRes, castRes);
        for (unsigned i = 0; i < array->length(); i++)
            ArrayItemHelper<T>::Traits::template assertCorrectValue<T>(array->get(i).get());
    }
#endif // !ASSERT_DISABLED
};

struct StructItemTraits {
    static void pushRefPtr(InspectorArray* array, PassRefPtr<InspectorValue> value)
    {
        array->pushValue(value);
    }

#if !ASSERT_DISABLED
    template<typename T>
    static void assertCorrectValue(InspectorValue* value)
    {
        T::assertCorrectValue(value);
    }
#endif // !ASSERT_DISABLED
};

template<>
struct ArrayItemHelper<String> {
    struct Traits {
        static void pushRaw(InspectorArray* array, const String& value)
        {
            array->pushString(value);
        }

#if !ASSERT_DISABLED
        template<typename T>
        static void assertCorrectValue(InspectorValue* value)
        {
            RuntimeCastHelper::assertType<InspectorValue::TypeString>(value);
        }
#endif // !ASSERT_DISABLED
    };
};

template<>
struct ArrayItemHelper<int> {
    struct Traits {
        static void pushRaw(InspectorArray* array, int value)
        {
            array->pushInt(value);
        }

#if !ASSERT_DISABLED
        template<typename T>
        static void assertCorrectValue(InspectorValue* value)
        {
            RuntimeCastHelper::assertInt(value);
        }
#endif // !ASSERT_DISABLED
    };
};

template<>
struct ArrayItemHelper<double> {
    struct Traits {
        static void pushRaw(InspectorArray* array, double value)
        {
            array->pushNumber(value);
        }

#if !ASSERT_DISABLED
        template<typename T>
        static void assertCorrectValue(InspectorValue* value)
        {
            RuntimeCastHelper::assertType<InspectorValue::TypeNumber>(value);
        }
#endif // !ASSERT_DISABLED
    };
};

template<>
struct ArrayItemHelper<bool> {
    struct Traits {
        static void pushRaw(InspectorArray* array, bool value)
        {
            array->pushBoolean(value);
        }

#if !ASSERT_DISABLED
        template<typename T>
        static void assertCorrectValue(InspectorValue* value)
        {
            RuntimeCastHelper::assertType<InspectorValue::TypeBoolean>(value);
        }
#endif // !ASSERT_DISABLED
    };
};

template<>
struct ArrayItemHelper<InspectorValue> {
    struct Traits {
        static void pushRefPtr(InspectorArray* array, PassRefPtr<InspectorValue> value)
        {
            array->pushValue(value);
        }

#if !ASSERT_DISABLED
        template<typename T>
        static void assertCorrectValue(InspectorValue* value)
        {
            RuntimeCastHelper::assertAny(value);
        }
#endif // !ASSERT_DISABLED
    };
};

template<>
struct ArrayItemHelper<InspectorObject> {
    struct Traits {
        static void pushRefPtr(InspectorArray* array, PassRefPtr<InspectorValue> value)
        {
            array->pushValue(value);
        }

#if !ASSERT_DISABLED
        template<typename T>
        static void assertCorrectValue(InspectorValue* value)
        {
            RuntimeCastHelper::assertType<InspectorValue::TypeObject>(value);
        }
#endif // !ASSERT_DISABLED
    };
};

template<>
struct ArrayItemHelper<InspectorArray> {
    struct Traits {
        static void pushRefPtr(InspectorArray* array, PassRefPtr<InspectorArray> value)
        {
            array->pushArray(value);
        }

#if !ASSERT_DISABLED
        template<typename T>
        static void assertCorrectValue(InspectorValue* value)
        {
            RuntimeCastHelper::assertType<InspectorValue::TypeArray>(value);
        }
#endif // !ASSERT_DISABLED
    };
};

template<typename T>
struct ArrayItemHelper<TypeBuilder::Array<T>> {
    struct Traits {
        static void pushRefPtr(InspectorArray* array, PassRefPtr<TypeBuilder::Array<T>> value)
        {
            array->pushValue(value);
        }

#if !ASSERT_DISABLED
        template<typename S>
        static void assertCorrectValue(InspectorValue* value)
        {
            S::assertCorrectValue(value);
        }
#endif // !ASSERT_DISABLED
    };
};

} // namespace TypeBuilder

} // namespace Inspector

#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorTypeBuilder_h)
