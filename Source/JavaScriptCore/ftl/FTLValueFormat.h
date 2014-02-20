/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef FTLValueFormat_h
#define FTLValueFormat_h

#if ENABLE(FTL_JIT)

#include "GPRInfo.h"
#include <wtf/PrintStream.h>

namespace JSC {

class AssemblyHelpers;

namespace FTL {

// Note that this is awkwardly similar to DataFormat in other parts of JSC, except that
// unlike DataFormat and like ValueRecovery, it distinguishes between UInt32 and Int32.

enum ValueFormat {
    InvalidValueFormat,
    ValueFormatInt32,
    ValueFormatInt52,
    ValueFormatStrictInt52,
    ValueFormatBoolean,
    ValueFormatJSValue,
    ValueFormatDouble
};

void reboxAccordingToFormat(
    ValueFormat, AssemblyHelpers&, GPRReg value, GPRReg scratch1, GPRReg scratch2);

} } // namespace JSC::FTL

namespace WTF {

void printInternal(PrintStream&, JSC::FTL::ValueFormat);

} // namespace WTF

#endif // ENABLE(FTL_JIT)

#endif // FTLValueFormat_h

