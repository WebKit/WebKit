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

#include "AirCode.h"
#include "AirInsertionSet.h"
#include "AirInstInlines.h"
#include "AirStackSlot.h"
#include "B3AddressMatcher.h"
#include "B3ArgumentRegValue.h"
#include "B3BasicBlockInlines.h"
#include "B3CheckSpecial.h"
#include "B3Commutativity.h"
#include "B3IndexMap.h"
#include "B3IndexSet.h"
#include "B3LoweringMatcher.h"
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
        : valueToTmp(procedure.values().size())
        , blockToBlock(procedure.size())
        , useCounts(procedure)
        , addressSelector(*this)
        , procedure(procedure)
        , code(code)
    {
    }

    void run()
    {
        for (B3::BasicBlock* block : procedure)
            blockToBlock[block] = code.addBlock(block->frequency());
        for (Value* value : procedure.values()) {
            if (StackSlotValue* stackSlotValue = value->as<StackSlotValue>())
                stackToStack.add(stackSlotValue, code.addStackSlot(stackSlotValue));
        }

        procedure.resetValueOwners(); // Used by crossesInterference().

        // Lower defs before uses on a global level. This is a good heuristic to lock down a
        // hoisted address expression before we duplicate it back into the loop.
        for (B3::BasicBlock* block : procedure.blocksInPreOrder()) {
            currentBlock = block;
            // Reset some state.
            insts.resize(0);

            if (verbose)
                dataLog("Lowering Block ", *block, ":\n");
            
            // Process blocks in reverse order so we see uses before defs. That's what allows us
            // to match patterns effectively.
            for (unsigned i = block->size(); i--;) {
                currentIndex = i;
                currentValue = block->at(i);
                if (locked.contains(currentValue))
                    continue;
                insts.append(Vector<Inst>());
                if (verbose)
                    dataLog("Lowering ", deepDump(currentValue), ":\n");
                bool result = runLoweringMatcher(currentValue, *this);
                if (!result) {
                    dataLog("FATAL: could not lower ", deepDump(currentValue), "\n");
                    RELEASE_ASSERT_NOT_REACHED();
                }
                if (verbose) {
                    for (Inst& inst : insts.last())
                        dataLog("    ", inst, "\n");
                }
            }

            // Now append the instructions. insts contains them in reverse order, so we process
            // it in reverse.
            for (unsigned i = insts.size(); i--;) {
                for (Inst& inst : insts[i])
                    blockToBlock[block]->appendInst(WTF::move(inst));
            }

            // Make sure that the successors are set up correctly.
            ASSERT(block->successors().size() <= 2);
            for (B3::BasicBlock* successor : block->successorBlocks())
                blockToBlock[block]->successors().append(blockToBlock[successor]);
        }

        Air::InsertionSet insertionSet(code);
        for (Inst& inst : prologue)
            insertionSet.insertInst(0, WTF::move(inst));
        insertionSet.execute(code[0]);
    }

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
    //             return Inst(Foo, currentValue, left, right);
    //         return Inst();
    //     };
    //     if (Inst result = tryThings(loadAddr(left), tmp(right)))
    //         return result;
    //     if (Inst result = tryThings(tmp(left), loadAddr(right))) // this never succeeds.
    //         return result;
    //     return Inst(Foo, currentValue, tmp(left), tmp(right));
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
    //             return Inst(Foo, currentValue, left.consume(*this), right.consume(*this));
    //         return Inst();
    //     };
    //     if (Inst result = tryThings(loadPromise(left), tmpPromise(right)))
    //         return result;
    //     if (Inst result = tryThings(tmpPromise(left), loadPromise(right)))
    //         return result;
    //     return Inst(Foo, currentValue, tmp(left), tmp(right));
    //
    // Notice that we did use tmp in the fall-back case at the end, because by then, we know for sure
    // that we want a tmp. But using tmpPromise in the tryThings() calls ensures that doing so
    // doesn't prevent us from trying loadPromise on the same value.
    Tmp tmp(Value* value)
    {
        Tmp& tmp = valueToTmp[value];
        if (!tmp) {
            while (shouldCopyPropagate(value))
                value = value->child(0);
            Tmp& realTmp = valueToTmp[value];
            if (!realTmp)
                realTmp = code.newTmp(Arg::typeForB3Type(value->type()));
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
        if (valueToTmp[value])
            return false;
        
        // We require internals to have only one use - us.
        if (useCounts[value] != 1)
            return false;

        return true;
    }

    // If you ask canBeInternal() and then construct something from that, and you commit to emitting
    // that code, then you must commitInternal() on that value. This is tricky, and you only need to
    // do it if you're pattern matching by hand rather than using the patterns language. Long story
    // short, you should avoid this by using the pattern matcher to match patterns.
    void commitInternal(Value* value)
    {
        locked.add(value);
    }

    bool crossesInterference(Value* value)
    {
        // If it's in a foreign block, then be conservative. We could handle this if we were
        // willing to do heavier analysis. For example, if we had liveness, then we could label
        // values as "crossing interference" if they interfere with anything that they are live
        // across. But, it's not clear how useful this would be.
        if (value->owner != currentValue->owner)
            return true;

        Effects effects = value->effects();

        for (unsigned i = currentIndex; i--;) {
            Value* otherValue = currentBlock->at(i);
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
        addressSelector.selectedAddress = Arg();
        if (runAddressMatcher(address, addressSelector))
            return addressSelector.selectedAddress;

        return Arg::addr(tmp(address));
    }

    Arg effectiveAddr(Value* address, Value* memoryValue)
    {
        Arg result = effectiveAddr(address);
        ASSERT(result);

        int32_t offset = memoryValue->as<MemoryValue>()->offset();
        Arg offsetResult = result.withOffset(offset);
        if (!offsetResult)
            return Arg::addr(tmp(address), offset);
        return offsetResult;
    }

    // This gives you the address of the given Load or Store. If it's not a Load or Store, then
    // it returns Arg().
    Arg addr(Value* memoryValue)
    {
        MemoryValue* value = memoryValue->as<MemoryValue>();
        if (!value)
            return Arg();

        return effectiveAddr(value->lastChild(), value);
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
        if (value->representableAs<int32_t>())
            return Arg::imm(value->asNumber<int32_t>());
        return Arg();
    }

    Arg immOrTmp(Value* value)
    {
        if (Arg result = imm(value))
            return result;
        return tmp(value);
    }

    template<Air::Opcode opcode>
    void appendUnOp(Value* value)
    {
        Tmp result = tmp(currentValue);

        // Two operand forms like:
        //     Op a, b
        // mean something like:
        //     b = Op a

        if (isValidForm(opcode, Arg::Tmp, Arg::Tmp)) {
            append(opcode, tmp(value), result);
            return;
        }

        append(relaxedMoveForType(currentValue->type()), tmp(value), result);
        append(opcode, result);
    }

    template<Air::Opcode opcode, Commutativity commutativity = NotCommutative>
    void appendBinOp(Value* left, Value* right)
    {
        Tmp result = tmp(currentValue);
        
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

        // Note that no known architecture has a three-operand form of binary operations that also
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
                append(relaxedMoveForType(currentValue->type()), tmp(right), result);
                append(opcode, leftAddr.consume(*this), result);
                return;
            }
        }

        ArgPromise rightAddr = loadPromise(right);
        if (isValidForm(opcode, rightAddr.kind(), Arg::Tmp)) {
            append(relaxedMoveForType(currentValue->type()), tmp(left), result);
            append(opcode, rightAddr.consume(*this), result);
            return;
        }

        if (imm(right) && isValidForm(opcode, Arg::Imm, Arg::Tmp)) {
            append(relaxedMoveForType(currentValue->type()), tmp(left), result);
            append(opcode, imm(right), result);
            return;
        }

        if (isValidForm(opcode, Arg::Tmp, Arg::Tmp, Arg::Tmp)) {
            append(opcode, tmp(left), tmp(right), result);
            return;
        }

        append(relaxedMoveForType(currentValue->type()), tmp(left), result);
        append(opcode, tmp(right), result);
    }

    template<Air::Opcode opcode>
    bool tryAppendStoreUnOp(Value* value)
    {
        Arg storeAddr = addr(currentValue);
        ASSERT(storeAddr);

        ArgPromise loadPromise = this->loadPromise(value);
        if (loadPromise.peek() != storeAddr)
            return false;

        loadPromise.consume(*this);
        append(opcode, storeAddr);
        return true;
    }

    template<Air::Opcode opcode, Commutativity commutativity = NotCommutative>
    bool tryAppendStoreBinOp(Value* left, Value* right)
    {
        Arg storeAddr = addr(currentValue);
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

    void appendStore(Value* value, const Arg& dest)
    {
        Air::Opcode move = moveForType(value->type());

        if (imm(value) && isValidForm(move, Arg::Imm, dest.kind())) {
            append(move, imm(value), dest);
            return;
        }

        append(move, tmp(value), dest);
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
        insts.last().append(Inst(opcode, currentValue, std::forward<Arguments>(arguments)...));
    }

    template<typename T, typename... Arguments>
    T* ensureSpecial(T*& field, Arguments&&... arguments)
    {
        if (!field) {
            field = static_cast<T*>(
                code.addSpecial(std::make_unique<T>(std::forward<Arguments>(arguments)...)));
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
                arg = immOrTmp(value.value());
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
    
    IndexSet<Value> locked; // These are values that will have no Tmp in Air.
    IndexMap<Value, Tmp> valueToTmp; // These are values that must have a Tmp in Air. We say that a Value* with a non-null Tmp is "pinned".
    IndexMap<B3::BasicBlock, Air::BasicBlock*> blockToBlock;
    HashMap<StackSlotValue*, Air::StackSlot*> stackToStack;

    UseCounts useCounts;

    Vector<Vector<Inst, 4>> insts;
    Vector<Inst> prologue;

    B3::BasicBlock* currentBlock;
    unsigned currentIndex;
    Value* currentValue;

    // The address selector will match any pattern where the input operands are available as Tmps.
    // It doesn't care about sharing. It will happily emit the same address expression over and over
    // again regardless of the expression's complexity. This works out fine, since at the machine
    // level, address expressions are super cheap. However, this does have a hack to avoid
    // "unhoisting" address expressions.
    class AddressSelector {
    public:
        AddressSelector(LowerToAir& lower)
            : lower(lower)
        {
        }

        bool acceptRoot(Value* root)
        {
            this->root = root;
            // We don't want to match an address expression that has already been computed
            // explicitly. This mostly makes sense. Note that we never hoist basic address
            // expressions like offset(base), because those are sunk into the MemoryValue. So,
            // this is mainly just relevant to Index expressions and other more complicated
            // things. It stands to reason that most of the time, we won't hoist an Index
            // expression. And if we do, then probably it's a good thing to compute it outside
            // a loop. That being said, this might turn into a problem. For example, it will
            // require an extra register to be live. That's unlikely to be profitable.
            // FIXME: Consider matching an address expression even if we've already assigned a
            // Tmp to it. https://bugs.webkit.org/show_bug.cgi?id=150777
            return !lower.valueToTmp[root];
        }

        void acceptRootLate(Value*) { }

        template<typename... Arguments>
        bool acceptInternals(Value* value, Arguments... arguments)
        {
            if (lower.valueToTmp[value])
                return false;
            return acceptInternals(arguments...);
        }
        bool acceptInternals() { return true; }

        template<typename... Arguments>
        void acceptInternalsLate(Arguments...) { }

        template<typename... Arguments>
        bool acceptOperands(Value* value, Arguments... arguments)
        {
            if (lower.locked.contains(value))
                return false;
            return acceptOperands(arguments...);
        }
        bool acceptOperands() { return true; }

        template<typename... Arguments>
        void acceptOperandsLate(Arguments...) { }

        bool tryAddShift1(Value* index, Value* logScale, Value* base)
        {
            if (logScale->asInt() < 0 || logScale->asInt() > 3)
                return false;
            selectedAddress = Arg::index(
                lower.tmp(base), lower.tmp(index), 1 << logScale->asInt());
            return true;
        }

        bool tryAddShift2(Value* base, Value* index, Value* logScale)
        {
            return tryAddShift1(index, logScale, base);
        }

        bool tryAdd(Value* left, Value* right)
        {
            if (right->hasInt32()) {
                // This production isn't strictly necessary since we expect
                // Load(Add(@x, @const1), offset = const2) to have already been strength-reduced
                // to Load(@x, offset = const1 + const2).
                selectedAddress = Arg::addr(lower.tmp(left), right->asInt32());
                return true;
            }

            selectedAddress = Arg::index(lower.tmp(left), lower.tmp(right));
            return true;
        }

        bool tryFramePointer()
        {
            selectedAddress = Arg::addr(Tmp(GPRInfo::callFrameRegister));
            return true;
        }

        bool tryStackSlot()
        {
            selectedAddress = Arg::stack(lower.stackToStack.get(root->as<StackSlotValue>()));
            return true;
        }

        bool tryDirect()
        {
            selectedAddress = Arg::addr(lower.tmp(root));
            return true;
        }
        
        LowerToAir& lower;
        Value* root;
        Arg selectedAddress;
    };
    AddressSelector addressSelector;

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
            if (!canBeInternal(value) && value != currentValue)
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
                return tryCompare(width, tmpPromise(left), tmpPromise(right));
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

        if (canBeInternal(value) || value == currentValue) {
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
                            Branch8, currentValue, relCond,
                            left.consume(*this), right.consume(*this));
                    }
                    return Inst();
                case Arg::Width16:
                    return Inst();
                case Arg::Width32:
                    if (isValidForm(Branch32, Arg::RelCond, left.kind(), right.kind())) {
                        return Inst(
                            Branch32, currentValue, relCond,
                            left.consume(*this), right.consume(*this));
                    }
                    return Inst();
                case Arg::Width64:
                    if (isValidForm(Branch64, Arg::RelCond, left.kind(), right.kind())) {
                        return Inst(
                            Branch64, currentValue, relCond,
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
                            BranchTest8, currentValue, resCond,
                            left.consume(*this), right.consume(*this));
                    }
                    return Inst();
                case Arg::Width16:
                    return Inst();
                case Arg::Width32:
                    if (isValidForm(BranchTest32, Arg::ResCond, left.kind(), right.kind())) {
                        return Inst(
                            BranchTest32, currentValue, resCond,
                            left.consume(*this), right.consume(*this));
                    }
                    return Inst();
                case Arg::Width64:
                    if (isValidForm(BranchTest64, Arg::ResCond, left.kind(), right.kind())) {
                        return Inst(
                            BranchTest64, currentValue, resCond,
                            left.consume(*this), right.consume(*this));
                    }
                    return Inst();
                }
            },
            [this] (Arg doubleCond, const ArgPromise& left, const ArgPromise& right) -> Inst {
                if (isValidForm(BranchDouble, Arg::DoubleCond, left.kind(), right.kind())) {
                    return Inst(
                        BranchDouble, currentValue, doubleCond,
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
                            Compare32, currentValue, relCond,
                            left.consume(*this), right.consume(*this), tmp(currentValue));
                    }
                    return Inst();
                case Arg::Width64:
                    if (isValidForm(Compare64, Arg::RelCond, left.kind(), right.kind(), Arg::Tmp)) {
                        return Inst(
                            Compare64, currentValue, relCond,
                            left.consume(*this), right.consume(*this), tmp(currentValue));
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
                            Test32, currentValue, resCond,
                            left.consume(*this), right.consume(*this), tmp(currentValue));
                    }
                    return Inst();
                case Arg::Width64:
                    if (isValidForm(Test64, Arg::ResCond, left.kind(), right.kind(), Arg::Tmp)) {
                        return Inst(
                            Test64, currentValue, resCond,
                            left.consume(*this), right.consume(*this), tmp(currentValue));
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

    // Below is the code for a lowering selector, so that we can pass *this to runLoweringMatcher.
    // This will match complex multi-value expressions, but only if there is no sharing. For example,
    // it won't match a Load twice and cause the generated code to do two loads when the original
    // code just did one.
    
    bool acceptRoot(Value* root)
    {
        ASSERT_UNUSED(root, !locked.contains(root));
        return true;
    }
    
    void acceptRootLate(Value*) { }
    
    template<typename... Arguments>
    bool acceptInternals(Value* value, Arguments... arguments)
    {
        if (!canBeInternal(value))
            return false;
        
        return acceptInternals(arguments...);
    }
    bool acceptInternals() { return true; }
    
    template<typename... Arguments>
    void acceptInternalsLate(Value* value, Arguments... arguments)
    {
        commitInternal(value);
        acceptInternalsLate(arguments...);
    }
    void acceptInternalsLate() { }
    
    template<typename... Arguments>
    bool acceptOperands(Value* value, Arguments... arguments)
    {
        if (locked.contains(value))
            return false;
        return acceptOperands(arguments...);
    }
    bool acceptOperands() { return true; }
    
    template<typename... Arguments>
    void acceptOperandsLate(Arguments...) { }
    
    bool tryLoad(Value* address)
    {
        append(
            moveForType(currentValue->type()),
            effectiveAddr(address, currentValue), tmp(currentValue));
        return true;
    }

    bool tryLoad8S(Value* address)
    {
        append(Load8SignedExtendTo32, effectiveAddr(address, currentValue), tmp(currentValue));
        return true;
    }
    
    bool tryLoad8Z(Value* address)
    {
        append(Load8, effectiveAddr(address, currentValue), tmp(currentValue));
        return true;
    }
    
    bool tryLoad16S(Value* address)
    {
        append(Load16SignedExtendTo32, effectiveAddr(address, currentValue), tmp(currentValue));
        return true;
    }
    
    bool tryLoad16Z(Value* address)
    {
        append(Load16, effectiveAddr(address, currentValue), tmp(currentValue));
        return true;
    }
    
    bool tryAdd(Value* left, Value* right)
    {
        switch (left->type()) {
        case Int32:
            appendBinOp<Add32, Commutative>(left, right);
            return true;
        case Int64:
            appendBinOp<Add64, Commutative>(left, right);
            return true;
        default:
            // FIXME: Implement more types!
            return false;
        }
    }

    bool trySub(Value* left, Value* right)
    {
        switch (left->type()) {
        case Int32:
            if (left->isInt32(0))
                appendUnOp<Neg32>(right);
            else
                appendBinOp<Sub32>(left, right);
            return true;
        case Int64:
            if (left->isInt64(0))
                appendUnOp<Neg64>(right);
            else
                appendBinOp<Sub64>(left, right);
            return true;
        default:
            // FIXME: Implement more types!
            return false;
        }
    }
    
    bool tryAnd(Value* left, Value* right)
    {
        switch (left->type()) {
        case Int32:
            appendBinOp<And32, Commutative>(left, right);
            return true;
        case Int64:
            appendBinOp<And64, Commutative>(left, right);
            return true;
        default:
            // FIXME: Implement more types!
            return false;
        }
    }

    bool tryOr(Value* left, Value* right)
    {
        switch (left->type()) {
        case Int32:
            appendBinOp<Or32, Commutative>(left, right);
            return true;
        case Int64:
            appendBinOp<Or64, Commutative>(left, right);
            return true;
        default:
            // FIXME: Implement more types!
            return false;
        }
    }

    bool tryXor(Value* left, Value* right)
    {
        switch (left->type()) {
        case Int32:
            appendBinOp<Xor32, Commutative>(left, right);
            return true;
        case Int64:
            appendBinOp<Xor64, Commutative>(left, right);
            return true;
        default:
            // FIXME: Implement more types!
            return false;
        }
    }

    void appendShift(Air::Opcode opcode, Value* value, Value* amount)
    {
        if (imm(amount)) {
            append(Move, tmp(value), tmp(currentValue));
            append(opcode, imm(amount), tmp(currentValue));
            return;
        }

        append(Move, tmp(value), tmp(currentValue));
        append(Move, tmp(amount), Tmp(X86Registers::ecx));
        append(opcode, Tmp(X86Registers::ecx), tmp(currentValue));
    }

    bool tryShl(Value* value, Value* amount)
    {
        Air::Opcode opcode = value->type() == Int32 ? Lshift32 : Lshift64;
        appendShift(opcode, value, amount);
        return true;
    }

    bool trySShr(Value* value, Value* amount)
    {
        Air::Opcode opcode = value->type() == Int32 ? Rshift32 : Rshift64;
        appendShift(opcode, value, amount);
        return true;
    }

    bool tryZShr(Value* value, Value* amount)
    {
        Air::Opcode opcode = value->type() == Int32 ? Urshift32 : Urshift64;
        appendShift(opcode, value, amount);
        return true;
    }
    
    bool tryStoreAddLoad(Value* left, Value* right, Value*)
    {
        switch (left->type()) {
        case Int32:
            return tryAppendStoreBinOp<Add32, Commutative>(left, right);
        default:
            // FIXME: Implement more types!
            return false;
        }
    }

    bool tryStoreSubLoad(Value* left, Value* right, Value*)
    {
        switch (left->type()) {
        case Int32:
            if (left->isInt32(0))
                return tryAppendStoreUnOp<Neg32>(right);
            return tryAppendStoreBinOp<Sub32, NotCommutative>(left, right);
        default:
            // FIXME: Implement more types!
            return false;
        }
    }

    bool tryStoreAndLoad(Value* left, Value* right, Value*)
    {
        switch (left->type()) {
        case Int32:
            return tryAppendStoreBinOp<And32, Commutative>(left, right);
        default:
            // FIXME: Implement more types!
            return false;
        }
    }
    
    bool tryStore(Value* value, Value* address)
    {
        appendStore(value, effectiveAddr(address, currentValue));
        return true;
    }

    bool tryTrunc(Value* value)
    {
        ASSERT_UNUSED(value, tmp(value) == tmp(currentValue));
        return true;
    }

    bool tryZExt32(Value* value)
    {
        if (highBitsAreZero(value)) {
            ASSERT(tmp(value) == tmp(currentValue));
            return true;
        }
        
        append(Move32, tmp(value), tmp(currentValue));
        return true;
    }

    bool tryArgumentReg()
    {
        prologue.append(Inst(
            moveForType(currentValue->type()), currentValue,
            Tmp(currentValue->as<ArgumentRegValue>()->argumentReg()),
            tmp(currentValue)));
        return true;
    }
    
    bool tryConst32()
    {
        append(Move, imm(currentValue), tmp(currentValue));
        return true;
    }
    
    bool tryConst64()
    {
        if (imm(currentValue)) {
            append(Move, imm(currentValue), tmp(currentValue));
            return true;
        }
        append(Move, Arg::imm64(currentValue->asInt64()), tmp(currentValue));
        return true;
    }

    bool tryFramePointer()
    {
        append(Move, Tmp(GPRInfo::callFrameRegister), tmp(currentValue));
        return true;
    }

    bool tryStackSlot()
    {
        append(
            Lea,
            Arg::stack(stackToStack.get(currentValue->as<StackSlotValue>())),
            tmp(currentValue));
        return true;
    }

    bool tryEqual()
    {
        insts.last().append(createCompare(currentValue));
        return true;
    }

    bool tryNotEqual()
    {
        insts.last().append(createCompare(currentValue));
        return true;
    }

    bool tryLessThan()
    {
        insts.last().append(createCompare(currentValue));
        return true;
    }

    bool tryGreaterThan()
    {
        insts.last().append(createCompare(currentValue));
        return true;
    }

    bool tryLessEqual()
    {
        insts.last().append(createCompare(currentValue));
        return true;
    }

    bool tryGreaterEqual()
    {
        insts.last().append(createCompare(currentValue));
        return true;
    }

    bool tryAbove()
    {
        insts.last().append(createCompare(currentValue));
        return true;
    }

    bool tryBelow()
    {
        insts.last().append(createCompare(currentValue));
        return true;
    }

    bool tryAboveEqual()
    {
        insts.last().append(createCompare(currentValue));
        return true;
    }

    bool tryBelowEqual()
    {
        insts.last().append(createCompare(currentValue));
        return true;
    }

    PatchpointSpecial* patchpointSpecial { nullptr };
    bool tryPatchpoint()
    {
        PatchpointValue* patchpointValue = currentValue->as<PatchpointValue>();
        ensureSpecial(patchpointSpecial);

        Inst inst(Patch, patchpointValue, Arg::special(patchpointSpecial));

        if (patchpointValue->type() != Void)
            inst.args.append(tmp(patchpointValue));

        fillStackmap(inst, patchpointValue, 0);
        
        insts.last().append(WTF::move(inst));
        return true;
    }

    HashMap<CheckSpecial::Key, CheckSpecial*> checkSpecials;
    bool tryCheck(Value* value)
    {
        Inst branch = createBranch(value);

        CheckSpecial::Key key(branch);
        auto result = checkSpecials.add(key, nullptr);
        Special* special = ensureSpecial(result.iterator->value, key);

        CheckValue* checkValue = currentValue->as<CheckValue>();

        Inst inst(Patch, checkValue, Arg::special(special));
        inst.args.appendVector(branch.args);

        fillStackmap(inst, checkValue, 1);

        insts.last().append(WTF::move(inst));
        return true;
    }

    bool tryUpsilon(Value* value)
    {
        append(
            relaxedMoveForType(value->type()),
            immOrTmp(value),
            tmp(currentValue->as<UpsilonValue>()->phi()));
        return true;
    }

    bool tryPhi()
    {
        // Our semantics are determined by Upsilons, so we have nothing to do here.
        return true;
    }

    bool tryBranch(Value* value)
    {
        insts.last().append(createBranch(value));
        return true;
    }

    bool tryJump()
    {
        append(Air::Jump);
        return true;
    }

    bool tryIdentity(Value* value)
    {
        ASSERT_UNUSED(value, tmp(value) == tmp(currentValue));
        return true;
    }
    
    bool tryReturn(Value* value)
    {
        append(relaxedMoveForType(value->type()), immOrTmp(value), Tmp(GPRInfo::returnValueGPR));
        append(Ret);
        return true;
    }
    
    Procedure& procedure;
    Code& code;
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

