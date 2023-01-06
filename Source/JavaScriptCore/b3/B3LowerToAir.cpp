/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#include "B3LowerToAir.h"

#if ENABLE(B3_JIT)

#include "AirBlockInsertionSet.h"
#include "AirCode.h"
#include "AirHelpers.h"
#include "AirInsertionSet.h"
#include "AirInstInlines.h"
#include "AirPrintSpecial.h"
#include "B3ArgumentRegValue.h"
#include "B3AtomicValue.h"
#include "B3BlockWorklist.h"
#include "B3CCallValue.h"
#include "B3CheckSpecial.h"
#include "B3Commutativity.h"
#include "B3Dominators.h"
#include "B3ExtractValue.h"
#include "B3FenceValue.h"
#include "B3MemoryValueInlines.h"
#include "B3PatchpointSpecial.h"
#include "B3PatchpointValue.h"
#include "B3PhaseScope.h"
#include "B3PhiChildren.h"
#include "B3Procedure.h"
#include "B3ProcedureInlines.h"
#include "B3SlotBaseValue.h"
#include "B3UpsilonValue.h"
#include "B3UseCounts.h"
#include "B3ValueInlines.h"
#include "B3Variable.h"
#include "B3VariableValue.h"
#include "B3WasmAddressValue.h"
#include <wtf/IndexMap.h>
#include <wtf/IndexSet.h>
#include <wtf/StdLibExtras.h>

#if !ASSERT_ENABLED
IGNORE_RETURN_TYPE_WARNINGS_BEGIN
#endif

namespace JSC { namespace B3 {

namespace {

namespace B3LowerToAirInternal {
static constexpr bool verbose = false;
}

using Arg = Air::Arg;
using Inst = Air::Inst;
using Code = Air::Code;
using Tmp = Air::Tmp;

using Air::moveForType;
using Air::relaxedMoveForType;

// FIXME: We wouldn't need this if Air supported Width modifiers in Air::Kind.
// https://bugs.webkit.org/show_bug.cgi?id=169247
#define OPCODE_FOR_WIDTH(opcode, width) ( \
    (width) == Width8 ? Air::opcode ## 8 : \
    (width) == Width16 ? Air::opcode ## 16 :    \
    (width) == Width32 ? Air::opcode ## 32 :    \
    Air::opcode ## 64)
#define OPCODE_FOR_CANONICAL_WIDTH(opcode, width) ( \
    (width) == Width64 ? Air::opcode ## 64 : Air::opcode ## 32)

class LowerToAir {
public:
    LowerToAir(Procedure& procedure)
        : m_valueToTmp(procedure.values().size())
        , m_phiToTmp(procedure.values().size())
        , m_blockToBlock(procedure.size())
        , m_useCounts(procedure)
        , m_phiChildren(procedure)
        , m_dominators(procedure.dominators())
        , m_procedure(procedure)
        , m_code(procedure.code())
        , m_blockInsertionSet(m_code)
#if CPU(X86) || CPU(X86_64)
        , m_eax(X86Registers::eax)
        , m_ecx(X86Registers::ecx)
        , m_edx(X86Registers::edx)
#endif
    {
    }

    void run()
    {
        using namespace Air;
        for (B3::BasicBlock* block : m_procedure)
            m_blockToBlock[block] = m_code.addBlock(block->frequency());

        auto ensureTupleTmps = [&] (Value* tupleValue, auto& hashTable) {
            hashTable.ensure(tupleValue, [&] {
                const auto tuple = m_procedure.tupleForType(tupleValue->type());
                Vector<Tmp> tmps(tuple.size());

                for (unsigned i = 0; i < tuple.size(); ++i)
                    tmps[i] = tmpForType(tuple[i]);
                return tmps;
            });
        };

        for (Value* value : m_procedure.values()) {
            switch (value->opcode()) {
            case Phi: {
                if (value->type().isTuple()) {
                    ensureTupleTmps(value, m_tuplePhiToTmps);
                    ensureTupleTmps(value, m_tupleValueToTmps);
                    break;
                }

                m_phiToTmp[value] = m_code.newTmp(value->resultBank());
                if (B3LowerToAirInternal::verbose)
                    dataLog("Phi tmp for ", *value, ": ", m_phiToTmp[value], "\n");
                break;
            }
            case Get:
            case Patchpoint:
            case BottomTuple: {
                if (value->type().isTuple())
                    ensureTupleTmps(value, m_tupleValueToTmps);
                break;
            }
            default:
                break;
            }
        }

        for (Variable* variable : m_procedure.variables()) {
            auto addResult = m_variableToTmps.add(variable, Vector<Tmp, 1>(m_procedure.resultCount(variable->type())));
            ASSERT(addResult.isNewEntry);
            for (unsigned i = 0; i < m_procedure.resultCount(variable->type()); ++i)
                addResult.iterator->value[i] = tmpForType(m_procedure.typeAtOffset(variable->type(), i));
        }

        // Figure out which blocks are not rare.
        m_fastWorklist.push(m_procedure[0]);
        while (B3::BasicBlock* block = m_fastWorklist.pop()) {
            for (B3::FrequentedBlock& successor : block->successors()) {
                if (!successor.isRare())
                    m_fastWorklist.push(successor.block());
            }
        }

        m_procedure.resetValueOwners(); // Used by crossesInterference().

        // Lower defs before uses on a global level. This is a good heuristic to lock down a
        // hoisted address expression before we duplicate it back into the loop.
        for (B3::BasicBlock* block : m_procedure.blocksInPreOrder()) {
            m_block = block;

            m_isRare = !m_fastWorklist.saw(block);

            if (B3LowerToAirInternal::verbose)
                dataLog("Lowering Block ", *block, ":\n");
            
            // Make sure that the successors are set up correctly.
            for (B3::FrequentedBlock successor : block->successors()) {
                m_blockToBlock[block]->successors().append(
                    Air::FrequentedBlock(m_blockToBlock[successor.block()], successor.frequency()));
            }

            // Process blocks in reverse order so we see uses before defs. That's what allows us
            // to match patterns effectively.
            for (unsigned i = block->size(); i--;) {
                m_index = i;
                m_value = block->at(i);
                if (m_locked.contains(m_value))
                    continue;
                m_insts.append(Vector<Inst>());
                if (B3LowerToAirInternal::verbose)
                    dataLog("Lowering ", deepDump(m_procedure, m_value), ":\n");
                lower();
                if (B3LowerToAirInternal::verbose) {
                    for (Inst& inst : m_insts.last())
                        dataLog("    ", inst, "\n");
                }
            }

            finishAppendingInstructions(m_blockToBlock[block]);
        }

        m_blockInsertionSet.execute();

        Air::InsertionSet insertionSet(m_code);
        for (Inst& inst : m_prologue)
            insertionSet.insertInst(0, WTFMove(inst));
        insertionSet.execute(m_code[0]);
    }

private:
    bool shouldCopyPropagate(Value* value)
    {
        switch (value->opcode()) {
        case Trunc:
        case Identity:
        case Opaque:
            return true;
        default:
            return false;
        }
    }

    class ArgPromise {
        WTF_MAKE_NONCOPYABLE(ArgPromise);
    public:
        ArgPromise() { }

        ArgPromise(const Arg& arg, Value* valueToLock = nullptr)
            : m_arg(arg)
            , m_value(valueToLock)
        {
        }
        
        void swap(ArgPromise& other)
        {
            std::swap(m_arg, other.m_arg);
            std::swap(m_value, other.m_value);
            std::swap(m_wasConsumed, other.m_wasConsumed);
            std::swap(m_wasWrapped, other.m_wasWrapped);
            std::swap(m_traps, other.m_traps);
        }
        
        ArgPromise(ArgPromise&& other)
        {
            swap(other);
        }
        
        ArgPromise& operator=(ArgPromise&& other)
        {
            swap(other);
            return *this;
        }
        
        ~ArgPromise()
        {
            if (m_wasConsumed)
                RELEASE_ASSERT(m_wasWrapped);
        }
        
        void setTraps(bool value)
        {
            m_traps = value;
        }

        static ArgPromise tmp(Value* value)
        {
            ArgPromise result;
            result.m_value = value;
            return result;
        }

        explicit operator bool() const { return m_arg || m_value; }

        Arg::Kind kind() const
        {
            if (!m_arg && m_value)
                return Arg::Tmp;
            return m_arg.kind();
        }

        const Arg& peek() const
        {
            return m_arg;
        }

        Arg consume(LowerToAir& lower)
        {
            m_wasConsumed = true;
            if (!m_arg && m_value)
                return lower.tmp(m_value);
            if (m_value)
                lower.commitInternal(m_value);
            return m_arg;
        }
        
        template<typename... Args>
        Inst inst(Args&&... args)
        {
            Inst result(std::forward<Args>(args)...);
            result.kind.effects |= m_traps;
            m_wasWrapped = true;
            return result;
        }
        
    private:
        // Three forms:
        // Everything null: invalid.
        // Arg non-null, value null: just use the arg, nothing special.
        // Arg null, value non-null: it's a tmp, pin it when necessary.
        // Arg non-null, value non-null: use the arg, lock the value.
        Arg m_arg;
        Value* m_value { nullptr };
        bool m_wasConsumed { false };
        bool m_wasWrapped { false };
        bool m_traps { false };
    };

    // Consider using tmpPromise() in cases where you aren't sure that you want to pin the value yet.
    // Here are three canonical ways of using tmp() and tmpPromise():
    //
    // Idiom #1: You know that you want a tmp() and you know that it will be valid for the
    // instruction you're emitting.
    //
    //     append(Foo, tmp(bar));
    //
    // Idiom #2: You don't know if you want to use a tmp() because you haven't determined if the
    // instruction will accept it, so you query first. Note that the call to tmp() happens only after
    // you are sure that you will use it.
    //
    //     if (isValidForm(Foo, Arg::Tmp))
    //         append(Foo, tmp(bar))
    //
    // Idiom #3: Same as Idiom #2, but using tmpPromise. Notice that this calls consume() only after
    // it's sure it will use the tmp. That's deliberate. Also note that you're required to pass any
    // Inst you create with consumed promises through that promise's inst() function.
    //
    //     ArgPromise promise = tmpPromise(bar);
    //     if (isValidForm(Foo, promise.kind()))
    //         append(promise.inst(Foo, promise.consume(*this)))
    //
    // In both idiom #2 and idiom #3, we don't pin the value to a temporary except when we actually
    // emit the instruction. Both tmp() and tmpPromise().consume(*this) will pin it. Pinning means
    // that we will henceforth require that the value of 'bar' is generated as a separate
    // instruction. We don't want to pin the value to a temporary if we might change our minds, and
    // pass an address operand representing 'bar' to Foo instead.
    //
    // Because tmp() pins, the following is not an idiom you should use:
    //
    //     Tmp tmp = this->tmp(bar);
    //     if (isValidForm(Foo, tmp.kind()))
    //         append(Foo, tmp);
    //
    // That's because if isValidForm() returns false, you will have already pinned the 'bar' to a
    // temporary. You might later want to try to do something like loadPromise(), and that will fail.
    // This arises in operations that have both a Addr,Tmp and Tmp,Addr forms. The following code
    // seems right, but will actually fail to ever match the Tmp,Addr form because by then, the right
    // value is already pinned.
    //
    //     auto tryThings = [this] (const Arg& left, const Arg& right) {
    //         if (isValidForm(Foo, left.kind(), right.kind()))
    //             return Inst(Foo, m_value, left, right);
    //         return Inst();
    //     };
    //     if (Inst result = tryThings(loadAddr(left), tmp(right)))
    //         return result;
    //     if (Inst result = tryThings(tmp(left), loadAddr(right))) // this never succeeds.
    //         return result;
    //     return Inst(Foo, m_value, tmp(left), tmp(right));
    //
    // If you imagine that loadAddr(value) is just loadPromise(value).consume(*this), then this code
    // will run correctly - it will generate OK code - but the second form is never matched.
    // loadAddr(right) will never succeed because it will observe that 'right' is already pinned.
    // Of course, it's exactly because of the risky nature of such code that we don't have a
    // loadAddr() helper and require you to balance ArgPromise's in code like this. Such code will
    // work fine if written as:
    //
    //     auto tryThings = [this] (ArgPromise& left, ArgPromise& right) {
    //         if (isValidForm(Foo, left.kind(), right.kind()))
    //             return left.inst(right.inst(Foo, m_value, left.consume(*this), right.consume(*this)));
    //         return Inst();
    //     };
    //     if (Inst result = tryThings(loadPromise(left), tmpPromise(right)))
    //         return result;
    //     if (Inst result = tryThings(tmpPromise(left), loadPromise(right)))
    //         return result;
    //     return Inst(Foo, m_value, tmp(left), tmp(right));
    //
    // Notice that we did use tmp in the fall-back case at the end, because by then, we know for sure
    // that we want a tmp. But using tmpPromise in the tryThings() calls ensures that doing so
    // doesn't prevent us from trying loadPromise on the same value.
    Tmp tmp(Value* value)
    {
        Tmp& tmp = m_valueToTmp[value];
        if (!tmp) {
            while (shouldCopyPropagate(value))
                value = value->child(0);

            if (value->opcode() == FramePointer)
                return Tmp(GPRInfo::callFrameRegister);

            Tmp& realTmp = m_valueToTmp[value];
            if (!realTmp) {
                realTmp = m_code.newTmp(value->resultBank());
                if (m_procedure.isFastConstant(value->key()))
                    m_code.addFastTmp(realTmp);
                if (B3LowerToAirInternal::verbose)
                    dataLog("Tmp for ", *value, ": ", realTmp, "\n");
            }
            tmp = realTmp;
        }
        return tmp;
    }

    ArgPromise tmpPromise(Value* value)
    {
        return ArgPromise::tmp(value);
    }

    Tmp tmpForType(Type type)
    {
        return m_code.newTmp(bankForType(type));
    }

    const Vector<Tmp>& tmpsForTuple(Value* tupleValue)
    {
        ASSERT(tupleValue->type().isTuple());

        switch (tupleValue->opcode()) {
        case Phi:
        case Patchpoint:
        case BottomTuple: {
            return m_tupleValueToTmps.find(tupleValue)->value;
        }
        case Get:
        case Set:
            return m_variableToTmps.find(tupleValue->as<VariableValue>()->variable())->value;
        default:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool canBeInternal(Value* value)
    {
        // If one of the internal things has already been computed, then we don't want to cause
        // it to be recomputed again.
        if (m_valueToTmp[value])
            return false;
        
        // We require internals to have only one use - us. It's not clear if this should be numUses() or
        // numUsingInstructions(). Ideally, it would be numUsingInstructions(), except that it's not clear
        // if we'd actually do the right thing when matching over such a DAG pattern. For now, it simply
        // doesn't matter because we don't implement patterns that would trigger this.
        if (m_useCounts.numUses(value) != 1)
            return false;

        return true;
    }

    // If you ask canBeInternal() and then construct something from that, and you commit to emitting
    // that code, then you must commitInternal() on that value. This is tricky, and you only need to
    // do it if you're pattern matching by hand rather than using the patterns language. Long story
    // short, you should avoid this by using the pattern matcher to match patterns.
    void commitInternal(Value* value)
    {
        if (value)
            m_locked.add(value);
    }

    bool crossesInterference(Value* value)
    {
        // If it's in a foreign block, then be conservative. We could handle this if we were
        // willing to do heavier analysis. For example, if we had liveness, then we could label
        // values as "crossing interference" if they interfere with anything that they are live
        // across. But, it's not clear how useful this would be.
        if (value->owner != m_value->owner)
            return true;

        Effects effects = value->effects();

        for (unsigned i = m_index; i--;) {
            Value* otherValue = m_block->at(i);
            if (otherValue == value)
                return false;
            if (effects.interferes(otherValue->effects()))
                return true;
        }

        ASSERT_NOT_REACHED();
        return true;
    }

    bool isMergeableValue(Value* v, B3::Opcode b3Opcode, bool checkCanBeInternal = false)
    { 
        if (v->opcode() != b3Opcode)
            return false;
        if (checkCanBeInternal && !canBeInternal(v))
            return false;
        if (m_locked.contains(v->child(0)))
            return false;
        return true;
    }

    template<typename Int, typename = Value::IsLegalOffset<Int>>
    Arg indexArg(Tmp base, Value* index, unsigned scale, Int offset)
    {
#if CPU(ARM64)
        // Maybe, the ideal approach is to introduce a decorator (Index@EXT) to the Air operand 
        // to provide an extension opportunity for the specific form under the Air opcode.
        if (isMergeableValue(index, ZExt32))
            return Arg::index(base, tmp(index->child(0)), scale, offset, MacroAssembler::Extend::ZExt32);
        if (isMergeableValue(index, SExt32))
            return Arg::index(base, tmp(index->child(0)), scale, offset, MacroAssembler::Extend::SExt32);
#endif
        return Arg::index(base, tmp(index), scale, offset);
    }

    template<typename Int, typename = Value::IsLegalOffset<Int>>
    std::optional<unsigned> scaleForShl(Value* shl, Int offset, std::optional<Width> width = std::nullopt)
    {
        if (shl->opcode() != Shl)
            return std::nullopt;
        if (!shl->child(1)->hasInt32())
            return std::nullopt;
        unsigned logScale = shl->child(1)->asInt32();
        if (shl->type() == Int32)
            logScale &= 31;
        else
            logScale &= 63;
        // Use 64-bit math to perform the shift so that <<32 does the right thing, but then switch
        // to signed since that's what all of our APIs want.
        int64_t bigScale = static_cast<uint64_t>(1) << static_cast<uint64_t>(logScale);
        if (!isRepresentableAs<int32_t>(bigScale))
            return std::nullopt;
        unsigned scale = static_cast<int32_t>(bigScale);
        if (!Arg::isValidIndexForm(scale, offset, width))
            return std::nullopt;
        return scale;
    }

    // This turns the given operand into an address.
    template<typename Int, typename = Value::IsLegalOffset<Int>>
    Arg effectiveAddr(Value* address, Int offset, Width width)
    {
        // This function currently is currently only used for loads/stores, so
        // using Air::Move is appropriate.
        ASSERT(Arg::isValidAddrForm(Air::Move, offset, width));
        
        auto fallback = [&] () -> Arg {
            return Arg::addr(tmp(address), offset);
        };
        
        static constexpr unsigned lotsOfUses = 10; // This is arbitrary and we should tune it eventually.

        // Only match if the address value isn't used in some large number of places.
        if (m_useCounts.numUses(address) > lotsOfUses)
            return fallback();
        
        switch (address->opcode()) {
        case Add: {
            Value* left = address->child(0);
            Value* right = address->child(1);

            auto tryIndex = [&] (Value* index, Value* base) -> Arg {
                std::optional<unsigned> scale = scaleForShl(index, offset, width);
                if (!scale)
                    return Arg();
                if (m_locked.contains(index->child(0)) || m_locked.contains(base))
                    return Arg();
                return indexArg(tmp(base), index->child(0), *scale, offset);
            };

            if (Arg result = tryIndex(left, right))
                return result;
            if (Arg result = tryIndex(right, left))
                return result;

            if (m_locked.contains(left) || m_locked.contains(right)
                || !Arg::isValidIndexForm(1, offset, width))
                return fallback();
            
            return indexArg(tmp(left), right, 1, offset);
        }

        case Shl: {
            Value* left = address->child(0);

            // We'll never see child(1)->isInt32(0), since that would have been reduced. If the shift
            // amount is greater than 1, then there isn't really anything smart that we could do here.
            // We avoid using baseless indexes because their encoding isn't particularly efficient.
            if (m_locked.contains(left) || !address->child(1)->isInt32(1)
                || !Arg::isValidIndexForm(1, offset, width))
                return fallback();

            return indexArg(tmp(left), left, 1, offset);
        }

        case FramePointer:
            return Arg::addr(Tmp(GPRInfo::callFrameRegister), offset);

        case SlotBase:
            return Arg::stack(address->as<SlotBaseValue>()->slot(), offset);

        case WasmAddress: {
            WasmAddressValue* wasmAddress = address->as<WasmAddressValue>();
            Value* pointer = wasmAddress->child(0);
            if (!Arg::isValidIndexForm(1, offset, width) || m_locked.contains(pointer))
                return fallback();

            // FIXME: We should support ARM64 LDR 32-bit addressing, which will
            // allow us to fuse a Shl ptr, 2 into the address. Additionally, and
            // perhaps more importantly, it would allow us to avoid a truncating
            // move. See: https://bugs.webkit.org/show_bug.cgi?id=163465

            return indexArg(Tmp(wasmAddress->pinnedGPR()), pointer, 1, offset);
        }

        default:
            return fallback();
        }
    }

    // This gives you the address of the given Load or Store. If it's not a Load or Store, then
    // it returns Arg().
    Arg addr(Value* memoryValue)
    {
        MemoryValue* value = memoryValue->as<MemoryValue>();
        if (!value)
            return Arg();
        
        if (value->requiresSimpleAddr())
            return Arg::simpleAddr(tmp(value->lastChild()));

        Value::OffsetType offset = value->offset();
        Width width = value->accessWidth();

        Arg result = effectiveAddr(value->lastChild(), offset, width);
        RELEASE_ASSERT(result.isValidForm(Air::Move, width));

        return result;
    }
    
    template<typename... Args>
    Inst trappingInst(bool traps, Args&&... args)
    {
        Inst result(std::forward<Args>(args)...);
        result.kind.effects |= traps;
        return result;
    }
    
    template<typename... Args>
    Inst trappingInst(Value* value, Args&&... args)
    {
        return trappingInst(value->traps(), std::forward<Args>(args)...);
    }
    
    ArgPromise loadPromiseAnyOpcode(Value* loadValue)
    {
        RELEASE_ASSERT(loadValue->as<MemoryValue>());
        if (!canBeInternal(loadValue))
            return Arg();
        if (crossesInterference(loadValue))
            return Arg();
        // On x86, all loads have fences. Doing this kind of instruction selection will move the load,
        // but that's fine because our interference analysis stops the motion of fences around other
        // fences. So, any load motion we introduce here would not be observable.
        if (!isX86() && loadValue->as<MemoryValue>()->hasFence())
            return Arg();
        Arg loadAddr = addr(loadValue);
        RELEASE_ASSERT(loadAddr);
        ArgPromise result(loadAddr, loadValue);
        if (loadValue->traps())
            result.setTraps(true);
        return result;
    }

    ArgPromise loadPromise(Value* loadValue, B3::Opcode loadOpcode)
    {
        if (loadValue->opcode() != loadOpcode)
            return Arg();
        return loadPromiseAnyOpcode(loadValue);
    }

    ArgPromise loadPromise(Value* loadValue)
    {
        return loadPromise(loadValue, Load);
    }

    Arg imm(int64_t intValue)
    {
        if (Arg::isValidImmForm(intValue))
            return Arg::imm(intValue);
        return Arg();
    }

    Arg imm(Value* value)
    {
        if (value->hasInt())
            return imm(value->asInt());
        return Arg();
    }

    Arg bitImm(Value* value)
    {
        if (value->hasInt()) {
            int64_t intValue = value->asInt();
            if (Arg::isValidBitImmForm(intValue))
                return Arg::bitImm(intValue);
        }
        return Arg();
    }

    Arg bitImm64(Value* value)
    {
        if (value->hasInt()) {
            int64_t intValue = value->asInt();
            if (Arg::isValidBitImm64Form(intValue))
                return Arg::bitImm64(intValue);
        }
        return Arg();
    }

    Arg zeroReg()
    {
        return Arg::zeroReg();
    }

    Arg immOrTmp(Value* value)
    {
        if (Arg result = imm(value))
            return result;
        return tmp(value);
    }

    template<typename Functor>
    void forEachImmOrTmp(Value* value, const Functor& func)
    {
        ASSERT(value->type() != Void);
        if (!value->type().isTuple()) {
            func(immOrTmp(value), value->type(), 0);
            return;
        }

        const Vector<Type>& tuple = m_procedure.tupleForType(value->type());
        const auto& tmps = tmpsForTuple(value);
        for (unsigned i = 0; i < tuple.size(); ++i)
            func(tmps[i], tuple[i], i);
    }

    // By convention, we use Oops to mean "I don't know".
    Air::Opcode tryOpcodeForType(
        Air::Opcode opcode32, Air::Opcode opcode64, Air::Opcode opcodeDouble, Air::Opcode opcodeFloat, Type type)
    {
        Air::Opcode opcode;
        switch (type.kind()) {
        case Int32:
            opcode = opcode32;
            break;
        case Int64:
            opcode = opcode64;
            break;
        case Float:
            opcode = opcodeFloat;
            break;
        case Double:
            opcode = opcodeDouble;
            break;
        default:
            opcode = Air::Oops;
            break;
        }

        return opcode;
    }

    Air::Opcode tryOpcodeForType(Air::Opcode opcode32, Air::Opcode opcode64, Type type)
    {
        return tryOpcodeForType(opcode32, opcode64, Air::Oops, Air::Oops, type);
    }

    Air::Opcode opcodeForType(
        Air::Opcode opcode32, Air::Opcode opcode64, Air::Opcode opcodeDouble, Air::Opcode opcodeFloat, Type type)
    {
        Air::Opcode opcode = tryOpcodeForType(opcode32, opcode64, opcodeDouble, opcodeFloat, type);
        RELEASE_ASSERT(opcode != Air::Oops);
        return opcode;
    }

    Air::Opcode opcodeForType(Air::Opcode opcode32, Air::Opcode opcode64, Type type)
    {
        return tryOpcodeForType(opcode32, opcode64, Air::Oops, Air::Oops, type);
    }

    template<Air::Opcode opcode32, Air::Opcode opcode64, Air::Opcode opcodeDouble = Air::Oops, Air::Opcode opcodeFloat = Air::Oops>
    void appendUnOp(Value* value)
    {
        Air::Opcode opcode = opcodeForType(opcode32, opcode64, opcodeDouble, opcodeFloat, value->type());
        
        Tmp result = tmp(m_value);

        // Two operand forms like:
        //     Op a, b
        // mean something like:
        //     b = Op a

        ArgPromise addr = loadPromise(value);
        if (isValidForm(opcode, addr.kind(), Arg::Tmp)) {
            append(addr.inst(opcode, m_value, addr.consume(*this), result));
            return;
        }

        if (isValidForm(opcode, Arg::Tmp, Arg::Tmp)) {
            append(opcode, tmp(value), result);
            return;
        }

        ASSERT(value->type() == m_value->type());
        append(relaxedMoveForType(m_value->type()), tmp(value), result);
        append(opcode, result);
    }

    Air::Opcode opcodeBasedOnShiftKind(B3::Opcode b3Opcode, 
        Air::Opcode shl32, Air::Opcode shl64, 
        Air::Opcode sshr32, Air::Opcode sshr64, 
        Air::Opcode zshr32, Air::Opcode zshr64)
    {
        switch (b3Opcode) {
        case Shl:
            return tryOpcodeForType(shl32, shl64, m_value->type());
        case SShr:
            return tryOpcodeForType(sshr32, sshr64, m_value->type());
        case ZShr:
            return tryOpcodeForType(zshr32, zshr64, m_value->type());
        default:
            return Air::Oops;
        }
    }

    // Call this method when doing two-operand lowering of a commutative operation. You have a choice of
    // which incoming Value is moved into the result. This will select which one is likely to be most
    // profitable to use as the result. Doing the right thing can have big performance consequences in tight
    // kernels.
    bool preferRightForResult(Value* left, Value* right)
    {
        // The default is to move left into result, because that's required for non-commutative instructions.
        // The value that we want to move into result position is the one that dies here. So, if we're
        // compiling a commutative operation and we know that actually right is the one that dies right here,
        // then we can flip things around to help coalescing, which then kills the move instruction.
        //
        // But it's more complicated:
        // - Used-once is a bad estimate of whether the variable dies here.
        // - A child might be a candidate for coalescing with this value.
        //
        // Currently, we have machinery in place to recognize super obvious forms of the latter issue.
        
        // We recognize when a child is a Phi that has this value as one of its children. We're very
        // conservative about this; for example we don't even consider transitive Phi children.
        bool leftIsPhiWithThis = m_phiChildren[left].transitivelyUses(m_value);
        bool rightIsPhiWithThis = m_phiChildren[right].transitivelyUses(m_value);

        if (leftIsPhiWithThis != rightIsPhiWithThis)
            return rightIsPhiWithThis;

        if (m_useCounts.numUsingInstructions(right) != 1)
            return false;
        
        if (m_useCounts.numUsingInstructions(left) != 1)
            return true;

        // The use count might be 1 if the variable is live around a loop. We can guarantee that we
        // pick the variable that is least likely to suffer this problem if we pick the one that
        // is closest to us in an idom walk. By convention, we slightly bias this in favor of
        // returning true.

        // We cannot prefer right if right is further away in an idom walk.
        if (m_dominators.strictlyDominates(right->owner, left->owner))
            return false;

        return true;
    }
    
    template<Air::Opcode opcode32, Air::Opcode opcode64, Air::Opcode opcodeDouble, Air::Opcode opcodeFloat, Commutativity commutativity = NotCommutative>
    void appendBinOp(Value* left, Value* right)
    {
        Air::Opcode opcode = opcodeForType(opcode32, opcode64, opcodeDouble, opcodeFloat, left->type());
        
        Tmp result = tmp(m_value);
        
        // Three-operand forms like:
        //     Op a, b, c
        // mean something like:
        //     c = a Op b

        if (isValidForm(opcode, Arg::Imm, Arg::Tmp, Arg::Tmp)) {
            if (commutativity == Commutative) {
                if (imm(right)) {
                    append(opcode, imm(right), tmp(left), result);
                    return;
                }
            } else {
                // A non-commutative operation could have an immediate in left.
                if (imm(left)) {
                    append(opcode, imm(left), tmp(right), result);
                    return;
                }
            }
        }

        if (isValidForm(opcode, Arg::BitImm, Arg::Tmp, Arg::Tmp)) {
            if (commutativity == Commutative) {
                if (Arg rightArg = bitImm(right)) {
                    append(opcode, rightArg, tmp(left), result);
                    return;
                }
            } else {
                // A non-commutative operation could have an immediate in left.
                if (Arg leftArg = bitImm(left)) {
                    append(opcode, leftArg, tmp(right), result);
                    return;
                }
            }
        }

        if (isValidForm(opcode, Arg::BitImm64, Arg::Tmp, Arg::Tmp)) {
            if (commutativity == Commutative) {
                if (Arg rightArg = bitImm64(right)) {
                    append(opcode, rightArg, tmp(left), result);
                    return;
                }
            } else {
                // A non-commutative operation could have an immediate in left.
                if (Arg leftArg = bitImm64(left)) {
                    append(opcode, leftArg, tmp(right), result);
                    return;
                }
            }
        }

        if (imm(right) && isValidForm(opcode, Arg::Tmp, Arg::Imm, Arg::Tmp)) {
            append(opcode, tmp(left), imm(right), result);
            return;
        }

        // Note that no extant architecture has a three-operand form of binary operations that also
        // load from memory. If such an abomination did exist, we would handle it somewhere around
        // here.

        // Two-operand forms like:
        //     Op a, b
        // mean something like:
        //     b = b Op a

        // At this point, we prefer versions of the operation that have a fused load or an immediate
        // over three operand forms.

        if (left != right) {
            ArgPromise leftAddr = loadPromise(left);
            if (isValidForm(opcode, leftAddr.kind(), Arg::Tmp, Arg::Tmp)) {
                append(leftAddr.inst(opcode, m_value, leftAddr.consume(*this), tmp(right), result));
                return;
            }

            if (commutativity == Commutative) {
                if (isValidForm(opcode, leftAddr.kind(), Arg::Tmp)) {
                    append(relaxedMoveForType(m_value->type()), tmp(right), result);
                    append(leftAddr.inst(opcode, m_value, leftAddr.consume(*this), result));
                    return;
                }
            }

            ArgPromise rightAddr = loadPromise(right);
            if (isValidForm(opcode, Arg::Tmp, rightAddr.kind(), Arg::Tmp)) {
                append(rightAddr.inst(opcode, m_value, tmp(left), rightAddr.consume(*this), result));
                return;
            }

            if (commutativity == Commutative) {
                if (isValidForm(opcode, rightAddr.kind(), Arg::Tmp, Arg::Tmp)) {
                    append(rightAddr.inst(opcode, m_value, rightAddr.consume(*this), tmp(left), result));
                    return;
                }
            }

            if (isValidForm(opcode, rightAddr.kind(), Arg::Tmp)) {
                append(relaxedMoveForType(m_value->type()), tmp(left), result);
                append(rightAddr.inst(opcode, m_value, rightAddr.consume(*this), result));
                return;
            }
        }

        if (imm(right) && isValidForm(opcode, Arg::Imm, Arg::Tmp)) {
            append(relaxedMoveForType(m_value->type()), tmp(left), result);
            append(opcode, imm(right), result);
            return;
        }

        if (isValidForm(opcode, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
            append(opcode, tmp(left), tmp(right), result);
            return;
        }

        if (commutativity == Commutative && preferRightForResult(left, right)) {
            append(relaxedMoveForType(m_value->type()), tmp(right), result);
            append(opcode, tmp(left), result);
            return;
        }
        
        append(relaxedMoveForType(m_value->type()), tmp(left), result);
        append(opcode, tmp(right), result);
    }

    template<Air::Opcode opcode32, Air::Opcode opcode64, Commutativity commutativity = NotCommutative>
    void appendBinOp(Value* left, Value* right)
    {
        appendBinOp<opcode32, opcode64, Air::Oops, Air::Oops, commutativity>(left, right);
    }

    template<Air::Opcode opcode32, Air::Opcode opcode64>
    void appendShift(Value* value, Value* amount)
    {
        using namespace Air;
        Air::Opcode opcode = opcodeForType(opcode32, opcode64, value->type());
        
        if (imm(amount)) {
            if (isValidForm(opcode, Arg::Tmp, Arg::Imm, Arg::Tmp)) {
                append(opcode, tmp(value), imm(amount), tmp(m_value));
                return;
            }
            if (isValidForm(opcode, Arg::Imm, Arg::Tmp)) {
                append(Move, tmp(value), tmp(m_value));
                append(opcode, imm(amount), tmp(m_value));
                return;
            }
        }

        if (isValidForm(opcode, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
            append(opcode, tmp(value), tmp(amount), tmp(m_value));
            return;
        }

        append(Move, tmp(value), tmp(m_value));
        append(Move, tmp(amount), m_ecx);
        append(opcode, m_ecx, tmp(m_value));
    }

    template<Air::Opcode opcode32, Air::Opcode opcode64>
    bool tryAppendStoreUnOp(Value* value)
    {
        Air::Opcode opcode = tryOpcodeForType(opcode32, opcode64, value->type());
        if (opcode == Air::Oops)
            return false;
        
        Arg storeAddr = addr(m_value);
        ASSERT(storeAddr);

        ArgPromise loadPromise = this->loadPromise(value);
        if (loadPromise.peek() != storeAddr)
            return false;

        if (!isValidForm(opcode, storeAddr.kind()))
            return false;
        
        loadPromise.consume(*this);
        append(trappingInst(m_value, loadPromise.inst(opcode, m_value, storeAddr)));
        return true;
    }

    template<
        Air::Opcode opcode32, Air::Opcode opcode64, Commutativity commutativity = NotCommutative>
    bool tryAppendStoreBinOp(Value* left, Value* right)
    {
        RELEASE_ASSERT(m_value->as<MemoryValue>());
        
        Air::Opcode opcode = tryOpcodeForType(opcode32, opcode64, left->type());
        if (opcode == Air::Oops)
            return false;
        
        if (m_value->as<MemoryValue>()->hasFence())
            return false;
        
        Arg storeAddr = addr(m_value);
        ASSERT(storeAddr);

        auto getLoadPromise = [&] (Value* load) -> ArgPromise {
            switch (m_value->opcode()) {
            case B3::Store:
                if (load->opcode() != B3::Load)
                    return ArgPromise();
                break;
            case B3::Store8:
                if (load->opcode() != B3::Load8Z && load->opcode() != B3::Load8S)
                    return ArgPromise();
                break;
            case B3::Store16:
                if (load->opcode() != B3::Load16Z && load->opcode() != B3::Load16S)
                    return ArgPromise();
                break;
            default:
                return ArgPromise();
            }
            return loadPromiseAnyOpcode(load);
        };
        
        ArgPromise loadPromise;
        Value* otherValue = nullptr;

        loadPromise = getLoadPromise(left);
        if (loadPromise.peek() == storeAddr)
            otherValue = right;
        else if (commutativity == Commutative) {
            loadPromise = getLoadPromise(right);
            if (loadPromise.peek() == storeAddr)
                otherValue = left;
        }

        if (!otherValue)
            return false;

        if (isValidForm(opcode, Arg::Imm, storeAddr.kind()) && imm(otherValue)) {
            loadPromise.consume(*this);
            append(trappingInst(m_value, loadPromise.inst(opcode, m_value, imm(otherValue), storeAddr)));
            return true;
        }

        if (!isValidForm(opcode, Arg::Tmp, storeAddr.kind()))
            return false;

        loadPromise.consume(*this);
        append(trappingInst(m_value, loadPromise.inst(opcode, m_value, tmp(otherValue), storeAddr)));
        return true;
    }

    Inst createStore(Air::Kind move, Value* value, const Arg& dest)
    {
        using namespace Air;
        if (auto imm_value = imm(value)) {
            if (!imm_value.value()) {
                switch (move.opcode) {
                default:
                    break;
                case Air::Move32:
                    if (isValidForm(Store32, Arg::ZeroReg, dest.kind()) && dest.isValidForm(Move, Width32))
                        return Inst(Store32, m_value, zeroReg(), dest);
                    break;
                case Air::Move:
                    if (isValidForm(Store64, Arg::ZeroReg, dest.kind()) && dest.isValidForm(Move, Width64))
                        return Inst(Store64, m_value, zeroReg(), dest);
                    break;
                }
            }
            if (isValidForm(move.opcode, Arg::Imm, dest.kind()))
                return Inst(move, m_value, imm_value, dest);
        }

        return Inst(move, m_value, tmp(value), dest);
    }
    
    Air::Opcode storeOpcode(Width width, Bank bank)
    {
        using namespace Air;
        switch (width) {
        case Width8:
            RELEASE_ASSERT(bank == GP);
            return Air::Store8;
        case Width16:
            RELEASE_ASSERT(bank == GP);
            return Air::Store16;
        case Width32:
            switch (bank) {
            case GP:
                return Move32;
            case FP:
                return MoveFloat;
            }
            break;
        case Width64:
            RELEASE_ASSERT(is64Bit());
            switch (bank) {
            case GP:
                return Move;
            case FP:
                return MoveDouble;
            }
            break;
        case Width128:
            RELEASE_ASSERT(is64Bit() && Options::useWebAssemblySIMD());
            RELEASE_ASSERT(bank == FP);
            return MoveVector;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    void appendStore(Value* value, const Arg& dest)
    {
        using namespace Air;
        MemoryValue* memory = value->as<MemoryValue>();
        RELEASE_ASSERT(memory->isStore());

        Air::Kind kind;
        if (memory->hasFence()) {
            RELEASE_ASSERT(memory->accessBank() == GP);
            
            if (isX86()) {
                kind = OPCODE_FOR_WIDTH(Xchg, memory->accessWidth());
                kind.effects = true;
                Tmp swapTmp = m_code.newTmp(GP);
                append(relaxedMoveForType(memory->accessType()), tmp(memory->child(0)), swapTmp);
                append(kind, swapTmp, dest);
                return;
            }
            
            kind = OPCODE_FOR_WIDTH(StoreRel, memory->accessWidth());
        } else
            kind = storeOpcode(memory->accessWidth(), memory->accessBank());
        
        kind.effects |= memory->traps();
        
        append(createStore(kind, memory->child(0), dest));
    }

    template<Air::Opcode i8, Air::Opcode i16, Air::Opcode i32, Air::Opcode i64, Air::Opcode f32, Air::Opcode f64>
    constexpr Air::Opcode simdOpcode(SIMDLane lane)
    {
        if (scalarTypeIsFloatingPoint(lane)) {
            switch (elementByteSize(lane)) {
            case 4:
                return f32;
            case 8:
                return f64;
            }
        } else {
            switch (elementByteSize(lane)) {
            case 1:
                return i8;
            case 2:
                return i16;
            case 4:
                return i32;
            case 8:
                return i64;
            }
        }

        RELEASE_ASSERT_NOT_REACHED();
    }

    template<Air::Opcode unsignedI8, Air::Opcode signedI8, Air::Opcode unsignedI16, Air::Opcode signedI16, Air::Opcode i32, Air::Opcode i64, Air::Opcode f32, Air::Opcode f64>
    constexpr Air::Opcode simdOpcode(SIMDLane lane, SIMDSignMode signMode)
    {
        if (scalarTypeIsFloatingPoint(lane)) {
            switch (elementByteSize(lane)) {
            case 4:
                return f32;
            case 8:
                return f64;
            }
        } else {
            switch (elementByteSize(lane)) {
            case 1:
                RELEASE_ASSERT(signMode == SIMDSignMode::Signed || signMode == SIMDSignMode::Unsigned);
                return signMode == SIMDSignMode::Signed ? signedI8 : unsignedI8;
            case 2:
                RELEASE_ASSERT(signMode == SIMDSignMode::Signed || signMode == SIMDSignMode::Unsigned);
                return signMode == SIMDSignMode::Signed ? signedI16 : unsignedI16;
            case 4:
                return i32;
            case 8:
                return i64;
            }
        }

        RELEASE_ASSERT_NOT_REACHED();
    }

#define GET_SIMD_OPCODE(info, opcode) \
    simdOpcode<opcode##Int8, opcode##Int16, opcode##Int32, opcode##Int64, opcode##Float32, opcode##Float64>(lane)

#define GET_SIGNED_SIMD_OPCODE(lane, signMode, opcode) \
    simdOpcode<opcode##UnsignedInt8, opcode##SignedInt8, opcode##UnsignedInt16, opcode##SignedInt16, opcode##Int32, opcode##Int64, opcode##Float32, opcode##Float64>(lane, signMode)

    void emitSIMDCompare(Air::Arg relCond, Air::Arg doubleCond)
    {
        SIMDValue* value = m_value->as<SIMDValue>();
        auto lane = value->simdLane();
        bool isFloatingPoint = scalarTypeIsFloatingPoint(lane);
        auto cond = isFloatingPoint ? doubleCond : relCond;
        auto condKind = isFloatingPoint ? Arg::DoubleCond : Arg::RelCond;
        RELEASE_ASSERT(cond);
        Air::Opcode airOp = isFloatingPoint ? Air::CompareFloatingPointVector : Air::CompareIntegerVector;
        if (isValidForm(airOp, condKind, Arg::SIMDInfo, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
            append(airOp,
                cond, Arg::simdInfo(value->simdInfo()), tmp(value->child(0)), tmp(value->child(1)), tmp(value));
            return;
        }
        if (isValidForm(airOp, condKind, Arg::SIMDInfo, Arg::Tmp, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
            append(airOp,
                cond, Arg::simdInfo(value->simdInfo()), tmp(value->child(0)), tmp(value->child(1)), tmp(value), m_code.newTmp(FP));
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    void emitSIMDCompare(Air::Arg cond)
    {
        if (cond.isRelCond())
            emitSIMDCompare(cond, Air::Arg());
        else if (cond.isDoubleCond())
            emitSIMDCompare(Air::Arg(), cond);
    }

    template<unsigned numScratch = 0>
    void emitSIMDBinaryOp(Air::Opcode op)
    {
        SIMDValue* value = m_value->as<SIMDValue>();
        if constexpr (!numScratch)
            append(op, Arg::simdInfo(value->simdInfo()), tmp(value->child(0)), tmp(value->child(1)), tmp(value));
        else if (numScratch == 1)
            append(op, Arg::simdInfo(value->simdInfo()), tmp(value->child(0)), tmp(value->child(1)), tmp(value), m_code.newTmp(FP));
        else
            RELEASE_ASSERT_NOT_REACHED();
    }

    template<unsigned numScratch = 0>
    void emitSIMDMonomorphicBinaryOp(Air::Opcode op)
    {
        SIMDValue* value = m_value->as<SIMDValue>();
        if constexpr (!numScratch)
            append(op, tmp(value->child(0)), tmp(value->child(1)), tmp(value));
        else if (numScratch == 1)
            append(op, tmp(value->child(0)), tmp(value->child(1)), tmp(value), m_code.newTmp(FP));
        else
            RELEASE_ASSERT_NOT_REACHED();
    }

    void emitSIMDUnaryOp(Air::Opcode op)
    {
        SIMDValue* value = m_value->as<SIMDValue>();

        if (isValidForm(op, Arg::SIMDInfo, Arg::Tmp, Arg::Tmp)) {
            append(op, Arg::simdInfo(value->simdInfo()), tmp(value->child(0)), tmp(value));
            return;
        }
        if (isValidForm(op, Arg::SIMDInfo, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
            append(op, Arg::simdInfo(value->simdInfo()), tmp(value->child(0)), tmp(value), m_code.newTmp(FP));
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    void emitSIMDMonomorphicUnaryOp(Air::Opcode op)
    {
        SIMDValue* value = m_value->as<SIMDValue>();
        append(op, tmp(value->child(0)), tmp(value));
    }

    template<typename... Arguments>
    void print(Arguments&&... arguments)
    {
        Value* origin = m_value;
        print(origin, std::forward<Arguments>(arguments)...);
    }

    template<typename... Arguments>
    void print(Value* origin, Arguments&&... arguments)
    {
        auto printList = Printer::makePrintRecordList(arguments...);
        auto printSpecial = static_cast<Air::PrintSpecial*>(m_code.addSpecial(makeUnique<Air::PrintSpecial>(printList)));
        Inst inst(Air::Patch, origin, Arg::special(printSpecial));
        Printer::appendAirArgs(inst, std::forward<Arguments>(arguments)...);
        append(WTFMove(inst));
    }

    template<typename... Arguments>
    void append(Air::Kind kind, Arguments&&... arguments)
    {
        m_insts.last().append(Inst(kind, m_value, std::forward<Arguments>(arguments)...));
    }
    
    template<typename... Arguments>
    void appendTrapping(Air::Kind kind, Arguments&&... arguments)
    {
        m_insts.last().append(trappingInst(m_value, kind, m_value, std::forward<Arguments>(arguments)...));
    }
    
    void append(Inst&& inst)
    {
        m_insts.last().append(WTFMove(inst));
    }
    void append(const Inst& inst)
    {
        m_insts.last().append(inst);
    }
    
    void finishAppendingInstructions(Air::BasicBlock* target)
    {
        // Now append the instructions. m_insts contains them in reverse order, so we process
        // it in reverse.
        for (unsigned i = m_insts.size(); i--;) {
            for (Inst& inst : m_insts[i])
                target->appendInst(WTFMove(inst));
        }
        m_insts.shrink(0);
    }
    
    Air::BasicBlock* newBlock()
    {
        return m_blockInsertionSet.insertAfter(m_blockToBlock[m_block]);
    }

    // NOTE: This will create a continuation block (`nextBlock`) *after* any blocks you've created using
    // newBlock(). So, it's preferable to create all of your blocks upfront using newBlock(). Also note
    // that any code you emit before this will be prepended to the continuation, and any code you emit
    // after this will be appended to the previous block.
    void splitBlock(Air::BasicBlock*& previousBlock, Air::BasicBlock*& nextBlock)
    {
        Air::BasicBlock* block = m_blockToBlock[m_block];
        
        previousBlock = block;
        nextBlock = m_blockInsertionSet.insertAfter(block);
        
        finishAppendingInstructions(nextBlock);
        nextBlock->successors() = block->successors();
        block->successors().clear();
        
        m_insts.append(Vector<Inst>());
    }
    
    template<typename T, typename... Arguments>
    T* ensureSpecial(T*& field, Arguments&&... arguments)
    {
        if (!field) {
            field = static_cast<T*>(
                m_code.addSpecial(makeUnique<T>(std::forward<Arguments>(arguments)...)));
        }
        return field;
    }

    template<typename... Arguments>
    CheckSpecial* ensureCheckSpecial(Arguments&&... arguments)
    {
        CheckSpecial::Key key(std::forward<Arguments>(arguments)...);
        auto result = m_checkSpecials.add(key, nullptr);
        return ensureSpecial(result.iterator->value, key);
    }

    void fillStackmap(Inst& inst, StackmapValue* stackmap, unsigned numSkipped)
    {
        for (unsigned i = numSkipped; i < stackmap->numChildren(); ++i) {
            ConstrainedValue value = stackmap->constrainedChild(i);

            Arg arg;
            switch (value.rep().kind()) {
            case ValueRep::WarmAny:
            case ValueRep::ColdAny:
            case ValueRep::LateColdAny:
                if (imm(value.value()))
                    arg = imm(value.value());
                else if (value.value()->hasInt64())
                    arg = Arg::bigImm(value.value()->asInt64());
                else if (value.value()->hasDouble() && canBeInternal(value.value())) {
                    commitInternal(value.value());
                    arg = Arg::bigImm(bitwise_cast<int64_t>(value.value()->asDouble()));
                } else if (value.value()->hasFloat() && canBeInternal(value.value())) {
                    commitInternal(value.value());
                    arg = Arg::bigImm(static_cast<uint64_t>(bitwise_cast<uint32_t>(value.value()->asFloat())));
                } else
                    arg = tmp(value.value());
                break;
            case ValueRep::SomeRegister:
            case ValueRep::SomeLateRegister:
                arg = tmp(value.value());
                break;
            case ValueRep::SomeRegisterWithClobber: {
                Tmp dstTmp = m_code.newTmp(value.value()->resultBank());
                append(relaxedMoveForType(value.value()->type()), immOrTmp(value.value()), dstTmp);
                arg = dstTmp;
                break;
            }
            case ValueRep::LateRegister:
            case ValueRep::Register:
                stackmap->earlyClobbered().remove(value.rep().reg());
                arg = Tmp(value.rep().reg());
                append(relaxedMoveForType(value.value()->type()), immOrTmp(value.value()), arg);
                break;
            case ValueRep::StackArgument:
                arg = Arg::callArg(value.rep().offsetFromSP());
                append(trappingInst(m_value, createStore(moveForType(value.value()->type()), value.value(), arg)));
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            inst.args.append(arg);
        }
    }
    
    // Create an Inst to do the comparison specified by the given value.
    template<typename CompareFunctor, typename TestFunctor, typename CompareDoubleFunctor, typename CompareFloatFunctor>
    Inst createGenericCompare(
        Value* value,
        const CompareFunctor& compare, // Signature: (Width, Arg relCond, Arg, Arg) -> Inst
        const TestFunctor& test, // Signature: (Width, Arg resCond, Arg, Arg) -> Inst
        const CompareDoubleFunctor& compareDouble, // Signature: (Arg doubleCond, Arg, Arg) -> Inst
        const CompareFloatFunctor& compareFloat, // Signature: (Arg doubleCond, Arg, Arg) -> Inst
        bool inverted = false)
    {
        // NOTE: This is totally happy to match comparisons that have already been computed elsewhere
        // since on most architectures, the cost of branching on a previously computed comparison
        // result is almost always higher than just doing another fused compare/branch. The only time
        // it could be worse is if we have a binary comparison and both operands are variables (not
        // constants), and we encounter register pressure. Even in this case, duplicating the compare
        // so that we can fuse it to the branch will be more efficient most of the time, since
        // register pressure is not *that* common. For this reason, this algorithm will always
        // duplicate the comparison.
        //
        // However, we cannot duplicate loads. The canBeInternal() on a load will assume that we
        // already validated canBeInternal() on all of the values that got us to the load. So, even
        // if we are sharing a value, we still need to call canBeInternal() for the purpose of
        // tracking whether we are still in good shape to fuse loads.
        //
        // We could even have a chain of compare values that we fuse, and any member of the chain
        // could be shared. Once any of them are shared, then the shared one's transitive children
        // cannot be locked (i.e. commitInternal()). But if none of them are shared, then we want to
        // lock all of them because that's a prerequisite to fusing the loads so that the loads don't
        // get duplicated. For example, we might have: 
        //
        //     @tmp1 = LessThan(@a, @b)
        //     @tmp2 = Equal(@tmp1, 0)
        //     Branch(@tmp2)
        //
        // If either @a or @b are loads, then we want to have locked @tmp1 and @tmp2 so that they
        // don't emit the loads a second time. But if we had another use of @tmp2, then we cannot
        // lock @tmp1 (or @a or @b) because then we'll get into trouble when the other values that
        // try to share @tmp1 with us try to do their lowering.
        //
        // There's one more wrinkle. If we don't lock an internal value, then this internal value may
        // have already separately locked its children. So, if we're not locking a value then we need
        // to make sure that its children aren't locked. We encapsulate this in two ways:
        //
        // canCommitInternal: This variable tells us if the values that we've fused so far are
        // locked. This means that we're not sharing any of them with anyone. This permits us to fuse
        // loads. If it's false, then we cannot fuse loads and we also need to ensure that the
        // children of any values we try to fuse-by-sharing are not already locked. You don't have to
        // worry about the children locking thing if you use prepareToFuse() before trying to fuse a
        // sharable value. But, you do need to guard any load fusion by checking if canCommitInternal
        // is true.
        //
        // FusionResult prepareToFuse(value): Call this when you think that you would like to fuse
        // some value and that value is not a load. It will automatically handle the shared-or-locked
        // issues and it will clear canCommitInternal if necessary. This will return CannotFuse
        // (which acts like false) if the value cannot be locked and its children are locked. That's
        // rare, but you just need to make sure that you do smart things when this happens (i.e. just
        // use the value rather than trying to fuse it). After you call prepareToFuse(), you can
        // still change your mind about whether you will actually fuse the value. If you do fuse it,
        // you need to call commitFusion(value, fusionResult).
        //
        // commitFusion(value, fusionResult): Handles calling commitInternal(value) if fusionResult
        // is FuseAndCommit.
        
        bool canCommitInternal = true;

        enum FusionResult {
            CannotFuse,
            FuseAndCommit,
            Fuse
        };
        auto prepareToFuse = [&] (Value* value) -> FusionResult {
            if (value == m_value) {
                // It's not actually internal. It's the root value. We're good to go.
                return Fuse;
            }

            if (canCommitInternal && canBeInternal(value)) {
                // We are the only users of this value. This also means that the value's children
                // could not have been locked, since we have now proved that m_value dominates value
                // in the data flow graph. To only other way to value is from a user of m_value. If
                // value's children are shared with others, then they could not have been locked
                // because their use count is greater than 1. If they are only used from value, then
                // in order for value's children to be locked, value would also have to be locked,
                // and we just proved that it wasn't.
                return FuseAndCommit;
            }

            // We're going to try to share value with others. It's possible that some other basic
            // block had already emitted code for value and then matched over its children and then
            // locked them, in which case we just want to use value instead of duplicating it. So, we
            // validate the children. Note that this only arises in linear chains like:
            //
            //     BB#1:
            //         @1 = Foo(...)
            //         @2 = Bar(@1)
            //         Jump(#2)
            //     BB#2:
            //         @3 = Baz(@2)
            //
            // Notice how we could start by generating code for BB#1 and then decide to lock @1 when
            // generating code for @2, if we have some way of fusing Bar and Foo into a single
            // instruction. This is legal, since indeed @1 only has one user. The fact that @2 now
            // has a tmp (i.e. @2 is pinned), canBeInternal(@2) will return false, which brings us
            // here. In that case, we cannot match over @2 because then we'd hit a hazard if we end
            // up deciding not to fuse Foo into the fused Baz/Bar.
            //
            // Happily, there are only two places where this kind of child validation happens is in
            // rules that admit sharing, like this and effectiveAddress().
            //
            // N.B. We could probably avoid the need to do value locking if we committed to a well
            // chosen code generation order. For example, if we guaranteed that all of the users of
            // a value get generated before that value, then there's no way for the lowering of @3 to
            // see @1 locked. But we don't want to do that, since this is a greedy instruction
            // selector and so we want to be able to play with order.
            for (Value* child : value->children()) {
                if (m_locked.contains(child))
                    return CannotFuse;
            }

            // It's safe to share value, but since we're sharing, it means that we aren't locking it.
            // If we don't lock it, then fusing loads is off limits and all of value's children will
            // have to go through the sharing path as well. Fusing loads is off limits because the load
            // could already have been emitted elsehwere - so fusing it here would duplicate the load.
            // We don't consider that to be a legal optimization.
            canCommitInternal = false;
            
            return Fuse;
        };

        auto commitFusion = [&] (Value* value, FusionResult result) {
            if (result == FuseAndCommit)
                commitInternal(value);
        };
        
        // Chew through any inversions. This loop isn't necessary for comparisons and branches, but
        // we do need at least one iteration of it for Check.
        for (;;) {
            bool shouldInvert =
                (value->opcode() == BitXor && value->child(1)->hasInt() && (value->child(1)->asInt() == 1) && value->child(0)->returnsBool())
                || (value->opcode() == Equal && value->child(1)->isInt(0));
            if (!shouldInvert)
                break;

            FusionResult fusionResult = prepareToFuse(value);
            if (fusionResult == CannotFuse)
                break;
            commitFusion(value, fusionResult);
            
            value = value->child(0);
            inverted = !inverted;
        }

        auto createRelCond = [&] (
            MacroAssembler::RelationalCondition relationalCondition,
            MacroAssembler::DoubleCondition doubleCondition) {
            Arg relCond = Arg::relCond(relationalCondition).inverted(inverted);
            Arg doubleCond = Arg::doubleCond(doubleCondition).inverted(inverted);
            Value* left = value->child(0);
            Value* right = value->child(1);

            if (value->child(0)->type().isInt()) {
                Arg rightImm = imm(right);

                auto tryCompare = [&] (
                    Width width, ArgPromise&& left, ArgPromise&& right) -> Inst {
                    if (Inst result = compare(width, relCond, left, right))
                        return result;
                    if (Inst result = compare(width, relCond.flipped(), right, left))
                        return result;
                    return Inst();
                };

                auto tryCompareLoadImm = [&] (
                    Width width, B3::Opcode loadOpcode, Arg::Signedness signedness) -> Inst {
                    if (rightImm && rightImm.isRepresentableAs(width, signedness)) {
                        if (Inst result = tryCompare(width, loadPromise(left, loadOpcode), rightImm)) {
                            commitInternal(left);
                            return result;
                        }
                    }
                    return Inst();
                };

                Width width = value->child(0)->resultWidth();
                
                if (canCommitInternal) {
                    // First handle compares that involve fewer bits than B3's type system supports.
                    // This is pretty important. For example, we want this to be a single
                    // instruction:
                    //
                    //     @1 = Load8S(...)
                    //     @2 = Const32(...)
                    //     @3 = LessThan(@1, @2)
                    //     Branch(@3)
                
                    if (relCond.isSignedCond()) {
                        if (Inst result = tryCompareLoadImm(Width8, Load8S, Arg::Signed))
                            return result;
                    }
                
                    if (relCond.isUnsignedCond()) {
                        if (Inst result = tryCompareLoadImm(Width8, Load8Z, Arg::Unsigned))
                            return result;
                    }

                    if (relCond.isSignedCond()) {
                        if (Inst result = tryCompareLoadImm(Width16, Load16S, Arg::Signed))
                            return result;
                    }
                
                    if (relCond.isUnsignedCond()) {
                        if (Inst result = tryCompareLoadImm(Width16, Load16Z, Arg::Unsigned))
                            return result;
                    }

                    // Now handle compares that involve a load and an immediate.

                    if (Inst result = tryCompareLoadImm(width, Load, Arg::Signed))
                        return result;

                    // Now handle compares that involve a load. It's not obvious that it's better to
                    // handle this before the immediate cases or not. Probably doesn't matter.

                    if (Inst result = tryCompare(width, loadPromise(left), tmpPromise(right))) {
                        commitInternal(left);
                        return result;
                    }
                
                    if (Inst result = tryCompare(width, tmpPromise(left), loadPromise(right))) {
                        commitInternal(right);
                        return result;
                    }
                }

                // Now handle compares that involve an immediate and a tmp.
                
                if (rightImm && rightImm.isRepresentableAs<int32_t>()) {
                    if (Inst result = tryCompare(width, tmpPromise(left), rightImm))
                        return result;
                }

                // Finally, handle comparison between tmps.
                ArgPromise leftPromise = tmpPromise(left);
                ArgPromise rightPromise = tmpPromise(right);
                return compare(width, relCond, leftPromise, rightPromise);
            }

            // Floating point comparisons can't really do anything smart.
            ArgPromise leftPromise = tmpPromise(left);
            ArgPromise rightPromise = tmpPromise(right);
            if (value->child(0)->type() == Float)
                return compareFloat(doubleCond, leftPromise, rightPromise);
            return compareDouble(doubleCond, leftPromise, rightPromise);
        };

        Width width = value->resultWidth();
        Arg resCond = Arg::resCond(MacroAssembler::NonZero).inverted(inverted);
        
        auto tryTest = [&] (
            Width width, ArgPromise&& left, ArgPromise&& right) -> Inst {
            if (Inst result = test(width, resCond, left, right))
                return result;
            if (Inst result = test(width, resCond, right, left))
                return result;
            return Inst();
        };

        auto attemptFused = [&] () -> Inst {
            switch (value->opcode()) {
            case NotEqual:
                return createRelCond(MacroAssembler::NotEqual, MacroAssembler::DoubleNotEqualOrUnordered);
            case Equal:
                return createRelCond(MacroAssembler::Equal, MacroAssembler::DoubleEqualAndOrdered);
            case LessThan:
                return createRelCond(MacroAssembler::LessThan, MacroAssembler::DoubleLessThanAndOrdered);
            case GreaterThan:
                return createRelCond(MacroAssembler::GreaterThan, MacroAssembler::DoubleGreaterThanAndOrdered);
            case LessEqual:
                return createRelCond(MacroAssembler::LessThanOrEqual, MacroAssembler::DoubleLessThanOrEqualAndOrdered);
            case GreaterEqual:
                return createRelCond(MacroAssembler::GreaterThanOrEqual, MacroAssembler::DoubleGreaterThanOrEqualAndOrdered);
            case EqualOrUnordered:
                // The integer condition is never used in this case.
                return createRelCond(MacroAssembler::Equal, MacroAssembler::DoubleEqualOrUnordered);
            case Above:
                // We use a bogus double condition because these integer comparisons won't got down that
                // path anyway.
                return createRelCond(MacroAssembler::Above, MacroAssembler::DoubleEqualAndOrdered);
            case Below:
                return createRelCond(MacroAssembler::Below, MacroAssembler::DoubleEqualAndOrdered);
            case AboveEqual:
                return createRelCond(MacroAssembler::AboveOrEqual, MacroAssembler::DoubleEqualAndOrdered);
            case BelowEqual:
                return createRelCond(MacroAssembler::BelowOrEqual, MacroAssembler::DoubleEqualAndOrdered);
            case BitAnd: {
                Value* left = value->child(0);
                Value* right = value->child(1);

                bool hasRightConst;
                int64_t rightConst;
                Arg rightImm;
                Arg rightImm64;

                hasRightConst = right->hasInt();
                if (hasRightConst) {
                    rightConst = right->asInt();
                    rightImm = bitImm(right);
                    rightImm64 = bitImm64(right);
                }
                
                auto tryTestLoadImm = [&] (Width width, Arg::Signedness signedness, B3::Opcode loadOpcode) -> Inst {
                    if (!hasRightConst)
                        return Inst();
                    // Signed loads will create high bits, so if the immediate has high bits
                    // then we cannot proceed. Consider BitAnd(Load8S(ptr), 0x101). This cannot
                    // be turned into testb (ptr), $1, since if the high bit within that byte
                    // was set then it would be extended to include 0x100. The handling below
                    // won't anticipate this, so we need to catch it here.
                    if (signedness == Arg::Signed
                        && !Arg::isRepresentableAs(width, Arg::Unsigned, rightConst))
                        return Inst();
                    
                    // FIXME: If this is unsigned then we can chop things off of the immediate.
                    // This might make the immediate more legal. Perhaps that's a job for
                    // strength reduction?
                    // https://bugs.webkit.org/show_bug.cgi?id=169248
                    
                    if (rightImm) {
                        if (Inst result = tryTest(width, loadPromise(left, loadOpcode), rightImm)) {
                            commitInternal(left);
                            return result;
                        }
                    }
                    if (rightImm64) {
                        if (Inst result = tryTest(width, loadPromise(left, loadOpcode), rightImm64)) {
                            commitInternal(left);
                            return result;
                        }
                    }
                    return Inst();
                };

                if (canCommitInternal) {
                    // First handle test's that involve fewer bits than B3's type system supports.

                    if (Inst result = tryTestLoadImm(Width8, Arg::Unsigned, Load8Z))
                        return result;
                    
                    if (Inst result = tryTestLoadImm(Width8, Arg::Signed, Load8S))
                        return result;
                    
                    if (Inst result = tryTestLoadImm(Width16, Arg::Unsigned, Load16Z))
                        return result;
                    
                    if (Inst result = tryTestLoadImm(Width16, Arg::Signed, Load16S))
                        return result;

                    // This allows us to use a 32-bit test for 64-bit BitAnd if the immediate is
                    // representable as an unsigned 32-bit value. The logic involved is the same
                    // as if we were pondering using a 32-bit test for
                    // BitAnd(SExt(Load(ptr)), const), in the sense that in both cases we have
                    // to worry about high bits. So, we use the "Signed" version of this helper.
                    if (Inst result = tryTestLoadImm(Width32, Arg::Signed, Load))
                        return result;
                    
                    // This is needed to handle 32-bit test for arbitrary 32-bit immediates.
                    if (Inst result = tryTestLoadImm(width, Arg::Unsigned, Load))
                        return result;
                    
                    // Now handle test's that involve a load.
                    
                    Width width = value->child(0)->resultWidth();
                    if (Inst result = tryTest(width, loadPromise(left), tmpPromise(right))) {
                        commitInternal(left);
                        return result;
                    }
                    
                    if (Inst result = tryTest(width, tmpPromise(left), loadPromise(right))) {
                        commitInternal(right);
                        return result;
                    }
                }

                // Now handle test's that involve an immediate and a tmp.

                if (hasRightConst) {
                    if ((width == Width32 && rightConst == 0xffffffff)
                        || (width == Width64 && rightConst == -1)) {
                        if (Inst result = tryTest(width, tmpPromise(left), tmpPromise(left)))
                            return result;
                    }
                    if (isRepresentableAs<uint32_t>(rightConst)) {
                        if (Inst result = tryTest(Width32, tmpPromise(left), rightImm))
                            return result;
                        if (Inst result = tryTest(Width32, tmpPromise(left), rightImm64))
                            return result;
                    }
                    if (Inst result = tryTest(width, tmpPromise(left), rightImm))
                        return result;
                    if (Inst result = tryTest(width, tmpPromise(left), rightImm64))
                        return result;
                }

                // Finally, just do tmp's.
                return tryTest(width, tmpPromise(left), tmpPromise(right));
            }
            default:
                return Inst();
            }
        };

        if (FusionResult fusionResult = prepareToFuse(value)) {
            if (Inst result = attemptFused()) {
                commitFusion(value, fusionResult);
                return result;
            }
        }

        if (Arg::isValidBitImmForm(-1)) {
            if (canCommitInternal && value->as<MemoryValue>()) {
                // Handle things like Branch(Load8Z(value))

                if (Inst result = tryTest(Width8, loadPromise(value, Load8Z), Arg::bitImm(-1))) {
                    commitInternal(value);
                    return result;
                }

                if (Inst result = tryTest(Width8, loadPromise(value, Load8S), Arg::bitImm(-1))) {
                    commitInternal(value);
                    return result;
                }

                if (Inst result = tryTest(Width16, loadPromise(value, Load16Z), Arg::bitImm(-1))) {
                    commitInternal(value);
                    return result;
                }

                if (Inst result = tryTest(Width16, loadPromise(value, Load16S), Arg::bitImm(-1))) {
                    commitInternal(value);
                    return result;
                }

                if (Inst result = tryTest(width, loadPromise(value), Arg::bitImm(-1))) {
                    commitInternal(value);
                    return result;
                }
            }

            ArgPromise leftPromise = tmpPromise(value);
            ArgPromise rightPromise = Arg::bitImm(-1);
            if (Inst result = test(width, resCond, leftPromise, rightPromise))
                return result;
        }
        
        // Sometimes this is the only form of test available. We prefer not to use this because
        // it's less canonical.
        ArgPromise leftPromise = tmpPromise(value);
        ArgPromise rightPromise = tmpPromise(value);
        return test(width, resCond, leftPromise, rightPromise);
    }

    Inst createBranch(Value* value, bool inverted = false)
    {
        using namespace Air;
        return createGenericCompare(
            value,
            [this] (
                Width width, const Arg& relCond,
                ArgPromise& left, ArgPromise& right) -> Inst {
                switch (width) {
                case Width8:
                    if (isValidForm(Branch8, Arg::RelCond, left.kind(), right.kind())) {
                        return left.inst(right.inst(
                            Branch8, m_value, relCond,
                            left.consume(*this), right.consume(*this)));
                    }
                    return Inst();
                case Width16:
                    return Inst();
                case Width32:
                    if (isValidForm(Branch32, Arg::RelCond, left.kind(), right.kind())) {
                        return left.inst(right.inst(
                            Branch32, m_value, relCond,
                            left.consume(*this), right.consume(*this)));
                    }
                    return Inst();
                case Width64:
                    if (isValidForm(Branch64, Arg::RelCond, left.kind(), right.kind())) {
                        return left.inst(right.inst(
                            Branch64, m_value, relCond,
                            left.consume(*this), right.consume(*this)));
                    }
                    return Inst();
                case Width128:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
                ASSERT_NOT_REACHED();
            },
            [this] (
                Width width, const Arg& resCond,
                ArgPromise& left, ArgPromise& right) -> Inst {
                switch (width) {
                case Width8:
                    if (isValidForm(BranchTest8, Arg::ResCond, left.kind(), right.kind())) {
                        return left.inst(right.inst(
                            BranchTest8, m_value, resCond,
                            left.consume(*this), right.consume(*this)));
                    }
                    return Inst();
                case Width16:
                    return Inst();
                case Width32:
                    if (isValidForm(BranchTest32, Arg::ResCond, left.kind(), right.kind())) {
                        return left.inst(right.inst(
                            BranchTest32, m_value, resCond,
                            left.consume(*this), right.consume(*this)));
                    }
                    return Inst();
                case Width64:
                    if (isValidForm(BranchTest64, Arg::ResCond, left.kind(), right.kind())) {
                        return left.inst(right.inst(
                            BranchTest64, m_value, resCond,
                            left.consume(*this), right.consume(*this)));
                    }
                    return Inst();
                case Width128:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
                ASSERT_NOT_REACHED();
            },
            [this] (Arg doubleCond, ArgPromise& left, ArgPromise& right) -> Inst {
                if (isValidForm(BranchDouble, Arg::DoubleCond, left.kind(), right.kind())) {
                    return left.inst(right.inst(
                        BranchDouble, m_value, doubleCond,
                        left.consume(*this), right.consume(*this)));
                }
                return Inst();
            },
            [this] (Arg doubleCond, ArgPromise& left, ArgPromise& right) -> Inst {
                if (isValidForm(BranchFloat, Arg::DoubleCond, left.kind(), right.kind())) {
                    return left.inst(right.inst(
                        BranchFloat, m_value, doubleCond,
                        left.consume(*this), right.consume(*this)));
                }
                return Inst();
            },
            inverted);
    }

    Inst createCompare(Value* value, bool inverted = false)
    {
        using namespace Air;
        return createGenericCompare(
            value,
            [this] (
                Width width, const Arg& relCond,
                ArgPromise& left, ArgPromise& right) -> Inst {
                switch (width) {
                case Width8:
                case Width16:
                    return Inst();
                case Width32:
                    if (isValidForm(Compare32, Arg::RelCond, left.kind(), right.kind(), Arg::Tmp)) {
                        return left.inst(right.inst(
                            Compare32, m_value, relCond,
                            left.consume(*this), right.consume(*this), tmp(m_value)));
                    }
                    return Inst();
                case Width64:
                    if (isValidForm(Compare64, Arg::RelCond, left.kind(), right.kind(), Arg::Tmp)) {
                        return left.inst(right.inst(
                            Compare64, m_value, relCond,
                            left.consume(*this), right.consume(*this), tmp(m_value)));
                    }
                    return Inst();
                case Width128:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
                ASSERT_NOT_REACHED();
                return Inst();
            },
            [this] (
                Width width, const Arg& resCond,
                ArgPromise& left, ArgPromise& right) -> Inst {
                switch (width) {
                case Width8:
                case Width16:
                    return Inst();
                case Width32:
                    if (isValidForm(Test32, Arg::ResCond, left.kind(), right.kind(), Arg::Tmp)) {
                        return left.inst(right.inst(
                            Test32, m_value, resCond,
                            left.consume(*this), right.consume(*this), tmp(m_value)));
                    }
                    return Inst();
                case Width64:
                    if (isValidForm(Test64, Arg::ResCond, left.kind(), right.kind(), Arg::Tmp)) {
                        return left.inst(right.inst(
                            Test64, m_value, resCond,
                            left.consume(*this), right.consume(*this), tmp(m_value)));
                    }
                    return Inst();
                case Width128:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
                ASSERT_NOT_REACHED();
                return Inst();
            },
            [this] (const Arg& doubleCond, ArgPromise& left, ArgPromise& right) -> Inst {
                if (isValidForm(CompareDouble, Arg::DoubleCond, left.kind(), right.kind(), Arg::Tmp)) {
                    return left.inst(right.inst(
                        CompareDouble, m_value, doubleCond,
                        left.consume(*this), right.consume(*this), tmp(m_value)));
                }
                return Inst();
            },
            [this] (const Arg& doubleCond, ArgPromise& left, ArgPromise& right) -> Inst {
                if (isValidForm(CompareFloat, Arg::DoubleCond, left.kind(), right.kind(), Arg::Tmp)) {
                    return left.inst(right.inst(
                        CompareFloat, m_value, doubleCond,
                        left.consume(*this), right.consume(*this), tmp(m_value)));
                }
                return Inst();
            },
            inverted);
    }

    struct MoveConditionallyConfig {
        Air::Opcode moveConditionally32;
        Air::Opcode moveConditionally64;
        Air::Opcode moveConditionallyTest32;
        Air::Opcode moveConditionallyTest64;
        Air::Opcode moveConditionallyDouble;
        Air::Opcode moveConditionallyFloat;
    };
    Inst createSelect(const MoveConditionallyConfig& config)
    {
        using namespace Air;
        auto createSelectInstruction = [&] (Air::Opcode opcode, const Arg& condition, ArgPromise& left, ArgPromise& right) -> Inst {
            if (isValidForm(opcode, condition.kind(), left.kind(), right.kind(), Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
                Tmp result = tmp(m_value);
                Tmp thenCase = tmp(m_value->child(1));
                Tmp elseCase = tmp(m_value->child(2));
                return left.inst(right.inst(
                    opcode, m_value, condition,
                    left.consume(*this), right.consume(*this), thenCase, elseCase, result));
            }
            if (isValidForm(opcode, condition.kind(), left.kind(), right.kind(), Arg::Tmp, Arg::Tmp)) {
                Tmp result = tmp(m_value);
                Tmp source = tmp(m_value->child(1));
                append(relaxedMoveForType(m_value->type()), tmp(m_value->child(2)), result);
                return left.inst(right.inst(
                    opcode, m_value, condition,
                    left.consume(*this), right.consume(*this), source, result));
            }
            return Inst();
        };

        return createGenericCompare(
            m_value->child(0),
            [&] (Width width, const Arg& relCond, ArgPromise& left, ArgPromise& right) -> Inst {
                switch (width) {
                case Width8:
                    // FIXME: Support these things.
                    // https://bugs.webkit.org/show_bug.cgi?id=151504
                    return Inst();
                case Width16:
                    return Inst();
                case Width32:
                    return createSelectInstruction(config.moveConditionally32, relCond, left, right);
                case Width64:
                    return createSelectInstruction(config.moveConditionally64, relCond, left, right);
                case Width128:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
                ASSERT_NOT_REACHED();
            },
            [&] (Width width, const Arg& resCond, ArgPromise& left, ArgPromise& right) -> Inst {
                switch (width) {
                case Width8:
                    // FIXME: Support more things.
                    // https://bugs.webkit.org/show_bug.cgi?id=151504
                    return Inst();
                case Width16:
                    return Inst();
                case Width32:
                    return createSelectInstruction(config.moveConditionallyTest32, resCond, left, right);
                case Width64:
                    return createSelectInstruction(config.moveConditionallyTest64, resCond, left, right);
                case Width128:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
                ASSERT_NOT_REACHED();
            },
            [&] (Arg doubleCond, ArgPromise& left, ArgPromise& right) -> Inst {
                return createSelectInstruction(config.moveConditionallyDouble, doubleCond, left, right);
            },
            [&] (Arg doubleCond, ArgPromise& left, ArgPromise& right) -> Inst {
                return createSelectInstruction(config.moveConditionallyFloat, doubleCond, left, right);
            },
            false);
    }
    
    bool tryAppendLea()
    {
        using namespace Air;
        Air::Opcode leaOpcode = tryOpcodeForType(Lea32, Lea64, m_value->type());
        if (!isValidForm(leaOpcode, Arg::Index, Arg::Tmp))
            return false;
        
        // This lets us turn things like this:
        //
        //     Add(Add(@x, Shl(@y, $2)), $100)
        //
        // Into this:
        //
        //     lea 100(%rdi,%rsi,4), %rax
        //
        // We have a choice here between committing the internal bits of an index or sharing
        // them. There are solid arguments for both.
        //
        // Sharing: The word on the street is that the cost of a lea is one cycle no matter
        // what it does. Every experiment I've ever seen seems to confirm this. So, sharing
        // helps us in situations where Wasm input did this:
        //
        //     x = a[i].x;
        //     y = a[i].y;
        //
        // With sharing we would do:
        //
        //     leal (%a,%i,4), %tmp
        //     cmp (%size, %tmp)
        //     ja _fail
        //     movl (%base, %tmp), %x
        //     leal 4(%a,%i,4), %tmp
        //     cmp (%size, %tmp)
        //     ja _fail
        //     movl (%base, %tmp), %y
        //
        // In the absence of sharing, we may find ourselves needing separate registers for
        // the innards of the index. That's relatively unlikely to be a thing due to other
        // optimizations that we already have, but it could happen
        //
        // Committing: The worst case is that there is a complicated graph of additions and
        // shifts, where each value has multiple uses. In that case, it's better to compute
        // each one separately from the others since that way, each calculation will use a
        // relatively nearby tmp as its input. That seems uncommon, but in those cases,
        // committing is a clear winner: it would result in a simple interference graph
        // while sharing would result in a complex one. Interference sucks because it means
        // more time in IRC and it means worse code.
        //
        // It's not super clear if any of these corner cases would ever arise. Committing
        // has the benefit that it's easier to reason about, and protects a much darker
        // corner case (more interference).
                
        // Here are the things we want to match:
        // Add(Add(@x, @y), $c)
        // Add(Shl(@x, $c), @y)
        // Add(@x, Shl(@y, $c))
        // Add(Add(@x, Shl(@y, $c)), $d)
        // Add(Add(Shl(@x, $c), @y), $d)
        //
        // Note that if you do Add(Shl(@x, $c), $d) then we will treat $d as a non-constant and
        // force it to materialize. You'll get something like this:
        //
        // movl $d, %tmp
        // leal (%tmp,%x,1<<c), %result
        //
        // Which is pretty close to optimal and has the nice effect of being able to handle large
        // constants gracefully.
        
        Value* innerAdd = nullptr;
        
        Value* value = m_value;
        
        // We're going to consume Add(Add(_), $c). If we succeed at consuming it then we have these
        // patterns left (i.e. in the Add(_)):
        //
        // Add(Add(@x, @y), $c)
        // Add(Add(@x, Shl(@y, $c)), $d)
        // Add(Add(Shl(@x, $c), @y), $d)
        //
        // Otherwise we are looking at these patterns:
        //
        // Add(Shl(@x, $c), @y)
        // Add(@x, Shl(@y, $c))
        //
        // This means that the subsequent code only has to worry about three patterns:
        //
        // Add(Shl(@x, $c), @y)
        // Add(@x, Shl(@y, $c))
        // Add(@x, @y) (only if offset != 0)
        Value::OffsetType offset = 0;
        if (value->child(1)->isRepresentableAs<Value::OffsetType>()
            && canBeInternal(value->child(0))
            && value->child(0)->opcode() == Add) {
            innerAdd = value->child(0);
            offset = static_cast<Value::OffsetType>(value->child(1)->asInt());
            value = value->child(0);
        }
        
        auto tryShl = [&] (Value* shl, Value* other) -> bool {
            std::optional<unsigned> scale = scaleForShl(shl, offset);
            if (!scale)
                return false;
            if (!canBeInternal(shl))
                return false;
            
            ASSERT(!m_locked.contains(shl->child(0)));
            ASSERT(!m_locked.contains(other));
            
            append(leaOpcode, indexArg(tmp(other), shl->child(0), *scale, offset), tmp(m_value));
            commitInternal(innerAdd);
            commitInternal(shl);
            return true;
        };
        
        if (tryShl(value->child(0), value->child(1)))
            return true;
        if (tryShl(value->child(1), value->child(0)))
            return true;
        
        // The remaining pattern is just:
        // Add(@x, @y) (only if offset != 0)
        if (!offset)
            return false;
        ASSERT(!m_locked.contains(value->child(0)));
        ASSERT(!m_locked.contains(value->child(1)));
        append(leaOpcode, indexArg(tmp(value->child(0)), value->child(1), 1, offset), tmp(m_value));
        commitInternal(innerAdd);
        return true;
    }

    void appendX86Div(B3::Opcode op)
    {
        using namespace Air;
        Air::Opcode convertToDoubleWord;
        Air::Opcode div;
        switch (m_value->type().kind()) {
        case Int32:
            convertToDoubleWord = X86ConvertToDoubleWord32;
            div = X86Div32;
            break;
        case Int64:
            convertToDoubleWord = X86ConvertToQuadWord64;
            div = X86Div64;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }

        ASSERT(op == Div || op == Mod);
        Tmp result = op == Div ? m_eax : m_edx;

        append(Move, tmp(m_value->child(0)), m_eax);
        append(convertToDoubleWord, m_eax, m_edx);
        append(div, m_eax, m_edx, tmp(m_value->child(1)));
        append(Move, result, tmp(m_value));
    }

    void appendX86UDiv(B3::Opcode op)
    {
        using namespace Air;
        Air::Opcode div = m_value->type() == Int32 ? X86UDiv32 : X86UDiv64;

        ASSERT(op == UDiv || op == UMod);
        Tmp result = op == UDiv ? m_eax : m_edx;

        append(Move, tmp(m_value->child(0)), m_eax);
        append(Move, Arg::imm(0), m_edx);
        append(div, m_eax, m_edx, tmp(m_value->child(1)));
        append(Move, result, tmp(m_value));
    }
    
    Air::Opcode loadLinkOpcode(Width width, bool fence)
    {
        return fence ? OPCODE_FOR_WIDTH(LoadLinkAcq, width) : OPCODE_FOR_WIDTH(LoadLink, width);
    }
    
    Air::Opcode storeCondOpcode(Width width, bool fence)
    {
        return fence ? OPCODE_FOR_WIDTH(StoreCondRel, width) : OPCODE_FOR_WIDTH(StoreCond, width);
    }
    
    // This can emit code for the following patterns:
    // AtomicWeakCAS
    // BitXor(AtomicWeakCAS, 1)
    // AtomicStrongCAS
    // Equal(AtomicStrongCAS, expected)
    // NotEqual(AtomicStrongCAS, expected)
    // Branch(AtomicWeakCAS)
    // Branch(Equal(AtomicStrongCAS, expected))
    // Branch(NotEqual(AtomicStrongCAS, expected))
    //
    // It assumes that atomicValue points to the CAS, and m_value points to the instruction being
    // generated. It assumes that you've consumed everything that needs to be consumed.
    void appendCAS(Value* atomicValue, bool invert)
    {
        using namespace Air;
        AtomicValue* atomic = atomicValue->as<AtomicValue>();
        RELEASE_ASSERT(atomic);
        
        bool isBranch = m_value->opcode() == Branch;
        bool isStrong = atomic->opcode() == AtomicStrongCAS;
        bool returnsOldValue = m_value->opcode() == AtomicStrongCAS;
        bool hasFence = atomic->hasFence();
        
        Width width = atomic->accessWidth();
        Arg address = addr(atomic);

        Tmp valueResultTmp;
        Tmp boolResultTmp;
        if (returnsOldValue) {
            RELEASE_ASSERT(!invert);
            valueResultTmp = tmp(m_value);
            boolResultTmp = m_code.newTmp(GP);
        } else if (isBranch) {
            valueResultTmp = m_code.newTmp(GP);
            boolResultTmp = m_code.newTmp(GP);
        } else {
            valueResultTmp = m_code.newTmp(GP);
            boolResultTmp = tmp(m_value);
        }
        
        Tmp successBoolResultTmp;
        if (isStrong && !isBranch)
            successBoolResultTmp = m_code.newTmp(GP);
        else
            successBoolResultTmp = boolResultTmp;

        Tmp expectedValueTmp = tmp(atomic->child(0));
        Tmp newValueTmp = tmp(atomic->child(1));
        
        Air::FrequentedBlock success;
        Air::FrequentedBlock failure;
        if (isBranch) {
            success = m_blockToBlock[m_block]->successor(invert);
            failure = m_blockToBlock[m_block]->successor(!invert);
        }
        
        if (isX86()) {
            append(relaxedMoveForType(atomic->accessType()), immOrTmp(atomic->child(0)), m_eax);
            if (returnsOldValue) {
                appendTrapping(OPCODE_FOR_WIDTH(AtomicStrongCAS, width), m_eax, newValueTmp, address);
                append(relaxedMoveForType(atomic->accessType()), m_eax, valueResultTmp);
            } else if (isBranch) {
                appendTrapping(OPCODE_FOR_WIDTH(BranchAtomicStrongCAS, width), Arg::statusCond(MacroAssembler::Success), m_eax, newValueTmp, address);
                m_blockToBlock[m_block]->setSuccessors(success, failure);
            } else
                appendTrapping(OPCODE_FOR_WIDTH(AtomicStrongCAS, width), Arg::statusCond(invert ? MacroAssembler::Failure : MacroAssembler::Success), m_eax, tmp(atomic->child(1)), address, boolResultTmp);
            return;
        }

        if (isARM64_LSE()) {
            if (isBranch) {
                switch (width) {
                case Width8:
                    append(Air::ZeroExtend8To32, expectedValueTmp, expectedValueTmp);
                    break;
                case Width16:
                    append(Air::ZeroExtend16To32, expectedValueTmp, expectedValueTmp);
                    break;
                case Width32:
                case Width64:
                    break;
                case Width128:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
            }
            append(relaxedMoveForType(atomic->accessType()), expectedValueTmp, valueResultTmp);
            appendTrapping(OPCODE_FOR_WIDTH(AtomicStrongCAS, width), valueResultTmp, newValueTmp, address);
            if (returnsOldValue)
                return;
            if (isBranch) {
                switch (width) {
                case Width8:
                case Width16:
                case Width32:
                    appendTrapping(Air::Branch32, Arg::relCond(MacroAssembler::Equal), valueResultTmp, expectedValueTmp);
                    break;
                case Width64:
                    appendTrapping(Air::Branch64, Arg::relCond(MacroAssembler::Equal), valueResultTmp, expectedValueTmp);
                    break;
                case Width128:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
                m_blockToBlock[m_block]->setSuccessors(success, failure);
                return;
            }
            append(OPCODE_FOR_CANONICAL_WIDTH(Compare, width), Arg::relCond(invert ? MacroAssembler::NotEqual : MacroAssembler::Equal), valueResultTmp, expectedValueTmp, boolResultTmp);
            return;
        }
        
        RELEASE_ASSERT(isARM64());
        // We wish to emit:
        //
        // Block #reloop:
        //     LoadLink
        //     Branch NotEqual
        //   Successors: Then:#fail, Else: #store
        // Block #store:
        //     StoreCond
        //     Xor $1, %result    <--- only if !invert
        //     Jump
        //   Successors: #done
        // Block #fail:
        //     Move $invert, %result
        //     Jump
        //   Successors: #done
        // Block #done:
        
        Air::BasicBlock* reloopBlock = newBlock();
        Air::BasicBlock* storeBlock = newBlock();
        Air::BasicBlock* successBlock = nullptr;
        if (!isBranch && isStrong)
            successBlock = newBlock();
        Air::BasicBlock* failBlock = nullptr;
        if (!isBranch) {
            failBlock = newBlock();
            failure = failBlock;
        }
        Air::BasicBlock* strongFailBlock = nullptr;
        if (isStrong && hasFence)
            strongFailBlock = newBlock();
        Air::FrequentedBlock comparisonFail = failure;
        Air::FrequentedBlock weakFail;
        if (isStrong) {
            if (hasFence)
                comparisonFail = strongFailBlock;
            weakFail = reloopBlock;
        } else 
            weakFail = failure;
        Air::BasicBlock* beginBlock;
        Air::BasicBlock* doneBlock;
        splitBlock(beginBlock, doneBlock);
        
        append(Air::Jump);
        beginBlock->setSuccessors(reloopBlock);
        
        reloopBlock->append(trappingInst(m_value, loadLinkOpcode(width, atomic->hasFence()), m_value, address, valueResultTmp));
        reloopBlock->append(OPCODE_FOR_CANONICAL_WIDTH(Branch, width), m_value, Arg::relCond(MacroAssembler::NotEqual), valueResultTmp, expectedValueTmp);
        reloopBlock->setSuccessors(comparisonFail, storeBlock);
        
        storeBlock->append(trappingInst(m_value, storeCondOpcode(width, atomic->hasFence()), m_value, newValueTmp, address, successBoolResultTmp));
        if (isBranch) {
            storeBlock->append(BranchTest32, m_value, Arg::resCond(MacroAssembler::Zero), boolResultTmp, boolResultTmp);
            storeBlock->setSuccessors(success, weakFail);
            doneBlock->successors().clear();
            RELEASE_ASSERT(!doneBlock->size());
            doneBlock->append(Air::Oops, m_value);
        } else {
            if (isStrong) {
                storeBlock->append(BranchTest32, m_value, Arg::resCond(MacroAssembler::Zero), successBoolResultTmp, successBoolResultTmp);
                storeBlock->setSuccessors(successBlock, reloopBlock);
                
                successBlock->append(Move, m_value, Arg::imm(!invert), boolResultTmp);
                successBlock->append(Air::Jump, m_value);
                successBlock->setSuccessors(doneBlock);
            } else {
                if (!invert)
                    storeBlock->append(Xor32, m_value, Arg::bitImm(1), boolResultTmp, boolResultTmp);
                
                storeBlock->append(Air::Jump, m_value);
                storeBlock->setSuccessors(doneBlock);
            }
            
            failBlock->append(Move, m_value, Arg::imm(invert), boolResultTmp);
            failBlock->append(Air::Jump, m_value);
            failBlock->setSuccessors(doneBlock);
        }
        
        if (isStrong && hasFence) {
            Tmp tmp = m_code.newTmp(GP);
            strongFailBlock->append(trappingInst(m_value, storeCondOpcode(width, atomic->hasFence()), m_value, valueResultTmp, address, tmp));
            strongFailBlock->append(BranchTest32, m_value, Arg::resCond(MacroAssembler::Zero), tmp, tmp);
            strongFailBlock->setSuccessors(failure, reloopBlock);
        }
    }
    
    bool appendVoidAtomic(Air::Opcode atomicOpcode)
    {
        if (m_useCounts.numUses(m_value))
            return false;
        
        Arg address = addr(m_value);
        
        if (isValidForm(atomicOpcode, Arg::Imm, address.kind()) && imm(m_value->child(0))) {
            appendTrapping(atomicOpcode, imm(m_value->child(0)), address);
            return true;
        }
        
        if (isValidForm(atomicOpcode, Arg::Tmp, address.kind())) {
            appendTrapping(atomicOpcode, tmp(m_value->child(0)), address);
            return true;
        }
        
        return false;
    }
    
    void appendGeneralAtomic(Air::Opcode opcode, Commutativity commutativity = NotCommutative)
    {
        using namespace Air;
        AtomicValue* atomic = m_value->as<AtomicValue>();
        
        Arg address = addr(m_value);
        Tmp oldValue = m_code.newTmp(GP);
        Tmp newValue = opcode == Air::Nop ? tmp(atomic->child(0)) : m_code.newTmp(GP);
        
        // We need a CAS loop or a LL/SC loop. Using prepare/attempt jargon, we want:
        //
        // Block #reloop:
        //     Prepare
        //     opcode
        //     Attempt
        //   Successors: Then:#done, Else:#reloop
        // Block #done:
        //     Move oldValue, result
        
        append(relaxedMoveForType(atomic->type()), oldValue, tmp(atomic));
        
        Air::BasicBlock* reloopBlock = newBlock();
        Air::BasicBlock* beginBlock;
        Air::BasicBlock* doneBlock;
        splitBlock(beginBlock, doneBlock);
        
        append(Air::Jump);
        beginBlock->setSuccessors(reloopBlock);
        
        Air::Opcode prepareOpcode;
        if (isX86()) {
            switch (atomic->accessWidth()) {
            case Width8:
                prepareOpcode = Load8SignedExtendTo32;
                break;
            case Width16:
                prepareOpcode = Load16SignedExtendTo32;
                break;
            case Width32:
                prepareOpcode = Move32;
                break;
            case Width64:
                prepareOpcode = Move;
                break;
            case Width128:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        } else {
            RELEASE_ASSERT(isARM64());
            prepareOpcode = loadLinkOpcode(atomic->accessWidth(), atomic->hasFence());
        }
        reloopBlock->append(trappingInst(m_value, prepareOpcode, m_value, address, oldValue));
        
        if (opcode != Air::Nop) {
            // FIXME: If we ever have to write this again, we need to find a way to share the code with
            // appendBinOp.
            // https://bugs.webkit.org/show_bug.cgi?id=169249
            if (commutativity == Commutative && imm(atomic->child(0)) && isValidForm(opcode, Arg::Imm, Arg::Tmp, Arg::Tmp))
                reloopBlock->append(opcode, m_value, imm(atomic->child(0)), oldValue, newValue);
            else if (imm(atomic->child(0)) && isValidForm(opcode, Arg::Tmp, Arg::Imm, Arg::Tmp))
                reloopBlock->append(opcode, m_value, oldValue, imm(atomic->child(0)), newValue);
            else if (commutativity == Commutative && bitImm(atomic->child(0)) && isValidForm(opcode, Arg::BitImm, Arg::Tmp, Arg::Tmp))
                reloopBlock->append(opcode, m_value, bitImm(atomic->child(0)), oldValue, newValue);
            else if (isValidForm(opcode, Arg::Tmp, Arg::Tmp, Arg::Tmp))
                reloopBlock->append(opcode, m_value, oldValue, tmp(atomic->child(0)), newValue);
            else {
                reloopBlock->append(relaxedMoveForType(atomic->type()), m_value, oldValue, newValue);
                if (imm(atomic->child(0)) && isValidForm(opcode, Arg::Imm, Arg::Tmp))
                    reloopBlock->append(opcode, m_value, imm(atomic->child(0)), newValue);
                else
                    reloopBlock->append(opcode, m_value, tmp(atomic->child(0)), newValue);
            }
        }

        if (isX86()) {
            Air::Opcode casOpcode = OPCODE_FOR_WIDTH(BranchAtomicStrongCAS, atomic->accessWidth());
            reloopBlock->append(relaxedMoveForType(atomic->type()), m_value, oldValue, m_eax);
            reloopBlock->append(trappingInst(m_value, casOpcode, m_value, Arg::statusCond(MacroAssembler::Success), m_eax, newValue, address));
        } else {
            RELEASE_ASSERT(isARM64());
            Tmp boolResult = m_code.newTmp(GP);
            reloopBlock->append(trappingInst(m_value, storeCondOpcode(atomic->accessWidth(), atomic->hasFence()), m_value, newValue, address, boolResult));
            reloopBlock->append(BranchTest32, m_value, Arg::resCond(MacroAssembler::Zero), boolResult, boolResult);
        }
        reloopBlock->setSuccessors(doneBlock, reloopBlock);
    }

    bool tryAppendBitOpWithShift(Value* left, Value* right, Air::Opcode opcode)
    {
        if (!isValidForm(opcode, Arg::Tmp, Arg::Tmp, Arg::Imm, Arg::Tmp))
            return false;
        if (!canBeInternal(right) || !imm(right->child(1)) || right->child(1)->asInt() < 0)
            return false;

        uint64_t amount = right->child(1)->asInt();
        uint64_t datasize = m_value->type() == Int32 ? 32 : 64;
        if (amount >= datasize)
            return false;

        append(opcode, tmp(left), tmp(right->child(0)), imm(right->child(1)), tmp(m_value));
        commitInternal(right);
        return true;
    }

    void lower()
    {
        using namespace Air;
        switch (m_value->opcode()) {
        case B3::Nop: {
            // Yes, we will totally see Nop's because some phases will replaceWithNop() instead of
            // properly removing things.
            return;
        }
            
        case Load: {
            MemoryValue* memory = m_value->as<MemoryValue>();
            Air::Kind kind = moveForType(memory->type());
            if (memory->hasFence()) {
                if (isX86())
                    kind.effects = true;
                else {
                    switch (memory->type().kind()) {
                    case Int32:
                        kind = LoadAcq32;
                        break;
                    case Int64:
                        kind = LoadAcq64;
                        break;
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                        break;
                    }
                }
            }

            // Pre-Index Canonical Form:
            //     address = Add(base, offset)    --->   Move %base %address
            //     memory = Load(base, offset)           MoveWithIncrement (%address, prefix(offset)) %memory
            // Post-Index Canonical Form:
            //     address = Add(base, offset)    --->   Move %base %address
            //     memory = Load(base, 0)                MoveWithIncrement (%address, postfix(offset)) %memory
            auto tryAppendIncrementAddress = [&] () -> bool {
                Air::Opcode opcode = tryOpcodeForType(MoveWithIncrement32, MoveWithIncrement64, memory->type());
                if (!isValidForm(opcode, Arg::PreIndex, Arg::Tmp) || !m_index)
                    return false;
                Value* address = m_block->at(m_index - 1);
                if (address->opcode() != Add || address->type() != Int64)
                    return false;

                Value* base1 = address->child(0);
                Value* base2 = memory->lastChild();
                if (base1 != base2 || !address->child(1)->hasIntPtr())
                    return false;
                intptr_t offset = address->child(1)->asIntPtr();
                Value::OffsetType smallOffset = static_cast<Value::OffsetType>(offset);
                if (smallOffset != offset || !Arg::isValidIncrementIndexForm(smallOffset))
                    return false;
                if (m_locked.contains(address) || m_locked.contains(base1))
                    return false;

                Arg incrementArg = Arg();
                if (memory->offset()) {
                    if (smallOffset == memory->offset())
                        incrementArg = Arg::preIndex(tmp(address), smallOffset);
                } else
                    incrementArg = Arg::postIndex(tmp(address), smallOffset);

                if (incrementArg) {
                    append(relaxedMoveForType(address->type()), tmp(address->child(0)), tmp(address));
                    append(opcode, incrementArg, tmp(memory));
                    m_locked.add(address);
                    return true;
                }
                return false;
            };

            if (tryAppendIncrementAddress())
                return;

            append(trappingInst(m_value, kind, m_value, addr(m_value), tmp(m_value)));
            return;
        }
            
        case Load8S: {
            Air::Kind kind = Load8SignedExtendTo32;
            if (m_value->as<MemoryValue>()->hasFence()) {
                if (isX86())
                    kind.effects = true;
                else
                    kind = LoadAcq8SignedExtendTo32;
            }
            append(trappingInst(m_value, kind, m_value, addr(m_value), tmp(m_value)));
            return;
        }

        case Load8Z: {
            Air::Kind kind = Load8;
            if (m_value->as<MemoryValue>()->hasFence()) {
                if (isX86())
                    kind.effects = true;
                else
                    kind = LoadAcq8;
            }
            append(trappingInst(m_value, kind, m_value, addr(m_value), tmp(m_value)));
            return;
        }

        case Load16S: {
            Air::Kind kind = Load16SignedExtendTo32;
            if (m_value->as<MemoryValue>()->hasFence()) {
                if (isX86())
                    kind.effects = true;
                else
                    kind = LoadAcq16SignedExtendTo32;
            }
            append(trappingInst(m_value, kind, m_value, addr(m_value), tmp(m_value)));
            return;
        }

        case Load16Z: {
            Air::Kind kind = Load16;
            if (m_value->as<MemoryValue>()->hasFence()) {
                if (isX86())
                    kind.effects = true;
                else
                    kind = LoadAcq16;
            }
            append(trappingInst(m_value, kind, m_value, addr(m_value), tmp(m_value)));
            return;
        }

        case Add: {
            if (tryAppendLea())
                return;
            Value* left = m_value->child(0);
            Value* right = m_value->child(1);

            auto tryMultiplyAdd = [&] () -> bool {
                if (imm(right) && !m_valueToTmp[right])
                    return false;

                // MADD: d = n * m + a
                auto tryAppendMultiplyAdd = [&] (Value* left, Value* right) -> bool {
                    if (left->opcode() != Mul || !canBeInternal(left) || m_locked.contains(right))
                        return false;
                    Value* multiplyLeft = left->child(0);
                    Value* multiplyRight = left->child(1);
                    Air::Opcode airOpcode = tryOpcodeForType(MultiplyAdd32, MultiplyAdd64, m_value->type());
                    auto tryNewAirOpcode = [&] () -> Air::Opcode {
                        if (airOpcode != MultiplyAdd64)
                            return Air::Oops;
                        // SMADDL: d = SExt32(n) * SExt32(m) + a
                        if (isMergeableValue(multiplyLeft, SExt32) && isMergeableValue(multiplyRight, SExt32))
                            return MultiplyAddSignExtend32;
                        // UMADDL: d = ZExt32(n) * ZExt32(m) + a
                        if (isMergeableValue(multiplyLeft, ZExt32) && isMergeableValue(multiplyRight, ZExt32))
                            return MultiplyAddZeroExtend32;
                        return Air::Oops;
                    };

                    Air::Opcode newAirOpcode = tryNewAirOpcode();
                    if (isValidForm(newAirOpcode, Arg::Tmp, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
                        append(newAirOpcode, tmp(multiplyLeft->child(0)), tmp(multiplyRight->child(0)), tmp(right), tmp(m_value));
                        commitInternal(left);
                        return true;
                    }

                    if (!isValidForm(airOpcode, Arg::Tmp, Arg::Tmp, Arg::Tmp, Arg::Tmp))
                        return false;
                    if (m_locked.contains(multiplyLeft) || m_locked.contains(multiplyRight))
                        return false;
                    append(airOpcode, tmp(multiplyLeft), tmp(multiplyRight), tmp(right), tmp(m_value));
                    commitInternal(left);
                    return true;
                };

                return tryAppendMultiplyAdd(left, right) || tryAppendMultiplyAdd(right, left);
            };

            if (tryMultiplyAdd())
                return;

            // add-with-shift Pattern: left + (right ShiftType amount)
            auto tryAppendAddWithShift = [&] (Value* left, Value* right) -> bool {
                Air::Opcode opcode = opcodeBasedOnShiftKind(right->opcode(),
                    AddLeftShift32, AddLeftShift64,
                    AddRightShift32, AddRightShift64,
                    AddUnsignedRightShift32, AddUnsignedRightShift64);
                return tryAppendBitOpWithShift(left, right, opcode);
            };

            if (tryAppendAddWithShift(left, right) || tryAppendAddWithShift(right, left))
                return;

            appendBinOp<Add32, Add64, AddDouble, AddFloat, Commutative>(left, right);
            return;
        }

        case Sub: {
            Value* left = m_value->child(0);
            Value* right = m_value->child(1);

            auto tryAppendMultiplySub = [&] () -> bool {
                if (imm(right) && !m_valueToTmp[right])
                    return false;

                // MSUB: d = a - n * m
                if (m_locked.contains(left) || right->opcode() != Mul || !canBeInternal(right))
                    return false;
                Value* multiplyLeft = right->child(0);
                Value* multiplyRight = right->child(1);
                Air::Opcode airOpcode = tryOpcodeForType(MultiplySub32, MultiplySub64, m_value->type());
                auto tryNewAirOpcode = [&] () -> Air::Opcode {
                    if (airOpcode != MultiplySub64)
                        return Air::Oops;
                    // SMSUBL: d = a - SExt32(n) * SExt32(m)
                    if (isMergeableValue(multiplyLeft, SExt32) && isMergeableValue(multiplyRight, SExt32))
                        return MultiplySubSignExtend32;
                    // UMSUBL: d = a - ZExt32(n) * ZExt32(m)
                    if (isMergeableValue(multiplyLeft, ZExt32) && isMergeableValue(multiplyRight, ZExt32))
                        return MultiplySubZeroExtend32;
                    return Air::Oops;
                };

                Air::Opcode newAirOpcode = tryNewAirOpcode();
                if (isValidForm(newAirOpcode, Arg::Tmp, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
                    append(newAirOpcode, tmp(multiplyLeft->child(0)), tmp(multiplyRight->child(0)), tmp(left), tmp(m_value));
                    commitInternal(right);
                    return true;
                }

                if (!isValidForm(airOpcode, Arg::Tmp, Arg::Tmp, Arg::Tmp, Arg::Tmp))
                    return false;
                if (m_locked.contains(multiplyLeft) || m_locked.contains(multiplyRight))
                    return false;
                append(airOpcode, tmp(multiplyLeft), tmp(multiplyRight), tmp(left), tmp(m_value));
                commitInternal(right);
                return true;
            };

            if (tryAppendMultiplySub())
                return;

            // sub-with-shift Pattern: left - (right ShiftType amount)
            Air::Opcode opcode = opcodeBasedOnShiftKind(right->opcode(),
                SubLeftShift32, SubLeftShift64,
                SubRightShift32, SubRightShift64,
                SubUnsignedRightShift32, SubUnsignedRightShift64);
            if (tryAppendBitOpWithShift(left, right, opcode))
                return;

            appendBinOp<Sub32, Sub64, SubDouble, SubFloat>(left, right);
            return;
        }

        case Neg: {
            auto tryAppendMultiplyNeg = [&] () -> bool {
                // MNEG : d = -(n * m)
                if (m_value->child(0)->opcode() != Mul || !canBeInternal(m_value->child(0)))
                    return false;
                Value* multiplyLeft = m_value->child(0)->child(0);
                Value* multiplyRight = m_value->child(0)->child(1);
                Air::Opcode airOpcode = tryOpcodeForType(MultiplyNeg32, MultiplyNeg64, m_value->type());
                auto tryNewAirOpcode = [&] () -> Air::Opcode {
                    if (airOpcode != MultiplyNeg64)
                        return Air::Oops;
                    // SMNEGL: d = -(SExt32(n) * SExt32(m))
                    if (isMergeableValue(multiplyLeft, SExt32) && isMergeableValue(multiplyRight, SExt32))
                        return MultiplyNegSignExtend32;
                    // UMNEGL: d = -(ZExt32(n) * ZExt32(m))
                    if (isMergeableValue(multiplyLeft, ZExt32) && isMergeableValue(multiplyRight, ZExt32))
                        return MultiplyNegZeroExtend32;
                    return Air::Oops;
                };

                Air::Opcode newAirOpcode = tryNewAirOpcode();
                if (isValidForm(newAirOpcode, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
                    append(newAirOpcode, tmp(multiplyLeft->child(0)), tmp(multiplyRight->child(0)), tmp(m_value));
                    commitInternal(m_value->child(0));
                    return true;
                }

                if (!isValidForm(airOpcode, Arg::Tmp, Arg::Tmp, Arg::Tmp))
                    return false;
                if (m_locked.contains(multiplyLeft) || m_locked.contains(multiplyRight))
                    return false;
                append(airOpcode, tmp(multiplyLeft), tmp(multiplyRight), tmp(m_value));
                commitInternal(m_value->child(0));
                return true;
            };

            if (tryAppendMultiplyNeg())
                return;

            appendUnOp<Neg32, Neg64, NegateDouble, NegateFloat>(m_value->child(0));
            return;
        }

        case Mul: {
            Value* left = m_value->child(0);
            Value* right = m_value->child(1);

            auto tryAppendMultiplyWithExtend = [&] () -> bool {
                auto tryAirOpcode = [&] () -> Air::Opcode {
                    if (m_value->type() != Int64)
                        return Air::Oops;
                    // SMULL: d = SExt32(n) * SExt32(m)
                    if (isMergeableValue(left, SExt32) && isMergeableValue(right, SExt32)) 
                        return MultiplySignExtend32;
                    // UMULL: d = ZExt32(n) * ZExt32(m)
                    if (isMergeableValue(left, ZExt32) && isMergeableValue(right, ZExt32)) 
                        return MultiplyZeroExtend32;
                    return Air::Oops;
                };

                Air::Opcode opcode = tryAirOpcode();
                if (isValidForm(opcode, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
                    append(opcode, tmp(left->child(0)), tmp(right->child(0)), tmp(m_value));
                    return true;
                }
                return false;
            };

            if (tryAppendMultiplyWithExtend())
                return;

            appendBinOp<Mul32, Mul64, MulDouble, MulFloat, Commutative>(left, right);
            return;
        }

        case Div: {
            if (m_value->isChill())
                RELEASE_ASSERT(isARM64());
            if (m_value->type().isInt() && isX86()) {
                appendX86Div(Div);
                return;
            }
            ASSERT(!isX86() || m_value->type().isFloat());

            appendBinOp<Div32, Div64, DivDouble, DivFloat>(m_value->child(0), m_value->child(1));
            return;
        }

        case UDiv: {
            if (m_value->type().isInt() && isX86()) {
                appendX86UDiv(UDiv);
                return;
            }

            ASSERT(!isX86() && !m_value->type().isFloat());

            appendBinOp<UDiv32, UDiv64, Air::Oops, Air::Oops>(m_value->child(0), m_value->child(1));
            return;

        }

        case Mod: {
            RELEASE_ASSERT(isX86());
            RELEASE_ASSERT(!m_value->isChill());
            appendX86Div(Mod);
            return;
        }

        case UMod: {
            RELEASE_ASSERT(isX86());
            appendX86UDiv(UMod);
            return;
        }

        case FMin: {
            RELEASE_ASSERT(isARM64());
            append(m_value->type() == Float ? FloatMin : DoubleMin, tmp(m_value->child(0)), tmp(m_value->child(1)), tmp(m_value));
            return;
        }

        case FMax: {
            RELEASE_ASSERT(isARM64());
            append(m_value->type() == Float ? FloatMax : DoubleMax, tmp(m_value->child(0)), tmp(m_value->child(1)), tmp(m_value));
            return;
        }

        case BitAnd: {
            Value* left = m_value->child(0);
            Value* right = m_value->child(1);

            if (right->isInt(0xff)) {
                appendUnOp<ZeroExtend8To32, ZeroExtend8To32>(left);
                return;
            }

            if (right->isInt(0xffff)) {
                appendUnOp<ZeroExtend16To32, ZeroExtend16To32>(left);
                return;
            }

            if (right->isInt64(0xffffffff) || right->isInt32(0xffffffff)) {
                appendUnOp<Move32, Move32>(left);
                return;
            }

            // UBFX Pattern: dest = (src >> lsb) & mask 
            // Where: mask = (1 << width) - 1
            auto tryAppendUBFX = [&] () -> bool {
                Air::Opcode opcode = opcodeForType(ExtractUnsignedBitfield32, ExtractUnsignedBitfield64, m_value->type());
                if (!isValidForm(opcode, Arg::Tmp, Arg::Imm, Arg::Imm, Arg::Tmp)) 
                    return false;
                if (left->opcode() != ZShr)
                    return false;

                Value* srcValue = left->child(0);
                Value* lsbValue = left->child(1);
                if (m_locked.contains(srcValue) || !imm(lsbValue) || lsbValue->asInt() < 0 || !right->hasInt())
                    return false;

                uint64_t lsb = lsbValue->asInt();
                uint64_t mask = right->asInt();
                if (!mask || mask & (mask + 1))
                    return false;
                uint64_t width = WTF::bitCount(mask);
                uint64_t datasize = opcode == ExtractUnsignedBitfield32 ? 32 : 64;
                if (lsb + width > datasize)
                    return false;

                append(opcode, tmp(srcValue), imm(lsbValue), imm(width), tmp(m_value));
                return true;
            };

            if (tryAppendUBFX())
                return;

            // BIC Pattern: d = n & (m ^ -1)
            auto tryAppendBIC = [&] (Value* left, Value* right) -> bool {
                Air::Opcode opcode = opcodeForType(ClearBitsWithMask32, ClearBitsWithMask64, m_value->type());
                if (!isValidForm(opcode, Arg::Tmp, Arg::Tmp, Arg::Tmp)) 
                    return false;
                if (right->opcode() != BitXor)
                    return false;

                Value* nValue = left;
                Value* mValue = right->child(0);
                Value* minusOne = right->child(1);
                if (m_locked.contains(nValue) || m_locked.contains(mValue) || !minusOne->hasInt() || !minusOne->isInt(-1))
                    return false;

                append(opcode, tmp(nValue), tmp(mValue), tmp(m_value));
                return true;
            };

            if (tryAppendBIC(left, right) || tryAppendBIC(right, left))
                return;

            // and-with-shift Pattern: left & (right ShiftType amount)
            auto tryAppendAndWithShift = [&] (Value* left, Value* right) -> bool {
                Air::Opcode opcode = opcodeBasedOnShiftKind(right->opcode(),
                    AndLeftShift32, AndLeftShift64,
                    AndRightShift32, AndRightShift64,
                    AndUnsignedRightShift32, AndUnsignedRightShift64);
                return tryAppendBitOpWithShift(left, right, opcode);
            };

            if (tryAppendAndWithShift(left, right) || tryAppendAndWithShift(right, left))
                return;

            appendBinOp<And32, And64, AndDouble, AndFloat, Commutative>(left, right);
            return;
        }

        case BitOr: {
            Value* left = m_value->child(0);
            Value* right = m_value->child(1);

            // EXTR Pattern: d = ((n & mask) << highWidth) | (m >> lowWidth)
            // Where: highWidth = datasize - lowWidth
            //        mask = (1 << lowWidth) - 1
            auto tryAppendEXTR = [&] (Value* left, Value* right) -> bool {
                Air::Opcode opcode = opcodeForType(ExtractRegister32, ExtractRegister64, m_value->type());
                if (!isValidForm(opcode, Arg::Tmp, Arg::Tmp, Arg::Imm, Arg::Tmp))
                    return false;
                if (left->opcode() != Shl || left->child(0)->opcode() != BitAnd || right->opcode() != ZShr)
                    return false;

                Value* nValue = left->child(0)->child(0);
                Value* maskValue = left->child(0)->child(1);
                Value* highWidthValue = left->child(1);
                Value* mValue = right->child(0);
                Value* lowWidthValue = right->child(1);
                if (m_locked.contains(nValue) || m_locked.contains(mValue) || !maskValue->hasInt())
                    return false;
                if (!imm(highWidthValue) || highWidthValue->asInt() < 0)
                    return false;
                if (!imm(lowWidthValue) || lowWidthValue->asInt() < 0)
                    return false;

                uint64_t mask = maskValue->asInt();
                if (!mask || mask & (mask + 1))
                    return false;
                uint64_t maskBitCount = WTF::bitCount(mask);
                uint64_t highWidth = highWidthValue->asInt();
                uint64_t lowWidth = lowWidthValue->asInt();
                uint64_t datasize = opcode == ExtractRegister32 ? 32 : 64;
                if (lowWidth + highWidth != datasize || maskBitCount != lowWidth)
                    return false;

                append(opcode, tmp(nValue), tmp(mValue), imm(lowWidthValue), tmp(m_value));
                return true;
            };

            if (tryAppendEXTR(left, right) || tryAppendEXTR(right, left))
                return;           

            // BFI Pattern: d = ((n & mask1) << lsb) | (d & mask2)
            // Where: mask1 = ((1 << width) - 1)
            //        mask2 = ~(mask1 << lsb)
            auto tryAppendBFI = [&] (Value* left, Value* right) -> bool {
                Air::Opcode opcode = opcodeForType(InsertBitField32, InsertBitField64, m_value->type());
                if (!isValidForm(opcode, Arg::Tmp, Arg::Imm, Arg::Imm, Arg::Tmp)) 
                    return false;
                if (left->opcode() != Shl || right->opcode() != BitAnd || left->child(0)->opcode() != BitAnd)
                    return false;

                Value* nValue = left->child(0)->child(0);
                Value* maskValue1 = left->child(0)->child(1);
                Value* lsbValue = left->child(1);
                Value* dValue = right->child(0);
                Value* maskValue2 = right->child(1);
                if (m_locked.contains(nValue) || m_locked.contains(dValue))
                    return false;
                if (!maskValue1->hasInt() || !imm(lsbValue) || lsbValue->asInt() < 0 || !maskValue2->hasInt())
                    return false;

                uint64_t lsb = lsbValue->asInt();
                uint64_t mask1 = maskValue1->asInt();
                if (!mask1 || mask1 & (mask1 + 1))
                    return false;
                uint64_t datasize = opcode == InsertBitField32 ? 32 : 64;
                uint64_t width = WTF::bitCount(mask1);
                if (lsb + width > datasize)
                    return false;

                uint64_t mask2 = maskValue2->asInt();

                auto isValidMask2 = [&] () -> bool {
                    uint64_t mask = ((mask1 << lsb) ^ mask2);
                    uint64_t lowerSet = 0xffffffff;
                    uint64_t fullSet = 0xffffffffffffffff;
                    return datasize == 32 ? !((mask & lowerSet) ^ lowerSet) : !(mask ^ fullSet);
                };

                if (!isValidMask2())
                    return false;

                Tmp result = tmp(m_value);
                append(relaxedMoveForType(m_value->type()), tmp(dValue), result);
                append(opcode, tmp(nValue), imm(lsbValue), imm(width), result);
                return true;
            };

            if (tryAppendBFI(left, right) || tryAppendBFI(right, left))
                return;

            // BFXIL Pattern: d = ((n >> lsb) & mask1) | (d & mask2)
            // Where: mask1 = ((1 << width) - 1)
            //        mask2 = ~mask1
            auto tryAppendBFXIL = [&] (Value* left, Value* right) -> bool {
                Air::Opcode opcode = opcodeForType(ExtractInsertBitfieldAtLowEnd32, ExtractInsertBitfieldAtLowEnd64, m_value->type());
                if (!isValidForm(opcode, Arg::Tmp, Arg::Imm, Arg::Imm, Arg::Tmp)) 
                    return false;
                if (left->opcode() != BitAnd || left->child(0)->opcode() != ZShr || right->opcode() != BitAnd)
                    return false;

                Value* nValue = left->child(0)->child(0);
                Value* maskValue1 = left->child(1);
                Value* lsbValue = left->child(0)->child(1);
                Value* dValue = right->child(0);
                Value* maskValue2 = right->child(1);
                if (m_locked.contains(nValue) || m_locked.contains(dValue))
                    return false;
                if (!maskValue1->hasInt() || !imm(lsbValue) || lsbValue->asInt() < 0 || !maskValue2->hasInt())
                    return false;

                uint64_t lsb = lsbValue->asInt();
                uint64_t mask1 = maskValue1->asInt();
                if (!mask1 || mask1 & (mask1 + 1))
                    return false;
                uint64_t width = WTF::bitCount(mask1);
                uint64_t datasize = opcode == ExtractInsertBitfieldAtLowEnd32 ? 32 : 64;
                if (lsb + width > datasize)
                    return false;
                uint64_t mask2 = maskValue2->asInt();

                auto isValidMask2 = [&] () -> bool {
                    uint64_t mask = mask1 ^ mask2;
                    uint64_t lowerSet = 0xffffffff;
                    uint64_t fullSet = 0xffffffffffffffff;
                    return datasize == 32 ? !((mask & lowerSet) ^ lowerSet) : !(mask ^ fullSet);
                };

                if (!isValidMask2())
                    return false;

                Tmp result = tmp(m_value);
                append(relaxedMoveForType(m_value->type()), tmp(dValue), result);
                append(opcode, tmp(nValue), imm(lsbValue), imm(width), result);
                return true;
            };

            if (tryAppendBFXIL(left, right) || tryAppendBFXIL(right, left))
                return;

            // ORN Pattern: d = n | (m ^ -1)
            auto tryAppendORN = [&] (Value* left, Value* right) -> bool {
                Air::Opcode opcode = opcodeForType(OrNot32, OrNot64, m_value->type());
                if (!isValidForm(opcode, Arg::Tmp, Arg::Tmp, Arg::Tmp)) 
                    return false;
                if (right->opcode() != BitXor)
                    return false;

                Value* nValue = left;
                Value* mValue = right->child(0);
                Value* minusOne = right->child(1);
                if (m_locked.contains(nValue) || m_locked.contains(mValue) || !minusOne->hasInt() || !minusOne->isInt(-1))
                    return false;

                append(opcode, tmp(nValue), tmp(mValue), tmp(m_value));
                return true;
            };

            if (tryAppendORN(left, right) || tryAppendORN(right, left))
                return;

            // orr-with-shift Pattern: left | (right ShiftType amount)
            auto tryAppendOrWithShift = [&] (Value* left, Value* right) -> bool {
                Air::Opcode opcode = opcodeBasedOnShiftKind(right->opcode(),
                    OrLeftShift32, OrLeftShift64,
                    OrRightShift32, OrRightShift64,
                    OrUnsignedRightShift32, OrUnsignedRightShift64);
                return tryAppendBitOpWithShift(left, right, opcode);
            };

            if (tryAppendOrWithShift(left, right) || tryAppendOrWithShift(right, left))
                return;

            appendBinOp<Or32, Or64, OrDouble, OrFloat, Commutative>(left, right);
            return;
        }

        case BitXor: {
            Value* left = m_value->child(0);
            Value* right = m_value->child(1);

            // FIXME: If canBeInternal(child), we should generate this using the comparison path.
            // https://bugs.webkit.org/show_bug.cgi?id=152367
            
            if (right->isInt(-1)) {
                appendUnOp<Not32, Not64>(left);
                return;
            }
            
            // This pattern is super useful on both x86 and ARM64, since the inversion of the CAS result
            // can be done with zero cost on x86 (just flip the set from E to NE) and it's a progression
            // on ARM64 (since STX returns 0 on success, so ordinarily we have to flip it).
            if (right->isInt(1) && left->opcode() == AtomicWeakCAS && canBeInternal(left)) {
                commitInternal(left);
                appendCAS(left, true);
                return;
            }

            // EON Pattern: d = n ^ (m ^ -1)
            auto tryAppendEON = [&] (Value* left, Value* right) -> bool {
                if (right->opcode() != BitXor)
                    return false;
                Value* nValue = left;
                Value* minusOne = right->child(1);
                if (m_locked.contains(nValue) || !minusOne->hasInt() || !minusOne->isInt(-1))
                    return false;

                // eon-with-shift Pattern: d = n ^ ((m ShiftType amount) ^ -1)
                auto tryAppendEONWithShift = [&] (Value* shiftValue) -> bool {
                    Air::Opcode opcode = opcodeBasedOnShiftKind(shiftValue->opcode(), 
                        XorNotLeftShift32, XorNotLeftShift64, 
                        XorNotRightShift32, XorNotRightShift64, 
                        XorNotUnsignedRightShift32, XorNotUnsignedRightShift64);
                    if (!isValidForm(opcode, Arg::Tmp, Arg::Tmp, Arg::Imm, Arg::Tmp) || !canBeInternal(right))
                        return false;
                    Value* mValue = shiftValue->child(0);
                    Value* amountValue = shiftValue->child(1);
                    if (!canBeInternal(shiftValue) || m_locked.contains(mValue) || !imm(amountValue) || amountValue->asInt() < 0)
                        return false;
                    uint64_t amount = amountValue->asInt();
                    uint64_t datasize = m_value->type() == Int32 ? 32 : 64;
                    if (amount >= datasize)
                        return false;

                    append(opcode, tmp(nValue), tmp(mValue), imm(amountValue), tmp(m_value));
                    commitInternal(right);
                    commitInternal(shiftValue);
                    return true;
                };

                if (tryAppendEONWithShift(right->child(0)))
                    return true;

                Value* mValue = right->child(0);
                Air::Opcode opcode = opcodeForType(XorNot32, XorNot64, m_value->type());
                if (!isValidForm(opcode, Arg::Tmp, Arg::Tmp, Arg::Tmp) || m_locked.contains(mValue))
                    return false;
                append(opcode, tmp(nValue), tmp(mValue), tmp(m_value));
                return true;
            };

            if (tryAppendEON(left, right) || tryAppendEON(right, left))
                return;

            // eor-with-shift Pattern: left ^ (right ShiftType amount)
            auto tryAppendXorWithShift = [&] (Value* left, Value* right) -> bool {
                Air::Opcode opcode = opcodeBasedOnShiftKind(right->opcode(),
                    XorLeftShift32, XorLeftShift64,
                    XorRightShift32, XorRightShift64,
                    XorUnsignedRightShift32, XorUnsignedRightShift64);
                return tryAppendBitOpWithShift(left, right, opcode);
            };

            if (tryAppendXorWithShift(left, right) || tryAppendXorWithShift(right, left))
                return;

            appendBinOp<Xor32, Xor64, XorDouble, XorFloat, Commutative>(left, right);
            return;
        }
            
        case Depend: {
            RELEASE_ASSERT(isARM64());
            appendUnOp<Depend32, Depend64>(m_value->child(0));
            return;
        }

        case Shl: {
            Value* left = m_value->child(0);
            Value* right = m_value->child(1);

            // UBFIZ Pattern: d = (n & mask) << lsb 
            // Where: mask = (1 << width) - 1
            auto tryAppendUBFIZ = [&] () -> bool {
                Air::Opcode opcode = opcodeForType(InsertUnsignedBitfieldInZero32, InsertUnsignedBitfieldInZero64, m_value->type());
                if (!isValidForm(opcode, Arg::Tmp, Arg::Imm, Arg::Imm, Arg::Tmp))
                    return false;
                if (left->opcode() != BitAnd)
                    return false;

                Value* nValue = left->child(0);
                Value* maskValue = left->child(1);
                if (m_locked.contains(nValue) || !maskValue->hasInt() || !imm(right) || right->asInt() < 0) 
                    return false;

                uint64_t lsb = right->asInt();
                uint64_t mask = maskValue->asInt();
                if (!mask || mask & (mask + 1))
                    return false;
                uint64_t width = WTF::bitCount(mask);
                uint64_t datasize = opcode == InsertUnsignedBitfieldInZero32 ? 32 : 64;
                if (lsb + width > datasize)
                    return false;

                append(opcode, tmp(nValue), imm(right), imm(width), tmp(m_value));
                return true;
            };

            if (tryAppendUBFIZ())
                return;

            // SBFIZ Pattern: d = ((src << amount) >> amount) << lsb
            // where: amount = datasize - width
            auto tryAppendSBFIZ = [&] () -> bool {
                Air::Opcode opcode = opcodeForType(InsertSignedBitfieldInZero32, InsertSignedBitfieldInZero64, m_value->type());
                if (!isValidForm(opcode, Arg::Tmp, Arg::Imm, Arg::Imm, Arg::Tmp))
                    return false;
                if (left->opcode() != SShr || left->child(0)->opcode() != Shl)
                    return false;

                Value* srcValue = left->child(0)->child(0);
                Value* amount1Value = left->child(0)->child(1);
                Value* amount2Value = left->child(1);
                Value* lsbValue = right;
                if (m_locked.contains(srcValue))
                    return false;
                if (!imm(amount1Value) || !imm(amount2Value) || !imm(lsbValue))
                    return false;
                if (amount1Value->asInt() < 0 || amount2Value->asInt() < 0 || lsbValue->asInt() < 0)
                    return false;

                uint64_t amount1 = amount1Value->asInt();
                uint64_t amount2 = amount2Value->asInt();
                uint64_t lsb = lsbValue->asInt();
                uint64_t datasize = opcode == InsertSignedBitfieldInZero32 ? 32 : 64;
                uint64_t width = datasize - amount1;
                if (amount1 != amount2 || !width || lsb + width > datasize)
                    return false;

                append(opcode, tmp(srcValue), imm(lsbValue), imm(width), tmp(m_value));
                return true;
            };

            if (tryAppendSBFIZ())
                return;

            if (right->isInt32(1)) {
                appendBinOp<Add32, Add64, AddDouble, AddFloat, Commutative>(left, left);
                return;
            }

            appendShift<Lshift32, Lshift64>(left, right);
            return;
        }

        case SShr: {
            Value* left = m_value->child(0);
            Value* right = m_value->child(1);

            // SBFX Pattern: ((src >> lsb) << amount) >> amount
            // Where: amount = datasize - width
            auto tryAppendSBFX = [&] () -> bool {
                Air::Opcode opcode = opcodeForType(ExtractSignedBitfield32, ExtractSignedBitfield64, m_value->type());
                if (!isValidForm(opcode, Arg::Tmp, Arg::Imm, Arg::Imm, Arg::Tmp))
                    return false;
                if (left->opcode() != Shl || (left->child(0)->opcode() != ZShr && left->child(0)->opcode() != SShr))
                    return false;

                Value* srcValue = left->child(0)->child(0);
                Value* lsbValue = left->child(0)->child(1);
                Value* amount1Value = left->child(1);
                Value* amount2Value = right;
                if (m_locked.contains(srcValue))
                    return false;
                if (!imm(lsbValue) || !imm(amount1Value) || !imm(amount2Value))
                    return false;
                if (lsbValue->asInt() < 0 || amount1Value->asInt() < 0 || amount2Value->asInt() < 0)
                    return false;

                uint64_t amount1 = amount1Value->asInt();
                uint64_t amount2 = amount2Value->asInt();
                uint64_t lsb = lsbValue->asInt();
                uint64_t datasize = opcode == ExtractSignedBitfield32 ? 32 : 64;
                uint64_t width = datasize - amount1;
                if (amount1 != amount2 || !width || lsb + width > datasize)
                    return false;

                append(opcode, tmp(srcValue), imm(lsbValue), imm(width), tmp(m_value));
                return true;
            };

            if (tryAppendSBFX())
                return;

            appendShift<Rshift32, Rshift64>(left, right);
            return;
        }

        case ZShr: {
            Value* left = m_value->child(0);
            Value* right = m_value->child(1);

            appendShift<Urshift32, Urshift64>(left, right);
            return;
        }

        case RotR: {
            appendShift<RotateRight32, RotateRight64>(m_value->child(0), m_value->child(1));
            return;
        }

        case RotL: {
            appendShift<RotateLeft32, RotateLeft64>(m_value->child(0), m_value->child(1));
            return;
        }

        case Clz: {
            appendUnOp<CountLeadingZeros32, CountLeadingZeros64>(m_value->child(0));
            return;
        }

        case Abs: {
            RELEASE_ASSERT_WITH_MESSAGE(!isX86(), "Abs is not supported natively on x86. It must be replaced before generation.");
            appendUnOp<Air::Oops, Air::Oops, AbsDouble, AbsFloat>(m_value->child(0));
            return;
        }

        case Ceil: {
            appendUnOp<Air::Oops, Air::Oops, CeilDouble, CeilFloat>(m_value->child(0));
            return;
        }

        case Floor: {
            appendUnOp<Air::Oops, Air::Oops, FloorDouble, FloorFloat>(m_value->child(0));
            return;
        }

        case Sqrt: {
            appendUnOp<Air::Oops, Air::Oops, SqrtDouble, SqrtFloat>(m_value->child(0));
            return;
        }

        case BitwiseCast: {
            appendUnOp<Move32ToFloat, Move64ToDouble, MoveDoubleTo64, MoveFloatTo32>(m_value->child(0));
            return;
        }

        case Store: {
            // Pre-Index Canonical Form:
            //     address = Add(base, Offset)              --->    Move %base %address
            //     memory = Store(value, base, Offset)              MoveWithIncrement %value (%address, prefix(offset))
            // Post-Index Canonical Form:
            //     address = Add(base, Offset)              --->    Move %base %address
            //     memory = Store(value, base, 0)                   MoveWithIncrement %value (%address, postfix(offset))
            auto tryAppendIncrementAddress = [&] () -> bool {
                MemoryValue* memory = m_value->as<MemoryValue>();
                Value* value = memory->child(0);
                Air::Opcode opcode = tryOpcodeForType(MoveWithIncrement32, MoveWithIncrement64, value->type());
                if (!isValidForm(opcode, Arg::PreIndex, Arg::Tmp) || !m_index)
                    return false;
                Value* address = m_block->at(m_index - 1);
                if (address->opcode() != Add || address->type() != Int64)
                    return false;

                Value* base1 = address->child(0);
                Value* base2 = memory->child(1);
                if (base1 != base2 || !address->child(1)->hasIntPtr())
                    return false;
                intptr_t offset = address->child(1)->asIntPtr();
                Value::OffsetType smallOffset = static_cast<Value::OffsetType>(offset);
                if (smallOffset != offset || !Arg::isValidIncrementIndexForm(smallOffset))
                    return false;
                if (m_locked.contains(address) || m_locked.contains(base1) || m_locked.contains(value))
                    return false;

                Arg incrementArg = Arg();
                if (memory->offset()) {
                    if (smallOffset == memory->offset())
                        incrementArg = Arg::preIndex(tmp(address), smallOffset);
                } else
                    incrementArg = Arg::postIndex(tmp(address), smallOffset);

                if (incrementArg) {
                    append(relaxedMoveForType(address->type()), tmp(base1), tmp(address));
                    append(opcode, tmp(value), incrementArg);
                    m_locked.add(address);
                    return true;
                }
                return false;
            };

            if (tryAppendIncrementAddress())
                return;

            Value* valueToStore = m_value->child(0);
            if (canBeInternal(valueToStore)) {
                bool matched = false;
                switch (valueToStore->opcode()) {
                case Add:
                    matched = tryAppendStoreBinOp<Add32, Add64, Commutative>(
                        valueToStore->child(0), valueToStore->child(1));
                    break;
                case Sub:
                    if (valueToStore->child(0)->isInt(0)) {
                        matched = tryAppendStoreUnOp<Neg32, Neg64>(valueToStore->child(1));
                        break;
                    }
                    matched = tryAppendStoreBinOp<Sub32, Sub64>(
                        valueToStore->child(0), valueToStore->child(1));
                    break;
                case BitAnd:
                    matched = tryAppendStoreBinOp<And32, And64, Commutative>(
                        valueToStore->child(0), valueToStore->child(1));
                    break;
                case BitXor:
                    if (valueToStore->child(1)->isInt(-1)) {
                        matched = tryAppendStoreUnOp<Not32, Not64>(valueToStore->child(0));
                        break;
                    }
                    matched = tryAppendStoreBinOp<Xor32, Xor64, Commutative>(
                        valueToStore->child(0), valueToStore->child(1));
                    break;
                default:
                    break;
                }
                if (matched) {
                    commitInternal(valueToStore);
                    return;
                }
            }

            appendStore(m_value, addr(m_value));
            return;
        }

        case B3::Store8: {
            Value* valueToStore = m_value->child(0);
            if (canBeInternal(valueToStore)) {
                bool matched = false;
                switch (valueToStore->opcode()) {
                case Add:
                    matched = tryAppendStoreBinOp<Add8, Air::Oops, Commutative>(
                        valueToStore->child(0), valueToStore->child(1));
                    break;
                default:
                    break;
                }
                if (matched) {
                    commitInternal(valueToStore);
                    return;
                }
            }
            appendStore(m_value, addr(m_value));
            return;
        }

        case B3::Store16: {
            Value* valueToStore = m_value->child(0);
            if (canBeInternal(valueToStore)) {
                bool matched = false;
                switch (valueToStore->opcode()) {
                case Add:
                    matched = tryAppendStoreBinOp<Add16, Air::Oops, Commutative>(
                        valueToStore->child(0), valueToStore->child(1));
                    break;
                default:
                    break;
                }
                if (matched) {
                    commitInternal(valueToStore);
                    return;
                }
            }
            appendStore(m_value, addr(m_value));
            return;
        }

        case WasmAddress: {
            WasmAddressValue* address = m_value->as<WasmAddressValue>();

            if constexpr (isARM64()) {
                Value* index = m_value->child(0);
                if (canBeInternal(index)) {
                    // Maybe, the ideal approach is to introduce a decorator (Index@EXT) to the Air operand
                    // to provide an extension opportunity for the specific form under the Air opcode.
                    if (isMergeableValue(index, ZExt32)) {
                        append(AddZeroExtend64, Arg(address->pinnedGPR()), tmp(index->child(0)), tmp(address));
                        commitInternal(index);
                        return;
                    }

                    if (isMergeableValue(index, SExt32)) {
                        append(AddSignExtend64, Arg(address->pinnedGPR()), tmp(index->child(0)), tmp(address));
                        commitInternal(index);
                        return;
                    }
                }
            }

            append(Add64, Arg(address->pinnedGPR()), tmp(m_value->child(0)), tmp(address));
            return;
        }

        case B3::VectorExtractLane: {
            SIMDValue* value = m_value->as<SIMDValue>();
            auto lane = value->simdLane();
            auto signMode = value->signMode();
            append(GET_SIGNED_SIMD_OPCODE(lane, signMode, Air::VectorExtractLane), Arg::imm(value->immediate(0)), tmp(value->child(0)), tmp(value));
            return;
        }

        case B3::VectorReplaceLane: {
            SIMDValue* value = m_value->as<SIMDValue>();
            auto lane = value->simdLane();
            auto replacementScalar = tmp(value->child(1));
            Tmp result = tmp(value);
            append(Air::MoveVector, tmp(value->child(0)), result);
            append(GET_SIMD_OPCODE(lane, Air::VectorReplaceLane), Arg::imm(value->immediate(0)), replacementScalar, result);
            return;
        }

        case B3::VectorSplat: {
            SIMDValue* value = m_value->as<SIMDValue>();
            SIMDLane lane = value->simdLane();
            Tmp scalar = tmp(value->child(0));
            Air::Opcode op;
            switch (lane) {
            case SIMDLane::i8x16:
                op = VectorSplatInt8;
                break;
            case SIMDLane::i16x8:
                op = VectorSplatInt16;
                break;
            case SIMDLane::i32x4:
                op = VectorSplatInt32;
                break;
            case SIMDLane::i64x2:
                op = VectorSplatInt64;
                break;
            case SIMDLane::f32x4:
                op = VectorSplatFloat32;
                break;
            case SIMDLane::f64x2:
                op = VectorSplatFloat64;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            if (isValidForm(op, Arg::Tmp, Arg::Tmp)) {
                append(op, scalar, tmp(value));
                return;
            }
            RELEASE_ASSERT_NOT_REACHED();
        }

        case B3::VectorShr:
        case B3::VectorShl: {
            SIMDValue* value = m_value->as<SIMDValue>();
            SIMDLane lane = value->simdLane();

            int32_t mask = (elementByteSize(lane) * CHAR_BIT) - 1;

            auto v = tmp(value->child(0));
            auto shift = tmp(value->child(1));

            Tmp shiftAmount = m_code.newTmp(B3::GP);
            Tmp shiftVector = m_code.newTmp(B3::FP);

            if constexpr (isARM64()) {
                append(And32, Arg::bitImm(mask), shift, shiftAmount);
                if (value->opcode() == VectorShr) {
                    // ARM64 doesn't have a version of this instruction for right shift. Instead, if the input to
                    // left shift is negative, it's a right shift by the absolute value of that amount.
                    append(Neg32, shiftAmount);
                }
                append(VectorSplatInt8, shiftAmount, shiftVector);
                append(value->signMode() == SIMDSignMode::Signed ? VectorSshl : VectorUshl, Arg::simdInfo(value->simdInfo()), v, shiftVector, tmp(value));

                return;
            }
            
            if constexpr (isX86()) {
                append(Move, shift, shiftAmount);
                append(And32, Arg::imm(mask), shiftAmount);
                if (value->opcode() == VectorShr && value->signMode() == SIMDSignMode::Signed && value->simdLane() == SIMDLane::i64x2) {
                    // x86 has no SIMD 64-bit signed right shift instruction, so we scalarize it here.
                    Tmp lower = m_code.newTmp(B3::GP);
                    Tmp upper = m_code.newTmp(B3::GP);
                    Tmp result = tmp(value);

                    append(Move, shiftAmount, m_ecx);
                    append(VectorExtractLaneInt64, Arg::imm(0), v, lower);
                    append(VectorExtractLaneInt64, Arg::imm(1), v, upper);
                    append(Rshift64, m_ecx, lower);
                    append(Rshift64, m_ecx, upper);
                    append(VectorReplaceLaneInt64, Arg::imm(0), lower, result);
                    append(VectorReplaceLaneInt64, Arg::imm(1), upper, result);
                    return;
                }

                // Unlike ARM, x86 expects the shift provided as a *scalar*, stored in the lower 64 bits of a vector register.
                // So, we don't need to splat the shift amount like we do on ARM.
                append(Move64ToDouble, shiftAmount, shiftVector);

                // 8-bit shifts are pretty involved to implement on Intel, so they get their own instruction type with extra temps.
                if (value->opcode() == VectorShl && value->simdLane() == SIMDLane::i8x16) {
                    append(VectorUshl8, v, shiftVector, tmp(value), m_code.newTmp(B3::FP), m_code.newTmp(B3::FP));
                    return;
                }
                if (value->opcode() == VectorShr && value->simdLane() == SIMDLane::i8x16) {
                    append(value->signMode() == SIMDSignMode::Signed ? VectorSshr8 : VectorUshr8, v, shiftVector, tmp(value), m_code.newTmp(B3::FP), m_code.newTmp(B3::FP));
                    return;
                }

                if (value->opcode() == VectorShl)
                    append(VectorUshl, Arg::simdInfo(value->simdInfo()), v, shiftVector, tmp(value));
                else
                    append(value->signMode() == SIMDSignMode::Signed ? VectorSshr : VectorUshr, Arg::simdInfo(value->simdInfo()), v, shiftVector, tmp(value));
                return;
            }

            RELEASE_ASSERT_NOT_REACHED();
        }

        case B3::VectorEqual:
            emitSIMDCompare(Air::Arg::relCond(MacroAssembler::Equal), Air::Arg::doubleCond(MacroAssembler::DoubleEqualAndOrdered));
            return;
        case B3::VectorNotEqual:
            emitSIMDCompare(Air::Arg::relCond(MacroAssembler::NotEqual), Air::Arg::doubleCond(MacroAssembler::DoubleNotEqualOrUnordered));
            return;
        case B3::VectorLessThan:
            emitSIMDCompare(Air::Arg::relCond(MacroAssembler::LessThan), Air::Arg::doubleCond(MacroAssembler::DoubleLessThanAndOrdered));
            return;
        case B3::VectorLessThanOrEqual:
            emitSIMDCompare(Air::Arg::relCond(MacroAssembler::LessThanOrEqual), Air::Arg::doubleCond(MacroAssembler::DoubleLessThanOrEqualAndOrdered));
            return;
        case B3::VectorBelow:
            emitSIMDCompare(Air::Arg::relCond(MacroAssembler::Below));
            return;
        case B3::VectorBelowOrEqual:
            emitSIMDCompare(Air::Arg::relCond(MacroAssembler::BelowOrEqual));
            return;
        case B3::VectorGreaterThan:
            emitSIMDCompare(Air::Arg::relCond(MacroAssembler::GreaterThan), Air::Arg::doubleCond(MacroAssembler::DoubleGreaterThanAndOrdered));
            return;
        case B3::VectorGreaterThanOrEqual:
            emitSIMDCompare(Air::Arg::relCond(MacroAssembler::GreaterThanOrEqual), Air::Arg::doubleCond(MacroAssembler::DoubleGreaterThanOrEqualAndOrdered));
            return;
        case B3::VectorAbove:
            emitSIMDCompare(Air::Arg::relCond(MacroAssembler::Above));
            return;
        case B3::VectorAboveOrEqual:
            emitSIMDCompare(Air::Arg::relCond(MacroAssembler::AboveOrEqual));
            return;
        case B3::VectorAdd:
            emitSIMDBinaryOp(Air::VectorAdd);
            return;
        case B3::VectorSub:
            emitSIMDBinaryOp(Air::VectorSub);
            return;
        case B3::VectorAddSat:
            emitSIMDBinaryOp(Air::VectorAddSat);
            return;
        case B3::VectorSubSat:
            emitSIMDBinaryOp(Air::VectorSubSat);
            return;
        case B3::VectorMul:
            emitSIMDBinaryOp(Air::VectorMul);
            return;
        case B3::VectorDotProduct:
            if (isX86())
                emitSIMDMonomorphicBinaryOp(Air::VectorDotProduct);
            else if (isARM64())
                emitSIMDMonomorphicBinaryOp</* scratch = */ 1>(Air::VectorDotProduct);
            else
                RELEASE_ASSERT_NOT_REACHED();
            return; 
        case B3::VectorDiv:
            emitSIMDBinaryOp(Air::VectorDiv);
            return;
        case B3::VectorMin:
            emitSIMDBinaryOp(Air::VectorMin);
            return;
        case B3::VectorMax:
            emitSIMDBinaryOp(Air::VectorMax);
            return;
        case B3::VectorPmin:
            if (isX86()) {
                emitSIMDBinaryOp(Air::VectorPmin);
                return;
            }
            if (isARM64()) {
                emitSIMDBinaryOp</* scratch = */ 1>(Air::VectorPmin);
                return;
            }
            RELEASE_ASSERT_NOT_REACHED();
        case B3::VectorPmax:
            if (isX86()) {
                emitSIMDBinaryOp(Air::VectorPmax);
                return;
            }
            if (isARM64()) {
                emitSIMDBinaryOp</* scratch = */ 1>(Air::VectorPmax);
                return;
            }
            RELEASE_ASSERT_NOT_REACHED();
        case B3::VectorNarrow:
            emitSIMDBinaryOp</* scratch = */ 1>(Air::VectorNarrow);
            return; 
        case B3::VectorAnd:
            emitSIMDBinaryOp(Air::VectorAnd);
            return;
        case B3::VectorAndnot:
            emitSIMDBinaryOp(Air::VectorAndnot);
            return;
        case B3::VectorOr:
            emitSIMDBinaryOp(Air::VectorOr);
            return;
        case B3::VectorXor:
            emitSIMDBinaryOp(Air::VectorXor);
            return;
        case B3::VectorNot:
            emitSIMDUnaryOp(Air::VectorNot);
            return;
        case B3::VectorAbs:
            if (isX86() && m_value->as<SIMDValue>()->simdLane() == SIMDLane::i64x2) {
                SIMDValue* value = m_value->as<SIMDValue>();
                append(VectorAbsInt64, tmp(value->child(0)), tmp(value), m_code.newTmp(B3::FP));
                return;
            }
            emitSIMDUnaryOp(Air::VectorAbs);
            return;
        case B3::VectorNeg:
            emitSIMDUnaryOp(Air::VectorNeg);
            return;
        case B3::VectorPopcnt:
            emitSIMDUnaryOp(Air::VectorPopcnt);
            return;
        case B3::VectorCeil:
            emitSIMDUnaryOp(Air::VectorCeil);
            return;
        case B3::VectorFloor:
            emitSIMDUnaryOp(Air::VectorFloor);
            return;
        case B3::VectorTrunc:
            emitSIMDUnaryOp(Air::VectorTrunc);
            return;
        case B3::VectorTruncSat:
            if (isX86()) {
                SIMDValue* value = m_value->as<SIMDValue>();
                Tmp v = tmp(value->child(0));
                Tmp result = tmp(value);

                switch (value->simdLane()) {
                case SIMDLane::f64x2:
                    if (value->signMode() == SIMDSignMode::Signed)
                        append(VectorTruncSatSignedFloat64, v, result, m_code.newTmp(B3::GP), m_code.newTmp(B3::FP));
                    else
                        append(VectorTruncSatUnsignedFloat64, v, result, m_code.newTmp(B3::GP), m_code.newTmp(B3::FP));
                    return;
                case SIMDLane::f32x4:
                    if (value->signMode() == SIMDSignMode::Signed)
                        append(Air::VectorTruncSat, Arg::simdInfo(value->simdInfo()), v, result, m_code.newTmp(B3::GP), m_code.newTmp(B3::FP), m_code.newTmp(B3::FP));
                    else
                        append(VectorTruncSatUnsignedFloat32, v, result, m_code.newTmp(B3::GP), m_code.newTmp(B3::FP), m_code.newTmp(B3::FP));
                    return;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
            }
            emitSIMDUnaryOp(Air::VectorTruncSat);
            return;
        case B3::VectorConvert:
            if (isX86() && m_value->as<SIMDValue>()->signMode() == SIMDSignMode::Unsigned) {
                SIMDValue* value = m_value->as<SIMDValue>();
                append(VectorConvertUnsigned, tmp(value->child(0)), tmp(value), m_code.newTmp(B3::FP));
                return;
            }
            emitSIMDUnaryOp(Air::VectorConvert);
            return;
        case B3::VectorConvertLow:
            if (isX86()) {
                SIMDValue* value = m_value->as<SIMDValue>();
                if (value->signMode() == SIMDSignMode::Signed)
                    append(VectorConvertLowSignedInt32, tmp(value->child(0)), tmp(value));
                else
                    append(VectorConvertLowUnsignedInt32, tmp(value->child(0)), tmp(value), m_code.newTmp(B3::GP), m_code.newTmp(B3::FP));
                return;
            }
            emitSIMDUnaryOp(Air::VectorConvertLow);
            return;
        case B3::VectorNearest:
            emitSIMDUnaryOp(Air::VectorNearest);
            return;
        case B3::VectorSqrt:
            emitSIMDUnaryOp(Air::VectorSqrt);
            return;
        case B3::VectorExtendLow:
            emitSIMDUnaryOp(Air::VectorExtendLow);
            return;
        case B3::VectorExtendHigh:
            emitSIMDUnaryOp(Air::VectorExtendHigh);
            return;
        case B3::VectorPromote:
            emitSIMDUnaryOp(Air::VectorPromote);
            return;
        case B3::VectorDemote:
            emitSIMDUnaryOp(Air::VectorDemote);
            return;
        case B3::VectorAnyTrue: 
            emitSIMDMonomorphicUnaryOp(Air::VectorAnyTrue);
            return;
        case B3::VectorAllTrue:
            emitSIMDUnaryOp(Air::VectorAllTrue);
            return;
        case B3::VectorAvgRound:
            emitSIMDBinaryOp(Air::VectorAvgRound);
            return;
        case B3::VectorBitmask:
            emitSIMDUnaryOp(Air::VectorBitmask);
            return;
        case B3::VectorBitwiseSelect: {
            SIMDValue* value = m_value->as<SIMDValue>();
            auto resultTmp = m_code.newTmp(FP);
            append(MoveVector, tmp(value->child(2)), resultTmp);
            append(Air::VectorBitwiseSelect, tmp(value->child(0)), tmp(value->child(1)), resultTmp);
            append(MoveVector, resultTmp, tmp(value));
            return;
        }
        case B3::VectorExtaddPairwise:
            if (isX86()) {
                SIMDValue* value = m_value->as<SIMDValue>();
                if (value->simdLane() == SIMDLane::i16x8 && value->signMode() == SIMDSignMode::Unsigned)
                    append(VectorExtaddPairwiseUnsignedInt16, tmp(value->child(0)), tmp(value), m_code.newTmp(B3::FP));
                else
                    append(Air::VectorExtaddPairwise, Arg::simdInfo(value->simdInfo()), tmp(value->child(0)), tmp(value), m_code.newTmp(B3::GP), m_code.newTmp(B3::FP));
                return;
            }
            emitSIMDUnaryOp(Air::VectorExtaddPairwise);
            return;
        case B3::VectorMulSat:
            if constexpr (isX86()) {
                SIMDValue* value = m_value->as<SIMDValue>();
                append(Air::VectorMulSat, tmp(value->child(0)), tmp(value->child(1)), tmp(value), m_code.newTmp(GP), m_code.newTmp(FP));
                return;
            }
            emitSIMDMonomorphicBinaryOp(Air::VectorMulSat);
            return;
        case B3::VectorSwizzle:
            emitSIMDMonomorphicBinaryOp(Air::VectorSwizzle);
            return;
        case B3::VectorShuffle: {
            SIMDValue* value = m_value->as<SIMDValue>();
            auto imm = value->immediate();
            auto a = tmp(value->child(0));
            auto b = tmp(value->child(1));
            auto result = tmp(m_value);

            if constexpr (isX86()) {
                RELEASE_ASSERT_NOT_REACHED(); // FIXME
                return;
            }

            append(Air::VectorShuffle, Arg::bigImm(imm.u64x2[0]), Arg::bigImm(imm.u64x2[1]), a, b, result);
            return;
        }

        case Fence: {
            FenceValue* fence = m_value->as<FenceValue>();
            if (!fence->write && !fence->read)
                return;
            if (!fence->write) {
                // A fence that reads but does not write is for protecting motion of stores.
                append(StoreFence);
                return;
            }
            if (!fence->read) {
                // A fence that writes but does not read is for protecting motion of loads.
                append(LoadFence);
                return;
            }
            append(MemoryFence);
            return;
        }

        case Trunc: {
            ASSERT(tmp(m_value->child(0)) == tmp(m_value));
            return;
        }

        case SExt8: {
            appendUnOp<SignExtend8To32, Air::Oops>(m_value->child(0));
            return;
        }

        case SExt16: {
            appendUnOp<SignExtend16To32, Air::Oops>(m_value->child(0));
            return;
        }

        case ZExt32: {
            appendUnOp<Move32, Air::Oops>(m_value->child(0));
            return;
        }

        case SExt32: {
            // FIXME: We should have support for movsbq/movswq
            // https://bugs.webkit.org/show_bug.cgi?id=152232
            
            appendUnOp<SignExtend32ToPtr, Air::Oops>(m_value->child(0));
            return;
        }

        case FloatToDouble: {
            appendUnOp<Air::Oops, Air::Oops, Air::Oops, ConvertFloatToDouble>(m_value->child(0));
            return;
        }

        case DoubleToFloat: {
            appendUnOp<Air::Oops, Air::Oops, ConvertDoubleToFloat>(m_value->child(0));
            return;
        }

        case ArgumentReg: {
            m_prologue.append(Inst(
                moveForType(m_value->type()), m_value,
                Tmp(m_value->as<ArgumentRegValue>()->argumentReg()),
                tmp(m_value)));
            return;
        }

        case Const32:
        case Const64: {
            if (imm(m_value))
                append(Move, imm(m_value), tmp(m_value));
            else
                append(Move, Arg::bigImm(m_value->asInt()), tmp(m_value));
            return;
        }

        case ConstDouble:
        case ConstFloat: {
            // We expect that the moveConstants() phase has run, and any doubles referenced from
            // stackmaps get fused.
            RELEASE_ASSERT(m_value->opcode() == ConstFloat || isIdentical(m_value->asDouble(), 0.0));
            RELEASE_ASSERT(m_value->opcode() == ConstDouble || isIdentical(m_value->asFloat(), 0.0f));
            append(MoveZeroToDouble, tmp(m_value));
            return;
        }

        case Const128: {
            // We expect that the moveConstants() phase has run, and any constant vector referenced from stackmaps get fused.
            RELEASE_ASSERT(!m_value->asV128().u64x2[0] && !m_value->asV128().u64x2[1]);
            append(MoveZeroToVector, tmp(m_value));
            return;
        }

        case BottomTuple: {
            forEachImmOrTmp(m_value, [&] (Arg tmp, Type type, unsigned) {
                switch (type.kind()) {
                case Void:
                case Tuple:
                case V128:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                case Int32:
                case Int64:
                    ASSERT(Arg::isValidImmForm(0));
                    append(Move, imm(static_cast<int64_t>(0)), tmp.tmp());
                    break;
                case Float:
                case Double:
                    append(MoveZeroToDouble, tmp.tmp());
                    break;
                }
            });
            return;
        }

        case FramePointer: {
            ASSERT(tmp(m_value) == Tmp(GPRInfo::callFrameRegister));
            return;
        }

        case SlotBase: {
            append(
                pointerType() == Int64 ? Lea64 : Lea32,
                Arg::stack(m_value->as<SlotBaseValue>()->slot()),
                tmp(m_value));
            return;
        }

        case Equal:
        case NotEqual: {
            // FIXME: Teach this to match patterns that arise from subwidth CAS. The CAS's result has to
            // be either zero- or sign-extended, and the value it's compared to should also be zero- or
            // sign-extended in a matching way. It's not super clear that this is very profitable.
            // https://bugs.webkit.org/show_bug.cgi?id=169250
            if (m_value->child(0)->opcode() == AtomicStrongCAS
                && m_value->child(0)->as<AtomicValue>()->isCanonicalWidth()
                && m_value->child(0)->child(0) == m_value->child(1)
                && canBeInternal(m_value->child(0))) {
                ASSERT(!m_locked.contains(m_value->child(0)->child(1)));
                ASSERT(!m_locked.contains(m_value->child(1)));
                
                commitInternal(m_value->child(0));
                appendCAS(m_value->child(0), m_value->opcode() == NotEqual);
                return;
            }
                
            m_insts.last().append(createCompare(m_value));
            return;
        }
            
        case LessThan:
        case GreaterThan:
        case LessEqual:
        case GreaterEqual:
        case Above:
        case Below:
        case AboveEqual:
        case BelowEqual:
        case EqualOrUnordered: {
            m_insts.last().append(createCompare(m_value));
            return;
        }

        case Select: {
            MoveConditionallyConfig config;
            if (m_value->type().isInt()) {
                config.moveConditionally32 = MoveConditionally32;
                config.moveConditionally64 = MoveConditionally64;
                config.moveConditionallyTest32 = MoveConditionallyTest32;
                config.moveConditionallyTest64 = MoveConditionallyTest64;
                config.moveConditionallyDouble = MoveConditionallyDouble;
                config.moveConditionallyFloat = MoveConditionallyFloat;
            } else {
                // FIXME: it's not obvious that these are particularly efficient.
                // https://bugs.webkit.org/show_bug.cgi?id=169251
                config.moveConditionally32 = MoveDoubleConditionally32;
                config.moveConditionally64 = MoveDoubleConditionally64;
                config.moveConditionallyTest32 = MoveDoubleConditionallyTest32;
                config.moveConditionallyTest64 = MoveDoubleConditionallyTest64;
                config.moveConditionallyDouble = MoveDoubleConditionallyDouble;
                config.moveConditionallyFloat = MoveDoubleConditionallyFloat;
            }
            
            m_insts.last().append(createSelect(config));
            return;
        }

        case IToD: {
            appendUnOp<ConvertInt32ToDouble, ConvertInt64ToDouble>(m_value->child(0));
            return;
        }

        case IToF: {
            appendUnOp<ConvertInt32ToFloat, ConvertInt64ToFloat>(m_value->child(0));
            return;
        }

        case B3::CCall: {
            CCallValue* cCall = m_value->as<CCallValue>();

            Inst inst(m_isRare ? Air::ColdCCall : Air::CCall, cCall);

            // We have a ton of flexibility regarding the callee argument, but currently, we don't
            // use it yet. It gets weird for reasons:
            // 1) We probably will never take advantage of this. We don't have C calls to locations
            //    loaded from addresses. We have JS calls like that, but those use Patchpoints.
            // 2) On X86_64 we still don't support call with BaseIndex.
            // 3) On non-X86, we don't natively support any kind of loading from address.
            // 4) We don't have an isValidForm() for the CCallSpecial so we have no smart way to
            //    decide.
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=151052
            inst.args.append(tmp(cCall->child(0)));

            if (cCall->type() != Void)
                inst.args.append(tmp(cCall));

            for (unsigned i = 1; i < cCall->numChildren(); ++i)
                inst.args.append(immOrTmp(cCall->child(i)));

            m_insts.last().append(WTFMove(inst));
            return;
        }

        case Patchpoint: {
            PatchpointValue* patchpointValue = m_value->as<PatchpointValue>();
            ensureSpecial(m_patchpointSpecial);
            
            Inst inst(Patch, patchpointValue, Arg::special(m_patchpointSpecial));

            Vector<Inst> after;
            auto generateResultOperand = [&] (Type type, ValueRep rep, Tmp tmp) {
                switch (rep.kind()) {
                case ValueRep::WarmAny:
                case ValueRep::ColdAny:
                case ValueRep::LateColdAny:
                case ValueRep::SomeRegister:
                case ValueRep::SomeEarlyRegister:
                case ValueRep::SomeLateRegister:
                    inst.args.append(tmp);
                    return;
                case ValueRep::Register: {
                    Tmp reg = Tmp(rep.reg());
                    inst.args.append(reg);
                    after.append(Inst(relaxedMoveForType(type), m_value, reg, tmp));
                    return;
                }
                case ValueRep::StackArgument: {
                    Arg arg = Arg::callArg(rep.offsetFromSP());
                    inst.args.append(arg);
                    after.append(Inst(moveForType(type), m_value, arg, tmp));
                    return;
                }
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    return;
                }
            };

            if (patchpointValue->type() != Void) {
                forEachImmOrTmp(patchpointValue, [&] (Arg arg, Type type, unsigned index) {
                    generateResultOperand(type, patchpointValue->resultConstraints[index], arg.tmp());
                });
            }
            
            fillStackmap(inst, patchpointValue, 0);
            for (auto& constraint : patchpointValue->resultConstraints) {
                if (constraint.isReg())
                    patchpointValue->lateClobbered().remove(constraint.reg());
            }

            for (unsigned i = patchpointValue->numGPScratchRegisters; i--;)
                inst.args.append(m_code.newTmp(GP));
            for (unsigned i = patchpointValue->numFPScratchRegisters; i--;)
                inst.args.append(m_code.newTmp(FP));
            
            m_insts.last().append(WTFMove(inst));
            m_insts.last().appendVector(after);
            return;
        }

        case Extract: {
            Value* tupleValue = m_value->child(0);
            unsigned index = m_value->as<ExtractValue>()->index();

            const auto& tmps = tmpsForTuple(tupleValue);
            append(relaxedMoveForType(m_value->type()), tmps[index], tmp(m_value));
            return;
        }

        case CheckAdd:
        case CheckSub:
        case CheckMul: {
            CheckValue* checkValue = m_value->as<CheckValue>();

            Value* left = checkValue->child(0);
            Value* right = checkValue->child(1);

            Tmp result = tmp(m_value);

            // Handle checked negation.
            if (checkValue->opcode() == CheckSub && left->isInt(0)) {
                append(Move, tmp(right), result);

                Air::Opcode opcode =
                    opcodeForType(BranchNeg32, BranchNeg64, checkValue->type());
                CheckSpecial* special = ensureCheckSpecial(opcode, 2);

                Inst inst(Patch, checkValue, Arg::special(special));
                inst.args.append(Arg::resCond(MacroAssembler::Overflow));
                inst.args.append(result);

                fillStackmap(inst, checkValue, 2);

                m_insts.last().append(WTFMove(inst));
                return;
            }

            Air::Opcode opcode = Air::Oops;
            Commutativity commutativity = NotCommutative;
            StackmapSpecial::RoleMode stackmapRole = StackmapSpecial::SameAsRep;
            switch (m_value->opcode()) {
            case CheckAdd:
                opcode = opcodeForType(BranchAdd32, BranchAdd64, m_value->type());
                stackmapRole = StackmapSpecial::ForceLateUseUnlessRecoverable;
                commutativity = Commutative;
                break;
            case CheckSub:
                opcode = opcodeForType(BranchSub32, BranchSub64, m_value->type());
                break;
            case CheckMul:
                opcode = opcodeForType(BranchMul32, BranchMul64, checkValue->type());
                stackmapRole = StackmapSpecial::ForceLateUse;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }

            // FIXME: It would be great to fuse Loads into these. We currently don't do it because the
            // rule for stackmaps is that all addresses are just stack addresses. Maybe we could relax
            // this rule here.
            // https://bugs.webkit.org/show_bug.cgi?id=151228

            Vector<Arg, 2> sources;
            if (imm(right) && isValidForm(opcode, Arg::ResCond, Arg::Tmp, Arg::Imm, Arg::Tmp)) {
                sources.append(tmp(left));
                sources.append(imm(right));
            } else if (imm(right) && isValidForm(opcode, Arg::ResCond, Arg::Imm, Arg::Tmp)) {
                sources.append(imm(right));
                append(Move, tmp(left), result);
            } else if (isValidForm(opcode, Arg::ResCond, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
                sources.append(tmp(left));
                sources.append(tmp(right));
            }  else if (isValidForm(opcode, Arg::ResCond, Arg::Tmp, Arg::Tmp)) {
                if (commutativity == Commutative && preferRightForResult(left, right)) {
                    sources.append(tmp(left));
                    append(Move, tmp(right), result);
                } else {
                    sources.append(tmp(right));
                    append(Move, tmp(left), result);
                }
            } else if (isValidForm(opcode, Arg::ResCond, Arg::Tmp, Arg::Tmp, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
                sources.append(tmp(left));
                sources.append(tmp(right));
                sources.append(m_code.newTmp(m_value->resultBank()));
                sources.append(m_code.newTmp(m_value->resultBank()));
            }

            // There is a really hilarious case that arises when we do BranchAdd32(%x, %x). We won't emit
            // such code, but the coalescing in our register allocator also does copy propagation, so
            // although we emit:
            //
            //     Move %tmp1, %tmp2
            //     BranchAdd32 %tmp1, %tmp2
            //
            // The register allocator may turn this into:
            //
            //     BranchAdd32 %rax, %rax
            //
            // Currently we handle this by ensuring that even this kind of addition can be undone. We can
            // undo it by using the carry flag. It's tempting to get rid of that code and just "fix" this
            // here by forcing LateUse on the stackmap. If we did that unconditionally, we'd lose a lot of
            // performance. So it's tempting to do it only if left == right. But that creates an awkward
            // constraint on Air: it means that Air would not be allowed to do any copy propagation.
            // Notice that the %rax,%rax situation happened after Air copy-propagated the Move we are
            // emitting. We know that copy-propagating over that Move causes add-to-self. But what if we
            // emit something like a Move - or even do other kinds of copy-propagation on tmp's -
            // somewhere else in this code. The add-to-self situation may only emerge after some other Air
            // optimizations remove other Move's or identity-like operations. That's why we don't use
            // LateUse here to take care of add-to-self.
            
            CheckSpecial* special = ensureCheckSpecial(opcode, 2 + sources.size(), stackmapRole);
            
            Inst inst(Patch, checkValue, Arg::special(special));

            inst.args.append(Arg::resCond(MacroAssembler::Overflow));

            inst.args.appendVector(sources);
            inst.args.append(result);

            fillStackmap(inst, checkValue, 2);

            m_insts.last().append(WTFMove(inst));
            return;
        }

        case Check: {
            Inst branch = createBranch(m_value->child(0));

            CheckSpecial* special = ensureCheckSpecial(branch);
            
            CheckValue* checkValue = m_value->as<CheckValue>();
            
            Inst inst(Patch, checkValue, Arg::special(special));
            inst.args.appendVector(branch.args);
            
            fillStackmap(inst, checkValue, 1);
            
            m_insts.last().append(WTFMove(inst));
            return;
        }

        case B3::WasmBoundsCheck: {
            WasmBoundsCheckValue* value = m_value->as<WasmBoundsCheckValue>();

            Value* ptr = value->child(0);
            Tmp pointer = tmp(ptr);

            Arg ptrPlusImm = m_code.newTmp(GP);
            append(Inst(Move32, value, pointer, ptrPlusImm));
            if (value->offset()) {
                if (imm(value->offset()))
                    append(Add64, imm(value->offset()), ptrPlusImm);
                else {
                    Arg bigImm = m_code.newTmp(GP);
                    append(Move, Arg::bigImm(value->offset()), bigImm);
                    append(Add64, bigImm, ptrPlusImm);
                }
            }

            Arg limit;
            switch (value->boundsType()) {
            case WasmBoundsCheckValue::Type::Pinned:
                limit = Arg(value->bounds().pinnedSize);
                break;

            case WasmBoundsCheckValue::Type::Maximum:
                limit = m_code.newTmp(GP);
                if (imm(value->bounds().maximum))
                    append(Move, imm(value->bounds().maximum), limit);
                else
                    append(Move, Arg::bigImm(value->bounds().maximum), limit);
                break;
            }

            append(Inst(Air::WasmBoundsCheck, value, ptrPlusImm, limit));
            return;
        }

        case Upsilon: {
            Value* value = m_value->child(0);
            Value* phi = m_value->as<UpsilonValue>()->phi();
            if (value->type().isNumeric()) {
                append(relaxedMoveForType(value->type()), immOrTmp(value), m_phiToTmp[phi]);
                return;
            }

            const Vector<Type>& tuple = m_procedure.tupleForType(value->type());
            const auto& valueTmps = tmpsForTuple(value);
            const auto& phiTmps = m_tuplePhiToTmps.find(phi)->value;
            ASSERT(valueTmps.size() == phiTmps.size());
            for (unsigned i = 0; i < valueTmps.size(); ++i)
                append(relaxedMoveForType(tuple[i]), valueTmps[i], phiTmps[i]);
            return;
        }

        case Phi: {
            // Snapshot the value of the Phi. It may change under us because you could do:
            // a = Phi()
            // Upsilon(@x, ^a)
            // @a => this should get the value of the Phi before the Upsilon, i.e. not @x.

            if (m_value->type().isNumeric()) {
                append(relaxedMoveForType(m_value->type()), m_phiToTmp[m_value], tmp(m_value));
                return;
            }

            const Vector<Type>& tuple = m_procedure.tupleForType(m_value->type());
            const auto& valueTmps = tmpsForTuple(m_value);
            const auto& phiTmps = m_tuplePhiToTmps.find(m_value)->value;
            ASSERT(valueTmps.size() == phiTmps.size());
            for (unsigned i = 0; i < valueTmps.size(); ++i)
                append(relaxedMoveForType(tuple[i]), phiTmps[i], valueTmps[i]);
            return;
        }

        case Set: {
            Value* value = m_value->child(0);
            const Vector<Tmp>& variableTmps = m_variableToTmps.get(m_value->as<VariableValue>()->variable());
            forEachImmOrTmp(value, [&] (Arg immOrTmp, Type type, unsigned index) {
                append(relaxedMoveForType(type), immOrTmp, variableTmps[index]);
            });
            return;
        }

        case Get: {
            // Snapshot the value of the Get. It may change under us because you could do:
            // a = Get(var)
            // Set(@x, var)
            // @a => this should get the value of the Get before the Set, i.e. not @x.

            const Vector<Tmp>& variableTmps = m_variableToTmps.get(m_value->as<VariableValue>()->variable());
            forEachImmOrTmp(m_value, [&] (Arg tmp, Type type, unsigned index) {
                append(relaxedMoveForType(type), variableTmps[index], tmp.tmp());
            });
            return;
        }

        case Branch: {
            if (canBeInternal(m_value->child(0))) {
                Value* branchChild = m_value->child(0);

                switch (branchChild->opcode()) {
                case BitAnd: {
                    Value* andValue = branchChild->child(0);
                    Value* andMask = branchChild->child(1);
                    Air::Opcode opcode = opcodeForType(BranchTestBit32, BranchTestBit64, andValue->type());

                    Value* testValue = nullptr;
                    Value* bitOffset = nullptr;
                    Value* internalNode = nullptr;
                    Value* negationNode = nullptr;
                    bool inverted = false;

                    // if (~(val >> x)&1)
                    if (andMask->isInt(1)
                        && andValue->opcode() == BitXor && (andValue->child(1)->isInt32(-1) || andValue->child(1)->isInt64(-1l))
                        && (andValue->child(0)->opcode() == SShr || andValue->child(0)->opcode() == ZShr)) {

                        negationNode = andValue;
                        testValue = andValue->child(0)->child(0);
                        bitOffset = andValue->child(0)->child(1);
                        internalNode = andValue->child(0);
                        inverted = !inverted;
                    }

                    // Turn if ((val >> x)&1) -> Bt val x
                    if (andMask->isInt(1) && (andValue->opcode() == SShr || andValue->opcode() == ZShr)) {
                        testValue = andValue->child(0);
                        bitOffset = andValue->child(1);
                        internalNode = andValue;
                    }

                    // Turn if (val & (1<<x)) -> Bt val x
                    if ((andMask->opcode() == Shl) && andMask->child(0)->isInt(1)) {
                        testValue = andValue;
                        bitOffset = andMask->child(1);
                        internalNode = andMask;
                    }

                    // if (~val & (1<<x)) or if ((~val >> x)&1)
                    if (!negationNode && testValue && testValue->opcode() == BitXor && (testValue->child(1)->isInt32(-1) || testValue->child(1)->isInt64(-1l))) {
                        negationNode = testValue;
                        testValue = testValue->child(0);
                        inverted = !inverted;
                    }

                    if (testValue && bitOffset) {
                        for (auto& basePromise : Vector<ArgPromise>::from(loadPromise(testValue), tmpPromise(testValue))) {
                            bool hasLoad = basePromise.kind() != Arg::Tmp;
                            bool canMakeInternal = (hasLoad ? canBeInternal(testValue) : !m_locked.contains(testValue))
                                && (!negationNode || canBeInternal(negationNode))
                                && (!internalNode || canBeInternal(internalNode));

                            if (basePromise && canMakeInternal) {
                                if (bitOffset->hasInt() && isValidForm(opcode, Arg::ResCond, basePromise.kind(), Arg::Imm)) {
                                    commitInternal(branchChild);
                                    commitInternal(internalNode);
                                    if (hasLoad)
                                        commitInternal(testValue);
                                    commitInternal(negationNode);
                                    append(basePromise.inst(opcode, m_value, Arg::resCond(MacroAssembler::NonZero).inverted(inverted), basePromise.consume(*this), Arg::imm(bitOffset->asInt())));
                                    return;
                                }

                                if (!m_locked.contains(bitOffset) && isValidForm(opcode, Arg::ResCond, basePromise.kind(), Arg::Tmp)) {
                                    commitInternal(branchChild);
                                    commitInternal(internalNode);
                                    if (hasLoad)
                                        commitInternal(testValue);
                                    commitInternal(negationNode);
                                    append(basePromise.inst(opcode, m_value, Arg::resCond(MacroAssembler::NonZero).inverted(inverted), basePromise.consume(*this), tmp(bitOffset)));
                                    return;
                                }
                            }
                        }
                    }
                    break;
                }
                case AtomicWeakCAS:
                    commitInternal(branchChild);
                    appendCAS(branchChild, false);
                    return;
                    
                case AtomicStrongCAS:
                    // A branch is a comparison to zero.
                    // FIXME: Teach this to match patterns that arise from subwidth CAS.
                    // https://bugs.webkit.org/show_bug.cgi?id=169250
                    if (branchChild->child(0)->isInt(0)
                        && branchChild->as<AtomicValue>()->isCanonicalWidth()) {
                        commitInternal(branchChild);
                        appendCAS(branchChild, true);
                        return;
                    }
                    break;
                    
                case Equal:
                case NotEqual:
                    // FIXME: Teach this to match patterns that arise from subwidth CAS.
                    // https://bugs.webkit.org/show_bug.cgi?id=169250
                    if (branchChild->child(0)->opcode() == AtomicStrongCAS
                        && branchChild->child(0)->as<AtomicValue>()->isCanonicalWidth()
                        && canBeInternal(branchChild->child(0))
                        && branchChild->child(0)->child(0) == branchChild->child(1)) {
                        commitInternal(branchChild);
                        commitInternal(branchChild->child(0));
                        appendCAS(branchChild->child(0), branchChild->opcode() == NotEqual);
                        return;
                    }
                    break;
                    
                default:
                    break;
                }
            }
            
            m_insts.last().append(createBranch(m_value->child(0)));
            return;
        }

        case B3::Jump: {
            append(Air::Jump);
            return;
        }
            
        case Identity:
        case Opaque: {
            ASSERT(tmp(m_value->child(0)) == tmp(m_value));
            return;
        }

        case Return: {
            if (!m_value->numChildren()) {
                append(RetVoid);
                return;
            }
            Value* value = m_value->child(0);
            Tmp returnValueGPR = Tmp(GPRInfo::returnValueGPR);
            Tmp returnValueFPR = Tmp(FPRInfo::returnValueFPR);
            switch (value->type().kind()) {
            case Void:
            case Tuple:
            case V128:
                // It's impossible for a void value to be used as a child. We use RetVoid
                // for void returns.
                RELEASE_ASSERT_NOT_REACHED();
                break;
            case Int32:
                append(Move, immOrTmp(value), returnValueGPR);
                append(Ret32, returnValueGPR);
                break;
            case Int64:
                append(Move, immOrTmp(value), returnValueGPR);
                append(Ret64, returnValueGPR);
                break;
            case Float:
                append(MoveFloat, tmp(value), returnValueFPR);
                append(RetFloat, returnValueFPR);
                break;
            case Double:
                append(MoveDouble, tmp(value), returnValueFPR);
                append(RetDouble, returnValueFPR);
                break;
            }
            return;
        }

        case B3::Oops: {
            append(Air::Oops);
            return;
        }
            
        case B3::EntrySwitch: {
            append(Air::EntrySwitch);
            return;
        }
            
        case AtomicWeakCAS:
        case AtomicStrongCAS: {
            appendCAS(m_value, false);
            return;
        }
            
        case AtomicXchgAdd: {
            AtomicValue* atomic = m_value->as<AtomicValue>();
            if (appendVoidAtomic(OPCODE_FOR_WIDTH(AtomicAdd, atomic->accessWidth())))
                return;
            
            Arg address = addr(atomic);
            Air::Opcode opcode = OPCODE_FOR_WIDTH(AtomicXchgAdd, atomic->accessWidth());
            if (isValidForm(opcode, Arg::Tmp, address.kind(), Arg::Tmp)) {
                appendTrapping(opcode, tmp(atomic->child(0)), address, tmp(atomic));
                return;
            }

            if (isValidForm(opcode, Arg::Tmp, address.kind())) {
                append(relaxedMoveForType(atomic->type()), tmp(atomic->child(0)), tmp(atomic));
                appendTrapping(opcode, tmp(atomic), address);
                return;
            }

            appendGeneralAtomic(OPCODE_FOR_CANONICAL_WIDTH(Add, atomic->accessWidth()), Commutative);
            return;
        }

        case AtomicXchgSub: {
            AtomicValue* atomic = m_value->as<AtomicValue>();
            if (appendVoidAtomic(OPCODE_FOR_WIDTH(AtomicSub, atomic->accessWidth())))
                return;
            
            appendGeneralAtomic(OPCODE_FOR_CANONICAL_WIDTH(Sub, atomic->accessWidth()));
            return;
        }
            
        case AtomicXchgAnd: {
            AtomicValue* atomic = m_value->as<AtomicValue>();
            if (appendVoidAtomic(OPCODE_FOR_WIDTH(AtomicAnd, atomic->accessWidth())))
                return;

            if (isARM64_LSE()) {
                Arg address = addr(atomic);
                Air::Opcode opcode = OPCODE_FOR_WIDTH(AtomicXchgClear, atomic->accessWidth());
                if (isValidForm(opcode, Arg::Tmp, address.kind(), Arg::Tmp)) {
                    append(OPCODE_FOR_CANONICAL_WIDTH(Not, atomic->accessWidth()), tmp(atomic->child(0)), tmp(atomic));
                    appendTrapping(opcode, tmp(atomic), address, tmp(atomic));
                    return;
                }
            }
            
            appendGeneralAtomic(OPCODE_FOR_CANONICAL_WIDTH(And, atomic->accessWidth()), Commutative);
            return;
        }
            
        case AtomicXchgOr: {
            AtomicValue* atomic = m_value->as<AtomicValue>();
            if (appendVoidAtomic(OPCODE_FOR_WIDTH(AtomicOr, atomic->accessWidth())))
                return;

            Arg address = addr(atomic);
            Air::Opcode opcode = OPCODE_FOR_WIDTH(AtomicXchgOr, atomic->accessWidth());
            if (isValidForm(opcode, Arg::Tmp, address.kind(), Arg::Tmp)) {
                appendTrapping(opcode, tmp(atomic->child(0)), address, tmp(atomic));
                return;
            }
            
            appendGeneralAtomic(OPCODE_FOR_CANONICAL_WIDTH(Or, atomic->accessWidth()), Commutative);
            return;
        }
            
        case AtomicXchgXor: {
            AtomicValue* atomic = m_value->as<AtomicValue>();
            if (appendVoidAtomic(OPCODE_FOR_WIDTH(AtomicXor, atomic->accessWidth())))
                return;

            Arg address = addr(atomic);
            Air::Opcode opcode = OPCODE_FOR_WIDTH(AtomicXchgXor, atomic->accessWidth());
            if (isValidForm(opcode, Arg::Tmp, address.kind(), Arg::Tmp)) {
                appendTrapping(opcode, tmp(atomic->child(0)), address, tmp(atomic));
                return;
            }
            
            appendGeneralAtomic(OPCODE_FOR_CANONICAL_WIDTH(Xor, atomic->accessWidth()), Commutative);
            return;
        }
            
        case AtomicXchg: {
            AtomicValue* atomic = m_value->as<AtomicValue>();
            
            Arg address = addr(atomic);
            Air::Opcode opcode = OPCODE_FOR_WIDTH(AtomicXchg, atomic->accessWidth());
            if (isValidForm(opcode, Arg::Tmp, address.kind(), Arg::Tmp)) {
                appendTrapping(opcode, tmp(atomic->child(0)), address, tmp(atomic));
                return;
            }

            if (isValidForm(opcode, Arg::Tmp, address.kind())) {
                append(relaxedMoveForType(atomic->type()), tmp(atomic->child(0)), tmp(atomic));
                appendTrapping(opcode, tmp(atomic), address);
                return;
            }
            
            appendGeneralAtomic(Air::Nop);
            return;
        }
            
        default:
            break;
        }

        dataLog("FATAL: could not lower ", deepDump(m_procedure, m_value), "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    IndexSet<Value*> m_locked; // These are values that will have no Tmp in Air.
    IndexMap<Value*, Tmp> m_valueToTmp; // These are values that must have a Tmp in Air. We say that a Value* with a non-null Tmp is "pinned".
    IndexMap<Value*, Tmp> m_phiToTmp; // Each Phi gets its own Tmp.
    HashMap<Value*, Vector<Tmp>> m_tupleValueToTmps; // This is the same as m_valueToTmp for Values that are Tuples.
    HashMap<Value*, Vector<Tmp>> m_tuplePhiToTmps; // This is the same as m_phiToTmp for Phis that are Tuples.
    IndexMap<B3::BasicBlock*, Air::BasicBlock*> m_blockToBlock;
    HashMap<Variable*, Vector<Tmp>> m_variableToTmps;

    UseCounts m_useCounts;
    PhiChildren m_phiChildren;
    BlockWorklist m_fastWorklist;
    Dominators& m_dominators;

    Vector<Vector<Inst, 4>> m_insts;
    Vector<Inst> m_prologue;
    
    B3::BasicBlock* m_block;
    bool m_isRare;
    unsigned m_index;
    Value* m_value;

    PatchpointSpecial* m_patchpointSpecial { nullptr };
    HashMap<CheckSpecial::Key, CheckSpecial*> m_checkSpecials;

    Procedure& m_procedure;
    Code& m_code;

    Air::BlockInsertionSet m_blockInsertionSet;

    Tmp m_eax;
    Tmp m_ecx;
    Tmp m_edx;
};

} // anonymous namespace

void lowerToAir(Procedure& procedure)
{
    PhaseScope phaseScope(procedure, "lowerToAir");
    LowerToAir lowerToAir(procedure);
    lowerToAir.run();
}

} } // namespace JSC::B3

#if !ASSERT_ENABLED
IGNORE_RETURN_TYPE_WARNINGS_END
#endif

#endif // ENABLE(B3_JIT)
