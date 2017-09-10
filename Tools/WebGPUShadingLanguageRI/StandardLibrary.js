/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
"use strict";

// NOTE: The next line is line 28, and we rely on this in Prepare.js.
const standardLibrary = `
// This is the WSL standard library. Implementations of all of these things are in
// Intrinsics.js. The only thing that gets defined before we get here is the primitive
// protocol.

// Need to bootstrap void first.
native primitive typedef void;

native primitive typedef int32;
native primitive typedef uint32;
native primitive typedef bool;
typedef int = int32;
typedef uint = uint32;

native primitive typedef double;

native int operator+(int, int);
native uint operator+(uint, uint);
native int operator-(int, int);
native uint operator-(uint, uint);
native int operator*(int, int);
native uint operator*(uint, uint);
native int operator/(int, int);
native uint operator/(uint, uint);
native bool operator==(int, int);
native bool operator==(uint, uint);
native bool operator==(bool, bool);

protocol Equatable {
    bool operator==(Equatable, Equatable);
}

restricted operator<T> T()
{
    T defaultValue;
    return defaultValue;
}

restricted operator<T> T(T x)
{
    return x;
}

operator<T:Equatable> bool(T x)
{
    return x != T();
}

native thread T^ operator&[]<T>(thread T[], uint);
native threadgroup T^ operator&[]<T:primitive>(threadgroup T[], uint);
native device T^ operator&[]<T:primitive>(device T[], uint);
native constant T^ operator&[]<T:primitive>(constant T[], uint);
`;
