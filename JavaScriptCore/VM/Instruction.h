/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Instruction_h
#define Instruction_h

#include "Opcode.h"
#include "ResultType.h"
#include <wtf/VectorTraits.h>

namespace JSC {

    class JSCell;
    class StructureID;
    class StructureIDChain;

    struct Instruction {
        Instruction(Bytecode bytecode) { u.bytecode = bytecode; }
        Instruction(int operand)
        {
            // We have to initialize one of the pointer members to ensure that
            // the entire struct is initialised in 64-bit.
            u.jsCell = 0;
            u.operand = operand;
        }

        Instruction(StructureID* structureID) { u.structureID = structureID; }
        Instruction(StructureIDChain* structureIDChain) { u.structureIDChain = structureIDChain; }
        Instruction(JSCell* jsCell) { u.jsCell = jsCell; }

        union {
            Bytecode bytecode;
            int operand;
            StructureID* structureID;
            StructureIDChain* structureIDChain;
            JSCell* jsCell;
            ResultType::Type resultType;
        } u;
    };

} // namespace JSC

namespace WTF {

    template<> struct VectorTraits<JSC::Instruction> : VectorTraitsBase<true, JSC::Instruction> { };

} // namespace WTF

#endif // Instruction_h
