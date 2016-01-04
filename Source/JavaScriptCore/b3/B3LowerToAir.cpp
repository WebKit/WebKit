/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#include "AirCCallSpecial.h"
#include "AirCode.h"
#include "AirInsertionSet.h"
#include "AirInstInlines.h"
#include "AirStackSlot.h"
#include "B3ArgumentRegValue.h"
#include "B3BasicBlockInlines.h"
#include "B3CCallValue.h"
#include "B3CheckSpecial.h"
#include "B3Commutativity.h"
#include "B3IndexMap.h"
#include "B3IndexSet.h"
#include "B3MemoryValue.h"
#include "B3PatchpointSpecial.h"
#include "B3PatchpointValue.h"
#include "B3PhaseScope.h"
#include "B3PhiChildren.h"
#include "B3Procedure.h"
#include "B3StackSlotValue.h"
#include "B3UpsilonValue.h"
#include "B3UseCounts.h"
#include "B3ValueInlines.h"
#include <wtf/ListDump.h>

namespace JSC { namespace B3 {

using namespace Air;

namespace {

const bool verbose = false;

class LowerToAir {
public:
    LowerToAir(Procedure& procedure)
        : m_valueToTmp(procedure.values().size())
        , m_phiToTmp(procedure.values().size())
        , m_blockToBlock(procedure.size())
        , m_useCounts(procedure)
        , m_phiChildren(procedure)
        , m_procedure(procedure)
        , m_code(procedure.code())
    {
    }

    void run()
    {
        for (B3::BasicBlock* block : m_procedure)
            m_blockToBlock[block] = m_code.addBlock(block->frequency());
        
        for (Value* value : m_procedure.values()) {
            switch (value->opcode()) {
            case Phi: {
                m_phiToTmp[value] = m_code.newTmp(Arg::typeForB3Type(value->type()));
                break;
            }
            case B3::StackSlot: {
                StackSlotValue* stackSlotValue = value->as<StackSlotValue>();
                m_stackToStack.add(stackSlotValue, m_code.addStackSlot(stackSlotValue));
                break;
            }
            default:
                break;
            }
        }

        m_procedure.resetValueOwners(); // Used by crossesInterference().

        // Lower defs before uses on a global level. This is a good heuristic to lock down a
        // hoisted address expression before we duplicate it back into the loop.
        for (B3::BasicBlock* block : m_procedure.blocksInPreOrder()) {
            m_block = block;
            // Reset some state.
            m_insts.resize(0);

            if (verbose)
                dataLog("Lowering Block ", *block, ":\n");
            
            // Process blocks in reverse order so we see uses before defs. That's what allows us
            // to match patterns effectively.
            for (unsigned i = block->size(); i--;) {
                m_index = i;
                m_value = block->at(i);
                if (m_locked.contains(m_value))
                    continue;
                m_insts.append(Vector<Inst>());
                if (verbose)
                    dataLog("Lowering ", deepDump(m_procedure, m_value), ":\n");
                lower();
                if (verbose) {
                    for (Inst& inst : m_insts.last())
                        dataLog("    ", inst, "\n");
                }
            }

            // Now append the instructions. m_insts contains them in reverse order, so we process
            // it in reverse.
            for (unsigned i = m_insts.size(); i--;) {
                for (Inst& inst : m_insts[i])
                    m_blockToBlock[block]->appendInst(WTFMove(inst));
            }

            // Make sure that the successors are set up correctly.
            ASSERT(block->successors().size() <= 2);
            for (B3::FrequentedBlock successor : block->successors()) {
                m_blockToBlock[block]->successors().append(
                    Air::FrequentedBlock(m_blockToBlock[successor.block()], successor.frequency()));
            }
        }

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
            return true;
        default:
            return false;
        }
    }

    class ArgPromise {
    public:
        ArgPromise() { }

        ArgPromise(const Arg& arg, Value* valueToLock = nullptr)
            : m_arg(arg)
            , m_value(valueToLock)
        {
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

        Arg consume(LowerToAir& lower) const
        {
            if (!m_arg && m_value)
                return lower.tmp(m_value);
            if (m_value)
                lower.commitInternal(m_value);
            return m_arg;
        }
        
    private:
        // Three forms:
        // Everything null: invalid.
        // Arg non-null, value null: just use the arg, nothing special.
        // Arg null, value non-null: it's a tmp, pin it when necessary.
        // Arg non-null, value non-null: use the arg, lock the value.
        Arg m_arg;
        Value* m_value;
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
    // it's sure it will use the tmp. That's deliberate.
    //
    //     ArgPromise promise = tmpPromise(bar);
    //     if (isValidForm(Foo, promise.kind()))
    //         append(Foo, promise.consume(*this))
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
    //     auto tryThings = [this] (const ArgPromise& left, const ArgPromise& right) {
    //         if (isValidForm(Foo, left.kind(), right.kind()))
    //             return Inst(Foo, m_value, left.consume(*this), right.consume(*this));
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
            Tmp& realTmp = m_valueToTmp[value];
            if (!realTmp) {
                realTmp = m_code.newTmp(Arg::typeForB3Type(value->type()));
                if (m_procedure.isFastConstant(value->key()))
                    m_code.addFastTmp(realTmp);
            }
            tmp = realTmp;
        }
        return tmp;
    }

    ArgPromise tmpPromise(Value* value)
    {
        return ArgPromise::tmp(value);
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

    // This turns the given operand into an address.
    Arg effectiveAddr(Value* address, int32_t offset, Arg::Width width)
    {
        // B3 allows any memory operation to have a 32-bit offset. That's not how some architectures
        // work. We solve this by requiring a just-before-lowering phase that legalizes offsets.
        // FIXME: Implement such a legalization phase.
        // https://bugs.webkit.org/show_bug.cgi?id=152530
        ASSERT(Arg::isValidAddrForm(offset));

        auto fallback = [&] () -> Arg {
            return Arg::addr(tmp(address), offset);
        };
        
        static const unsigned lotsOfUses = 10; // This is arbitrary and we should tune it eventually.

        // Only match if the address value isn't used in some large number of places.
        if (m_useCounts.numUses(address) > lotsOfUses)
            return fallback();
        
        switch (address->opcode()) {
        case Add: {
            Value* left = address->child(0);
            Value* right = address->child(1);

            auto tryIndex = [&] (Value* index, Value* base) -> Arg {
                if (index->opcode() != Shl)
                    return Arg();
                if (m_locked.contains(index->child(0)) || m_locked.contains(base))
                    return Arg();
                if (!index->child(1)->hasInt32())
                    return Arg();
                
                unsigned scale = 1 << (index->child(1)->asInt32() & 31);
                if (!Arg::isValidIndexForm(scale, offset, width))
                    return Arg();

                return Arg::index(tmp(base), tmp(index->child(0)), scale, offset);
            };

            if (Arg result = tryIndex(left, right))
                return result;
            if (Arg result = tryIndex(right, left))
                return result;

            if (m_locked.contains(left) || m_locked.contains(right)
                || !Arg::isValidIndexForm(1, offset, width))
                return fallback();
            
            return Arg::index(tmp(left), tmp(right), 1, offset);
        }

        case Shl: {
            Value* left = address->child(0);

            // We'll never see child(1)->isInt32(0), since that would have been reduced. If the shift
            // amount is greater than 1, then there isn't really anything smart that we could do here.
            // We avoid using baseless indexes because their encoding isn't particularly efficient.
            if (m_locked.contains(left) || !address->child(1)->isInt32(1)
                || !Arg::isValidIndexForm(1, offset, width))
                return fallback();

            return Arg::index(tmp(left), tmp(left), 1, offset);
        }

        case FramePointer:
            return Arg::addr(Tmp(GPRInfo::callFrameRegister), offset);

        case B3::StackSlot:
            return Arg::stack(m_stackToStack.get(address->as<StackSlotValue>()), offset);

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

        int32_t offset = value->offset();
        Arg::Width width = Arg::widthForBytes(value->accessByteSize());

        Arg result = effectiveAddr(value->lastChild(), offset, width);
        ASSERT(result.isValidForm(width));

        return result;
    }

    ArgPromise loadPromise(Value* loadValue, B3::Opcode loadOpcode)
    {
        if (loadValue->opcode() != loadOpcode)
            return Arg();
        if (!canBeInternal(loadValue))
            return Arg();
        if (crossesInterference(loadValue))
            return Arg();
        return ArgPromise(addr(loadValue), loadValue);
    }

    ArgPromise loadPromise(Value* loadValue)
    {
        return loadPromise(loadValue, Load);
    }

    Arg imm(Value* value)
    {
        if (value->hasInt()) {
            int64_t intValue = value->asInt();
            if (Arg::isValidImmForm(intValue))
                return Arg::imm(intValue);
        }
        return Arg();
    }

    Arg immOrTmp(Value* value)
    {
        if (Arg result = imm(value))
            return result;
        return tmp(value);
    }

    // By convention, we use Oops to mean "I don't know".
    Air::Opcode tryOpcodeForType(
        Air::Opcode opcode32, Air::Opcode opcode64, Air::Opcode opcodeDouble, Air::Opcode opcodeFloat, Type type)
    {
        Air::Opcode opcode;
        switch (type) {
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
            append(opcode, addr.consume(*this), result);
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

        bool result = m_useCounts.numUsingInstructions(right) == 1;
        
        // We recognize when a child is a Phi that has this value as one of its children. We're very
        // conservative about this; for example we don't even consider transitive Phi children.
        bool leftIsPhiWithThis = m_phiChildren[left].transitivelyUses(m_value);
        bool rightIsPhiWithThis = m_phiChildren[right].transitivelyUses(m_value);
        
        if (leftIsPhiWithThis != rightIsPhiWithThis)
            result = rightIsPhiWithThis;

        return result;
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
            if (commutativity == Commutative) {
                ArgPromise leftAddr = loadPromise(left);
                if (isValidForm(opcode, leftAddr.kind(), Arg::Tmp)) {
                    append(relaxedMoveForType(m_value->type()), tmp(right), result);
                    append(opcode, leftAddr.consume(*this), result);
                    return;
                }
            }

            ArgPromise rightAddr = loadPromise(right);
            if (isValidForm(opcode, rightAddr.kind(), Arg::Tmp)) {
                append(relaxedMoveForType(m_value->type()), tmp(left), result);
                append(opcode, rightAddr.consume(*this), result);
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

#if CPU(X86) || CPU(X86_64)
        append(Move, tmp(value), tmp(m_value));
        append(Move, tmp(amount), Tmp(X86Registers::ecx));
        append(opcode, Tmp(X86Registers::ecx), tmp(m_value));
#endif
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
        append(opcode, storeAddr);
        return true;
    }

    template<
        Air::Opcode opcode32, Air::Opcode opcode64, Commutativity commutativity = NotCommutative>
    bool tryAppendStoreBinOp(Value* left, Value* right)
    {
        Air::Opcode opcode = tryOpcodeForType(opcode32, opcode64, left->type());
        if (opcode == Air::Oops)
            return false;
        
        Arg storeAddr = addr(m_value);
        ASSERT(storeAddr);

        ArgPromise loadPromise;
        Value* otherValue = nullptr;
        
        loadPromise = this->loadPromise(left);
        if (loadPromise.peek() == storeAddr)
            otherValue = right;
        else if (commutativity == Commutative) {
            loadPromise = this->loadPromise(right);
            if (loadPromise.peek() == storeAddr)
                otherValue = left;
        }

        if (!otherValue)
            return false;

        if (isValidForm(opcode, Arg::Imm, storeAddr.kind()) && imm(otherValue)) {
            loadPromise.consume(*this);
            append(opcode, imm(otherValue), storeAddr);
            return true;
        }

        if (!isValidForm(opcode, Arg::Tmp, storeAddr.kind()))
            return false;

        loadPromise.consume(*this);
        append(opcode, tmp(otherValue), storeAddr);
        return true;
    }

    Inst createStore(Air::Opcode move, Value* value, const Arg& dest)
    {
        if (imm(value) && isValidForm(move, Arg::Imm, dest.kind()))
            return Inst(move, m_value, imm(value), dest);

        return Inst(move, m_value, tmp(value), dest);
    }

    Inst createStore(Value* value, const Arg& dest)
    {
        Air::Opcode moveOpcode = moveForType(value->type());
        return createStore(moveOpcode, value, dest);
    }

    void appendStore(Value* value, const Arg& dest)
    {
        m_insts.last().append(createStore(value, dest));
    }

    Air::Opcode moveForType(Type type)
    {
        switch (type) {
        case Int32:
            return Move32;
        case Int64:
            RELEASE_ASSERT(is64Bit());
            return Move;
        case Float:
            return MoveFloat;
        case Double:
            return MoveDouble;
        case Void:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return Air::Oops;
    }

    Air::Opcode relaxedMoveForType(Type type)
    {
        switch (type) {
        case Int32:
        case Int64:
            return Move;
        case Float:
        case Double:
            return MoveDouble;
        case Void:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return Air::Oops;
    }

    template<typename... Arguments>
    void append(Air::Opcode opcode, Arguments&&... arguments)
    {
        m_insts.last().append(Inst(opcode, m_value, std::forward<Arguments>(arguments)...));
    }

    template<typename T, typename... Arguments>
    T* ensureSpecial(T*& field, Arguments&&... arguments)
    {
        if (!field) {
            field = static_cast<T*>(
                m_code.addSpecial(std::make_unique<T>(std::forward<Arguments>(arguments)...)));
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
                    arg = Arg::imm64(value.value()->asInt64());
                else if (value.value()->hasDouble() && canBeInternal(value.value())) {
                    commitInternal(value.value());
                    arg = Arg::imm64(bitwise_cast<int64_t>(value.value()->asDouble()));
                } else
                    arg = tmp(value.value());
                break;
            case ValueRep::SomeRegister:
                arg = tmp(value.value());
                break;
            case ValueRep::Register:
                arg = Tmp(value.rep().reg());
                append(Move, immOrTmp(value.value()), arg);
                break;
            case ValueRep::StackArgument:
                arg = Arg::callArg(value.rep().offsetFromSP());
                appendStore(value.value(), arg);
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
        const CompareFunctor& compare, // Signature: (Arg::Width, Arg relCond, Arg, Arg) -> Inst
        const TestFunctor& test, // Signature: (Arg::Width, Arg resCond, Arg, Arg) -> Inst
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
            // have to go through the sharing path as well.
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
                (value->opcode() == BitXor && value->child(1)->hasInt() && (value->child(1)->asInt() & 1) && value->child(0)->returnsBool())
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

            if (isInt(value->child(0)->type())) {
                // FIXME: We wouldn't have to worry about leftImm if we canonicalized integer
                // comparisons.
                // https://bugs.webkit.org/show_bug.cgi?id=150958
                
                Arg leftImm = imm(left);
                Arg rightImm = imm(right);

                auto tryCompare = [&] (
                    Arg::Width width, const ArgPromise& left, const ArgPromise& right) -> Inst {
                    if (Inst result = compare(width, relCond, left, right))
                        return result;
                    if (Inst result = compare(width, relCond.flipped(), right, left))
                        return result;
                    return Inst();
                };

                auto tryCompareLoadImm = [&] (
                    Arg::Width width, B3::Opcode loadOpcode, Arg::Signedness signedness) -> Inst {
                    if (rightImm && rightImm.isRepresentableAs(width, signedness)) {
                        if (Inst result = tryCompare(width, loadPromise(left, loadOpcode), rightImm)) {
                            commitInternal(left);
                            return result;
                        }
                    }
                    if (leftImm && leftImm.isRepresentableAs(width, signedness)) {
                        if (Inst result = tryCompare(width, leftImm, loadPromise(right, loadOpcode))) {
                            commitInternal(right);
                            return result;
                        }
                    }
                    return Inst();
                };

                Arg::Width width = Arg::widthForB3Type(value->child(0)->type());
                
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
                        if (Inst result = tryCompareLoadImm(Arg::Width8, Load8S, Arg::Signed))
                            return result;
                    }
                
                    if (relCond.isUnsignedCond()) {
                        if (Inst result = tryCompareLoadImm(Arg::Width8, Load8Z, Arg::Unsigned))
                            return result;
                    }

                    if (relCond.isSignedCond()) {
                        if (Inst result = tryCompareLoadImm(Arg::Width16, Load16S, Arg::Signed))
                            return result;
                    }
                
                    if (relCond.isUnsignedCond()) {
                        if (Inst result = tryCompareLoadImm(Arg::Width16, Load16Z, Arg::Unsigned))
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
                
                if (leftImm && leftImm.isRepresentableAs<int32_t>()) {
                    if (Inst result = tryCompare(width, leftImm, tmpPromise(right)))
                        return result;
                }
                
                if (rightImm && rightImm.isRepresentableAs<int32_t>()) {
                    if (Inst result = tryCompare(width, tmpPromise(left), rightImm))
                        return result;
                }

                // Finally, handle comparison between tmps.
                return compare(width, relCond, tmpPromise(left), tmpPromise(right));
            }

            // Floating point comparisons can't really do anything smart.
            if (value->child(0)->type() == Float)
                return compareFloat(doubleCond, tmpPromise(left), tmpPromise(right));
            return compareDouble(doubleCond, tmpPromise(left), tmpPromise(right));
        };

        Arg::Width width = Arg::widthForB3Type(value->type());
        Arg resCond = Arg::resCond(MacroAssembler::NonZero).inverted(inverted);
        
        auto tryTest = [&] (
            Arg::Width width, const ArgPromise& left, const ArgPromise& right) -> Inst {
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
                return createRelCond(MacroAssembler::Equal, MacroAssembler::DoubleEqual);
            case LessThan:
                return createRelCond(MacroAssembler::LessThan, MacroAssembler::DoubleLessThan);
            case GreaterThan:
                return createRelCond(MacroAssembler::GreaterThan, MacroAssembler::DoubleGreaterThan);
            case LessEqual:
                return createRelCond(MacroAssembler::LessThanOrEqual, MacroAssembler::DoubleLessThanOrEqual);
            case GreaterEqual:
                return createRelCond(MacroAssembler::GreaterThanOrEqual, MacroAssembler::DoubleGreaterThanOrEqual);
            case EqualOrUnordered:
                // The integer condition is never used in this case.
                return createRelCond(MacroAssembler::Equal, MacroAssembler::DoubleEqualOrUnordered);
            case Above:
                // We use a bogus double condition because these integer comparisons won't got down that
                // path anyway.
                return createRelCond(MacroAssembler::Above, MacroAssembler::DoubleEqual);
            case Below:
                return createRelCond(MacroAssembler::Below, MacroAssembler::DoubleEqual);
            case AboveEqual:
                return createRelCond(MacroAssembler::AboveOrEqual, MacroAssembler::DoubleEqual);
            case BelowEqual:
                return createRelCond(MacroAssembler::BelowOrEqual, MacroAssembler::DoubleEqual);
            case BitAnd: {
                Value* left = value->child(0);
                Value* right = value->child(1);

                // FIXME: We don't actually have to worry about leftImm.
                // https://bugs.webkit.org/show_bug.cgi?id=150954

                Arg leftImm = imm(left);
                Arg rightImm = imm(right);
                
                auto tryTestLoadImm = [&] (Arg::Width width, B3::Opcode loadOpcode) -> Inst {
                    if (rightImm && rightImm.isRepresentableAs(width, Arg::Unsigned)) {
                        if (Inst result = tryTest(width, loadPromise(left, loadOpcode), rightImm)) {
                            commitInternal(left);
                            return result;
                        }
                    }
                    if (leftImm && leftImm.isRepresentableAs(width, Arg::Unsigned)) {
                        if (Inst result = tryTest(width, leftImm, loadPromise(right, loadOpcode))) {
                            commitInternal(right);
                            return result;
                        }
                    }
                    return Inst();
                };

                if (canCommitInternal) {
                    // First handle test's that involve fewer bits than B3's type system supports.

                    if (Inst result = tryTestLoadImm(Arg::Width8, Load8Z))
                        return result;
                    
                    if (Inst result = tryTestLoadImm(Arg::Width8, Load8S))
                        return result;
                    
                    if (Inst result = tryTestLoadImm(Arg::Width16, Load16Z))
                        return result;
                    
                    if (Inst result = tryTestLoadImm(Arg::Width16, Load16S))
                        return result;

                    // Now handle test's that involve a load and an immediate. Note that immediates
                    // are 32-bit, and we want zero-extension. Hence, the immediate form is compiled
                    // as a 32-bit test. Note that this spits on the grave of inferior endians, such
                    // as the big one.
                    
                    if (Inst result = tryTestLoadImm(Arg::Width32, Load))
                        return result;
                    
                    // Now handle test's that involve a load.
                    
                    Arg::Width width = Arg::widthForB3Type(value->child(0)->type());
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

                if (leftImm && leftImm.isRepresentableAs<uint32_t>()) {
                    if (Inst result = tryTest(Arg::Width32, leftImm, tmpPromise(right)))
                        return result;
                }

                if (rightImm && rightImm.isRepresentableAs<uint32_t>()) {
                    if (Inst result = tryTest(Arg::Width32, tmpPromise(left), rightImm))
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

        if (canCommitInternal && value->as<MemoryValue>()) {
            // Handle things like Branch(Load8Z(value))

            if (Inst result = tryTest(Arg::Width8, loadPromise(value, Load8Z), Arg::imm(-1))) {
                commitInternal(value);
                return result;
            }

            if (Inst result = tryTest(Arg::Width8, loadPromise(value, Load8S), Arg::imm(-1))) {
                commitInternal(value);
                return result;
            }

            if (Inst result = tryTest(Arg::Width16, loadPromise(value, Load16Z), Arg::imm(-1))) {
                commitInternal(value);
                return result;
            }

            if (Inst result = tryTest(Arg::Width16, loadPromise(value, Load16S), Arg::imm(-1))) {
                commitInternal(value);
                return result;
            }

            if (Inst result = tryTest(width, loadPromise(value), Arg::imm(-1))) {
                commitInternal(value);
                return result;
            }
        }

        if (Inst result = test(width, resCond, tmpPromise(value), Arg::imm(-1)))
            return result;
        
        // Sometimes this is the only form of test available. We prefer not to use this because
        // it's less canonical.
        return test(width, resCond, tmpPromise(value), tmpPromise(value));
    }

    Inst createBranch(Value* value, bool inverted = false)
    {
        return createGenericCompare(
            value,
            [this] (
                Arg::Width width, const Arg& relCond,
                const ArgPromise& left, const ArgPromise& right) -> Inst {
                switch (width) {
                case Arg::Width8:
                    if (isValidForm(Branch8, Arg::RelCond, left.kind(), right.kind())) {
                        return Inst(
                            Branch8, m_value, relCond,
                            left.consume(*this), right.consume(*this));
                    }
                    return Inst();
                case Arg::Width16:
                    return Inst();
                case Arg::Width32:
                    if (isValidForm(Branch32, Arg::RelCond, left.kind(), right.kind())) {
                        return Inst(
                            Branch32, m_value, relCond,
                            left.consume(*this), right.consume(*this));
                    }
                    return Inst();
                case Arg::Width64:
                    if (isValidForm(Branch64, Arg::RelCond, left.kind(), right.kind())) {
                        return Inst(
                            Branch64, m_value, relCond,
                            left.consume(*this), right.consume(*this));
                    }
                    return Inst();
                }
            },
            [this] (
                Arg::Width width, const Arg& resCond,
                const ArgPromise& left, const ArgPromise& right) -> Inst {
                switch (width) {
                case Arg::Width8:
                    if (isValidForm(BranchTest8, Arg::ResCond, left.kind(), right.kind())) {
                        return Inst(
                            BranchTest8, m_value, resCond,
                            left.consume(*this), right.consume(*this));
                    }
                    return Inst();
                case Arg::Width16:
                    return Inst();
                case Arg::Width32:
                    if (isValidForm(BranchTest32, Arg::ResCond, left.kind(), right.kind())) {
                        return Inst(
                            BranchTest32, m_value, resCond,
                            left.consume(*this), right.consume(*this));
                    }
                    return Inst();
                case Arg::Width64:
                    if (isValidForm(BranchTest64, Arg::ResCond, left.kind(), right.kind())) {
                        return Inst(
                            BranchTest64, m_value, resCond,
                            left.consume(*this), right.consume(*this));
                    }
                    return Inst();
                }
            },
            [this] (Arg doubleCond, const ArgPromise& left, const ArgPromise& right) -> Inst {
                if (isValidForm(BranchDouble, Arg::DoubleCond, left.kind(), right.kind())) {
                    return Inst(
                        BranchDouble, m_value, doubleCond,
                        left.consume(*this), right.consume(*this));
                }
                return Inst();
            },
            [this] (Arg doubleCond, const ArgPromise& left, const ArgPromise& right) -> Inst {
                if (isValidForm(BranchFloat, Arg::DoubleCond, left.kind(), right.kind())) {
                    return Inst(
                        BranchFloat, m_value, doubleCond,
                        left.consume(*this), right.consume(*this));
                }
                return Inst();
            },
            inverted);
    }

    Inst createCompare(Value* value, bool inverted = false)
    {
        return createGenericCompare(
            value,
            [this] (
                Arg::Width width, const Arg& relCond,
                const ArgPromise& left, const ArgPromise& right) -> Inst {
                switch (width) {
                case Arg::Width8:
                case Arg::Width16:
                    return Inst();
                case Arg::Width32:
                    if (isValidForm(Compare32, Arg::RelCond, left.kind(), right.kind(), Arg::Tmp)) {
                        return Inst(
                            Compare32, m_value, relCond,
                            left.consume(*this), right.consume(*this), tmp(m_value));
                    }
                    return Inst();
                case Arg::Width64:
                    if (isValidForm(Compare64, Arg::RelCond, left.kind(), right.kind(), Arg::Tmp)) {
                        return Inst(
                            Compare64, m_value, relCond,
                            left.consume(*this), right.consume(*this), tmp(m_value));
                    }
                    return Inst();
                }
            },
            [this] (
                Arg::Width width, const Arg& resCond,
                const ArgPromise& left, const ArgPromise& right) -> Inst {
                switch (width) {
                case Arg::Width8:
                case Arg::Width16:
                    return Inst();
                case Arg::Width32:
                    if (isValidForm(Test32, Arg::ResCond, left.kind(), right.kind(), Arg::Tmp)) {
                        return Inst(
                            Test32, m_value, resCond,
                            left.consume(*this), right.consume(*this), tmp(m_value));
                    }
                    return Inst();
                case Arg::Width64:
                    if (isValidForm(Test64, Arg::ResCond, left.kind(), right.kind(), Arg::Tmp)) {
                        return Inst(
                            Test64, m_value, resCond,
                            left.consume(*this), right.consume(*this), tmp(m_value));
                    }
                    return Inst();
                }
            },
            [this] (const Arg& doubleCond, const ArgPromise& left, const ArgPromise& right) -> Inst {
                if (isValidForm(CompareDouble, Arg::DoubleCond, left.kind(), right.kind(), Arg::Tmp)) {
                    return Inst(
                        CompareDouble, m_value, doubleCond,
                        left.consume(*this), right.consume(*this), tmp(m_value));
                }
                return Inst();
            },
            [this] (const Arg& doubleCond, const ArgPromise& left, const ArgPromise& right) -> Inst {
                if (isValidForm(CompareFloat, Arg::DoubleCond, left.kind(), right.kind(), Arg::Tmp)) {
                    return Inst(
                        CompareFloat, m_value, doubleCond,
                        left.consume(*this), right.consume(*this), tmp(m_value));
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
        Tmp source;
        Tmp destination;
    };
    Inst createSelect(Value* value, const MoveConditionallyConfig& config, bool inverted = false)
    {
        return createGenericCompare(
            value,
            [&] (
                Arg::Width width, const Arg& relCond,
                const ArgPromise& left, const ArgPromise& right) -> Inst {
                switch (width) {
                case Arg::Width8:
                    // FIXME: Support these things.
                    // https://bugs.webkit.org/show_bug.cgi?id=151504
                    return Inst();
                case Arg::Width16:
                    return Inst();
                case Arg::Width32:
                    if (isValidForm(config.moveConditionally32, Arg::RelCond, left.kind(), right.kind(), Arg::Tmp, Arg::Tmp)) {
                        return Inst(
                            config.moveConditionally32, m_value, relCond,
                            left.consume(*this), right.consume(*this), config.source, config.destination);
                    }
                    return Inst();
                case Arg::Width64:
                    if (isValidForm(config.moveConditionally64, Arg::RelCond, left.kind(), right.kind(), Arg::Tmp, Arg::Tmp)) {
                        return Inst(
                            config.moveConditionally64, m_value, relCond,
                            left.consume(*this), right.consume(*this), config.source, config.destination);
                    }
                    return Inst();
                }
            },
            [&] (
                Arg::Width width, const Arg& resCond,
                const ArgPromise& left, const ArgPromise& right) -> Inst {
                switch (width) {
                case Arg::Width8:
                    // FIXME: Support more things.
                    // https://bugs.webkit.org/show_bug.cgi?id=151504
                    return Inst();
                case Arg::Width16:
                    return Inst();
                case Arg::Width32:
                    if (isValidForm(config.moveConditionallyTest32, Arg::ResCond, left.kind(), right.kind(), Arg::Tmp, Arg::Tmp)) {
                        return Inst(
                            config.moveConditionallyTest32, m_value, resCond,
                            left.consume(*this), right.consume(*this), config.source, config.destination);
                    }
                    return Inst();
                case Arg::Width64:
                    if (isValidForm(config.moveConditionallyTest64, Arg::ResCond, left.kind(), right.kind(), Arg::Tmp, Arg::Tmp)) {
                        return Inst(
                            config.moveConditionallyTest64, m_value, resCond,
                            left.consume(*this), right.consume(*this), config.source, config.destination);
                    }
                    return Inst();
                }
            },
            [&] (Arg doubleCond, const ArgPromise& left, const ArgPromise& right) -> Inst {
                if (isValidForm(config.moveConditionallyDouble, Arg::DoubleCond, left.kind(), right.kind(), Arg::Tmp, Arg::Tmp)) {
                    return Inst(
                        config.moveConditionallyDouble, m_value, doubleCond,
                        left.consume(*this), right.consume(*this), config.source, config.destination);
                }
                return Inst();
            },
            [&] (Arg doubleCond, const ArgPromise& left, const ArgPromise& right) -> Inst {
                if (isValidForm(config.moveConditionallyFloat, Arg::DoubleCond, left.kind(), right.kind(), Arg::Tmp, Arg::Tmp)) {
                    return Inst(
                        config.moveConditionallyFloat, m_value, doubleCond,
                        left.consume(*this), right.consume(*this), config.source, config.destination);
                }
                return Inst();
            },
            inverted);
    }

    template<typename BankInfo>
    Arg marshallCCallArgument(unsigned& argumentCount, unsigned& stackCount, Value* child)
    {
        unsigned argumentIndex = argumentCount++;
        if (argumentIndex < BankInfo::numberOfArgumentRegisters) {
            Tmp result = Tmp(BankInfo::toArgumentRegister(argumentIndex));
            append(relaxedMoveForType(child->type()), immOrTmp(child), result);
            return result;
        }

        // Compute the place that this goes onto the stack. On X86_64 and probably other calling
        // conventions that don't involve obsolete computers and operating systems, sub-pointer-size
        // arguments are still given a full pointer-sized stack slot. Hence we don't have to consider
        // the type of the argument when deducing the stack index.
        unsigned stackIndex = stackCount++;

        Arg result = Arg::callArg(stackIndex * sizeof(void*));
        
        // Put the code for storing the argument before anything else. This significantly eases the
        // burden on the register allocator. If we could, we'd hoist these stores as far as
        // possible.
        // FIXME: Add a phase to hoist stores as high as possible to relieve register pressure.
        // https://bugs.webkit.org/show_bug.cgi?id=151063
        m_insts.last().insert(0, createStore(child, result));
        
        return result;
    }

    void lower()
    {
        switch (m_value->opcode()) {
        case B3::Nop: {
            // Yes, we will totally see Nop's because some phases will replaceWithNop() instead of
            // properly removing things.
            return;
        }
            
        case Load: {
            append(
                moveForType(m_value->type()),
                addr(m_value), tmp(m_value));
            return;
        }
            
        case Load8S: {
            append(Load8SignedExtendTo32, addr(m_value), tmp(m_value));
            return;
        }

        case Load8Z: {
            append(Load8, addr(m_value), tmp(m_value));
            return;
        }

        case Load16S: {
            append(Load16SignedExtendTo32, addr(m_value), tmp(m_value));
            return;
        }

        case Load16Z: {
            append(Load16, addr(m_value), tmp(m_value));
            return;
        }

        case Add: {
            appendBinOp<Add32, Add64, AddDouble, AddFloat, Commutative>(
                m_value->child(0), m_value->child(1));
            return;
        }

        case Sub: {
            if (m_value->child(0)->isInt(0))
                appendUnOp<Neg32, Neg64>(m_value->child(1));
            else
                appendBinOp<Sub32, Sub64, SubDouble, SubFloat>(m_value->child(0), m_value->child(1));
            return;
        }

        case Mul: {
            appendBinOp<Mul32, Mul64, MulDouble, MulFloat, Commutative>(
                m_value->child(0), m_value->child(1));
            return;
        }

        case Div: {
            if (isInt(m_value->type())) {
#if CPU(X86) || CPU(X86_64)
                lowerX86Div();
                append(Move, Tmp(X86Registers::eax), tmp(m_value));
#endif
                return;
            }
            ASSERT(isFloat(m_value->type()));

            appendBinOp<Air::Oops, Air::Oops, DivDouble, DivFloat>(m_value->child(0), m_value->child(1));
            return;
        }

        case Mod: {
#if CPU(X86) || CPU(X86_64)
            lowerX86Div();
            append(Move, Tmp(X86Registers::edx), tmp(m_value));
#endif
            return;
        }

        case BitAnd: {
            if (m_value->child(1)->isInt(0xff)) {
                appendUnOp<ZeroExtend8To32, ZeroExtend8To32>(m_value->child(0));
                return;
            }
            
            if (m_value->child(1)->isInt(0xffff)) {
                appendUnOp<ZeroExtend16To32, ZeroExtend16To32>(m_value->child(0));
                return;
            }

            if (m_value->child(1)->isInt(0xffffffff)) {
                appendUnOp<Move32, Move32>(m_value->child(0));
                return;
            }
            
            appendBinOp<And32, And64, AndDouble, AndFloat, Commutative>(
                m_value->child(0), m_value->child(1));
            return;
        }

        case BitOr: {
            appendBinOp<Or32, Or64, Commutative>(
                m_value->child(0), m_value->child(1));
            return;
        }

        case BitXor: {
            // FIXME: If canBeInternal(child), we should generate this using the comparison path.
            // https://bugs.webkit.org/show_bug.cgi?id=152367
            
            if (m_value->child(1)->isInt(-1)) {
                appendUnOp<Not32, Not64>(m_value->child(0));
                return;
            }
            appendBinOp<Xor32, Xor64, Commutative>(
                m_value->child(0), m_value->child(1));
            return;
        }

        case Shl: {
            if (m_value->child(1)->isInt32(1)) {
                appendBinOp<Add32, Add64, AddDouble, AddFloat, Commutative>(m_value->child(0), m_value->child(0));
                return;
            }
            
            appendShift<Lshift32, Lshift64>(m_value->child(0), m_value->child(1));
            return;
        }

        case SShr: {
            appendShift<Rshift32, Rshift64>(m_value->child(0), m_value->child(1));
            return;
        }

        case ZShr: {
            appendShift<Urshift32, Urshift64>(m_value->child(0), m_value->child(1));
            return;
        }

        case Clz: {
            appendUnOp<CountLeadingZeros32, CountLeadingZeros64>(m_value->child(0));
            return;
        }

        case Ceil: {
            appendUnOp<Air::Oops, Air::Oops, CeilDouble, CeilFloat>(m_value->child(0));
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

            appendStore(valueToStore, addr(m_value));
            return;
        }

        case B3::Store8: {
            Value* valueToStore = m_value->child(0);
            m_insts.last().append(createStore(Air::Store8, valueToStore, addr(m_value)));
            return;
        }

        case B3::Store16: {
            Value* valueToStore = m_value->child(0);
            m_insts.last().append(createStore(Air::Store16, valueToStore, addr(m_value)));
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
                append(Move, Arg::imm64(m_value->asInt()), tmp(m_value));
            return;
        }

        case ConstDouble:
        case ConstFloat: {
            // We expect that the moveConstants() phase has run, and any doubles referenced from
            // stackmaps get fused.
            RELEASE_ASSERT(m_value->opcode() == ConstFloat || isIdentical(m_value->asDouble(), 0.0));
            RELEASE_ASSERT(m_value->opcode() == ConstDouble || isIdentical(m_value->asFloat(), 0.0));
            append(MoveZeroToDouble, tmp(m_value));
            return;
        }

        case FramePointer: {
            append(Move, Tmp(GPRInfo::callFrameRegister), tmp(m_value));
            return;
        }

        case B3::StackSlot: {
            append(
                Lea,
                Arg::stack(m_stackToStack.get(m_value->as<StackSlotValue>())),
                tmp(m_value));
            return;
        }

        case Equal:
        case NotEqual:
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
            Tmp result = tmp(m_value);
            append(relaxedMoveForType(m_value->type()), tmp(m_value->child(2)), result);

            MoveConditionallyConfig config;
            config.source = tmp(m_value->child(1));
            config.destination = result;

            if (isInt(m_value->type())) {
                config.moveConditionally32 = MoveConditionally32;
                config.moveConditionally64 = MoveConditionally64;
                config.moveConditionallyTest32 = MoveConditionallyTest32;
                config.moveConditionallyTest64 = MoveConditionallyTest64;
                config.moveConditionallyDouble = MoveConditionallyDouble;
                config.moveConditionallyFloat = MoveConditionallyFloat;
            } else {
                // FIXME: it's not obvious that these are particularly efficient.
                config.moveConditionally32 = MoveDoubleConditionally32;
                config.moveConditionally64 = MoveDoubleConditionally64;
                config.moveConditionallyTest32 = MoveDoubleConditionallyTest32;
                config.moveConditionallyTest64 = MoveDoubleConditionallyTest64;
                config.moveConditionallyDouble = MoveDoubleConditionallyDouble;
                config.moveConditionallyFloat = MoveDoubleConditionallyFloat;
            }
            
            m_insts.last().append(createSelect(m_value->child(0), config));
            return;
        }

        case IToD: {
            appendUnOp<ConvertInt32ToDouble, ConvertInt64ToDouble>(m_value->child(0));
            return;
        }

        case CCall: {
            CCallValue* cCall = m_value->as<CCallValue>();
            Inst inst(Patch, cCall, Arg::special(m_code.cCallSpecial()));

            // This is a bit weird - we have a super intense contract with Arg::CCallSpecial. It might
            // be better if we factored Air::CCallSpecial out of the Air namespace and made it a B3
            // thing.
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=151045

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

            // We need to tell Air what registers this defines.
            inst.args.append(Tmp(GPRInfo::returnValueGPR));
            inst.args.append(Tmp(GPRInfo::returnValueGPR2));
            inst.args.append(Tmp(FPRInfo::returnValueFPR));

            // Now marshall the arguments. This is where we implement the C calling convention. After
            // this, Air does not know what the convention is; it just takes our word for it.
            unsigned gpArgumentCount = 0;
            unsigned fpArgumentCount = 0;
            unsigned stackCount = 0;
            for (unsigned i = 1; i < cCall->numChildren(); ++i) {
                Value* argChild = cCall->child(i);
                Arg arg;
                
                switch (Arg::typeForB3Type(argChild->type())) {
                case Arg::GP:
                    arg = marshallCCallArgument<GPRInfo>(gpArgumentCount, stackCount, argChild);
                    break;

                case Arg::FP:
                    arg = marshallCCallArgument<FPRInfo>(fpArgumentCount, stackCount, argChild);
                    break;
                }

                if (arg.isTmp())
                    inst.args.append(arg);
            }
            
            m_insts.last().append(WTFMove(inst));

            switch (cCall->type()) {
            case Void:
                break;
            case Int32:
            case Int64:
                append(Move, Tmp(GPRInfo::returnValueGPR), tmp(cCall));
                break;
            case Float:
            case Double:
                append(MoveDouble, Tmp(FPRInfo::returnValueFPR), tmp(cCall));
                break;
            }
            return;
        }

        case Patchpoint: {
            PatchpointValue* patchpointValue = m_value->as<PatchpointValue>();
            ensureSpecial(m_patchpointSpecial);
            
            Inst inst(Patch, patchpointValue, Arg::special(m_patchpointSpecial));

            Vector<Inst> after;
            if (patchpointValue->type() != Void) {
                switch (patchpointValue->resultConstraint.kind()) {
                case ValueRep::WarmAny:
                case ValueRep::ColdAny:
                case ValueRep::LateColdAny:
                case ValueRep::SomeRegister:
                    inst.args.append(tmp(patchpointValue));
                    break;
                case ValueRep::Register: {
                    Tmp reg = Tmp(patchpointValue->resultConstraint.reg());
                    inst.args.append(reg);
                    after.append(Inst(
                        relaxedMoveForType(patchpointValue->type()), m_value, reg, tmp(patchpointValue)));
                    break;
                }
                case ValueRep::StackArgument: {
                    Arg arg = Arg::callArg(patchpointValue->resultConstraint.offsetFromSP());
                    inst.args.append(arg);
                    after.append(Inst(
                        moveForType(patchpointValue->type()), m_value, arg, tmp(patchpointValue)));
                    break;
                }
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
            }
            
            fillStackmap(inst, patchpointValue, 0);

            for (unsigned i = patchpointValue->numGPScratchRegisters; i--;)
                inst.args.append(m_code.newTmp(Arg::GP));
            for (unsigned i = patchpointValue->numFPScratchRegisters; i--;)
                inst.args.append(m_code.newTmp(Arg::FP));
            
            m_insts.last().append(WTFMove(inst));
            m_insts.last().appendVector(after);
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
            } else if (commutativity == Commutative && preferRightForResult(left, right)) {
                sources.append(tmp(left));
                append(Move, tmp(right), result);
            } else {
                sources.append(tmp(right));
                append(Move, tmp(left), result);
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

        case Upsilon: {
            Value* value = m_value->child(0);
            append(
                relaxedMoveForType(value->type()), immOrTmp(value),
                m_phiToTmp[m_value->as<UpsilonValue>()->phi()]);
            return;
        }

        case Phi: {
            // Snapshot the value of the Phi. It may change under us because you could do:
            // a = Phi()
            // Upsilon(@x, ^a)
            // @a => this should get the value of the Phi before the Upsilon, i.e. not @x.

            append(relaxedMoveForType(m_value->type()), m_phiToTmp[m_value], tmp(m_value));
            return;
        }

        case Branch: {
            m_insts.last().append(createBranch(m_value->child(0)));
            return;
        }

        case B3::Jump: {
            append(Air::Jump);
            return;
        }
            
        case Identity: {
            ASSERT(tmp(m_value->child(0)) == tmp(m_value));
            return;
        }

        case Return: {
            Value* value = m_value->child(0);
            Air::Opcode move;
            Tmp dest;
            if (isInt(value->type())) {
                move = Move;
                dest = Tmp(GPRInfo::returnValueGPR);
            } else {
                move = MoveDouble;
                dest = Tmp(FPRInfo::returnValueFPR);
            }
            append(move, immOrTmp(value), dest);
            append(Ret);
            return;
        }

        case B3::Oops: {
            append(Air::Oops);
            return;
        }

        default:
            break;
        }

        dataLog("FATAL: could not lower ", deepDump(m_procedure, m_value), "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }

#if CPU(X86) || CPU(X86_64)
    void lowerX86Div()
    {
        Tmp eax = Tmp(X86Registers::eax);
        Tmp edx = Tmp(X86Registers::edx);

        Air::Opcode convertToDoubleWord;
        Air::Opcode div;
        switch (m_value->type()) {
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
        
        append(Move, tmp(m_value->child(0)), eax);
        append(convertToDoubleWord, eax, edx);
        append(div, eax, edx, tmp(m_value->child(1)));
    }
#endif

    IndexSet<Value> m_locked; // These are values that will have no Tmp in Air.
    IndexMap<Value, Tmp> m_valueToTmp; // These are values that must have a Tmp in Air. We say that a Value* with a non-null Tmp is "pinned".
    IndexMap<Value, Tmp> m_phiToTmp; // Each Phi gets its own Tmp.
    IndexMap<B3::BasicBlock, Air::BasicBlock*> m_blockToBlock;
    HashMap<StackSlotValue*, Air::StackSlot*> m_stackToStack;

    UseCounts m_useCounts;
    PhiChildren m_phiChildren;

    Vector<Vector<Inst, 4>> m_insts;
    Vector<Inst> m_prologue;

    B3::BasicBlock* m_block;
    unsigned m_index;
    Value* m_value;

    PatchpointSpecial* m_patchpointSpecial { nullptr };
    HashMap<CheckSpecial::Key, CheckSpecial*> m_checkSpecials;

    Procedure& m_procedure;
    Code& m_code;
};

} // anonymous namespace

void lowerToAir(Procedure& procedure)
{
    PhaseScope phaseScope(procedure, "lowerToAir");
    LowerToAir lowerToAir(procedure);
    lowerToAir.run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

