/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef FTLCommonValues_h
#define FTLCommonValues_h

#include <wtf/Platform.h>

#if ENABLE(FTL_JIT)

#include "FTLAbbreviations.h"
#include "FTLValueRange.h"

namespace JSC { namespace FTL {

class CommonValues {
public:
    CommonValues(LContext context);
    
    void initialize(LModule module)
    {
        m_module = module;
    }
    
    const LType voidType;
    const LType boolean;
    const LType int8;
    const LType int16;
    const LType int32;
    const LType int64;
    const LType intPtr;
    const LType floatType;
    const LType doubleType;
    const LType ref8;
    const LType ref16;
    const LType ref32;
    const LType ref64;
    const LType refPtr;
    const LType refFloat;
    const LType refDouble;
    const LValue booleanTrue;
    const LValue booleanFalse;
    const LValue int8Zero;
    const LValue int32Zero;
    const LValue int32One;
    const LValue int64Zero;
    const LValue intPtrZero;
    const LValue intPtrOne;
    const LValue intPtrTwo;
    const LValue intPtrThree;
    const LValue intPtrFour;
    const LValue intPtrEight;
    const LValue intPtrPtr;
    const LValue doubleZero;
    
    const unsigned rangeKind;
    
    const ValueRange nonNegativeInt32;
    
    LContext const m_context;
    LModule m_module;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLCommonValues_h

