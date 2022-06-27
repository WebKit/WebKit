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
struct GCGLSpan;
template<typename T, size_t Extent>
struct GCGLSpan {
    explicit GCGLSpan(T* array, size_t size = Extent)
        : data(array)
    {
        ASSERT_UNUSED(size, size == Extent);
    }
    GCGLSpan(T (&array)[Extent])
        : data(array)
    { }
    template<typename U>
    GCGLSpan(const GCGLSpan<U, Extent>& other, std::enable_if_t<std::is_convertible_v<U(*)[], T(*)[]>, std::nullptr_t> = nullptr)
        : data(other.data)
    { }
    GCGLSpan(std::array<T, Extent>& array)
        : data(array.data())
    { }
    GCGLSpan(const std::array<T, Extent>& array)
        : data(array.data())
    { }
    T& operator[](size_t i) { RELEASE_ASSERT(data && i < bufSize); return data[i]; }
    T& operator*() { RELEASE_ASSERT(data && bufSize); return *data; }
    T* data;
    constexpr static size_t bufSize = Extent;
};

template<typename T>
struct GCGLSpan<T, gcGLSpanDynamicExtent> {
    GCGLSpan(T* array, size_t bufSize_)
        : data(array)
        , bufSize(bufSize_)
    { }
    template<size_t Sz>
    GCGLSpan(T (&array)[Sz])
        : data(array)
        , bufSize(Sz)
    { }
    template<typename U, size_t UExtent>
    GCGLSpan(const GCGLSpan<U, UExtent>& other, std::enable_if_t<std::is_convertible_v<U(*)[], T(*)[]>, std::nullptr_t> = nullptr)
        : data(other.data)
        , bufSize(other.bufSize)
    { }
    template<typename U, size_t inlineCapacity>
    GCGLSpan(Vector<U, inlineCapacity>& array, std::enable_if_t<std::is_convertible_v<U(*)[], T(*)[]>, std::nullptr_t> = nullptr)
        : data(array.data())
        , bufSize(array.size())
    { }
    T& operator[](size_t i) { RELEASE_ASSERT(data && i < bufSize); return data[i]; }
    T& operator*() { RELEASE_ASSERT(data && bufSize); return *data; }
    T* data;
    const size_t bufSize;
};

template<>
struct GCGLSpan<GCGLvoid> {
    GCGLSpan(GCGLvoid* array, size_t bufSize_)
        : data(array)
        , bufSize(bufSize_)
    { }
    template<typename U, size_t Sz>
    GCGLSpan(U (&array)[Sz])
        : data(array)
        , bufSize(Sz * sizeof(U))
    { }
    template<typename U, size_t UExtent>
    GCGLSpan(const GCGLSpan<U, UExtent>& other)
        : data(other.data)
        , bufSize(other.bufSize * sizeof(U))
    { }
    template<typename U, size_t inlineCapacity>
    GCGLSpan(Vector<U, inlineCapacity>& array)
        : data(array.data())
        , bufSize(array.size() * sizeof(U))
    { }
    GCGLvoid* data;
    const size_t bufSize;
};

template<>
struct GCGLSpan<const GCGLvoid> {
    GCGLSpan(const GCGLvoid* array, size_t bufSize_)
        : data(array)
        , bufSize(bufSize_)
    { }
    template<typename U, size_t Sz>
    GCGLSpan(U (&array)[Sz])
        : data(array)
        , bufSize(Sz * sizeof(U))
    { }
    GCGLSpan(const GCGLSpan<GCGLvoid>& other)
        : data(other.data)
        , bufSize(other.bufSize)
    { }
    template<typename U, size_t UExtent>
    GCGLSpan(const GCGLSpan<U, UExtent>& other)
        : data(other.data)
        , bufSize(other.bufSize * sizeof(U))
    { }
    const GCGLvoid* data;
    const size_t bufSize;
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
struct GCGLSpanTuple;

template<typename T0, typename T1>
struct GCGLSpanTuple<T0, T1> {
    GCGLSpanTuple(T0* data0_, T1* data1_, size_t bufSize_)
        : bufSize(bufSize_)
        , data0(data0_)
        , data1(data1_)
    {
    }
    template<typename U0, typename U1>
    GCGLSpanTuple(const Vector<U0>& data0_, const Vector<U1>& data1_)
        : bufSize(data0_.size())
        , data0(data0_.data())
        , data1(data1_.data())
    {
        ASSERT(data0_.size() == data1_.size());
    }
    const size_t bufSize;
    T0* const data0;
    T1* const data1;
};

template<typename T0, typename T1, typename T2>
struct GCGLSpanTuple<T0, T1, T2> : public GCGLSpanTuple<T0, T1> {
    GCGLSpanTuple(T0* data0_, T1* data1_, T2* data2_, size_t bufSize_)
        : GCGLSpanTuple<T0, T1>(data0_, data1_, bufSize_)
        , data2(data2_)
    {
    }
    template<typename U0, typename U1, typename U2>
    GCGLSpanTuple(const Vector<U0>& data0_, const Vector<U1>& data1_, const Vector<U2>& data2_)
        : GCGLSpanTuple<T0, T1>(data0_, data1_)
        , data2(data2_.data())
    {
        ASSERT(data2_.size() == data0_.size());
    }
    T2* const data2;
};

template<typename T0, typename T1, typename T2, typename T3>
struct GCGLSpanTuple<T0, T1, T2, T3> : public GCGLSpanTuple<T0, T1, T2> {
    GCGLSpanTuple(T0* data0_, T1* data1_, T2* data2_, T3* data3_, size_t bufSize_)
        : GCGLSpanTuple<T0, T1, T2>(data0_, data1_, data2_, bufSize_)
        , data3(data3_)
    {
    }
    template<typename U0, typename U1, typename U2, typename U3>
    GCGLSpanTuple(const Vector<U0>& data0_, const Vector<U1>& data1_, const Vector<U2>& data2_, const Vector<U3>& data3_)
        : GCGLSpanTuple<T0, T1, T2>(data0_, data1_, data2_)
        , data3(data3_.data())
    {
        ASSERT(data3_.size() == data0_.size());
    }
    T3* const data3;
};

template<typename T0, typename T1, typename T2, typename T3, typename T4>
struct GCGLSpanTuple<T0, T1, T2, T3, T4> : public GCGLSpanTuple<T0, T1, T2, T3> {
    GCGLSpanTuple(T0* data0_, T1* data1_, T2* data2_, T3* data3_, T4* data4_, size_t bufSize_)
        : GCGLSpanTuple<T0, T1, T2, T3>(data0_, data1_, data2_, data3_, bufSize_)
        , data4(data4_)
    {
    }
    template<typename U0, typename U1, typename U2, typename U3, typename U4>
    GCGLSpanTuple(const Vector<U0>& data0_, const Vector<U1>& data1_, const Vector<U2>& data2_, const Vector<U3>& data3_, const Vector<U4>& data4_)
        : GCGLSpanTuple<T0, T1, T2, T3>(data0_, data1_, data2_, data3_)
        , data4(data4_.data())
    {
        ASSERT(data4_.size() == data0_.size());
    }
    T4* const data4;
};

template<typename T0, typename T1>
GCGLSpanTuple(T0*, T1*, size_t) -> GCGLSpanTuple<T0, T1>;

template<typename T0, typename T1>
GCGLSpanTuple(const Vector<T0>&, const Vector<T1>&) -> GCGLSpanTuple<const T0, const T1>;

template<typename T0, typename T1, typename T2>
GCGLSpanTuple(T0*, T1*, T2*, size_t) -> GCGLSpanTuple<T0, T1, T2>;

template<typename T0, typename T1, typename T2>
GCGLSpanTuple(const Vector<T0>&, const Vector<T1>&, const Vector<T2>&) -> GCGLSpanTuple<const T0, const T1, const T2>;

template<typename T0, typename T1, typename T2, typename T3>
GCGLSpanTuple(T0*, T1*, T2*, T3*, size_t) -> GCGLSpanTuple<T0, T1, T2, T3>;

template<typename T0, typename T1, typename T2, typename T3>
GCGLSpanTuple(const Vector<T0>&, const Vector<T1>&, const Vector<T2>&, const Vector<T3>&) -> GCGLSpanTuple<const T0, const T1, const T2, const T3>;

template<typename T0, typename T1, typename T2, typename T3, typename T4>
GCGLSpanTuple(T0*, T1*, T2*, T3*, T4*, size_t) -> GCGLSpanTuple<T0, T1, T2, T3, T4>;

template<typename T0, typename T1, typename T2, typename T3, typename T4>
GCGLSpanTuple(const Vector<T0>&, const Vector<T1>&, const Vector<T2>&, const Vector<T3>&, const Vector<T4>&) -> GCGLSpanTuple<const T0, const T1, const T2, const T3, const T4>;
