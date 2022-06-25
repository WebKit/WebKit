/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
    constexpr JSValueRegs calleeJSR { JSRInfo::jsRegT10 };
    constexpr GPRReg calleeGPR { GPRInfo::regT0 };
    constexpr GPRReg callLinkInfoGPR { GPRInfo::regT2 };
}

namespace CheckTraps {
    constexpr GPRReg bytecodeOffsetGPR { GPRInfo::nonArgGPR0 };
}

namespace Enter {
    constexpr GPRReg canBeOptimizedGPR { GPRInfo::regT0 };
    constexpr GPRReg localsToInitGPR { GPRInfo::regT1 };
}

namespace Instanceof {
    using SlowOperation = decltype(operationInstanceOfOptimize);

    // Registers used on both Fast and Slow paths
    constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
    constexpr JSValueRegs valueJSR { preferredArgumentJSR<SlowOperation, 2>() };
    constexpr JSValueRegs protoJSR { preferredArgumentJSR<SlowOperation, 3>() };

    // Fast path only registers
    namespace FastPath {
        constexpr GPRReg stubInfoGPR { GPRInfo::argumentGPR1 };
        static_assert(noOverlap(valueJSR, protoJSR, stubInfoGPR), "Required for DataIC");
    }

    // Slow path only registers
    namespace SlowPath {
        constexpr GPRReg globalObjectGPR { preferredArgumentGPR<SlowOperation, 0>() };
        constexpr GPRReg stubInfoGPR { preferredArgumentGPR<SlowOperation, 1>() };
        static_assert(noOverlap(globalObjectGPR, stubInfoGPR, valueJSR, protoJSR), "Required for call to slow operation");
    }
}

namespace JFalse {
    constexpr JSValueRegs valueJSR { JSRInfo::jsRegT32 };
}

namespace JTrue {
    constexpr JSValueRegs valueJSR { JSRInfo::jsRegT32 };
}

namespace Throw {
    using SlowOperation = decltype(operationThrow);

    constexpr GPRReg globalObjectGPR { preferredArgumentGPR<SlowOperation, 0>() };
    constexpr JSValueRegs thrownValueJSR { preferredArgumentJSR<SlowOperation, 1>() };
    constexpr GPRReg bytecodeOffsetGPR { GPRInfo::nonArgGPR0 };
    static_assert(noOverlap(thrownValueJSR, bytecodeOffsetGPR), "Required for call to CTI thunk");
    static_assert(noOverlap(globalObjectGPR, thrownValueJSR), "Required for call to slow operation");
}

namespace ResolveScope {
    constexpr GPRReg metadataGPR { GPRInfo::regT2 };
    constexpr GPRReg scopeGPR { GPRInfo::regT0 };
    constexpr GPRReg bytecodeOffsetGPR { GPRInfo::regT3 };
    static_assert(noOverlap(metadataGPR, scopeGPR, bytecodeOffsetGPR), "Required for call to CTI thunk");
}

namespace GetFromScope {
    constexpr GPRReg metadataGPR { GPRInfo::regT4 };
    constexpr GPRReg scopeGPR { GPRInfo::regT2 };
    constexpr GPRReg bytecodeOffsetGPR { GPRInfo::regT3 };
    static_assert(noOverlap(metadataGPR, scopeGPR, bytecodeOffsetGPR), "Required for call to CTI thunk");
}

namespace PutToScope {
    constexpr GPRReg bytecodeOffsetGPR { GPRInfo::argumentGPR2 };
}

namespace GetById {
    // Registers used on both Fast and Slow paths
    constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
    constexpr JSValueRegs baseJSR { JSRInfo::returnValueJSR };

    // Fast path only registers
    namespace FastPath {
        constexpr GPRReg stubInfoGPR { GPRInfo::regT2 };
        constexpr GPRReg scratchGPR { GPRInfo::regT3 };
        constexpr JSValueRegs dontClobberJSR { JSRInfo::jsRegT54 };
        static_assert(noOverlap(baseJSR, stubInfoGPR, scratchGPR, dontClobberJSR), "Required for DataIC");
    }

    // Slow path only registers
    namespace SlowPath {
        constexpr GPRReg globalObjectGPR { GPRInfo::regT2 };
        constexpr GPRReg bytecodeOffsetGPR { globalObjectGPR };
        constexpr GPRReg stubInfoGPR { GPRInfo::regT3 };
        constexpr GPRReg propertyGPR { GPRInfo::regT4 };
        static_assert(noOverlap(baseJSR, bytecodeOffsetGPR, stubInfoGPR, propertyGPR), "Required for call to CTI thunk");
        static_assert(noOverlap(baseJSR, globalObjectGPR, stubInfoGPR, propertyGPR), "Required for call to slow operation");
    }
}

namespace GetByIdWithThis {
    // Registers used on both Fast and Slow paths
    constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
    constexpr JSValueRegs baseJSR { JSRInfo::jsRegT10 };
    constexpr JSValueRegs thisJSR { JSRInfo::jsRegT32 };

    // Fast path only registers
    namespace FastPath {
        constexpr GPRReg stubInfoGPR { GPRInfo::regT4 };
        constexpr GPRReg scratchGPR { GPRInfo::regT5 };
        static_assert(noOverlap(baseJSR, thisJSR, stubInfoGPR, scratchGPR), "Required for DataIC");
    }

    // Slow path only registers
    namespace SlowPath {
        constexpr GPRReg globalObjectGPR { GPRInfo::regT4 };
        constexpr GPRReg bytecodeOffsetGPR { globalObjectGPR };
        constexpr GPRReg stubInfoGPR { GPRInfo::regT5 };
        constexpr GPRReg propertyGPR {
#if USE(JSVALUE64)
            GPRInfo::regT1
#elif USE(JSVALUE32_64)
            GPRInfo::regT6
#endif
        };
        static_assert(noOverlap(baseJSR, thisJSR, bytecodeOffsetGPR, stubInfoGPR, propertyGPR), "Required for call to CTI thunk");
        static_assert(noOverlap(baseJSR, thisJSR, globalObjectGPR, stubInfoGPR, propertyGPR), "Required for call to slow operation");
    }
}

namespace GetByVal {
    // Registers used on both Fast and Slow paths
    constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
    constexpr JSValueRegs baseJSR { JSRInfo::jsRegT10 };
    constexpr JSValueRegs propertyJSR { JSRInfo::jsRegT32 };

    // Fast path only registers
    namespace FastPath {
        constexpr GPRReg stubInfoGPR { GPRInfo::regT4 };
        constexpr GPRReg scratchGPR { GPRInfo::regT5 };
        static_assert(noOverlap(baseJSR, propertyJSR, stubInfoGPR, scratchGPR), "Required for DataIC");
    }

    // Slow path only registers
    namespace SlowPath {
        constexpr GPRReg globalObjectGPR { GPRInfo::regT4 };
        constexpr GPRReg bytecodeOffsetGPR { globalObjectGPR };
        constexpr GPRReg stubInfoGPR { GPRInfo::regT5 };
        constexpr GPRReg profileGPR {
#if USE(JSVALUE64)
            GPRInfo::regT1
#elif USE(JSVALUE32_64)
            GPRInfo::regT6
#endif
        };
        static_assert(noOverlap(baseJSR, propertyJSR, bytecodeOffsetGPR, stubInfoGPR, profileGPR), "Required for call to CTI thunk");
        static_assert(noOverlap(baseJSR, propertyJSR, globalObjectGPR, stubInfoGPR, profileGPR), "Required for call to slow operation");
    }
}

#if USE(JSVALUE64)
namespace EnumeratorGetByVal {
    // We rely on using the same registers when linking a CodeBlock and initializing registers
    // for a GetByVal StubInfo.
    static constexpr JSValueRegs baseJSR { GetByVal::baseJSR };
    static constexpr JSValueRegs propertyJSR { GetByVal::propertyJSR };
    static constexpr JSValueRegs resultJSR { GetByVal::resultJSR };
    static constexpr GPRReg stubInfoGPR { GetByVal::FastPath::stubInfoGPR };
    static constexpr GPRReg scratch1 { GPRInfo::regT1 };
    static constexpr GPRReg scratch2 { GPRInfo::regT3 };
    static constexpr GPRReg scratch3 { GPRInfo::regT5 };
    static_assert(noOverlap(baseJSR, propertyJSR, stubInfoGPR, scratch1, scratch2, scratch3));
}
#endif

namespace PutById {
    // Registers used on both Fast and Slow paths
    constexpr JSValueRegs baseJSR { JSRInfo::jsRegT10 };
    constexpr JSValueRegs valueJSR { JSRInfo::jsRegT32 };

    // Fast path only registers
    namespace FastPath {
        constexpr GPRReg stubInfoGPR { GPRInfo::regT4 };
        constexpr GPRReg scratchGPR { GPRInfo::regT5 };
        // Fine to use regT1, which also yields better code size on ARM_THUMB2
        constexpr GPRReg scratch2GPR { GPRInfo::regT1 };
        static_assert(noOverlap(baseJSR, valueJSR, stubInfoGPR, scratchGPR), "Required for DataIC");
        static_assert(noOverlap(baseJSR.payloadGPR(), valueJSR, stubInfoGPR, scratchGPR, scratch2GPR), "Required for DataIC");
    }

    // Slow path only registers
    namespace SlowPath {
        constexpr GPRReg globalObjectGPR {
#if USE(JSVALUE64)
            GPRInfo::regT1
#elif USE(JSVALUE32_64)
            GPRInfo::regT6
#endif
        };
        constexpr GPRReg bytecodeOffsetGPR { globalObjectGPR };
        constexpr GPRReg stubInfoGPR { GPRInfo::regT4 };
        constexpr GPRReg propertyGPR { GPRInfo::regT5 };

        static_assert(noOverlap(baseJSR, valueJSR, bytecodeOffsetGPR, stubInfoGPR, propertyGPR), "Required for call to CTI thunk");
        static_assert(noOverlap(baseJSR, valueJSR, globalObjectGPR, stubInfoGPR, propertyGPR), "Required for call to slow operation");
    }
}

namespace PutByVal {
    constexpr JSValueRegs baseJSR { JSRInfo::jsRegT10 };
    constexpr JSValueRegs propertyJSR { JSRInfo::jsRegT32 };
    constexpr JSValueRegs valueJSR { JSRInfo::jsRegT54 };
    constexpr GPRReg profileGPR {
#if USE(JSVALUE64)
        GPRInfo::regT1
#elif USE(JSVALUE32_64)
        GPRInfo::regT6
#endif
    };
    constexpr GPRReg stubInfoGPR {
#if USE(JSVALUE64)
        GPRInfo::regT3
#elif USE(JSVALUE32_64)
        GPRInfo::regT7
#endif
    };

    static_assert(noOverlap(baseJSR, propertyJSR, valueJSR, profileGPR, stubInfoGPR), "Required for DataIC");

    // Slow path only registers
    namespace SlowPath {
        constexpr GPRReg globalObjectGPR {
#if USE(JSVALUE64)
            GPRInfo::regT5
#elif CPU(ARM_THUMB2)
            // We are a bit short on registers on ARM_THUMB2, but we can just about get away with this
            MacroAssemblerARMv7::s_scratchRegister
#else // Other JSVALUE32_64
            GPRInfo::regT8
#endif
        };
        constexpr GPRReg bytecodeOffsetGPR { globalObjectGPR };
        static_assert(noOverlap(baseJSR, propertyJSR, valueJSR, profileGPR, bytecodeOffsetGPR, stubInfoGPR), "Required for call to CTI thunk");
        static_assert(noOverlap(baseJSR, propertyJSR, valueJSR, profileGPR, globalObjectGPR, stubInfoGPR), "Required for call to slow operation");
    }
}

namespace InById {
    constexpr JSValueRegs baseJSR { GetById::baseJSR };
    constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
    constexpr GPRReg stubInfoGPR { GetById::FastPath::stubInfoGPR };
    constexpr GPRReg scratchGPR { GetById::FastPath::scratchGPR };
}

namespace InByVal {
    constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
    constexpr JSValueRegs baseJSR { JSRInfo::jsRegT10 };
    constexpr JSValueRegs propertyJSR { JSRInfo::jsRegT32 };
    constexpr GPRReg stubInfoGPR { GPRInfo::regT4 };
    constexpr GPRReg scratchGPR { GPRInfo::regT5 };
    static_assert(baseJSR == GetByVal::baseJSR);
    static_assert(propertyJSR == GetByVal::propertyJSR);
}

namespace DelById {
    // Registers used on both Fast and Slow paths
    constexpr JSValueRegs baseJSR { JSRInfo::jsRegT32 };

    // Fast path only registers
    namespace FastPath {
        constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
        constexpr GPRReg stubInfoGPR { GPRInfo::regT4 };
        static_assert(noOverlap(baseJSR, stubInfoGPR), "Required for DataIC");
    }

    // Slow path only registers
    namespace SlowPath {
        constexpr GPRReg globalObjectGPR { GPRInfo::regT0 };
        constexpr GPRReg bytecodeOffsetGPR { globalObjectGPR };
        constexpr GPRReg stubInfoGPR { GPRInfo::regT1 };
        constexpr GPRReg propertyGPR { GPRInfo::regT4 };
        constexpr GPRReg ecmaModeGPR { GPRInfo::regT5 };
        static_assert(noOverlap(baseJSR, bytecodeOffsetGPR, stubInfoGPR, propertyGPR, ecmaModeGPR), "Required for call to CTI thunk");
        static_assert(noOverlap(baseJSR, globalObjectGPR, stubInfoGPR, propertyGPR, ecmaModeGPR), "Required for call to slow operation");
    }
}

namespace DelByVal {
    // Registers used on both Fast and Slow paths
    constexpr JSValueRegs baseJSR { JSRInfo::jsRegT32 };
    constexpr JSValueRegs propertyJSR { JSRInfo::jsRegT10 };

    // Fast path only registers
    namespace FastPath {
        constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
        constexpr GPRReg stubInfoGPR { GPRInfo::regT4 };
        static_assert(noOverlap(baseJSR, propertyJSR, stubInfoGPR), "Required for DataIC");
    }

    // Slow path only registers
    namespace SlowPath {
        constexpr GPRReg globalObjectGPR { GPRInfo::regT4 };
        constexpr GPRReg bytecodeOffsetGPR { globalObjectGPR };
        constexpr GPRReg stubInfoGPR { GPRInfo::regT5 };
        constexpr GPRReg ecmaModeGPR {
#if USE(JSVALUE64)
            GPRInfo::regT1
#elif USE(JSVALUE32_64)
            GPRInfo::regT6
#endif
        };
        static_assert(noOverlap(baseJSR, propertyJSR, bytecodeOffsetGPR, stubInfoGPR, ecmaModeGPR), "Required for call to CTI thunk");
        static_assert(noOverlap(baseJSR, propertyJSR, globalObjectGPR, stubInfoGPR, ecmaModeGPR), "Required for call to slow operation");
    }
}

namespace PrivateBrand {
    constexpr JSValueRegs baseJSR { GetByVal::baseJSR }; // Required by shared slow path thunk
    constexpr JSValueRegs brandJSR { GetByVal::propertyJSR }; // Required by shared slow path thunk

    namespace FastPath {
        constexpr GPRReg stubInfoGPR { GetByVal::FastPath::stubInfoGPR };
        static_assert(noOverlap(baseJSR, brandJSR, stubInfoGPR), "Required for DataIC");
    }

    namespace SlowPath {
        constexpr GPRReg bytecodeOffsetGPR { GetByVal::SlowPath::bytecodeOffsetGPR }; // Required by shared slow path thunk
        constexpr GPRReg stubInfoGPR { GetByVal::SlowPath::stubInfoGPR }; // Required by shared slow path thunk
        static_assert(noOverlap(baseJSR, brandJSR, bytecodeOffsetGPR, stubInfoGPR), "Required for call to CTI thunk");
    }
}

} // namespace BaselineJITRegisters

} // namespace JSC

#endif // ENABLE(JIT)
