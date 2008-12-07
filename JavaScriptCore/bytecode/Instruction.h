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
#include <wtf/VectorTraits.h>

#define POLYMORPHIC_LIST_CACHE_SIZE 4

namespace JSC {

    class JSCell;
    class Structure;
    class StructureChain;

    // Structure used by op_get_by_id_proto_list instruction to hold data off the main opcode stream.
    struct PolymorphicAccessStructureList {
        struct PolymorphicStubInfo {
            unsigned cachedOffset : 31;
            unsigned isChain : 1;
            void* stubRoutine;
            Structure* base;
            union {
                Structure* proto;
                StructureChain* chain;
            } u;

            void set(int _cachedOffset, void* _stubRoutine, Structure* _base)
            {
                cachedOffset = _cachedOffset;
                stubRoutine = _stubRoutine;
                base = _base;
                u.proto = 0;
                isChain = false;
            }
            
            void set(int _cachedOffset, void* _stubRoutine, Structure* _base, Structure* _proto)
            {
                cachedOffset = _cachedOffset;
                stubRoutine = _stubRoutine;
                base = _base;
                u.proto = _proto;
                isChain = false;
            }
            
            void set(int _cachedOffset, void* _stubRoutine, Structure* _base, StructureChain* _chain)
            {
                cachedOffset = _cachedOffset;
                stubRoutine = _stubRoutine;
                base = _base;
                u.chain = _chain;
                isChain = true;
            }
        } list[POLYMORPHIC_LIST_CACHE_SIZE];
        
        PolymorphicAccessStructureList(int cachedOffset, void* stubRoutine, Structure* firstBase)
        {
            list[0].set(cachedOffset, stubRoutine, firstBase);
        }

        PolymorphicAccessStructureList(int cachedOffset, void* stubRoutine, Structure* firstBase, Structure* firstProto)
        {
            list[0].set(cachedOffset, stubRoutine, firstBase, firstProto);
        }

        PolymorphicAccessStructureList(int cachedOffset, void* stubRoutine, Structure* firstBase, StructureChain* firstChain)
        {
            list[0].set(cachedOffset, stubRoutine, firstBase, firstChain);
        }

        void derefStructures(int count)
        {
            for (int i = 0; i < count; ++i) {
                PolymorphicStubInfo& info = list[i];

                ASSERT(info.base);
                info.base->deref();

                if (info.u.proto) {
                    if (info.isChain)
                        info.u.chain->deref();
                    else
                        info.u.proto->deref();
                }
            }
        }
    };

    struct Instruction {
        Instruction(Opcode opcode) { u.opcode = opcode; }
        Instruction(int operand)
        {
            // We have to initialize one of the pointer members to ensure that
            // the entire struct is initialised in 64-bit.
            u.jsCell = 0;
            u.operand = operand;
        }

        Instruction(Structure* structure) { u.structure = structure; }
        Instruction(StructureChain* structureChain) { u.structureChain = structureChain; }
        Instruction(JSCell* jsCell) { u.jsCell = jsCell; }
        Instruction(PolymorphicAccessStructureList* polymorphicStructures) { u.polymorphicStructures = polymorphicStructures; }

        union {
            Opcode opcode;
            int operand;
            Structure* structure;
            StructureChain* structureChain;
            JSCell* jsCell;
            PolymorphicAccessStructureList* polymorphicStructures;
        } u;
    };

} // namespace JSC

namespace WTF {

    template<> struct VectorTraits<JSC::Instruction> : VectorTraitsBase<true, JSC::Instruction> { };

} // namespace WTF

#endif // Instruction_h
