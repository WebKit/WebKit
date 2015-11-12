/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
    LowerToAir(Procedure& procedure, Code& code)
        : m_valueToTmp(procedure.values().size())
        , m_blockToBlock(procedure.size())
        , m_useCounts(procedure)
        , m_procedure(procedure)
        , m_code(code)
    {
    }

    void run()
    {
        for (B3::BasicBlock* block : m_procedure)
            m_blockToBlock[block] = m_code.addBlock(block->frequency());
        for (Value* value : m_procedure.values()) {
            if (StackSlotValue* stackSlotValue = value->as<StackSlotValue>())
                m_stackToStack.add(stackSlotValue, m_code.addStackSlot(stackSlotValue));
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
                    dataLog("Lowering ", deepDump(m_value), ":\n");
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
                    m_blockToBlock[block]->appendInst(WTF::move(inst));
            }

            // Make sure that the successors are set up correctly.
            ASSERT(block->successors().size() <= 2);
            for (B3::BasicBlock* successor : block->successorBlocks())
                m_blockToBlock[block]->successors().append(m_blockToBlock[successor]);
        }

        Air::InsertionSet insertionSet(m_code);
        for (Inst& inst : m_prologue)
            insertionSet.insertInst(0, WTF::move(inst));
        insertionSet.execute(m_code[0]);
    }

private:
    bool highBitsAreZero(Value* value)
    {
        switch (value->opcode()) {
        case Const32:
            // We will use a Move immediate instruction, which may sign extend.
            return value->asInt32() >= 0;
        case Trunc:
            // Trunc is copy-propagated, so the value may have garbage in the high bits.
            return false;
        case CCall:
            // Calls are allowed to have garbage in their high bits.
            return false;
        case Patchpoint:
            // For now, we assume that patchpoints may return garbage in the high bits. This simplifies
            // the interface. We may revisit for performance reasons later.
            return false;
        case Phi:
            // FIXME: We could do this right.
            // https://bugs.webkit.org/show_bug.cgi?id=150845
            return false;
        default:
            // All other operations that return Int32 should lower to something that zero extends.
            return value->type() == Int32;
        }
    }

    // NOTE: This entire mechanism could be done over Air, if we felt that this would be fast enough.
    // For now we're assuming that it's faster to do this here, since analyzing B3 is so cheap.
    bool shouldCopyPropagate(Value* value)
    {
        switch (value->opcode()) {
        case Trunc:
        case Identity:
            return true;
        case ZExt32:
            return highBitsAreZero(value->child(0));
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
            if (!realTmp)
                realTmp = m_code.newTmp(Arg::typeForB3Type(value->type()));
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
        
        // We require internals to have only one use - us.
        if (m_useCounts[value] != 1)
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
    Arg effectiveAddr(Value* address)
    {
        // FIXME: Consider matching an address expression even if we've already assigned a
        // Tmp to it. https://bugs.webkit.org/show_bug.cgi?id=150777
        if (m_valueToTmp[address])
            return Arg::addr(tmp(address));
        
        switch (address->opcode()) {
        case Add: {
            Value* left = address->child(0);
            Value* right = address->child(1);

            auto tryIndex = [&] (Value* index, Value* offset) -> Arg {
                if (index->opcode() != Shl)
                    return Arg();
                if (m_locked.contains(index->child(0)) || m_locked.contains(offset))
                    return Arg();
                if (!index->child(1)->hasInt32())
                    return Arg();
                
                unsigned scale = 1 << (index->child(1)->asInt32() & 31);
                if (!Arg::isValidScale(scale))
                    return Arg();

                return Arg::index(tmp(offset), tmp(index->child(0)), scale);
            };

            if (Arg result = tryIndex(left, right))
                return result;
            if (Arg result = tryIndex(right, left))
                return result;

            if (m_locked.contains(left) || m_locked.contains(right))
                return Arg::addr(tmp(address));
            
            return Arg::index(tmp(left), tmp(right));
        }

        case FramePointer:
            return Arg::addr(Tmp(GPRInfo::callFrameRegister));

        case B3::StackSlot:
            return Arg::stack(m_stackToStack.get(address->as<StackSlotValue>()));

        default:
            return Arg::addr(tmp(address));
        }
    }

    // This gives you the address of the given Load or Store. If it's not a Load or Store, then
    // it returns Arg().
    Arg addr(Value* memoryValue)
    {
        MemoryValue* value = memoryValue->as<MemoryValue>();
        if (!value)
            return Arg();

        Arg result = effectiveAddr(value->lastChild());
        ASSERT(result);
        
        int32_t offset = memoryValue->as<MemoryValue>()->offset();
        Arg offsetResult = result.withOffset(offset);
        if (!offsetResult)
            return Arg::addr(tmp(value->lastChild()), offset);
        return offsetResult;
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
        if (value->hasInt() && value->representableAs<int32_t>())
            return Arg::imm(value->asNumber<int32_t>());
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
        Air::Opcode opcode32, Air::Opcode opcode64, Air::Opcode opcodeDouble, Type type)
    {
        Air::Opcode opcode;
        switch (type) {
        case Int32:
            opcode = opcode32;
            break;
        case Int64:
            opcode = opcode64;
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

    Air::Opcode opcodeForType(
        Air::Opcode opcode32, Air::Opcode opcode64, Air::Opcode opcodeDouble, Type type)
    {
        Air::Opcode opcode = tryOpcodeForType(opcode32, opcode64, opcodeDouble, type);
        RELEASE_ASSERT(opcode != Air::Oops);
        return opcode;
    }

    template<Air::Opcode opcode32, Air::Opcode opcode64, Air::Opcode opcodeDouble>
    void appendUnOp(Value* value)
    {
        Air::Opcode opcode = opcodeForType(opcode32, opcode64, opcodeDouble, value->type());
        
        Tmp result = tmp(m_value);

        // Two operand forms like:
        //     Op a, b
        // mean something like:
        //     b = Op a

        if (isValidForm(opcode, Arg::Tmp, Arg::Tmp)) {
            append(opcode, tmp(value), result);
            return;
        }

        append(relaxedMoveForType(m_value->type()), tmp(value), result);
        append(opcode, result);
    }

    template<
        Air::Opcode opcode32, Air::Opcode opcode64, Air::Opcode opcodeDouble,
        Commutativity commutativity = NotCommutative>
    void appendBinOp(Value* left, Value* right)
    {
        Air::Opcode opcode = opcodeForType(opcode32, opcode64, opcodeDouble, left->type());
        
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

        if (imm(right) && isValidForm(opcode, Arg::Imm, Arg::Tmp)) {
            append(relaxedMoveForType(m_value->type()), tmp(left), result);
            append(opcode, imm(right), result);
            return;
        }

        if (isValidForm(opcode, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
            append(opcode, tmp(left), tmp(right), result);
            return;
        }

        append(relaxedMoveForType(m_value->type()), tmp(left), result);
        append(opcode, tmp(right), result);
    }

    template<Air::Opcode opcode32, Air::Opcode opcode64>
    void appendShift(Value* value, Value* amount)
    {
        Air::Opcode opcode = opcodeForType(opcode32, opcode64, Air::Oops, value->type());
        
        if (imm(amount)) {
            append(Move, tmp(value), tmp(m_value));
            append(opcode, imm(amount), tmp(m_value));
            return;
        }

        append(Move, tmp(value), tmp(m_value));
        append(Move, tmp(amount), Tmp(X86Registers::ecx));
        append(opcode, Tmp(X86Registers::ecx), tmp(m_value));
    }

    template<Air::Opcode opcode32, Air::Opcode opcode64>
    bool tryAppendStoreUnOp(Value* value)
    {
        Air::Opcode opcode = tryOpcodeForType(opcode32, opcode64, Air::Oops, value->type());
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
        Air::Opcode opcode = tryOpcodeForType(opcode32, opcode64, Air::Oops, left->type());
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

    Inst createStore(Value* value, const Arg& dest)
    {
        Air::Opcode move = moveForType(value->type());

        if (imm(value) && isValidForm(move, Arg::Imm, dest.kind()))
            return Inst(move, m_value, imm(value), dest);

        return Inst(move, m_value, tmp(value), dest);
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
        case Double:
            return MoveDouble;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    Air::Opcode relaxedMoveForType(Type type)
    {
        switch (type) {
        case Int32:
        case Int64:
            return Move;
        case Double:
            return MoveDouble;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
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

    void fillStackmap(Inst& inst, StackmapValue* stackmap, unsigned numSkipped)
    {
        for (unsigned i = numSkipped; i < stackmap->numChildren(); ++i) {
            ConstrainedValue value = stackmap->constrainedChild(i);

            Arg arg;
            switch (value.rep().kind()) {
            case ValueRep::Any:
                if (imm(value.value()))
                    arg = imm(value.value());
                else if (value.value()->hasInt64())
                    arg = Arg::imm64(value.value()->asInt64());
                else if (value.value()->hasDouble())
                    arg = Arg::imm64(bitwise_cast<int64_t>(value.value()->asDouble()));
                else
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
    template<typename CompareFunctor, typename TestFunctor, typename CompareDoubleFunctor>
    Inst createGenericCompare(
        Value* value,
        const CompareFunctor& compare, // Signature: (Arg::Width, Arg relCond, Arg, Arg) -> Inst
        const TestFunctor& test, // Signature: (Arg::Width, Arg resCond, Arg, Arg) -> Inst
        const CompareDoubleFunctor& compareDouble, // Signature: (Arg doubleCond, Arg, Arg) -> Inst
        bool inverted = false)
    {
        // Chew through any negations. It's not strictly necessary for this to be a loop, but we like
        // to follow the rule that the instruction selector reduces strength whenever it doesn't
        // require making things more complicated.
        for (;;) {
            if (!canBeInternal(value) && value != m_value)
                break;
            bool shouldInvert =
                (value->opcode() == BitXor && value->child(1)->isInt(1) && value->child(0)->returnsBool())
                || (value->opcode() == Equal && value->child(1)->isInt(0));
            if (!shouldInvert)
                break;
            value = value->child(0);
            inverted = !inverted;
            commitInternal(value);
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

                // First handle compares that involve fewer bits than B3's type system supports.
                // This is pretty important. For example, we want this to be a single instruction:
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

                Arg::Width width = Arg::widthForB3Type(value->child(0)->type());
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

            // Double comparisons can't really do anything smart.
            return compareDouble(doubleCond, tmpPromise(left), tmpPromise(right));
        };

        Arg::Width width = Arg::widthForB3Type(value->type());
        Arg resCond = Arg::resCond(MacroAssembler::NonZero).inverted(inverted);
        
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
                
                auto tryTest = [&] (
                    Arg::Width width, const ArgPromise& left, const ArgPromise& right) -> Inst {
                    if (Inst result = test(width, resCond, left, right))
                        return result;
                    if (Inst result = test(width, resCond, right, left))
                        return result;
                    return Inst();
                };

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

                // First handle test's that involve fewer bits than B3's type system supports.

                if (Inst result = tryTestLoadImm(Arg::Width8, Load8Z))
                    return result;

                if (Inst result = tryTestLoadImm(Arg::Width8, Load8S))
                    return result;

                if (Inst result = tryTestLoadImm(Arg::Width16, Load16Z))
                    return result;

                if (Inst result = tryTestLoadImm(Arg::Width16, Load16S))
                    return result;

                // Now handle test's that involve a load and an immediate. Note that immediates are
                // 32-bit, and we want zero-extension. Hence, the immediate form is compiled as a
                // 32-bit test. Note that this spits on the grave of inferior endians, such as the
                // big one.
                
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

        if (canBeInternal(value) || value == m_value) {
            if (Inst result = attemptFused()) {
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
            [this] (const Arg&, const ArgPromise&, const ArgPromise&) -> Inst {
                // FIXME: Implement this.
                // https://bugs.webkit.org/show_bug.cgi?id=150903
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
            appendBinOp<Add32, Add64, AddDouble, Commutative>(
                m_value->child(0), m_value->child(1));
            return;
        }

        case Sub: {
            if (m_value->child(0)->isInt(0))
                appendUnOp<Neg32, Neg64, Air::Oops>(m_value->child(1));
            else
                appendBinOp<Sub32, Sub64, Air::Oops>(m_value->child(0), m_value->child(1));
            return;
        }

        case Mul: {
            appendBinOp<Mul32, Mul64, Air::Oops, Commutative>(
                m_value->child(0), m_value->child(1));
            return;
        }

        case Div: {
            if (isInt(m_value->type())) {
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
                append(Move, eax, tmp(m_value));
                return;
            }

            // FIXME: Support doubles.
            // https://bugs.webkit.org/show_bug.cgi?id=150991
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }

        case BitAnd: {
            appendBinOp<And32, And64, Air::Oops, Commutative>(
                m_value->child(0), m_value->child(1));
            return;
        }

        case BitOr: {
            appendBinOp<Or32, Or64, Air::Oops, Commutative>(
                m_value->child(0), m_value->child(1));
            return;
        }

        case BitXor: {
            appendBinOp<Xor32, Xor64, Air::Oops, Commutative>(
                m_value->child(0), m_value->child(1));
            return;
        }

        case Shl: {
            if (m_value->child(1)->isInt32(1)) {
                // This optimization makes sense on X86. I don't know if it makes sense anywhere else.
                append(Move, tmp(m_value->child(0)), tmp(m_value));
                append(
                    opcodeForType(Add32, Add64, Air::Oops, m_value->child(0)->type()),
                    tmp(m_value), tmp(m_value));
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

        case Trunc: {
            ASSERT(tmp(m_value->child(0)) == tmp(m_value));
            return;
        }

        case ZExt32: {
            if (highBitsAreZero(m_value->child(0))) {
                ASSERT(tmp(m_value->child(0)) == tmp(m_value));
                return;
            }

            append(Move32, tmp(m_value->child(0)), tmp(m_value));
            return;
        }

        case ArgumentReg: {
            m_prologue.append(Inst(
                moveForType(m_value->type()), m_value,
                Tmp(m_value->as<ArgumentRegValue>()->argumentReg()),
                tmp(m_value)));
            return;
        }

        case Const32: {
            append(Move, imm(m_value), tmp(m_value));
            return;
        }
        case Const64: {
            if (imm(m_value))
                append(Move, imm(m_value), tmp(m_value));
            else
                append(Move, Arg::imm64(m_value->asInt64()), tmp(m_value));
            return;
        }

        case ConstDouble: {
            // We expect that the moveConstants() phase has run.
            RELEASE_ASSERT(isIdentical(m_value->asDouble(), 0.0));
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
        case BelowEqual: {
            m_insts.last().append(createCompare(m_value));
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
            
            m_insts.last().append(WTF::move(inst));

            switch (cCall->type()) {
            case Void:
                break;
            case Int32:
            case Int64:
                append(Move, Tmp(GPRInfo::returnValueGPR), tmp(cCall));
                break;
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
            
            if (patchpointValue->type() != Void)
                inst.args.append(tmp(patchpointValue));
            
            fillStackmap(inst, patchpointValue, 0);
            
            m_insts.last().append(WTF::move(inst));
            return;
        }

        case Check: {
            Inst branch = createBranch(m_value->child(0));
            
            CheckSpecial::Key key(branch);
            auto result = m_checkSpecials.add(key, nullptr);
            Special* special = ensureSpecial(result.iterator->value, key);
            
            CheckValue* checkValue = m_value->as<CheckValue>();
            
            Inst inst(Patch, checkValue, Arg::special(special));
            inst.args.appendVector(branch.args);
            
            fillStackmap(inst, checkValue, 1);
            
            m_insts.last().append(WTF::move(inst));
            return;
        }

        case Upsilon: {
            Value* value = m_value->child(0);
            append(
                relaxedMoveForType(value->type()), immOrTmp(value),
                tmp(m_value->as<UpsilonValue>()->phi()));
            return;
        }

        case Phi: {
            // Our semantics are determined by Upsilons, so we have nothing to do here.
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

        default:
            break;
        }

        dataLog("FATAL: could not lower ", deepDump(m_value), "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    IndexSet<Value> m_locked; // These are values that will have no Tmp in Air.
    IndexMap<Value, Tmp> m_valueToTmp; // These are values that must have a Tmp in Air. We say that a Value* with a non-null Tmp is "pinned".
    IndexMap<B3::BasicBlock, Air::BasicBlock*> m_blockToBlock;
    HashMap<StackSlotValue*, Air::StackSlot*> m_stackToStack;

    UseCounts m_useCounts;

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

void lowerToAir(Procedure& procedure, Code& code)
{
    PhaseScope phaseScope(procedure, "lowerToAir");
    LowerToAir lowerToAir(procedure, code);
    lowerToAir.run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

