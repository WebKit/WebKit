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

using GCGLNativeDisplayType = int;

inline constexpr GCGLNativeDisplayType gcGLDefaultDisplay = 0;

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
