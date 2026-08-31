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

#include <wtf/Platform.h>

#if ENABLE(JIT)

#include <JavaScriptCore/GPRInfo.h>
#include <JavaScriptCore/JITOperations.h>

namespace JSC {

namespace BaselineJITRegisters {

namespace Call {
    static constexpr GPRReg calleeGPR { GPRInfo::regT0 };
    static constexpr GPRReg callLinkInfoGPR { GPRInfo::regT2 };
    static constexpr GPRReg callTargetGPR { GPRInfo::regT5 };
}

namespace CallDirectEval {
    namespace SlowPath {
        static constexpr GPRReg calleeFrameGPR { GPRInfo::regT0 };
        static constexpr GPRReg scopeGPR { GPRInfo::regT1 };
        static constexpr GPRReg thisValueGPR { GPRInfo::regT2 };
        static constexpr GPRReg codeBlockGPR { GPRInfo::regT3 };
        static constexpr GPRReg bytecodeIndexGPR { GPRInfo::regT4 };
        static_assert(noOverlap(calleeFrameGPR, scopeGPR, thisValueGPR, codeBlockGPR, bytecodeIndexGPR), "Required for call to slow operation");
    }
}

namespace CheckTraps {
    static constexpr GPRReg bytecodeOffsetGPR { GPRInfo::nonArgGPR0 };
}

namespace Enter {
    static constexpr GPRReg scratch1GPR { GPRInfo::regT4 };
    static constexpr GPRReg scratch2GPR { GPRInfo::regT5 };
    static constexpr GPRReg scratch3GPR { GPRInfo::regT2 };
}

namespace Instanceof {
    using SlowOperation = decltype(operationInstanceOfOptimize);

    // Registers used on both Fast and Slow paths
    static constexpr GPRReg resultGPR { GPRInfo::returnValueGPR };
    static constexpr GPRReg valueGPR { preferredArgumentGPR<SlowOperation, 0>() };
    static constexpr GPRReg protoGPR { preferredArgumentGPR<SlowOperation, 1>() };
    static constexpr GPRReg propertyCacheGPR { preferredArgumentGPR<SlowOperation, 2>() };
    static constexpr auto scratchRegisters = allocatedScratchRegisters<GPRInfo, valueGPR, protoGPR, propertyCacheGPR, GPRInfo::handlerGPR>;
    static constexpr GPRReg scratch1GPR { scratchRegisters[0] };
    static_assert(noOverlap(propertyCacheGPR, valueGPR, protoGPR, GPRInfo::handlerGPR, scratch1GPR), "Required for call to slow operation");
    static_assert(noOverlap(resultGPR, propertyCacheGPR));

    namespace Custom {
        static constexpr GPRReg globalObjectGPR = propertyCacheGPR;
        static constexpr GPRReg valueGPR = Instanceof::valueGPR;
        static constexpr GPRReg constructorGPR = scratch1GPR;
        static constexpr GPRReg hasInstanceGPR = protoGPR;
    }
}

namespace JFalse {
    static constexpr GPRReg valueGPR { GPRInfo::regT2 };
    static constexpr GPRReg scratch1GPR { GPRInfo::regT5 };
    static_assert(noOverlap(valueGPR, scratch1GPR));
}

namespace JTrue {
    static constexpr GPRReg valueGPR { GPRInfo::regT2 };
    static constexpr GPRReg scratch1GPR { GPRInfo::regT5 };
    static_assert(noOverlap(valueGPR, scratch1GPR));
}

namespace Throw {
    using SlowOperation = decltype(operationThrow);

    static constexpr GPRReg globalObjectGPR { preferredArgumentGPR<SlowOperation, 0>() };
    static constexpr GPRReg thrownValueGPR { preferredArgumentGPR<SlowOperation, 1>() };
    static constexpr GPRReg bytecodeOffsetGPR { GPRInfo::nonArgGPR0 };
    static_assert(noOverlap(thrownValueGPR, bytecodeOffsetGPR), "Required for call to CTI thunk");
    static_assert(noOverlap(globalObjectGPR, thrownValueGPR), "Required for call to slow operation");
}

namespace SwitchString {
    using SlowOperation = decltype(operationSwitchStringWithUnknownKeyType);

    static constexpr GPRReg globalObjectGPR { preferredArgumentGPR<SlowOperation, 0>() };
    static constexpr GPRReg scrutineeGPR { preferredArgumentGPR<SlowOperation, 1>() };
    static constexpr GPRReg scratch1GPR { GPRInfo::regT5 };
    static_assert(noOverlap(globalObjectGPR, scrutineeGPR, scratch1GPR), "Required for call to slow operation, and the scrutinee must survive the inline fast path");
}

namespace ResolveScope {
    static constexpr GPRReg metadataGPR { GPRInfo::regT2 };
    static constexpr GPRReg scopeGPR { GPRInfo::regT0 };
    static constexpr GPRReg bytecodeOffsetGPR { GPRInfo::regT3 };
    static constexpr GPRReg scratch1GPR { GPRInfo::regT5 };
    static constexpr GPRReg scratch2GPR { GPRInfo::regT1 };
    static_assert(noOverlap(metadataGPR, scopeGPR, bytecodeOffsetGPR, scratch1GPR, scratch2GPR), "Required for call to CTI thunk");
}

namespace GetFromScope {
    static constexpr GPRReg metadataGPR { GPRInfo::regT4 };
    static constexpr GPRReg scopeGPR { GPRInfo::regT2 };
    static constexpr GPRReg bytecodeOffsetGPR { GPRInfo::regT3 };
    static constexpr GPRReg scratch1GPR { GPRInfo::regT5 };
    static_assert(noOverlap(metadataGPR, scopeGPR, bytecodeOffsetGPR, scratch1GPR), "Required for call to CTI thunk");
}

namespace PutToScope {
    static constexpr GPRReg bytecodeOffsetGPR { GPRInfo::argumentGPR2 };
}

namespace GetById {
    // Registers used on both Fast and Slow paths
    using SlowOperation = decltype(operationGetByIdOptimize);

    static constexpr GPRReg resultGPR { GPRInfo::returnValueGPR };
    static constexpr GPRReg baseGPR { preferredArgumentGPR<SlowOperation, 0>() };
    static constexpr GPRReg propertyCacheGPR { preferredArgumentGPR<SlowOperation, 1>() };

    static constexpr auto scratchRegisters = allocatedScratchRegisters<GPRInfo, baseGPR, propertyCacheGPR, GPRInfo::handlerGPR>;
    static constexpr GPRReg scratch1GPR { scratchRegisters[0] };
    static constexpr GPRReg scratch2GPR { scratchRegisters[1] };
    static constexpr GPRReg scratch3GPR { scratchRegisters[2] };
    static constexpr GPRReg scratch4GPR { scratchRegisters[3] };
    static constexpr GPRReg scratch5GPR { scratchRegisters[4] };

    static_assert(noOverlap(baseGPR, propertyCacheGPR), "Required for DataIC");
    static_assert(noOverlap(resultGPR, propertyCacheGPR));
    static_assert(noOverlap(baseGPR, propertyCacheGPR, scratch1GPR, scratch2GPR, scratch3GPR, scratch4GPR, scratch5GPR), "Required for HandlerIC");
}

namespace GetByIdWithThis {
    // Registers used on both Fast and Slow paths
    using SlowOperation = decltype(operationGetByIdWithThisOptimize);

    static constexpr GPRReg resultGPR { GPRInfo::returnValueGPR };
    static constexpr GPRReg baseGPR { preferredArgumentGPR<SlowOperation, 0>() };
    static constexpr GPRReg thisGPR { preferredArgumentGPR<SlowOperation, 1>() };
    static constexpr GPRReg propertyCacheGPR { preferredArgumentGPR<SlowOperation, 2>() };
    static constexpr auto scratchRegisters = allocatedScratchRegisters<GPRInfo, baseGPR, thisGPR, propertyCacheGPR, GPRInfo::handlerGPR>;
    static constexpr GPRReg scratch1GPR { scratchRegisters[0] };
    static constexpr GPRReg scratch2GPR { scratchRegisters[1] };
    static_assert(noOverlap(baseGPR, thisGPR, propertyCacheGPR, scratch1GPR, scratch2GPR), "Required for call to slow operation");
    static_assert(noOverlap(resultGPR, propertyCacheGPR));
}

namespace GetByVal {
    // Registers used on both Fast and Slow paths
    using SlowOperation = decltype(operationGetByValOptimize);

    static constexpr GPRReg resultGPR { GPRInfo::returnValueGPR };
    static constexpr GPRReg baseGPR { preferredArgumentGPR<SlowOperation, 0>() };
    static constexpr GPRReg propertyGPR { preferredArgumentGPR<SlowOperation, 1>() };
    static constexpr GPRReg propertyCacheGPR { preferredArgumentGPR<SlowOperation, 2>() };
    static constexpr GPRReg profileGPR { preferredArgumentGPR<SlowOperation, 3>() };

    static_assert(noOverlap(baseGPR, propertyGPR, propertyCacheGPR, profileGPR), "Required for DataIC");
    static constexpr auto scratchRegisters = allocatedScratchRegisters<GPRInfo, baseGPR, propertyGPR, propertyCacheGPR, profileGPR, GPRInfo::handlerGPR>;
    static constexpr GPRReg scratch1GPR { scratchRegisters[0] };
    static constexpr GPRReg scratch2GPR { scratchRegisters[1] };
    static constexpr GPRReg scratch3GPR { scratchRegisters[2] };
    static_assert(noOverlap(baseGPR, propertyGPR, propertyCacheGPR, profileGPR, scratch1GPR, scratch2GPR, scratch3GPR), "Required for DataIC");
    static_assert(noOverlap(resultGPR, propertyCacheGPR));
}

namespace EnumeratorGetByVal {
    // We rely on using the same registers when linking a CodeBlock and initializing registers
    // for a GetByVal PropertyCache.
    using GetByVal::resultGPR;
    using GetByVal::baseGPR;
    using GetByVal::propertyGPR;
    using GetByVal::propertyCacheGPR;
    using GetByVal::profileGPR;
    using GetByVal::scratch1GPR;
    using GetByVal::scratch2GPR;
    using GetByVal::scratch3GPR;
    static_assert(noOverlap(baseGPR, propertyGPR, propertyCacheGPR, profileGPR, scratch1GPR, scratch2GPR, scratch3GPR));
    static_assert(noOverlap(resultGPR, propertyCacheGPR));
}

namespace GetByValWithThis {
    // Registers used on both Fast and Slow paths
    using SlowOperation = decltype(operationGetByValWithThisOptimize);

    static constexpr GPRReg resultGPR { GPRInfo::returnValueGPR };
    static constexpr GPRReg baseGPR { preferredArgumentGPR<SlowOperation, 0>() };
    static constexpr GPRReg propertyGPR { preferredArgumentGPR<SlowOperation, 1>() };
    static constexpr GPRReg thisGPR { preferredArgumentGPR<SlowOperation, 2>() };
    static constexpr GPRReg propertyCacheGPR { preferredArgumentGPR<SlowOperation, 3>() };
    static constexpr GPRReg profileGPR { preferredArgumentGPR<SlowOperation, 4>() };
    static constexpr auto scratchRegisters = allocatedScratchRegisters<GPRInfo, baseGPR, propertyGPR, thisGPR, propertyCacheGPR, profileGPR, GPRInfo::handlerGPR>;
    static constexpr GPRReg scratch1GPR { scratchRegisters[0] };
    static_assert(noOverlap(baseGPR, propertyGPR, thisGPR, propertyCacheGPR, profileGPR, GPRInfo::handlerGPR, scratch1GPR), "Required for call to slow operation");
    static_assert(noOverlap(resultGPR, propertyCacheGPR));
}

namespace PutById {
    // Registers used on both Fast and Slow paths
    using SlowOperation = decltype(operationPutByIdStrictOptimize);

    static constexpr GPRReg valueGPR { preferredArgumentGPR<SlowOperation, 0>() };
    static constexpr GPRReg baseGPR { preferredArgumentGPR<SlowOperation, 1>() };
    static constexpr GPRReg propertyCacheGPR { preferredArgumentGPR<SlowOperation, 2>() };
    static constexpr auto scratchRegisters = allocatedScratchRegisters<GPRInfo, valueGPR, baseGPR, propertyCacheGPR, GPRInfo::handlerGPR>;
    static constexpr GPRReg scratch1GPR { scratchRegisters[0] };
    static_assert(noOverlap(baseGPR, valueGPR, propertyCacheGPR, scratch1GPR), "Required for DataIC");
    static_assert(noOverlap(baseGPR, valueGPR, propertyCacheGPR, GPRInfo::handlerGPR, scratch1GPR), "Required for call to slow operation");

    static constexpr GPRReg scratch2GPR { scratchRegisters[1] };
    static constexpr GPRReg scratch3GPR { scratchRegisters[2] };
    static constexpr GPRReg scratch4GPR { scratchRegisters[3] };
    static_assert(noOverlap(baseGPR, valueGPR, propertyCacheGPR, GPRInfo::handlerGPR, scratch1GPR, scratch2GPR, scratch3GPR, scratch4GPR), "Required for HandlerIC");
}

namespace PutByVal {
    using SlowOperation = decltype(operationPutByValStrictOptimize);
    static constexpr GPRReg baseGPR { preferredArgumentGPR<SlowOperation, 0>() };
    static constexpr GPRReg propertyGPR { preferredArgumentGPR<SlowOperation, 1>() };
    static constexpr GPRReg valueGPR { preferredArgumentGPR<SlowOperation, 2>() };
    static constexpr GPRReg propertyCacheGPR { preferredArgumentGPR<SlowOperation, 3>() };
    static constexpr GPRReg profileGPR { preferredArgumentGPR<SlowOperation, 4>() };
    static constexpr auto scratchRegisters = allocatedScratchRegisters<GPRInfo, baseGPR, propertyGPR, valueGPR, propertyCacheGPR, profileGPR, GPRInfo::handlerGPR>;
    static constexpr GPRReg scratch1GPR { scratchRegisters[0] };
    static constexpr GPRReg scratch2GPR { scratchRegisters[1] };
    static_assert(noOverlap(baseGPR, propertyGPR, valueGPR, propertyCacheGPR, profileGPR, scratch1GPR, GPRInfo::handlerGPR), "Required for call to slow operation");
    static_assert(noOverlap(baseGPR, propertyGPR, valueGPR, propertyCacheGPR, profileGPR, scratch1GPR, scratch2GPR), "Required for HandlerIC");
}

namespace EnumeratorPutByVal {
    // We rely on using the same registers when linking a CodeBlock and initializing registers
    // for a PutByVal PropertyCache.
    using PutByVal::baseGPR;
    using PutByVal::propertyGPR;
    using PutByVal::valueGPR;
    using PutByVal::profileGPR;
    using PutByVal::propertyCacheGPR;
    using PutByVal::scratch1GPR;
    using PutByVal::scratch2GPR;
}

namespace InById {
    using GetById::resultGPR;
    using GetById::baseGPR;
    using GetById::propertyCacheGPR;
    using GetById::scratch1GPR;
    static_assert(noOverlap(resultGPR, propertyCacheGPR));
}

namespace InByVal {
    using GetByVal::resultGPR;
    using GetByVal::baseGPR;
    using GetByVal::propertyGPR;
    using GetByVal::propertyCacheGPR;
    using GetByVal::profileGPR;
    using GetByVal::scratch1GPR;
    static_assert(noOverlap(resultGPR, propertyCacheGPR));
}

namespace DelById {
    // Registers used on both Fast and Slow paths
    using SlowOperation = decltype(operationDeleteByIdStrictOptimize);

    static constexpr GPRReg resultGPR { GPRInfo::returnValueGPR };
    static constexpr GPRReg baseGPR { preferredArgumentGPR<SlowOperation, 0>() };
    static constexpr GPRReg propertyCacheGPR { preferredArgumentGPR<SlowOperation, 1>() };
    static constexpr auto scratchRegisters = allocatedScratchRegisters<GPRInfo, baseGPR, propertyCacheGPR, GPRInfo::handlerGPR>;
    static constexpr GPRReg scratch1GPR { scratchRegisters[0] };
    static constexpr GPRReg scratch2GPR { scratchRegisters[1] };
    static constexpr GPRReg scratch3GPR { scratchRegisters[2] };

    static_assert(noOverlap(baseGPR, propertyCacheGPR, scratch1GPR, scratch3GPR, GPRInfo::handlerGPR), "Required for call to slow operation");
    static_assert(noOverlap(resultGPR, propertyCacheGPR));
}

namespace DelByVal {
    // Registers used on both Fast and Slow paths
    using SlowOperation = decltype(operationDeleteByValStrictOptimize);
    static constexpr GPRReg resultGPR { GPRInfo::returnValueGPR };
    static constexpr GPRReg baseGPR { preferredArgumentGPR<SlowOperation, 0>() };
    static constexpr GPRReg propertyGPR { preferredArgumentGPR<SlowOperation, 1>() };
    static constexpr GPRReg propertyCacheGPR { preferredArgumentGPR<SlowOperation, 2>() };
    static constexpr auto scratchRegisters = allocatedScratchRegisters<GPRInfo, baseGPR, propertyGPR, propertyCacheGPR, GPRInfo::handlerGPR>;
    static constexpr GPRReg scratch1GPR { scratchRegisters[0] };
    static constexpr GPRReg scratch2GPR { scratchRegisters[1] };
    static constexpr GPRReg scratch3GPR { scratchRegisters[2] };

    static_assert(noOverlap(baseGPR, propertyGPR, propertyCacheGPR, scratch1GPR, scratch3GPR, GPRInfo::handlerGPR), "Required for call to slow operation");
    static_assert(noOverlap(resultGPR, propertyCacheGPR));
}

namespace PrivateBrand {
    using GetByVal::baseGPR;
    using GetByVal::propertyGPR;
    using GetByVal::propertyCacheGPR;
    using GetByVal::scratch1GPR;
    static_assert(noOverlap(baseGPR, propertyGPR, propertyCacheGPR), "Required for DataIC");
}

} // namespace BaselineJITRegisters

} // namespace JSC

#endif // ENABLE(JIT)
