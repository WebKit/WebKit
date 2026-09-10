//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifdef UNSAFE_BUFFERS_BUILD
#pragma allow_unsafe_buffers
#endif

// Color.inc : Inline definitions of some functions from Color.h
namespace angle
{

template <typename T>
Color<T>::Color() : Color(0, 0, 0, 0)
{
}

template <typename T>
constexpr Color<T>::Color(T r, T g, T b, T a) : red(r), green(g), blue(b), alpha(a)
{
}

template <typename T>
bool operator==(const Color<T> &a, const Color<T> &b)
{
    // Use memcmp so that NaN values compare as equal and -0 and +0 are different.
    return memcmp(&a, &b, sizeof(Color<T>)) == 0;
}

template <typename T>
bool operator!=(const Color<T> &a, const Color<T> &b)
{
    return !(a == b);
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const Color<T> &color)
{
    os << "r = " << color.red << ", g = " << color.green << ", b = " << color.blue << ", a = " << color.alpha;
    return os;
}

ColorGeneric::ColorGeneric() : colorF(), type(Type::Float) {}

ColorGeneric::ColorGeneric(const ColorF &color) : colorF(color), type(Type::Float) {}

ColorGeneric::ColorGeneric(const ColorI &color) : colorI(color), type(Type::Int) {}

ColorGeneric::ColorGeneric(const ColorUI &color) : colorUI(color), type(Type::UInt) {}

bool operator==(const ColorGeneric &a, const ColorGeneric &b)
{
    // Switching on the type and comparing individual
    // channels yield much less efficient code.
    return memcmp(&a, &b, sizeof(ColorGeneric)) == 0;
}

bool operator!=(const ColorGeneric &a, const ColorGeneric &b)
{
    return !(a == b);
}

}  // namespace angle
