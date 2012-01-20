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

#include "MacroAssembler.h"
#include "Opcode.h"
#include "PropertySlot.h"
#include "Structure.h"
#include "StructureChain.h"
#include <wtf/VectorTraits.h>

#define POLYMORPHIC_LIST_CACHE_SIZE 8

namespace JSC {

    // *Sigh*, If the JIT is enabled we need to track the stubRountine (of type CodeLocationLabel),
    // If the JIT is not in use we don't actually need the variable (that said, if the JIT is not in use we don't
    // curently actually use PolymorphicAccessStructureLists, which we should).  Anyway, this seems like the best
    // solution for now - will need to something smarter if/when we actually want mixed-mode operation.

    class JSCell;
    class Structure;
    class StructureChain;
    struct ValueProfile;

#if ENABLE(JIT)
    typedef MacroAssemblerCodeRef PolymorphicAccessStructureListStubRoutineType;

    // Structure used by op_get_by_id_self_list and op_get_by_id_proto_list instruction to hold data off the main opcode stream.
    struct PolymorphicAccessStructureList {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        struct PolymorphicStubInfo {
            bool isChain;
            bool isDirect;
            PolymorphicAccessStructureListStubRoutineType stubRoutine;
            WriteBarrier<Structure> base;
            union {
                WriteBarrierBase<Structure> proto;
                WriteBarrierBase<StructureChain> chain;
            } u;

            PolymorphicStubInfo()
            {
                u.proto.clear();
            }

            void set(JSGlobalData& globalData, JSCell* owner, PolymorphicAccessStructureListStubRoutineType _stubRoutine, Structure* _base, bool isDirect)
            {
                stubRoutine = _stubRoutine;
                base.set(globalData, owner, _base);
                u.proto.clear();
                isChain = false;
                this->isDirect = isDirect;
            }
            
            void set(JSGlobalData& globalData, JSCell* owner, PolymorphicAccessStructureListStubRoutineType _stubRoutine, Structure* _base, Structure* _proto, bool isDirect)
            {
                stubRoutine = _stubRoutine;
                base.set(globalData, owner, _base);
                u.proto.set(globalData, owner, _proto);
                isChain = false;
                this->isDirect = isDirect;
            }
            
            void set(JSGlobalData& globalData, JSCell* owner, PolymorphicAccessStructureListStubRoutineType _stubRoutine, Structure* _base, StructureChain* _chain, bool isDirect)
            {
                stubRoutine = _stubRoutine;
                base.set(globalData, owner, _base);
                u.chain.set(globalData, owner, _chain);
                isChain = true;
                this->isDirect = isDirect;
            }
        } list[POLYMORPHIC_LIST_CACHE_SIZE];
        
        PolymorphicAccessStructureList()
        {
        }
        
        PolymorphicAccessStructureList(JSGlobalData& globalData, JSCell* owner, PolymorphicAccessStructureListStubRoutineType stubRoutine, Structure* firstBase, bool isDirect)
        {
            list[0].set(globalData, owner, stubRoutine, firstBase, isDirect);
        }

        PolymorphicAccessStructureList(JSGlobalData& globalData, JSCell* owner, PolymorphicAccessStructureListStubRoutineType stubRoutine, Structure* firstBase, Structure* firstProto, bool isDirect)
        {
            list[0].set(globalData, owner, stubRoutine, firstBase, firstProto, isDirect);
        }

        PolymorphicAccessStructureList(JSGlobalData& globalData, JSCell* owner, PolymorphicAccessStructureListStubRoutineType stubRoutine, Structure* firstBase, StructureChain* firstChain, bool isDirect)
        {
            list[0].set(globalData, owner, stubRoutine, firstBase, firstChain, isDirect);
        }

        bool visitWeak(int count)
        {
            for (int i = 0; i < count; ++i) {
                PolymorphicStubInfo& info = list[i];
                if (!info.base) {
                    // We're being marked during initialisation of an entry
                    ASSERT(!info.u.proto);
                    continue;
                }
                
                if (!Heap::isMarked(info.base.get()))
                    return false;
                if (info.u.proto && !info.isChain
                    && !Heap::isMarked(info.u.proto.get()))
                    return false;
                if (info.u.chain && info.isChain
                    && !Heap::isMarked(info.u.chain.get()))
                    return false;
            }
            
            return true;
        }
    };

#endif

    struct Instruction {
        Instruction(Opcode opcode)
        {
#if !ENABLE(COMPUTED_GOTO_INTERPRETER)
            // We have to initialize one of the pointer members to ensure that
            // the entire struct is initialized, when opcode is not a pointer.
            u.jsCell.clear();
#endif
            u.opcode = opcode;
        }

        Instruction(int operand)
        {
            // We have to initialize one of the pointer members to ensure that
            // the entire struct is initialized in 64-bit.
            u.jsCell.clear();
            u.operand = operand;
        }

        Instruction(JSGlobalData& globalData, JSCell* owner, Structure* structure)
        {
            u.structure.clear();
            u.structure.set(globalData, owner, structure);
        }
        Instruction(JSGlobalData& globalData, JSCell* owner, StructureChain* structureChain)
        {
            u.structureChain.clear();
            u.structureChain.set(globalData, owner, structureChain);
        }
        Instruction(JSGlobalData& globalData, JSCell* owner, JSCell* jsCell)
        {
            u.jsCell.clear();
            u.jsCell.set(globalData, owner, jsCell);
        }

        Instruction(PropertySlot::GetValueFunc getterFunc) { u.getterFunc = getterFunc; }
        
        Instruction(ValueProfile* profile) { u.profile = profile; }

        union {
            Opcode opcode;
            int operand;
            WriteBarrierBase<Structure> structure;
            WriteBarrierBase<StructureChain> structureChain;
            WriteBarrierBase<JSCell> jsCell;
            PropertySlot::GetValueFunc getterFunc;
            ValueProfile* profile;
        } u;
        
    private:
        Instruction(StructureChain*);
        Instruction(Structure*);
    };

} // namespace JSC

namespace WTF {

    template<> struct VectorTraits<JSC::Instruction> : VectorTraitsBase<true, JSC::Instruction> { };

} // namespace WTF

#endif // Instruction_h
