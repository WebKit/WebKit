/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

/*
 A system of macros to make it more ergonomic to implement C++ class member functions in Swift.

 Use HAS_SWIFTCXX_THUNK (from WebGPUExt.h) in headers, to mark a C++ member function as having
 a specialized definition in Swift.

 Use DEFINE_SWIFTCXX_THUNK in implementation sources, to define a function that calls through to a
 "<Class>_<Member>_thunk" free function. These functions are expected to be implemented in Swift and
 available through reverse interop.
 */

#define _DEFINE_SWIFTCXX_THUNK0(Class, Member, ReturnType) \
ReturnType Class::Member() { \
    return Class ## _ ## Member ## _thunk(this); \
}

#define _DEFINE_SWIFTCXX_THUNK1(Class, Member, ReturnType, TypeOfArg1) \
ReturnType Class::Member(TypeOfArg1 arg1) { \
    return Class ## _ ## Member ## _thunk(this, arg1); \
}

#define _DEFINE_SWIFTCXX_THUNK2(Class, Member, ReturnType, TypeOfArg1, TypeOfArg2) \
ReturnType Class::Member(TypeOfArg1 arg1, TypeOfArg2 arg2) { \
    return Class ## _ ## Member ## _thunk(this, arg1, arg2); \
}

#define _DEFINE_SWIFTCXX_THUNK3(Class, Member, ReturnType, TypeOfArg1, TypeOfArg2, TypeOfArg3) \
ReturnType Class::Member(TypeOfArg1 arg1, TypeOfArg2 arg2, TypeOfArg3 arg3) { \
    return Class ## _ ## Member ## _thunk(this, arg1, arg2, arg3); \
}

#define _DEFINE_SWIFTCXX_THUNK4(Class, Member, ReturnType, TypeOfArg1, TypeOfArg2, TypeOfArg3, TypeOfArg4) \
ReturnType Class::Member(TypeOfArg1 arg1, TypeOfArg2 arg2, TypeOfArg3 arg3, TypeOfArg4 arg4) { \
    return Class ## _ ## Member ## _thunk(this, arg1, arg2, arg3, arg4); \
}

#define _DEFINE_SWIFTCXX_THUNK5(Class, Member, ReturnType, TypeOfArg1, TypeOfArg2, TypeOfArg3, TypeOfArg4, TypeOfArg5) \
ReturnType Class::Member(TypeOfArg1 arg1, TypeOfArg2 arg2, TypeOfArg3 arg3, TypeOfArg4 arg4, TypeOfArg5 arg5) { \
    return Class ## _ ## Member ## _thunk(this, arg1, arg2, arg3, arg4, arg5); \
}

#define _GET_NTH_ARG(_1, _2, _3, _4, _5, NAME, ...) NAME

#define DEFINE_SWIFTCXX_THUNK(Class, Member, ReturnType, ...) \
    _GET_NTH_ARG(__VA_ARGS__, _DEFINE_SWIFTCXX_THUNK5, _DEFINE_SWIFTCXX_THUNK4, _DEFINE_SWIFTCXX_THUNK3, _DEFINE_SWIFTCXX_THUNK2, _DEFINE_SWIFTCXX_THUNK1, _DEFINE_SWIFTCXX_THUNK0)(Class, Member, ReturnType, ##__VA_ARGS__)
