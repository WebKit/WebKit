/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

#if ENABLE(JIT)

#include "GPRInfo.h"
#include "JITOperations.h"

namespace JSC {

namespace BaselineJITRegisters {

namespace Call {
    static constexpr JSValueRegs calleeJSR { JSRInfo::jsRegT10 };
    static constexpr GPRReg calleeGPR { GPRInfo::regT0 };
    static constexpr GPRReg callLinkInfoGPR { GPRInfo::regT2 };
    static constexpr GPRReg callTargetGPR { GPRInfo::regT5 };
}

namespace CallDirectEval {
    namespace SlowPath {
        static constexpr GPRReg calleeFrameGPR { GPRInfo::regT0 };
#if USE(JSVALUE64)
        static constexpr GPRReg scopeGPR { GPRInfo::regT1 };
        static constexpr JSValueRegs thisValueJSR { GPRInfo::regT2 };
#else
        static constexpr GPRReg scopeGPR { GPRInfo::regT1 };
        static constexpr JSValueRegs thisValueJSR { JSRInfo::jsRegT32 };
#endif
    }
}

namespace CheckTraps {
    static constexpr GPRReg bytecodeOffsetGPR { GPRInfo::nonArgGPR0 };
}

namespace Enter {
    static constexpr GPRReg canBeOptimizedGPR { GPRInfo::regT0 };
    static constexpr GPRReg localsToInitGPR { GPRInfo::regT1 };
}

namespace Instanceof {
    using SlowOperation = decltype(operationInstanceOfOptimize);

    // Registers used on both Fast and Slow paths
    static constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
    static constexpr JSValueRegs valueJSR { preferredArgumentJSR<SlowOperation, 0>() };
    static constexpr JSValueRegs protoJSR { preferredArgumentJSR<SlowOperation, 1>() };
    static constexpr GPRReg globalObjectGPR { preferredArgumentGPR<SlowOperation, 2>() };
    static constexpr GPRReg stubInfoGPR { preferredArgumentGPR<SlowOperation, 3>() };
    static_assert(noOverlap(globalObjectGPR, stubInfoGPR, valueJSR, protoJSR), "Required for call to slow operation");
    static_assert(noOverlap(resultJSR, stubInfoGPR));
}

namespace JFalse {
    static constexpr JSValueRegs valueJSR { JSRInfo::jsRegT32 };
}

namespace JTrue {
    static constexpr JSValueRegs valueJSR { JSRInfo::jsRegT32 };
}

namespace Throw {
    using SlowOperation = decltype(operationThrow);

    static constexpr GPRReg globalObjectGPR { preferredArgumentGPR<SlowOperation, 0>() };
    static constexpr JSValueRegs thrownValueJSR { preferredArgumentJSR<SlowOperation, 1>() };
    static constexpr GPRReg bytecodeOffsetGPR { GPRInfo::nonArgGPR0 };
    static_assert(noOverlap(thrownValueJSR, bytecodeOffsetGPR), "Required for call to CTI thunk");
    static_assert(noOverlap(globalObjectGPR, thrownValueJSR), "Required for call to slow operation");
}

namespace ResolveScope {
    static constexpr GPRReg metadataGPR { GPRInfo::regT2 };
    static constexpr GPRReg scopeGPR { GPRInfo::regT0 };
    static constexpr GPRReg bytecodeOffsetGPR { GPRInfo::regT3 };
    static_assert(noOverlap(metadataGPR, scopeGPR, bytecodeOffsetGPR), "Required for call to CTI thunk");
}

namespace GetFromScope {
    static constexpr GPRReg metadataGPR { GPRInfo::regT4 };
    static constexpr GPRReg scopeGPR { GPRInfo::regT2 };
    static constexpr GPRReg bytecodeOffsetGPR { GPRInfo::regT3 };
    static_assert(noOverlap(metadataGPR, scopeGPR, bytecodeOffsetGPR), "Required for call to CTI thunk");
}

namespace PutToScope {
    static constexpr GPRReg bytecodeOffsetGPR { GPRInfo::argumentGPR2 };
}

namespace GetById {
    // Registers used on both Fast and Slow paths
    using SlowOperation = decltype(operationGetByIdOptimize);

    static constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
    static constexpr JSValueRegs baseJSR { preferredArgumentJSR<SlowOperation, 0>() };
    static constexpr GPRReg globalObjectGPR { preferredArgumentGPR<SlowOperation, 1>() };
    static constexpr GPRReg stubInfoGPR { preferredArgumentGPR<SlowOperation, 2>() };
    static constexpr GPRReg scratch1GPR { globalObjectGPR };
    static_assert(noOverlap(baseJSR, stubInfoGPR, globalObjectGPR), "Required for DataIC");
    static_assert(noOverlap(resultJSR, stubInfoGPR));
}

namespace GetByIdWithThis {
    // Registers used on both Fast and Slow paths
    using SlowOperation = decltype(operationGetByIdWithThisOptimize);

    static constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
    static constexpr JSValueRegs baseJSR { preferredArgumentJSR<SlowOperation, 0>() };
    static constexpr JSValueRegs thisJSR { preferredArgumentJSR<SlowOperation, 1>() };
    static constexpr GPRReg globalObjectGPR { preferredArgumentGPR<SlowOperation, 2>() };
    static constexpr GPRReg stubInfoGPR { preferredArgumentGPR<SlowOperation, 3>() };
    static constexpr GPRReg scratch1GPR { globalObjectGPR };
    static_assert(noOverlap(baseJSR, thisJSR, globalObjectGPR, stubInfoGPR), "Required for call to slow operation");
    static_assert(noOverlap(resultJSR, stubInfoGPR));
}

namespace GetByVal {
    // Registers used on both Fast and Slow paths
    using SlowOperation = decltype(operationGetByValOptimize);

    static constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
    static constexpr JSValueRegs baseJSR { preferredArgumentJSR<SlowOperation, 0>() };
    static constexpr JSValueRegs propertyJSR { preferredArgumentJSR<SlowOperation, 1>() };
    static constexpr GPRReg globalObjectGPR { preferredArgumentGPR<SlowOperation, 2>() };
    static constexpr GPRReg stubInfoGPR { preferredArgumentGPR<SlowOperation, 3>() };
    static constexpr GPRReg profileGPR { preferredArgumentGPR<SlowOperation, 4>() };
    static constexpr GPRReg scratch1GPR { globalObjectGPR };
    static_assert(noOverlap(baseJSR, propertyJSR, stubInfoGPR, profileGPR, globalObjectGPR), "Required for DataIC");
    static_assert(noOverlap(resultJSR, stubInfoGPR));
}

#if USE(JSVALUE64) && !OS(WINDOWS)
namespace EnumeratorGetByVal {
    // We rely on using the same registers when linking a CodeBlock and initializing registers
    // for a GetByVal StubInfo.
    using GetByVal::resultJSR;
    using GetByVal::baseJSR;
    using GetByVal::propertyJSR;
    using GetByVal::stubInfoGPR;
    using GetByVal::profileGPR;
    using GetByVal::globalObjectGPR;
    using GetByVal::scratch1GPR;
    static constexpr GPRReg scratch2GPR { GPRInfo::regT5 };
    static constexpr GPRReg scratch3GPR { GPRInfo::regT7 };
    static_assert(noOverlap(baseJSR, propertyJSR, stubInfoGPR, profileGPR, scratch1GPR, scratch2GPR, scratch3GPR));
    static_assert(noOverlap(resultJSR, stubInfoGPR));
}
#endif

#if USE(JSVALUE64)
namespace GetByValWithThis {
    // Registers used on both Fast and Slow paths
    using SlowOperation = decltype(operationGetByValWithThisOptimize);

    static constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
    static constexpr JSValueRegs baseJSR { preferredArgumentJSR<SlowOperation, 0>() };
    static constexpr JSValueRegs propertyJSR { preferredArgumentJSR<SlowOperation, 1>() };
    static constexpr JSValueRegs thisJSR { preferredArgumentJSR<SlowOperation, 2>() };
    static constexpr GPRReg globalObjectGPR { preferredArgumentGPR<SlowOperation, 3>() };
    static constexpr GPRReg stubInfoGPR { preferredArgumentGPR<SlowOperation, 4>() };
    static constexpr GPRReg profileGPR { preferredArgumentGPR<SlowOperation, 5>() };
    static constexpr GPRReg scratch1GPR { globalObjectGPR };
    static_assert(noOverlap(baseJSR, propertyJSR, thisJSR, globalObjectGPR, stubInfoGPR, profileGPR), "Required for call to slow operation");
    static_assert(noOverlap(resultJSR, stubInfoGPR));
}
#endif

namespace PutById {
    // Registers used on both Fast and Slow paths
    using SlowOperation = decltype(operationPutByIdStrictOptimize);

    static constexpr JSValueRegs valueJSR { preferredArgumentJSR<SlowOperation, 0>() };
    static constexpr JSValueRegs baseJSR { preferredArgumentJSR<SlowOperation, 1>() };
    static constexpr GPRReg globalObjectGPR { preferredArgumentGPR<SlowOperation, 2>() };
    static constexpr GPRReg stubInfoGPR { preferredArgumentGPR<SlowOperation, 3>() };
    static constexpr GPRReg scratch1GPR { globalObjectGPR };
    static_assert(noOverlap(baseJSR, valueJSR, stubInfoGPR, scratch1GPR), "Required for DataIC");
    static_assert(noOverlap(baseJSR, valueJSR, globalObjectGPR, stubInfoGPR), "Required for call to slow operation");
}

namespace PutByVal {
    using SlowOperation = decltype(operationPutByValStrictOptimize);
    static constexpr JSValueRegs baseJSR { preferredArgumentJSR<SlowOperation, 0>() };
    static constexpr JSValueRegs propertyJSR { preferredArgumentJSR<SlowOperation, 1>() };
    static constexpr JSValueRegs valueJSR { preferredArgumentJSR<SlowOperation, 2>() };
    static constexpr GPRReg globalObjectGPR { preferredArgumentGPR<SlowOperation, 3>() };
    static constexpr GPRReg stubInfoGPR { preferredArgumentGPR<SlowOperation, 4>() };
    static constexpr GPRReg profileGPR { preferredArgumentGPR<SlowOperation, 5>() };
    static constexpr GPRReg scratch1GPR { globalObjectGPR };
    static_assert(noOverlap(baseJSR, propertyJSR, valueJSR, stubInfoGPR, profileGPR, globalObjectGPR), "Required for call to slow operation");
}

#if USE(JSVALUE64) && !OS(WINDOWS)
namespace EnumeratorPutByVal {
    // We rely on using the same registers when linking a CodeBlock and initializing registers
    // for a PutByVal StubInfo.
    using PutByVal::baseJSR;
    using PutByVal::propertyJSR;
    using PutByVal::valueJSR;
    using PutByVal::profileGPR;
    using PutByVal::stubInfoGPR;
    using PutByVal::scratch1GPR;
    using PutByVal::globalObjectGPR;
    static constexpr GPRReg scratch2GPR {
#if CPU(X86_64)
        GPRInfo::regT5
#else
        GPRInfo::regT7
#endif
    };
    static_assert(noOverlap(baseJSR, propertyJSR, valueJSR, stubInfoGPR, profileGPR, globalObjectGPR, scratch2GPR));
}
#endif

namespace InById {
    using GetById::resultJSR;
    using GetById::baseJSR;
    using GetById::stubInfoGPR;
    using GetById::globalObjectGPR;
    using GetById::scratch1GPR;
    static_assert(noOverlap(resultJSR, stubInfoGPR));
}

namespace InByVal {
    using GetByVal::resultJSR;
    using GetByVal::baseJSR;
    using GetByVal::propertyJSR;
    using GetByVal::stubInfoGPR;
    using GetByVal::profileGPR;
    using GetByVal::globalObjectGPR;
    using GetByVal::scratch1GPR;
    static_assert(noOverlap(resultJSR, stubInfoGPR));
}

namespace DelById {
    // Registers used on both Fast and Slow paths
    using SlowOperation = decltype(operationDeleteByIdStrictOptimize);

    static constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
    static constexpr JSValueRegs baseJSR { preferredArgumentJSR<SlowOperation, 0>() };
    static constexpr GPRReg globalObjectGPR { preferredArgumentGPR<SlowOperation, 1>() };
    static constexpr GPRReg stubInfoGPR { preferredArgumentGPR<SlowOperation, 2>() };
    static constexpr GPRReg scratch1GPR { globalObjectGPR };
    static_assert(noOverlap(baseJSR, globalObjectGPR, stubInfoGPR), "Required for call to slow operation");
    static_assert(noOverlap(resultJSR.payloadGPR(), stubInfoGPR));
}

namespace DelByVal {
    // Registers used on both Fast and Slow paths
    using SlowOperation = decltype(operationDeleteByValStrictOptimize);
    static constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
    static constexpr JSValueRegs baseJSR { preferredArgumentJSR<SlowOperation, 0>() };
    static constexpr JSValueRegs propertyJSR { preferredArgumentJSR<SlowOperation, 1>() };
    static constexpr GPRReg globalObjectGPR { preferredArgumentGPR<SlowOperation, 2>() };
    static constexpr GPRReg stubInfoGPR { preferredArgumentGPR<SlowOperation, 3>() };
    static constexpr GPRReg scratch1GPR { globalObjectGPR };
    static_assert(noOverlap(baseJSR, propertyJSR, globalObjectGPR, stubInfoGPR), "Required for call to slow operation");
    static_assert(noOverlap(resultJSR.payloadGPR(), stubInfoGPR));
}

namespace PrivateBrand {
    using GetByVal::baseJSR;
    using GetByVal::propertyJSR;
    using GetByVal::stubInfoGPR;
    using GetByVal::globalObjectGPR;
    using GetByVal::scratch1GPR;
    static_assert(noOverlap(baseJSR, propertyJSR, stubInfoGPR), "Required for DataIC");
}

} // namespace BaselineJITRegisters

} // namespace JSC

#endif // ENABLE(JIT)
