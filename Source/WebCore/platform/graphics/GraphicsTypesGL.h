/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#include <JavaScriptCore/ArrayBufferView.h>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <wtf/Forward.h>
#include <wtf/Vector.h>

// GCGL types match the corresponding GL types as defined in OpenGL ES 2.0
// header file gl2.h from khronos.org.
typedef unsigned GCGLenum;
typedef unsigned char GCGLboolean;
typedef unsigned GCGLbitfield;
typedef signed char GCGLbyte;
typedef unsigned char GCGLubyte;
typedef short GCGLshort;
typedef unsigned short GCGLushort;
typedef int GCGLint;
typedef int GCGLsizei;
typedef unsigned GCGLuint;
typedef float GCGLfloat;
typedef unsigned short GCGLhalffloat;
typedef float GCGLclampf;
typedef char GCGLchar;
typedef void* GCGLsync;
typedef void GCGLvoid;

// These GCGL types do not strictly match the GL types as defined in OpenGL ES 2.0
// header file for all platforms.
typedef intptr_t GCGLintptr;
typedef intptr_t GCGLsizeiptr;
typedef intptr_t GCGLvoidptr;
typedef int64_t GCGLint64;
typedef uint64_t GCGLuint64;

typedef GCGLuint PlatformGLObject;
using GCGLDisplay = void*;
using GCGLConfig = void*;
using GCGLContext = void*;

#if !PLATFORM(COCOA)
typedef unsigned GLuint;
#endif

inline constexpr size_t gcGLSpanDynamicExtent = std::numeric_limits<size_t>::max();

template<typename T, size_t Extent = gcGLSpanDynamicExtent>
class GCGLSpan;
template<typename T, size_t Extent>
class GCGLSpan {
public:
    explicit GCGLSpan(T* array, size_t size = Extent)
        : m_data(array)
    {
        ASSERT_UNUSED(size, size == Extent);
    }
    GCGLSpan(T (&array)[Extent])
        : m_data(array)
    { }
    template<typename U>
    GCGLSpan(const GCGLSpan<U, Extent>& other, std::enable_if_t<std::is_convertible_v<U(*)[], T(*)[]>, std::nullptr_t> = nullptr)
        : m_data(other.m_data)
    { }
    GCGLSpan(std::array<T, Extent>& array)
        : m_data(array.data())
    { }
    GCGLSpan(const std::array<T, Extent>& array)
        : m_data(array.data())
    { }
    T* data() const noexcept { return m_data; }
    size_t size() const noexcept { return m_bufSize; }
    T& operator[](size_t i) { RELEASE_ASSERT(m_data && i < m_bufSize); return m_data[i]; }
    T& operator*() { RELEASE_ASSERT(m_data && m_bufSize); return *m_data; }
private:
    T* m_data;
    constexpr static size_t m_bufSize = Extent;
};

template<typename T>
class GCGLSpan<T, gcGLSpanDynamicExtent> {
public:
    GCGLSpan(T* array, size_t bufSize)
        : m_data(array)
        , m_bufSize(bufSize)
    { }
    template<size_t Sz>
    GCGLSpan(T (&array)[Sz])
        : GCGLSpan(array, Sz)
    { }
    template<typename U, size_t UExtent>
    GCGLSpan(const GCGLSpan<U, UExtent>& other, std::enable_if_t<std::is_convertible_v<U(*)[], T(*)[]>, std::nullptr_t> = nullptr)
        : GCGLSpan(other.data(), other.size())
    { }
    template<typename U, size_t inlineCapacity>
    GCGLSpan(Vector<U, inlineCapacity>& array, std::enable_if_t<std::is_convertible_v<U(*)[], T(*)[]>, std::nullptr_t> = nullptr)
        : GCGLSpan(array.data(), array.size())
    { }
    GCGLSpan(const ArrayBufferView& view)
        : GCGLSpan(view.baseAddress(), view.byteLength())
    { }
    T* data() const noexcept { return m_data; }
    size_t size() const noexcept { return m_bufSize; }
    T& operator[](size_t i) { RELEASE_ASSERT(m_data && i < m_bufSize); return m_data[i]; }
    T& operator*() { RELEASE_ASSERT(m_data && m_bufSize); return *m_data; }
private:
    T* m_data;
    size_t m_bufSize;
};

template<>
class GCGLSpan<GCGLvoid> {
public:
    GCGLSpan(GCGLvoid* array, size_t bufSize)
        : m_data(array)
        , m_bufSize(bufSize)
    { }
    template<typename U, size_t Sz>
    GCGLSpan(U (&array)[Sz])
        : GCGLSpan(array, Sz * sizeof(U))
    { }
    GCGLSpan(const GCGLSpan<GCGLvoid>& other)
        : GCGLSpan(other.data(), other.size())
    { }
    template<typename U, size_t UExtent>
    GCGLSpan(const GCGLSpan<U, UExtent>& other)
        : GCGLSpan(other.data(), other.size() * sizeof(U))
    { }
    template<typename U, size_t inlineCapacity>
    GCGLSpan(Vector<U, inlineCapacity>& array)
        : GCGLSpan(array.data(), array.size() * sizeof(U))
    { }
    GCGLSpan(const ArrayBufferView& view)
        : GCGLSpan(view.baseAddress(), view.byteLength())
    { }
    void* data() const noexcept { return m_data; }
    size_t size() const noexcept { return m_bufSize; }
private:
    GCGLvoid* m_data;
    size_t m_bufSize;
};

template<>
class GCGLSpan<const GCGLvoid> {
public:
    GCGLSpan(const GCGLvoid* array, size_t bufSize)
        : m_data(array)
        , m_bufSize(bufSize)
    { }
    template<typename U, size_t Sz>
    GCGLSpan(U (&array)[Sz])
        : GCGLSpan(array, Sz * sizeof(U))
    { }
    GCGLSpan(const GCGLSpan<GCGLvoid>& other)
        : GCGLSpan(other.data(), other.size())
    { }
    template<typename U, size_t UExtent>
    GCGLSpan(const GCGLSpan<U, UExtent>& other)
        : GCGLSpan(other.data(), other.size() * sizeof(U))
    { }
    GCGLSpan(const ArrayBufferView& view)
        : GCGLSpan(view.baseAddress(), view.byteLength())
    { }
    const void* data() const noexcept { return m_data; }
    size_t size() const noexcept { return m_bufSize; }
private:
    const GCGLvoid* m_data;
    size_t m_bufSize;
};

template<typename T, size_t N>
GCGLSpan(T (&)[N]) -> GCGLSpan<T, N>;

template<typename T, size_t N>
GCGLSpan(std::array<T, N>&) -> GCGLSpan<T, N>;

template<typename T, size_t inlineCapacity>
GCGLSpan(Vector<T, inlineCapacity>&) -> GCGLSpan<T>;

template<typename T>
GCGLSpan<T> makeGCGLSpan(T* data, size_t count)
{
    return GCGLSpan<T> { data, count };
}

template<size_t Extent, typename T>
GCGLSpan<T, Extent> makeGCGLSpan(T* data)
{
    return GCGLSpan<T, Extent> { data };
}

template<typename... Types>
struct GCGLSpanTuple {
    GCGLSpanTuple(Types*... dataPointers, size_t bufSize)
        : bufSize { bufSize }
        , dataTuple { dataPointers... }
    { }

    template<typename... VectorTypes>
    GCGLSpanTuple(const Vector<VectorTypes>&... dataVectors)
        : bufSize(
            [](auto& firstVector, auto&... otherVectors) {
                auto size = firstVector.size();
                RELEASE_ASSERT(((otherVectors.size() == size) && ...));
                return size;
            }(dataVectors...))
        , dataTuple { dataVectors.data()... }
    { }

    template<unsigned I>
    auto data() const
    {
        return std::get<I>(dataTuple);
    }

    const size_t bufSize;
    std::tuple<Types*...> dataTuple;
};

template<typename T0, typename T1>
GCGLSpanTuple(T0*, T1*, size_t) -> GCGLSpanTuple<T0, T1>;

template<typename T0, typename T1, typename T2>
GCGLSpanTuple(T0*, T1*, T2*, size_t) -> GCGLSpanTuple<T0, T1, T2>;

template<typename T0, typename T1, typename T2, typename T3>
GCGLSpanTuple(T0*, T1*, T2*, T3*, size_t) -> GCGLSpanTuple<T0, T1, T2, T3>;

template<typename T0, typename T1, typename T2, typename T3, typename T4>
GCGLSpanTuple(T0*, T1*, T2*, T3*, T4*, size_t) -> GCGLSpanTuple<T0, T1, T2, T3, T4>;

template<typename T0, typename T1>
GCGLSpanTuple(const Vector<T0>&, const Vector<T1>&) -> GCGLSpanTuple<const T0, const T1>;

template<typename T0, typename T1, typename T2>
GCGLSpanTuple(const Vector<T0>&, const Vector<T1>&, const Vector<T2>&) -> GCGLSpanTuple<const T0, const T1, const T2>;

template<typename T0, typename T1, typename T2, typename T3>
GCGLSpanTuple(const Vector<T0>&, const Vector<T1>&, const Vector<T2>&, const Vector<T3>&) -> GCGLSpanTuple<const T0, const T1, const T2, const T3>;

template<typename T0, typename T1, typename T2, typename T3, typename T4>
GCGLSpanTuple(const Vector<T0>&, const Vector<T1>&, const Vector<T2>&, const Vector<T3>&, const Vector<T4>&) -> GCGLSpanTuple<const T0, const T1, const T2, const T3, const T4>;
