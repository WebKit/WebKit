/*
 * Copyright (C) 2010, 2012, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ThunkGenerators_h
#define ThunkGenerators_h

#include "CodeSpecializationKind.h"
#include "RegisterPreservationMode.h"
#include "ThunkGenerator.h"

#if ENABLE(JIT)
namespace JSC {

MacroAssemblerCodeRef throwExceptionFromCallSlowPathGenerator(VM*);

MacroAssemblerCodeRef linkCallThunkGenerator(VM*);
MacroAssemblerCodeRef linkConstructThunkGenerator(VM*);
MacroAssemblerCodeRef linkCallThatPreservesRegsThunkGenerator(VM*);
MacroAssemblerCodeRef linkConstructThatPreservesRegsThunkGenerator(VM*);

inline ThunkGenerator linkThunkGeneratorFor(
    CodeSpecializationKind kind, RegisterPreservationMode registers)
{
    switch (kind) {
    case CodeForCall:
        switch (registers) {
        case RegisterPreservationNotRequired:
            return linkCallThunkGenerator;
        case MustPreserveRegisters:
            return linkCallThatPreservesRegsThunkGenerator;
        }
        break;
    case CodeForConstruct:
        switch (registers) {
        case RegisterPreservationNotRequired:
            return linkConstructThunkGenerator;
        case MustPreserveRegisters:
            return linkConstructThatPreservesRegsThunkGenerator;
        }
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

MacroAssemblerCodeRef linkClosureCallThunkGenerator(VM*);
MacroAssemblerCodeRef linkClosureCallThatPreservesRegsThunkGenerator(VM*);

inline ThunkGenerator linkClosureCallThunkGeneratorFor(RegisterPreservationMode registers)
{
    switch (registers) {
    case RegisterPreservationNotRequired:
        return linkClosureCallThunkGenerator;
    case MustPreserveRegisters:
        return linkClosureCallThatPreservesRegsThunkGenerator;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

MacroAssemblerCodeRef virtualCallThunkGenerator(VM*);
MacroAssemblerCodeRef virtualConstructThunkGenerator(VM*);
MacroAssemblerCodeRef virtualCallThatPreservesRegsThunkGenerator(VM*);
MacroAssemblerCodeRef virtualConstructThatPreservesRegsThunkGenerator(VM*);

inline ThunkGenerator virtualThunkGeneratorFor(
    CodeSpecializationKind kind, RegisterPreservationMode registers)
{
    switch (kind) {
    case CodeForCall:
        switch (registers) {
        case RegisterPreservationNotRequired:
            return virtualCallThunkGenerator;
        case MustPreserveRegisters:
            return virtualCallThatPreservesRegsThunkGenerator;
        }
        break;
    case CodeForConstruct:
        switch (registers) {
        case RegisterPreservationNotRequired:
            return virtualConstructThunkGenerator;
        case MustPreserveRegisters:
            return virtualConstructThatPreservesRegsThunkGenerator;
        }
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

MacroAssemblerCodeRef nativeCallGenerator(VM*);
MacroAssemblerCodeRef nativeConstructGenerator(VM*);
MacroAssemblerCodeRef nativeTailCallGenerator(VM*);
MacroAssemblerCodeRef arityFixup(VM*);

MacroAssemblerCodeRef charCodeAtThunkGenerator(VM*);
MacroAssemblerCodeRef charAtThunkGenerator(VM*);
MacroAssemblerCodeRef fromCharCodeThunkGenerator(VM*);
MacroAssemblerCodeRef absThunkGenerator(VM*);
MacroAssemblerCodeRef ceilThunkGenerator(VM*);
MacroAssemblerCodeRef expThunkGenerator(VM*);
MacroAssemblerCodeRef floorThunkGenerator(VM*);
MacroAssemblerCodeRef logThunkGenerator(VM*);
MacroAssemblerCodeRef roundThunkGenerator(VM*);
MacroAssemblerCodeRef sqrtThunkGenerator(VM*);
MacroAssemblerCodeRef powThunkGenerator(VM*);
MacroAssemblerCodeRef imulThunkGenerator(VM*);
MacroAssemblerCodeRef arrayIteratorNextKeyThunkGenerator(VM*);
MacroAssemblerCodeRef arrayIteratorNextValueThunkGenerator(VM*);

}
#endif // ENABLE(JIT)

#endif // ThunkGenerator_h
