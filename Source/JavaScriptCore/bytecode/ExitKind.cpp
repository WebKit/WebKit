/*
 * Copyright (C) 2012-2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ExitKind.h"

#include <wtf/Assertions.h>
#include <wtf/PrintStream.h>
#include <wtf/text/ASCIILiteral.h>

namespace JSC {

ASCIILiteral exitKindToString(ExitKind kind)
{
    switch (kind) {
    case ExitKindUnset:
        return "Unset"_s;
    case BadType:
        return "BadType"_s;
    case BadConstantValue:
        return "BadConstantValue"_s;
    case BadIdent:
        return "BadIdent"_s;
    case BadExecutable:
        return "BadExecutable"_s;
    case BadCache:
        return "BadCache"_s;
    case BadConstantCache:
        return "BadConstantCache"_s;
    case BadIndexingType:
        return "BadIndexingType"_s;
    case BadTypeInfoFlags:
        return "BadTypeInfoFlags"_s;
    case Overflow:
        return "Overflow"_s;
    case NegativeZero:
        return "NegativeZero"_s;
    case NegativeIndex:
        return "NegativeIndex"_s;
    case Int52Overflow:
        return "Int52Overflow"_s;
    case StoreToHole:
        return "StoreToHole"_s;
    case LoadFromHole:
        return "LoadFromHole"_s;
    case OutOfBounds:
        return "OutOfBounds"_s;
    case InadequateCoverage:
        return "InadequateCoverage"_s;
    case ArgumentsEscaped:
        return "ArgumentsEscaped"_s;
    case ExoticObjectMode:
        return "ExoticObjectMode"_s;
    case VarargsOverflow:
        return "VarargsOverflow"_s;
    case TDZFailure:
        return "TDZFailure"_s;
    case HoistingFailed:
        return "HoistingFailed"_s;
    case Uncountable:
        return "Uncountable"_s;
    case UncountableInvalidation:
        return "UncountableInvalidation"_s;
    case WatchdogTimerFired:
        return "WatchdogTimerFired"_s;
    case DebuggerEvent:
        return "DebuggerEvent"_s;
    case ExceptionCheck:
        return "ExceptionCheck"_s;
    case GenericUnwind:
        return "GenericUnwind"_s;
    case BigInt32Overflow:
        return "BigInt32Overflow"_s;
    case UnexpectedResizableArrayBufferView:
        return "UnexpectedResizableArrayBufferView"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return "Unknown"_s;
}

bool exitKindMayJettison(ExitKind kind)
{
    switch (kind) {
    case ExceptionCheck:
    case GenericUnwind:
        return false;
    default:
        return true;
    }
}

} // namespace JSC

namespace WTF {

void printInternal(PrintStream& out, JSC::ExitKind kind)
{
    out.print(exitKindToString(kind));
}

} // namespace WTF

