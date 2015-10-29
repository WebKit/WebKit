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
#include "B3Commutativity.h"
#include "B3IndexMap.h"
#include "B3IndexSet.h"
#include "B3LoweringMatcher.h"
#include "B3MemoryValue.h"
#include "B3PhaseScope.h"
#include "B3Procedure.h"
#include "B3StackSlotValue.h"
#include "B3UseCounts.h"
#include "B3ValueInlines.h"
#include <wtf/ListDump.h>

namespace JSC { namespace B3 {

using namespace Air;

namespace {

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

        // Lower defs before uses on a global level. This is a good heuristic to lock down a
        // hoisted address expression before we duplicate it back into the loop.
        for (B3::BasicBlock* block : procedure.blocksInPreOrder()) {
            // Reset some state.
            insts.resize(0);
            
            // Process blocks in reverse order so we see uses before defs. That's what allows us
            // to match patterns effectively.
            for (unsigned i = block->size(); i--;) {
                currentValue = block->at(i);
                if (locked.contains(currentValue))
                    continue;
                insts.append(Vector<Inst>());
                bool result = runLoweringMatcher(currentValue, *this);
                if (!result) {
                    dataLog("FATAL: could not lower ", deepDump(currentValue), "\n");
                    RELEASE_ASSERT_NOT_REACHED();
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

    Tmp tmp(Value* value)
    {
        Tmp& tmp = valueToTmp[value];
        if (!tmp)
            tmp = code.newTmp(isInt(value->type()) ? Arg::GP : Arg::FP);
        return tmp;
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

    Arg loadAddr(Value* loadValue)
    {
        if (!canBeInternal(loadValue))
            return Arg();
        if (loadValue->opcode() == Load)
            return addr(loadValue);
        return Arg();
    }

    Arg imm(Value* value)
    {
        if (value->hasInt32())
            return Arg::imm(value->asInt32());
        return Arg();
    }

    Arg immAnyInt(Value* value)
    {
        if (value->hasInt()) {
            int64_t fullValue = value->asInt();
            int32_t immediateValue = static_cast<int32_t>(fullValue);
            if (fullValue == immediateValue)
                return Arg::imm(immediateValue);
        }
        return Arg();
    }

    Arg immOrTmp(Value* value)
    {
        if (Arg result = imm(value))
            return result;
        return tmp(value);
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
            Arg leftAddr = loadAddr(left);
            if (isValidForm(opcode, leftAddr.kind(), Arg::Tmp)) {
                commitInternal(left);
                append(relaxedMoveForType(currentValue->type()), tmp(right), result);
                append(opcode, leftAddr, result);
                return;
            }
        }

        Arg rightAddr = loadAddr(right);
        if (isValidForm(opcode, rightAddr.kind(), Arg::Tmp)) {
            commitInternal(right);
            append(relaxedMoveForType(currentValue->type()), tmp(left), result);
            append(opcode, rightAddr, result);
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

    template<Air::Opcode opcode, Commutativity commutativity = NotCommutative>
    bool tryAppendStoreBinOp(Value* left, Value* right)
    {
        // FIXME: This fails to check if there are any effects between the Load and the Store that
        // could clobber the loaded value. We need to check such things because we are effectively
        // sinking the load.
        // https://bugs.webkit.org/show_bug.cgi?id=150534
        
        Arg storeAddr = addr(currentValue);
        ASSERT(storeAddr);

        Value* loadValue;
        Value* otherValue;
        if (loadAddr(left) == storeAddr) {
            loadValue = left;
            otherValue = right;
        } else if (commutativity == Commutative && loadAddr(right) == storeAddr) {
            loadValue = right;
            otherValue = left;
        } else
            return false;

        if (isValidForm(opcode, Arg::Imm, storeAddr.kind()) && imm(otherValue)) {
            commitInternal(loadValue);
            append(opcode, imm(otherValue), storeAddr);
            return true;
        }

        if (!isValidForm(opcode, Arg::Tmp, storeAddr.kind()))
            return false;

        commitInternal(loadValue);
        append(opcode, tmp(otherValue), storeAddr);
        return true;
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
    
    IndexSet<Value> locked; // These are values that will have no Tmp in Air.
    IndexMap<Value, Tmp> valueToTmp; // These are values that must have a Tmp in Air. We say that a Value* with a non-null Tmp is "pinned".
    IndexMap<B3::BasicBlock, Air::BasicBlock*> blockToBlock;
    HashMap<StackSlotValue*, Air::StackSlot*> stackToStack;

    UseCounts useCounts;

    Vector<Vector<Inst, 4>> insts;
    Vector<Inst> prologue;

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
            // explicitly.
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
        Air::Opcode move = moveForType(value->type());
        Arg destination = effectiveAddr(address);

        Arg imm = immAnyInt(value);
        if (imm && isValidForm(move, Arg::Imm, destination.kind())) {
            append(moveForType(value->type()), imm, effectiveAddr(address, currentValue));
            return true;
        }
        
        append(moveForType(value->type()), tmp(value), effectiveAddr(address, currentValue));
        return true;
    }

    bool tryTruncArgumentReg(Value* argumentReg)
    {
        prologue.append(Inst(
            Move, currentValue,
            Tmp(argumentReg->as<ArgumentRegValue>()->argumentReg()),
            tmp(currentValue)));
        return true;
    }

    bool tryTrunc(Value* value)
    {
        append(Move, tmp(value), tmp(currentValue));
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

    bool tryIdentity(Value* value)
    {
        append(relaxedMoveForType(value->type()), immOrTmp(value), tmp(currentValue));
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

