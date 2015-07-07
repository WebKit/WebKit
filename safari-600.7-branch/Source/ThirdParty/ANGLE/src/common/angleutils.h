//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// angleutils.h: Common ANGLE utilities.

#ifndef COMMON_ANGLEUTILS_H_
#define COMMON_ANGLEUTILS_H_

#include <stddef.h>

// A macro to disallow the copy constructor and operator= functions
// This must be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

template <typename T, unsigned int N>
inline unsigned int ArraySize(T(&)[N])
{
    return N;
}

template <typename T, unsigned int N>
void SafeRelease(T (&resourceBlock)[N])
{
    for (unsigned int i = 0; i < N; i++)
    {
        SafeRelease(resourceBlock[i]);
    }
}

template <typename T>
void SafeRelease(T& resource)
{
    if (resource)
    {
        resource->Release();
        resource = NULL;
    }
}

template <typename T>
void SafeDelete(T*& resource)
{
    delete resource;
    resource = NULL;
}

template <typename T>
void SafeDeleteArray(T*& resource)
{
    delete[] resource;
    resource = NULL;
}

// Provide a less-than function for comparing structs
// Note: struct memory must be initialized to zero, because of packing gaps
template <typename T>
inline bool StructLessThan(const T &a, const T &b)
{
    return (memcmp(&a, &b, sizeof(T)) < 0);
}

// Provide a less-than function for comparing structs
// Note: struct memory must be initialized to zero, because of packing gaps
template <typename T>
inline bool StructEquals(const T &a, const T &b)
{
    return (memcmp(&a, &b, sizeof(T)) == 0);
}

template <typename T>
inline void StructZero(T *obj)
{
    memset(obj, 0, sizeof(T));
}

#if defined(_MSC_VER)
#define snprintf _snprintf
#endif

#define VENDOR_ID_AMD 0x1002
#define VENDOR_ID_INTEL 0x8086
#define VENDOR_ID_NVIDIA 0x10DE

#define GL_BGRA4_ANGLEX 0x6ABC
#define GL_BGR5_A1_ANGLEX 0x6ABD
#define GL_INT_64_ANGLEX 0x6ABE
#define GL_STRUCT_ANGLEX 0x6ABF

#endif // COMMON_ANGLEUTILS_H_
