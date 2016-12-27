//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Color.h : Defines the Color type used throughout the ANGLE libraries

#ifndef COMMON_COLOR_H_
#define COMMON_COLOR_H_

namespace angle
{

template <typename T>
struct Color
{
    T red;
    T green;
    T blue;
    T alpha;

    Color();
    Color(T r, T g, T b, T a);
};

template <typename T>
bool operator==(const Color<T> &a, const Color<T> &b);

template <typename T>
bool operator!=(const Color<T> &a, const Color<T> &b);

typedef Color<float> ColorF;
typedef Color<int> ColorI;
typedef Color<unsigned int> ColorUI;

}  // namespace angle

// TODO: Move this fully into the angle namespace
namespace gl
{

template <typename T>
using Color   = angle::Color<T>;
using ColorF  = angle::ColorF;
using ColorI  = angle::ColorI;
using ColorUI = angle::ColorUI;

}  // namespace gl

#include "Color.inl"

#endif  // COMMON_COLOR_H_
