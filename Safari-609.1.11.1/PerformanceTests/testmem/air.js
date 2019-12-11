/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
"use strict";

// This file is for misc symbols.

// B3 types
const Void = Symbol("Void");
const Int32 = Symbol("Int32");
const Int64 = Symbol("Int64");
const Float = Symbol("Float");
const Double = Symbol("Double");

// Arg type
const GP = Symbol("GP");
const FP = Symbol("FP");

// Stack slot kind
const Locked = Symbol("Locked");
const Spill = Symbol("Spill");

// Frequency class
const Normal = Symbol("Normal");
const Rare = Symbol("Rare");

// Relational conditions
const Equal = Symbol("Equal");
const NotEqual = Symbol("NotEqual");
const Above = Symbol("Above");
const AboveOrEqual = Symbol("AboveOrEqual");
const Below = Symbol("Below");
const BelowOrEqual = Symbol("BelowOrEqual");
const GreaterThan = Symbol("GreaterThan");
const GreaterThanOrEqual = Symbol("GreaterThanOrEqual");
const LessThan = Symbol("LessThan");
const LessThanOrEqual = Symbol("LessThanOrEqual");

function relCondCode(cond)
{
    switch (cond) {
    case Equal:
        return 4;
    case NotEqual:
        return 5;
    case Above:
        return 7;
    case AboveOrEqual:
        return 3;
    case Below:
        return 2;
    case BelowOrEqual:
        return 6;
    case GreaterThan:
        return 15;
    case GreaterThanOrEqual:
        return 13;
    case LessThan:
        return 12;
    case LessThanOrEqual:
        return 14;
    default:
        throw new Error("Bad rel cond");
    }
}

// Result conditions
const Overflow = Symbol("Overflow");
const Signed = Symbol("Signed");
const PositiveOrZero = Symbol("PositiveOrZero");
const Zero = Symbol("Zero");
const NonZero = Symbol("NonZero");

function resCondCode(cond)
{
    switch (cond) {
    case Overflow:
        return 0;
    case Signed:
        return 8;
    case PositiveOrZero:
        return 9;
    case Zero:
        return 4;
    case NonZero:
        return 5;
    default:
        throw new Error("Bad res cond: " + cond.toString());
    }
}

// Double conditions
const DoubleEqual = Symbol("DoubleEqual");
const DoubleNotEqual = Symbol("DoubleNotEqual");
const DoubleGreaterThan = Symbol("DoubleGreaterThan");
const DoubleGreaterThanOrEqual = Symbol("DoubleGreaterThanOrEqual");
const DoubleLessThan = Symbol("DoubleLessThan");
const DoubleLessThanOrEqual = Symbol("DoubleLessThanOrEqual");
const DoubleEqualOrUnordered = Symbol("DoubleEqualOrUnordered");
const DoubleNotEqualOrUnordered = Symbol("DoubleNotEqualOrUnordered");
const DoubleGreaterThanOrUnordered = Symbol("DoubleGreaterThanOrUnordered");
const DoubleGreaterThanOrEqualOrUnordered = Symbol("DoubleGreaterThanOrEqualOrUnordered");
const DoubleLessThanOrUnordered = Symbol("DoubleLessThanOrUnordered");
const DoubleLessThanOrEqualOrUnordered = Symbol("DoubleLessThanOrEqualOrUnordered");

function doubleCondCode(cond)
{
    const bitInvert = 0x10;
    const bitSpecial = 0x20;
    switch (cond) {
    case DoubleEqual:
        return 4 | bitSpecial;
    case DoubleNotEqual:
        return 5;
    case DoubleGreaterThan:
        return 7;
    case DoubleGreaterThanOrEqual:
        return 3;
    case DoubleLessThan:
        return 7 | bitInvert;
    case DoubleLessThanOrEqual:
        return 3 | bitInvert;
    case DoubleEqualOrUnordered:
        return 4;
    case DoubleNotEqualOrUnordered:
        return 5 | bitSpecial;
    case DoubleGreaterThanOrUnordered:
        return 2 | bitInvert;
    case DoubleGreaterThanOrEqualOrUnordered:
        return 6 | bitInvert;
    case DoubleLessThanOrUnordered:
        return 2;
    case DoubleLessThanOrEqualOrUnordered:
        return 6;
    default:
        throw new Error("Bad cond");
    }
}

// Define pointerType()
const Ptr = 64;

class TmpBase {
    get isGP() { return this.type == GP; }
    get isFP() { return this.type == FP; }
    
    get isGPR() { return this.isReg && this.isGP; }
    get isFPR() { return this.isReg && this.isFP; }
    
    get reg()
    {
        if (!this.isReg)
            throw new Error("Called .reg on non-Reg");
        return this;
    }

    get gpr()
    {
        if (!this.isGPR)
            throw new Error("Called .gpr on non-GPR");
        return this;
    }
    
    get fpr()
    {
        if (!this.isFPR)
            throw new Error("Called .fpr on non-FPR");
        return this;
    }
}

class Arg {
    constructor()
    {
        this._kind = Arg.Invalid;
    }
    
    static isAnyUse(role)
    {
        switch (role) {
        case Arg.Use:
        case Arg.ColdUse:
        case Arg.UseDef:
        case Arg.UseZDef:
        case Arg.LateUse:
        case Arg.LateColdUse:
        case Arg.Scratch:
            return true;
        case Arg.Def:
        case Arg.ZDef:
        case Arg.UseAddr:
        case Arg.EarlyDef:
            return false;
        default:
            throw new Error("Bad role");
        }
    }
    
    static isColdUse(role)
    {
        switch (role) {
        case Arg.ColdUse:
        case Arg.LateColdUse:
            return true;
        case Arg.Use:
        case Arg.UseDef:
        case Arg.UseZDef:
        case Arg.LateUse:
        case Arg.Def:
        case Arg.ZDef:
        case Arg.UseAddr:
        case Arg.Scratch:
        case Arg.EarlyDef:
            return false;
        default:
            throw new Error("Bad role");
        }
    }
    
    static isWarmUse(role)
    {
        return Arg.isAnyUse(role) && !Arg.isColdUse(role);
    }
    
    static cooled(role)
    {
        switch (role) {
        case Arg.ColdUse:
        case Arg.LateColdUse:
        case Arg.UseDef:
        case Arg.UseZDef:
        case Arg.Def:
        case Arg.ZDef:
        case Arg.UseAddr:
        case Arg.Scratch:
        case Arg.EarlyDef:
            return role;
        case Arg.Use:
            return Arg.ColdUse;
        case Arg.LateUse:
            return Arg.LateColdUse;
        default:
            throw new Error("Bad role");
        }
    }

    static isEarlyUse(role)
    {
        switch (role) {
        case Arg.Use:
        case Arg.ColdUse:
        case Arg.UseDef:
        case Arg.UseZDef:
            return true;
        case Arg.Def:
        case Arg.ZDef:
        case Arg.UseAddr:
        case Arg.LateUse:
        case Arg.LateColdUse:
        case Arg.Scratch:
        case Arg.EarlyDef:
            return false;
        default:
            throw new Error("Bad role");
        }
    }
    
    static isLateUse(role)
    {
        switch (role) {
        case Arg.LateUse:
        case Arg.LateColdUse:
        case Arg.Scratch:
            return true;
        case Arg.ColdUse:
        case Arg.Use:
        case Arg.UseDef:
        case Arg.UseZDef:
        case Arg.Def:
        case Arg.ZDef:
        case Arg.UseAddr:
        case Arg.EarlyDef:
            return false;
        default:
            throw new Error("Bad role");
        }
    }
    
    static isAnyDef(role)
    {
        switch (role) {
        case Arg.Use:
        case Arg.ColdUse:
        case Arg.UseAddr:
        case Arg.LateUse:
        case Arg.LateColdUse:
            return false;
        case Arg.Def:
        case Arg.UseDef:
        case Arg.ZDef:
        case Arg.UseZDef:
        case Arg.EarlyDef:
        case Arg.Scratch:
            return true;
        default:
            throw new Error("Bad role");
        }
    }
    
    static isEarlyDef(role)
    {
        switch (role) {
        case Arg.Use:
        case Arg.ColdUse:
        case Arg.UseAddr:
        case Arg.LateUse:
        case Arg.Def:
        case Arg.UseDef:
        case Arg.ZDef:
        case Arg.UseZDef:
        case Arg.LateColdUse:
            return false;
        case Arg.EarlyDef:
        case Arg.Scratch:
            return true;
        default:
            throw new Error("Bad role");
        }
    }
    
    static isLateDef(role)
    {
        switch (role) {
        case Arg.Use:
        case Arg.ColdUse:
        case Arg.UseAddr:
        case Arg.LateUse:
        case Arg.EarlyDef:
        case Arg.Scratch:
        case Arg.LateColdUse:
            return false;
        case Arg.Def:
        case Arg.UseDef:
        case Arg.ZDef:
        case Arg.UseZDef:
            return true;
        default:
            throw new Error("Bad role");
        }
    }
    
    static isZDef(role)
    {
        switch (role) {
        case Arg.Use:
        case Arg.ColdUse:
        case Arg.UseAddr:
        case Arg.LateUse:
        case Arg.Def:
        case Arg.UseDef:
        case Arg.EarlyDef:
        case Arg.Scratch:
        case Arg.LateColdUse:
            return false;
        case Arg.ZDef:
        case Arg.UseZDef:
            return true;
        default:
            throw new Error("Bad role");
        }
    }
    
    static typeForB3Type(type)
    {
        switch (type) {
        case Int32:
        case Int64:
            return GP;
        case Float:
        case Double:
            return FP;
        default:
            throw new Error("Bad B3 type");
        }
    }
    
    static widthForB3Type(type)
    {
        switch (type) {
        case Int32:
        case Float:
            return 32;
        case Int64:
        case Double:
            return 64;
        default:
            throw new Error("Bad B3 type");
        }
    }
    
    static conservativeWidth(type)
    {
        return type == GP ? Ptr : 64;
    }
    
    static minimumWidth(type)
    {
        return type == GP ? 8 : 32;
    }
    
    static bytes(width)
    {
        return width / 8;
    }
    
    static widthForBytes(bytes)
    {
        switch (bytes) {
        case 0:
        case 1:
            return 8;
        case 2:
            return 16;
        case 3:
        case 4:
            return 32;
        default:
            if (bytes > 8)
                throw new Error("Bad number of bytes");
            return 64;
        }
    }
    
    static createTmp(tmp)
    {
        let result = new Arg();
        result._kind = Arg.Tmp;
        result._tmp = tmp;
        return result;
    }
    
    static fromReg(reg)
    {
        return Arg.createTmp(reg);
    }
    
    static createImm(value)
    {
        let result = new Arg();
        result._kind = Arg.Imm;
        result._value = value;
        return result;
    }
    
    static createBigImm(lowValue, highValue = 0)
    {
        let result = new Arg();
        result._kind = Arg.BigImm;
        result._lowValue = lowValue;
        result._highValue = highValue;
        return result;
    }
    
    static createBitImm(value)
    {
        let result = new Arg();
        result._kind = Arg.BitImm;
        result._value = value;
        return result;
    }
    
    static createBitImm64(lowValue, highValue = 0)
    {
        let result = new Arg();
        result._kind = Arg.BitImm64;
        result._lowValue = lowValue;
        result._highValue = highValue;
        return result;
    }
    
    static createAddr(base, offset = 0)
    {
        let result = new Arg();
        result._kind = Arg.Addr;
        result._base = base;
        result._offset = offset;
        return result;
    }
    
    static createStack(slot, offset = 0)
    {
        let result = new Arg();
        result._kind = Arg.Stack;
        result._slot = slot;
        result._offset = offset;
        return result;
    }
    
    static createCallArg(offset)
    {
        let result = new Arg();
        result._kind = Arg.CallArg;
        result._offset = offset;
        return result;
    }
    
    static createStackAddr(offsetFromFP, frameSize, width)
    {
        let result = Arg.createAddr(Reg.callFrameRegister, offsetFromFP);
        if (!result.isValidForm(width))
            result = Arg.createAddr(Reg.stackPointerRegister, offsetFromFP + frameSize);
        return result;
    }
    
    static isValidScale(scale, width)
    {
        switch (scale) {
        case 1:
        case 2:
        case 4:
        case 8:
            return true;
        default:
            return false;
        }
    }
    
    static logScale(scale)
    {
        switch (scale) {
        case 1:
            return 0;
        case 2:
            return 1;
        case 4:
            return 2;
        case 8:
            return 3;
        default:
            throw new Error("Bad scale");
        }
    }
    
    static createIndex(base, index, scale = 1, offset = 0)
    {
        let result = new Arg();
        result._kind = Arg.Index;
        result._base = base;
        result._index = index;
        result._scale = scale;
        result._offset = offset;
        return result;
    }
    
    static createRelCond(condition)
    {
        let result = new Arg();
        result._kind = Arg.RelCond;
        result._condition = condition;
        return result;
    }
    
    static createResCond(condition)
    {
        let result = new Arg();
        result._kind = Arg.ResCond;
        result._condition = condition;
        return result;
    }
    
    static createDoubleCond(condition)
    {
        let result = new Arg();
        result._kind = Arg.DoubleCond;
        result._condition = condition;
        return result;
    }
    
    static createWidth(width)
    {
        let result = new Arg();
        result._kind = Arg.Width;
        result._width = width;
        return result;
    }
    
    static createSpecial()
    {
        let result = new Arg();
        result._kind = Arg.Special;
        return result;
    }
    
    get kind() { return this._kind; }
    get isTmp() { return this._kind == Arg.Tmp; }
    get isImm() { return this._kind == Arg.Imm; }
    get isBigImm() { return this._kind == Arg.BigImm; }
    get isBitImm() { return this._kind == Arg.BitImm; }
    get isBitImm64() { return this._kind == Arg.BitImm64; }
    get isSomeImm()
    {
        switch (this._kind) {
        case Arg.Imm:
        case Arg.BitImm:
            return true;
        default:
            return false;
        }
    }
    get isSomeBigImm()
    {
        switch (this._kind) {
        case Arg.BigImm:
        case Arg.BitImm64:
            return true;
        default:
            return false;
        }
    }
    get isAddr() { return this._kind == Arg.Addr; }
    get isStack() { return this._kind == Arg.Stack; }
    get isCallArg() { return this._kind == Arg.CallArg; }
    get isIndex() { return this._kind == Arg.Index; }
    get isMemory()
    {
        switch (this._kind) {
        case Arg.Addr:
        case Arg.Stack:
        case Arg.CallArg:
        case Arg.Index:
            return true;
        default:
            return false;
        }
    }
    get isStackMemory()
    {
        switch (this._kind) {
        case Arg.Addr:
            return this._base == Reg.callFrameRegister
                || this._base == Reg.stackPointerRegister;
        case Arg.Stack:
        case Arg.CallArg:
            return true;
        default:
            return false;
        }
    }
    get isRelCond() { return this._kind == Arg.RelCond; }
    get isResCond() { return this._kind == Arg.ResCond; }
    get isDoubleCond() { return this._kind == Arg.DoubleCond; }
    get isCondition()
    {
        switch (this._kind) {
        case Arg.RelCond:
        case Arg.ResCond:
        case Arg.DoubleCond:
            return true;
        default:
            return false;
        }
    }
    get isWidth() { return this._kind == Arg.Width; }
    get isSpecial() { return this._kind == Arg.Special; }
    get isAlive() { return this.isTmp || this.isStack; }
    
    get tmp()
    {
        if (this._kind != Arg.Tmp)
            throw new Error("Called .tmp for non-tmp");
        return this._tmp;
    }
    
    get value()
    {
        if (!this.isSomeImm)
            throw new Error("Called .value for non-imm");
        return this._value;
    }
    
    get lowValue()
    {
        if (!this.isSomeBigImm)
            throw new Error("Called .lowValue for non-big-imm");
        return this._lowValue;
    }
    
    get highValue()
    {
        if (!this.isSomeBigImm)
            throw new Error("Called .highValue for non-big-imm");
        return this._highValue;
    }
    
    get base()
    {
        switch (this._kind) {
        case Arg.Addr:
        case Arg.Index:
            return this._base;
        default:
            throw new Error("Called .base for non-address");
        }
    }
    
    get hasOffset() { return this.isMemory; }
    
    get offset()
    {
        switch (this._kind) {
        case Arg.Addr:
        case Arg.Index:
        case Arg.Stack:
        case Arg.CallArg:
            return this._offset;
        default:
            throw new Error("Called .offset for non-address");
        }
    }
    
    get stackSlot()
    {
        if (this._kind != Arg.Stack)
            throw new Error("Called .stackSlot for non-address");
        return this._slot;
    }
    
    get index()
    {
        if (this._kind != Arg.Index)
            throw new Error("Called .index for non-Index");
        return this._index;
    }
    
    get scale()
    {
        if (this._kind != Arg.Index)
            throw new Error("Called .scale for non-Index");
        return this._scale;
    }
    
    get logScale()
    {
        return Arg.logScale(this.scale);
    }
    
    get width()
    {
        if (this._kind != Arg.Width)
            throw new Error("Called .width for non-Width");
        return this._width;
    }
    
    get isGPTmp() { return this.isTmp && this.tmp.isGP; }
    get isFPTmp() { return this.isTmp && this.tmp.isFP; }
    
    get isGP()
    {
        switch (this._kind) {
        case Arg.Imm:
        case Arg.BigImm:
        case Arg.BitImm:
        case Arg.BitImm64:
        case Arg.Addr:
        case Arg.Index:
        case Arg.Stack:
        case Arg.CallArg:
        case Arg.RelCond:
        case Arg.ResCond:
        case Arg.DoubleCond:
        case Arg.Width:
        case Arg.Special:
            return true;
        case Arg.Tmp:
            return this.isGPTmp;
        case Arg.Invalid:
            return false;
        default:
            throw new Error("Bad kind");
        }
    }
    
    get isFP()
    {
        switch (this._kind) {
        case Arg.Imm:
        case Arg.BitImm:
        case Arg.BitImm64:
        case Arg.RelCond:
        case Arg.ResCond:
        case Arg.DoubleCond:
        case Arg.Width:
        case Arg.Special:
        case Arg.Invalid:
            return false;
        case Arg.Addr:
        case Arg.Index:
        case Arg.Stack:
        case Arg.CallArg:
        case Arg.BigImm:
            return true;
        case Arg.Tmp:
            return this.isFPTmp;
        default:
            throw new Error("Bad kind");
        }
    }
    
    get hasType()
    {
        switch (this._kind) {
        case Arg.Imm:
        case Arg.BitImm:
        case Arg.BitImm64:
        case Arg.Tmp:
            return true;
        default:
            return false;
        }
    }
    
    get type()
    {
        return this.isGP ? GP : FP;
    }
    
    isType(type)
    {
        switch (type) {
        case Arg.GP:
            return this.isGP;
        case Arg.FP:
            return this.isFP;
        default:
            throw new Error("Bad type");
        }
    }
    
    isCompatibleType(other)
    {
        if (this.hasType)
            return other.isType(this.type);
        if (other.hasType)
            return this.isType(other.type);
        return true;
    }
    
    get isGPR() { return this.isTmp && this.tmp.isGPR; }
    get gpr() { return this.tmp.gpr; }
    get isFPR() { return this.isTmp && this.tmp.isFPR; }
    get fpr() { return this.tmp.fpr; }
    get isReg() { return this.isTmp && this.tmp.isReg; }
    get reg() { return this.tmp.reg; }
    
    static isValidImmForm(value)
    {
        return isRepresentableAsInt32(value);
    }
    static isValidBitImmForm(value)
    {
        return isRepresentableAsInt32(value);
    }
    static isValidBitImm64Form(value)
    {
        return isRepresentableAsInt32(value);
    }
    
    static isValidAddrForm(offset, width)
    {
        return true;
    }
    
    static isValidIndexForm(scale, offset, width)
    {
        if (!isValidScale(scale, width))
            return false;
        return true;
    }
    
    isValidForm(width)
    {
        switch (this._kind) {
        case Arg.Invalid:
            return false;
        case Arg.Tmp:
            return true;
        case Arg.Imm:
            return Arg.isValidImmForm(this.value);
        case Arg.BigImm:
            return true;
        case Arg.BitImm:
            return Arg.isValidBitImmForm(this.value);
        case Arg.BitImm64:
            return Arg.isValidBitImm64Form(this.value);
        case Arg.Addr:
        case Arg.Stack:
        case Arg.CallArg:
            return Arg.isValidAddrForm(this.offset, width);
        case Arg.Index:
            return Arg.isValidIndexForm(this.scale, this.offset, width);
        case Arg.RelCond:
        case Arg.ResCond:
        case Arg.DoubleCond:
        case Arg.Width:
        case Arg.Special:
            return true;
        default:
            throw new Error("Bad kind");
        }
    }
    
    forEachTmpFast(func)
    {
        switch (this._kind) {
        case Arg.Tmp: {
            let replacement;
            if (replacement = func(this._tmp))
                return Arg.createTmp(replacement);
            break;
        }
        case Arg.Addr: {
            let replacement;
            if (replacement = func(this._base))
                return Arg.createAddr(replacement, this._offset);
            break;
        }
        case Arg.Index: {
            let baseReplacement = func(this._base);
            let indexReplacement = func(this._index);
            if (baseReplacement || indexReplacement) {
                return Arg.createIndex(
                    baseReplacement ? baseReplacement : this._base,
                    indexReplacement ? indexReplacement : this._index,
                    this._scale, this._offset);
            }
            break;
        }
        default:
            break;
        }
    }
    
    usesTmp(expectedTmp)
    {
        let usesTmp = false;
        forEachTmpFast(tmp => {
            usesTmp |= tmp == expectedTmp;
        });
        return usesTmp;
    }
    
    forEachTmp(role, type, width, func)
    {
        switch (this._kind) {
        case Arg.Tmp: {
            let replacement;
            if (replacement = func(this._tmp, role, type, width))
                return Arg.createTmp(replacement);
            break;
        }
        case Arg.Addr: {
            let replacement;
            if (replacement = func(this._base, Arg.Use, GP, role == Arg.UseAddr ? width : Ptr))
                return Arg.createAddr(replacement, this._offset);
            break;
        }
        case Arg.Index: {
            let baseReplacement = func(this._base, Arg.Use, GP, role == Arg.UseAddr ? width : Ptr);
            let indexReplacement = func(this._index, Arg.Use, GP, role == Arg.UseAddr ? width : Ptr);
            if (baseReplacement || indexReplacement) {
                return Arg.createIndex(
                    baseReplacement ? baseReplacement : this._base,
                    indexReplacement ? indexReplacement : this._index,
                    this._scale, this._offset);
            }
            break;
        }
        default:
            break;
        }
    }
    
    is(thing) { return !!thing.extract(this); }
    as(thing) { return thing.extract(this); }
    
    // This lets you say things like:
    // arg.forEach(Tmp | Reg | Arg | StackSlot, ...)
    //
    // It's used for abstract liveness analysis.
    forEachFast(thing, func)
    {
        return thing.forEachFast(this, func);
    }
    forEach(thing, role, type, width, func)
    {
        return thing.forEach(this, role, type, width, func);
    }
    
    static extract(arg) { return arg; }
    static forEachFast(arg, func) { return func(arg); }
    static forEach(arg, role, type, width, func) { return func(arg, role, type, width); }

    get condition()
    {
        switch (this._kind) {
        case Arg.RelCond:
        case Arg.ResCond:
        case Arg.DoubleCond:
            return this._condition;
        default:
            throw new Error("Called .condition for non-condition");
        }
    }
    
    get isInvertible()
    {
        switch (this._kind) {
        case Arg.RelCond:
        case Arg.DoubleCold:
            return true;
        case Arg.ResCond:
            switch (this._condition) {
            case Zero:
            case NonZero:
            case Signed:
            case PositiveOrZero:
                return true;
            default:
                return false;
            }
        default:
            return false;
        }
    }
    
    static kindCode(kind)
    {
        switch (kind) {
        case Arg.Invalid:
            return 0;
        case Arg.Tmp:
            return 1;
        case Arg.Imm:
            return 2;
        case Arg.BigImm:
            return 3;
        case Arg.BitImm:
            return 4;
        case Arg.BitImm64:
            return 5;
        case Arg.Addr:
            return 6;
        case Arg.Stack:
            return 7;
        case Arg.CallArg:
            return 8;
        case Arg.Index:
            return 9;
        case Arg.RelCond:
            return 10;
        case Arg.ResCond:
            return 11;
        case Arg.DoubleCond:
            return 12;
        case Arg.Special:
            return 13;
        case Arg.WidthArg:
            return 14;
        default:
            throw new Error("Bad kind");
        }
    }
    
    hash()
    {
        let result = Arg.kindCode(this._kind);
        
        switch (this._kind) {
        case Arg.Invalid:
        case Arg.Special:
            break;
        case Arg.Tmp:
            result += this._tmp.hash();
            result |= 0;
            break;
        case Arg.Imm:
        case Arg.BitImm:
            result += this._value;
            result |= 0;
            break;
        case Arg.BigImm:
        case Arg.BitImm64:
            result += this._lowValue;
            result |= 0;
            result += this._highValue;
            result |= 0;
            break;
        case Arg.CallArg:
            result += this._offset;
            result |= 0;
            break;
        case Arg.RelCond:
            result += relCondCode(this._condition);
            result |= 0;
            break;
        case Arg.ResCond:
            result += resCondCode(this._condition);
            result |= 0;
            break;
        case Arg.DoubleCond:
            result += doubleCondCode(this._condition);
            result |= 0;
            break;
        case Arg.WidthArg:
            result += this._width;
            result |= 0;
            break;
        case Arg.Addr:
            result += this._offset;
            result |= 0;
            result += this._base.hash();
            result |= 0;
            break;
        case Arg.Index:
            result += this._offset;
            result |= 0;
            result += this._scale;
            result |= 0;
            result += this._base.hash();
            result |= 0;
            result += this._index.hash();
            result |= 0;
            break;
        case Arg.Stack:
            result += this._offset;
            result |= 0;
            result += this.stackSlot.index;
            result |= 0;
            break;
        }
        
        return result >>> 0;
    }
    
    toString()
    {
        switch (this._kind) {
        case Arg.Invalid:
            return "<invalid>";
        case Arg.Tmp:
            return this._tmp.toString();
        case Arg.Imm:
            return "$" + this._value;
        case Arg.BigImm:
        case Arg.BitImm64:
            return "$0x" + this._highValue.toString(16) + ":" + this._lowValue.toString(16);
        case Arg.Addr:
            return "" + (this._offset ? this._offset : "") + "(" + this._base + ")";
        case Arg.Index:
            return "" + (this._offset ? this._offset : "") + "(" + this._base +
                "," + this._index + (this._scale == 1 ? "" : "," + this._scale) + ")";
        case Arg.Stack:
            return "" + (this._offset ? this._offset : "") + "(" + this._slot + ")";
        case Arg.CallArg:
            return "" + (this._offset ? this._offset : "") + "(callArg)";
        case Arg.RelCond:
        case Arg.ResCond:
        case Arg.DoubleCond:
            return symbolName(this._condition);
        case Arg.Special:
            return "special";
        case Arg.Width:
            return "" + this._value;
        default:
            throw new Error("Bad kind");
        }
    }
}

// Arg kinds
Arg.Invalid = Symbol("Invalid");
Arg.Tmp = Symbol("Tmp");
Arg.Imm = Symbol("Imm");
Arg.BigImm = Symbol("BigImm");
Arg.BitImm = Symbol("BitImm");
Arg.BitImm64 = Symbol("BitImm64");
Arg.Addr = Symbol("Addr");
Arg.Stack = Symbol("Stack");
Arg.CallArg = Symbol("CallArg");
Arg.Index = Symbol("Index");
Arg.RelCond = Symbol("RelCond");
Arg.ResCond = Symbol("ResCond");
Arg.DoubleCond = Symbol("DoubleCond");
Arg.Special = Symbol("Special");
Arg.Width = Symbol("Width");

// Arg roles
Arg.Use = Symbol("Use");
Arg.ColdUse = Symbol("ColdUse");
Arg.LateUse = Symbol("LateUse");
Arg.LateColdUse = Symbol("LateColdUse");
Arg.Def = Symbol("Def");
Arg.ZDef = Symbol("ZDef");
Arg.UseDef = Symbol("UseDef");
Arg.UseZDef = Symbol("UseZDef");
Arg.EarlyDef = Symbol("EarlyDef");
Arg.Scratch = Symbol("Scratch");
Arg.UseAddr = Symbol("UseAddr");

class BasicBlock {
    constructor(index, frequency)
    {
        this._index = index;
        this._frequency = frequency;
        this._insts = [];
        this._successors = [];
        this._predecessors = [];
    }
    
    get index() { return this._index; }
    get size() { return this._insts.length; }
    
    [Symbol.iterator]()
    {
        return this._insts[Symbol.iterator]();
    }
    
    at(index)
    {
        if (index >= this._insts.length)
            throw new Error("Out of bounds access");
        return this._insts[index];
    }
    
    get(index)
    {
        if (index < 0 || index >= this._insts.length)
            return null;
        return this._insts[index];
    }
    
    get last()
    {
        return this._insts[this._insts.length - 1];
    }
    
    get insts() { return this._insts; }
    
    append(inst) { this._insts.push(inst); }
    
    get numSuccessors() { return this._successors.length; }
    successor(index) { return this._successors[index]; }
    get successors() { return this._successors; }
    
    successorBlock(index) { return this._successors[index].block; }
    get successorBlocks()
    {
        return new Proxy(this._successors, {
            get(target, property) {
                if (typeof property == "string"
                    && (property | 0) == property)
                    return target[property].block;
                return target[property];
            },
            
            set(target, property, value) {
                if (typeof property == "string"
                    && (property | 0) == property) {
                    var oldValue = target[property];
                    target[property] = new FrequentedBlock(
                        value, oldValue ? oldValue.frequency : Normal);
                    return;
                }
                
                target[property] = value;
            }
        });
    }
    
    get numPredecessors() { return this._predecessors.length; }
    predecessor(index) { return this._predecessors[index]; }
    get predecessors() { return this._predecessors; }
    
    get frequency() { return this._frequency; }

    toString()
    {
        return "#" + this._index;
    }
    
    get headerString()
    {
        let result = "";
        result += `BB${this}: ; frequency = ${this._frequency}\n`;
        if (this._predecessors.length)
            result += "  Predecessors: " + this._predecessors.join(", ") + "\n";
        return result;
    }
    
    get footerString()
    {
        let result = "";
        if (this._successors.length)
            result += "  Successors: " + this._successors.join(", ") + "\n";
        return result;
    }
    
    toStringDeep()
    {
        let result = "";
        result += this.headerString;
        for (let inst of this)
            result += `    ${inst}\n`;
        result += this.footerString;
        return result;
    }
}


class Code {
    constructor()
    {
        this._blocks = [];
        this._stackSlots = [];
        this._gpTmps = [];
        this._fpTmps = [];
        this._callArgAreaSize = 0;
        this._frameSize = 0;
    }
    
    addBlock(frequency = 1)
    {
        return addIndexed(this._blocks, BasicBlock, frequency);
    }
    
    addStackSlot(byteSize, kind)
    {
        return addIndexed(this._stackSlots, StackSlot, byteSize, kind);
    }
    
    newTmp(type)
    {
        return addIndexed(this[`_${lowerSymbolName(type)}Tmps`], Tmp, type);
    }
    
    get size() { return this._blocks.length; }
    at(index) { return this._blocks[index]; }
    
    [Symbol.iterator]()
    {
        return this._blocks[Symbol.iterator]();
    }
    
    get blocks() { return this._blocks; }
    get stackSlots() { return this._stackSlots; }
    
    tmps(type) { return this[`_${lowerSymbolName(type)}Tmps`]; }
    
    get callArgAreaSize() { return this._callArgAreaSize; }
    
    requestCallArgAreaSize(size)
    {
        this._callArgAreaSize = Math.max(this._callArgAreaSize, roundUpToMultipleOf(stackAlignmentBytes, size));
    }
    
    get frameSize() { return this._frameSize; }
    
    setFrameSize(frameSize) { this._frameSize = frameSize; }
    
    hash()
    {
        let result = 0;
        for (let block of this) {
            result *= 1000001;
            result |= 0;
            for (let inst of block) {
                result *= 97;
                result |= 0;
                result += inst.hash();
                result |= 0;
            }
            for (let successor of block.successorBlocks) {
                result *= 7;
                result |= 0;
                result += successor.index;
                result |= 0;
            }
        }
        for (let slot of this.stackSlots) {
            result *= 101;
            result |= 0;
            result += slot.hash();
            result |= 0;
        }
        return result >>> 0;
    }
    
    toString()
    {
        let result = "";
        for (let block of this) {
            result += block.toStringDeep();
        }
        if (this.stackSlots.length) {
            result += "Stack slots:\n";
            for (let slot of this.stackSlots)
                result += `    ${slot}\n`;
        }
        if (this._frameSize)
            result += `Frame size: ${this._frameSize}\n`;
        if (this._callArgAreaSize)
            result += `Call arg area size: ${this._callArgAreaSize}\n`;
        return result;
    }
}

class FrequentedBlock {
    constructor(block, frequency)
    {
        this.block = block;
        this.frequency = frequency;
    }
    
    toString()
    {
        return (this.frequency == Normal ? "" : "Rare:") + this.block;
    }
}

class Inst {
    constructor(opcode, args = [])
    {
        this._opcode = opcode;
        this._args = args;
    }
    
    append(...args)
    {
        this._args.push(...args);
    }
    
    clear()
    {
        this._opcode = Nop;
        this._args = [];
    }
    
    get opcode() { return this._opcode; }
    get args() { return this._args; }
    
    visitArg(index, func, ...args)
    {
        let replacement = func(this._args[index], ...args);
        if (replacement)
            this._args[index] = replacement;
    }
    
    forEachTmpFast(func)
    {
        for (let i = 0; i < this._args.length; ++i) {
            let replacement;
            if (replacement = this._args[i].forEachTmpFast(func))
                this._args[i] = replacement;
        }
    }
    
    forEachArg(func)
    {
        Inst_forEachArg(this, func);
    }
    
    forEachTmp(func)
    {
        this.forEachArg((arg, role, type, width) => {
            return arg.forEachTmp(role, type, width, func);
        });
    }
    
    forEach(thing, func)
    {
        this.forEachArg((arg, role, type, width) => {
            return arg.forEach(thing, role, type, width, func);
        });
    }
    
    static forEachDef(thing, prevInst, nextInst, func)
    {
        if (prevInst) {
            prevInst.forEach(
                thing,
                (value, role, type, width) => {
                    if (Arg.isLateDef(role))
                        return func(value, role, type, width);
                });
        }
        
        if (nextInst) {
            nextInst.forEach(
                thing,
                (value, role, type, width) => {
                    if (Arg.isEarlyDef(role))
                        return func(value, role, type, width);
                });
        }
    }
    
    static forEachDefWithExtraClobberedRegs(thing, prevInst, nextInst, func)
    {
        forEachDef(thing, prevInst, nextInst, func);
        
        let regDefRole;
        
        let reportReg = reg => {
            let type = reg.isGPR ? GP : FP;
            func(thing.fromReg(reg), regDefRole, type, Arg.conservativeWidth(type));
        };
        
        if (prevInst && prevInst.opcode == Patch) {
            regDefRole = Arg.Def;
            prevInst.extraClobberedRegs.forEach(reportReg);
        }
        
        if (nextInst && nextInst.opcode == Patch) {
            regDefRole = Arg.EarlyDef;
            nextInst.extraEarlyClobberedRegs.forEach(reportReg);
        }
    }
    
    get hasNonArgEffects() { return Inst_hasNonArgEffects(this); }
    
    hash()
    {
        let result = opcodeCode(this.opcode);
        for (let arg of this.args) {
            result += arg.hash();
            result |= 0;
        }
        return result >>> 0;
    }
    
    toString()
    {
        return "" + symbolName(this._opcode) + " " + this._args.join(", ");
    }
}

"use strict";
// Generated by opcode_generator.rb from JavaScriptCore/b3/air/AirOpcode.opcodes -- do not edit!
const Nop = Symbol("Nop");
const Add32 = Symbol("Add32");
const Add8 = Symbol("Add8");
const Add16 = Symbol("Add16");
const Add64 = Symbol("Add64");
const AddDouble = Symbol("AddDouble");
const AddFloat = Symbol("AddFloat");
const Sub32 = Symbol("Sub32");
const Sub64 = Symbol("Sub64");
const SubDouble = Symbol("SubDouble");
const SubFloat = Symbol("SubFloat");
const Neg32 = Symbol("Neg32");
const Neg64 = Symbol("Neg64");
const NegateDouble = Symbol("NegateDouble");
const Mul32 = Symbol("Mul32");
const Mul64 = Symbol("Mul64");
const MultiplyAdd32 = Symbol("MultiplyAdd32");
const MultiplyAdd64 = Symbol("MultiplyAdd64");
const MultiplySub32 = Symbol("MultiplySub32");
const MultiplySub64 = Symbol("MultiplySub64");
const MultiplyNeg32 = Symbol("MultiplyNeg32");
const MultiplyNeg64 = Symbol("MultiplyNeg64");
const Div32 = Symbol("Div32");
const Div64 = Symbol("Div64");
const MulDouble = Symbol("MulDouble");
const MulFloat = Symbol("MulFloat");
const DivDouble = Symbol("DivDouble");
const DivFloat = Symbol("DivFloat");
const X86ConvertToDoubleWord32 = Symbol("X86ConvertToDoubleWord32");
const X86ConvertToQuadWord64 = Symbol("X86ConvertToQuadWord64");
const X86Div32 = Symbol("X86Div32");
const X86Div64 = Symbol("X86Div64");
const Lea = Symbol("Lea");
const And32 = Symbol("And32");
const And64 = Symbol("And64");
const AndDouble = Symbol("AndDouble");
const AndFloat = Symbol("AndFloat");
const XorDouble = Symbol("XorDouble");
const XorFloat = Symbol("XorFloat");
const Lshift32 = Symbol("Lshift32");
const Lshift64 = Symbol("Lshift64");
const Rshift32 = Symbol("Rshift32");
const Rshift64 = Symbol("Rshift64");
const Urshift32 = Symbol("Urshift32");
const Urshift64 = Symbol("Urshift64");
const Or32 = Symbol("Or32");
const Or64 = Symbol("Or64");
const Xor32 = Symbol("Xor32");
const Xor64 = Symbol("Xor64");
const Not32 = Symbol("Not32");
const Not64 = Symbol("Not64");
const AbsDouble = Symbol("AbsDouble");
const AbsFloat = Symbol("AbsFloat");
const CeilDouble = Symbol("CeilDouble");
const CeilFloat = Symbol("CeilFloat");
const FloorDouble = Symbol("FloorDouble");
const FloorFloat = Symbol("FloorFloat");
const SqrtDouble = Symbol("SqrtDouble");
const SqrtFloat = Symbol("SqrtFloat");
const ConvertInt32ToDouble = Symbol("ConvertInt32ToDouble");
const ConvertInt64ToDouble = Symbol("ConvertInt64ToDouble");
const ConvertInt32ToFloat = Symbol("ConvertInt32ToFloat");
const ConvertInt64ToFloat = Symbol("ConvertInt64ToFloat");
const CountLeadingZeros32 = Symbol("CountLeadingZeros32");
const CountLeadingZeros64 = Symbol("CountLeadingZeros64");
const ConvertDoubleToFloat = Symbol("ConvertDoubleToFloat");
const ConvertFloatToDouble = Symbol("ConvertFloatToDouble");
const Move = Symbol("Move");
const Swap32 = Symbol("Swap32");
const Swap64 = Symbol("Swap64");
const Move32 = Symbol("Move32");
const StoreZero32 = Symbol("StoreZero32");
const SignExtend32ToPtr = Symbol("SignExtend32ToPtr");
const ZeroExtend8To32 = Symbol("ZeroExtend8To32");
const SignExtend8To32 = Symbol("SignExtend8To32");
const ZeroExtend16To32 = Symbol("ZeroExtend16To32");
const SignExtend16To32 = Symbol("SignExtend16To32");
const MoveFloat = Symbol("MoveFloat");
const MoveDouble = Symbol("MoveDouble");
const MoveZeroToDouble = Symbol("MoveZeroToDouble");
const Move64ToDouble = Symbol("Move64ToDouble");
const Move32ToFloat = Symbol("Move32ToFloat");
const MoveDoubleTo64 = Symbol("MoveDoubleTo64");
const MoveFloatTo32 = Symbol("MoveFloatTo32");
const Load8 = Symbol("Load8");
const Store8 = Symbol("Store8");
const Load8SignedExtendTo32 = Symbol("Load8SignedExtendTo32");
const Load16 = Symbol("Load16");
const Load16SignedExtendTo32 = Symbol("Load16SignedExtendTo32");
const Store16 = Symbol("Store16");
const Compare32 = Symbol("Compare32");
const Compare64 = Symbol("Compare64");
const Test32 = Symbol("Test32");
const Test64 = Symbol("Test64");
const CompareDouble = Symbol("CompareDouble");
const CompareFloat = Symbol("CompareFloat");
const Branch8 = Symbol("Branch8");
const Branch32 = Symbol("Branch32");
const Branch64 = Symbol("Branch64");
const BranchTest8 = Symbol("BranchTest8");
const BranchTest32 = Symbol("BranchTest32");
const BranchTest64 = Symbol("BranchTest64");
const BranchDouble = Symbol("BranchDouble");
const BranchFloat = Symbol("BranchFloat");
const BranchAdd32 = Symbol("BranchAdd32");
const BranchAdd64 = Symbol("BranchAdd64");
const BranchMul32 = Symbol("BranchMul32");
const BranchMul64 = Symbol("BranchMul64");
const BranchSub32 = Symbol("BranchSub32");
const BranchSub64 = Symbol("BranchSub64");
const BranchNeg32 = Symbol("BranchNeg32");
const BranchNeg64 = Symbol("BranchNeg64");
const MoveConditionally32 = Symbol("MoveConditionally32");
const MoveConditionally64 = Symbol("MoveConditionally64");
const MoveConditionallyTest32 = Symbol("MoveConditionallyTest32");
const MoveConditionallyTest64 = Symbol("MoveConditionallyTest64");
const MoveConditionallyDouble = Symbol("MoveConditionallyDouble");
const MoveConditionallyFloat = Symbol("MoveConditionallyFloat");
const MoveDoubleConditionally32 = Symbol("MoveDoubleConditionally32");
const MoveDoubleConditionally64 = Symbol("MoveDoubleConditionally64");
const MoveDoubleConditionallyTest32 = Symbol("MoveDoubleConditionallyTest32");
const MoveDoubleConditionallyTest64 = Symbol("MoveDoubleConditionallyTest64");
const MoveDoubleConditionallyDouble = Symbol("MoveDoubleConditionallyDouble");
const MoveDoubleConditionallyFloat = Symbol("MoveDoubleConditionallyFloat");
const Jump = Symbol("Jump");
const Ret32 = Symbol("Ret32");
const Ret64 = Symbol("Ret64");
const RetFloat = Symbol("RetFloat");
const RetDouble = Symbol("RetDouble");
const Oops = Symbol("Oops");
const Shuffle = Symbol("Shuffle");
const Patch = Symbol("Patch");
const CCall = Symbol("CCall");
const ColdCCall = Symbol("ColdCCall");
function Inst_forEachArg(inst, func)
{
    let replacement;
    switch (inst.opcode) {
    case Nop:
        break;
        break;
    case Add32:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.ZDef, GP, 32);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.UseZDef, GP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Add8:
        inst.visitArg(0, func, Arg.Use, GP, 8);
        inst.visitArg(1, func, Arg.UseDef, GP, 8);
        break;
        break;
    case Add16:
        inst.visitArg(0, func, Arg.Use, GP, 16);
        inst.visitArg(1, func, Arg.UseDef, GP, 16);
        break;
        break;
    case Add64:
        switch (inst.args.length) {
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.UseDef, GP, 64);
            break;
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.Use, GP, 64);
            inst.visitArg(2, func, Arg.Def, GP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case AddDouble:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, FP, 64);
            inst.visitArg(1, func, Arg.Use, FP, 64);
            inst.visitArg(2, func, Arg.Def, FP, 64);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, FP, 64);
            inst.visitArg(1, func, Arg.UseDef, FP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case AddFloat:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, FP, 32);
            inst.visitArg(1, func, Arg.Use, FP, 32);
            inst.visitArg(2, func, Arg.Def, FP, 32);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, FP, 32);
            inst.visitArg(1, func, Arg.UseDef, FP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Sub32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.UseZDef, GP, 32);
        break;
        break;
    case Sub64:
        inst.visitArg(0, func, Arg.Use, GP, 64);
        inst.visitArg(1, func, Arg.UseDef, GP, 64);
        break;
        break;
    case SubDouble:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, FP, 64);
            inst.visitArg(1, func, Arg.Use, FP, 64);
            inst.visitArg(2, func, Arg.Def, FP, 64);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, FP, 64);
            inst.visitArg(1, func, Arg.UseDef, FP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case SubFloat:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, FP, 32);
            inst.visitArg(1, func, Arg.Use, FP, 32);
            inst.visitArg(2, func, Arg.Def, FP, 32);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, FP, 32);
            inst.visitArg(1, func, Arg.UseDef, FP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Neg32:
        inst.visitArg(0, func, Arg.UseZDef, GP, 32);
        break;
        break;
    case Neg64:
        inst.visitArg(0, func, Arg.UseDef, GP, 64);
        break;
        break;
    case NegateDouble:
        inst.visitArg(0, func, Arg.Use, FP, 64);
        inst.visitArg(1, func, Arg.Def, FP, 64);
        break;
        break;
    case Mul32:
        switch (inst.args.length) {
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.UseZDef, GP, 32);
            break;
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.ZDef, GP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Mul64:
        switch (inst.args.length) {
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.UseDef, GP, 64);
            break;
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.Use, GP, 64);
            inst.visitArg(2, func, Arg.Def, GP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case MultiplyAdd32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 32);
        inst.visitArg(2, func, Arg.Use, GP, 32);
        inst.visitArg(3, func, Arg.ZDef, GP, 32);
        break;
        break;
    case MultiplyAdd64:
        inst.visitArg(0, func, Arg.Use, GP, 64);
        inst.visitArg(1, func, Arg.Use, GP, 64);
        inst.visitArg(2, func, Arg.Use, GP, 64);
        inst.visitArg(3, func, Arg.Def, GP, 64);
        break;
        break;
    case MultiplySub32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 32);
        inst.visitArg(2, func, Arg.Use, GP, 32);
        inst.visitArg(3, func, Arg.ZDef, GP, 32);
        break;
        break;
    case MultiplySub64:
        inst.visitArg(0, func, Arg.Use, GP, 64);
        inst.visitArg(1, func, Arg.Use, GP, 64);
        inst.visitArg(2, func, Arg.Use, GP, 64);
        inst.visitArg(3, func, Arg.Def, GP, 64);
        break;
        break;
    case MultiplyNeg32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 32);
        inst.visitArg(2, func, Arg.ZDef, GP, 32);
        break;
        break;
    case MultiplyNeg64:
        inst.visitArg(0, func, Arg.Use, GP, 64);
        inst.visitArg(1, func, Arg.Use, GP, 64);
        inst.visitArg(2, func, Arg.ZDef, GP, 64);
        break;
        break;
    case Div32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 32);
        inst.visitArg(2, func, Arg.ZDef, GP, 32);
        break;
        break;
    case Div64:
        inst.visitArg(0, func, Arg.Use, GP, 64);
        inst.visitArg(1, func, Arg.Use, GP, 64);
        inst.visitArg(2, func, Arg.Def, GP, 64);
        break;
        break;
    case MulDouble:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, FP, 64);
            inst.visitArg(1, func, Arg.Use, FP, 64);
            inst.visitArg(2, func, Arg.Def, FP, 64);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, FP, 64);
            inst.visitArg(1, func, Arg.UseDef, FP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case MulFloat:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, FP, 32);
            inst.visitArg(1, func, Arg.Use, FP, 32);
            inst.visitArg(2, func, Arg.Def, FP, 32);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, FP, 32);
            inst.visitArg(1, func, Arg.UseDef, FP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case DivDouble:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, FP, 64);
            inst.visitArg(1, func, Arg.Use, FP, 32);
            inst.visitArg(2, func, Arg.Def, FP, 64);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, FP, 64);
            inst.visitArg(1, func, Arg.UseDef, FP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case DivFloat:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, FP, 32);
            inst.visitArg(1, func, Arg.Use, FP, 32);
            inst.visitArg(2, func, Arg.Def, FP, 32);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, FP, 32);
            inst.visitArg(1, func, Arg.UseDef, FP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case X86ConvertToDoubleWord32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.ZDef, GP, 32);
        break;
        break;
    case X86ConvertToQuadWord64:
        inst.visitArg(0, func, Arg.Use, GP, 64);
        inst.visitArg(1, func, Arg.Def, GP, 64);
        break;
        break;
    case X86Div32:
        inst.visitArg(0, func, Arg.UseZDef, GP, 32);
        inst.visitArg(1, func, Arg.UseZDef, GP, 32);
        inst.visitArg(2, func, Arg.Use, GP, 32);
        break;
        break;
    case X86Div64:
        inst.visitArg(0, func, Arg.UseZDef, GP, 64);
        inst.visitArg(1, func, Arg.UseZDef, GP, 64);
        inst.visitArg(2, func, Arg.Use, GP, 64);
        break;
        break;
    case Lea:
        inst.visitArg(0, func, Arg.UseAddr, GP, Ptr);
        inst.visitArg(1, func, Arg.Def, GP, Ptr);
        break;
        break;
    case And32:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.ZDef, GP, 32);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.UseZDef, GP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case And64:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.Use, GP, 64);
            inst.visitArg(2, func, Arg.Def, GP, 64);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.UseDef, GP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case AndDouble:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, FP, 64);
            inst.visitArg(1, func, Arg.Use, FP, 64);
            inst.visitArg(2, func, Arg.Def, FP, 64);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, FP, 64);
            inst.visitArg(1, func, Arg.UseDef, FP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case AndFloat:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, FP, 32);
            inst.visitArg(1, func, Arg.Use, FP, 32);
            inst.visitArg(2, func, Arg.Def, FP, 32);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, FP, 32);
            inst.visitArg(1, func, Arg.UseDef, FP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case XorDouble:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, FP, 64);
            inst.visitArg(1, func, Arg.Use, FP, 64);
            inst.visitArg(2, func, Arg.Def, FP, 64);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, FP, 64);
            inst.visitArg(1, func, Arg.UseDef, FP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case XorFloat:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, FP, 32);
            inst.visitArg(1, func, Arg.Use, FP, 32);
            inst.visitArg(2, func, Arg.Def, FP, 32);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, FP, 32);
            inst.visitArg(1, func, Arg.UseDef, FP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Lshift32:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.ZDef, GP, 32);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.UseZDef, GP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Lshift64:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.Use, GP, 64);
            inst.visitArg(2, func, Arg.ZDef, GP, 64);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.UseDef, GP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Rshift32:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.ZDef, GP, 32);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.UseZDef, GP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Rshift64:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.Use, GP, 64);
            inst.visitArg(2, func, Arg.ZDef, GP, 64);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.UseDef, GP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Urshift32:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.ZDef, GP, 32);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.UseZDef, GP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Urshift64:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.Use, GP, 64);
            inst.visitArg(2, func, Arg.ZDef, GP, 64);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.UseDef, GP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Or32:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.ZDef, GP, 32);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.UseZDef, GP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Or64:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.Use, GP, 64);
            inst.visitArg(2, func, Arg.Def, GP, 64);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.UseDef, GP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Xor32:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.ZDef, GP, 32);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.UseZDef, GP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Xor64:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.Use, GP, 64);
            inst.visitArg(2, func, Arg.Def, GP, 64);
            break;
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.UseDef, GP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Not32:
        switch (inst.args.length) {
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.ZDef, GP, 32);
            break;
        case 1:
            inst.visitArg(0, func, Arg.UseZDef, GP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case Not64:
        switch (inst.args.length) {
        case 2:
            inst.visitArg(0, func, Arg.Use, GP, 64);
            inst.visitArg(1, func, Arg.Def, GP, 64);
            break;
        case 1:
            inst.visitArg(0, func, Arg.UseDef, GP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case AbsDouble:
        inst.visitArg(0, func, Arg.Use, FP, 64);
        inst.visitArg(1, func, Arg.Def, FP, 64);
        break;
        break;
    case AbsFloat:
        inst.visitArg(0, func, Arg.Use, FP, 32);
        inst.visitArg(1, func, Arg.Def, FP, 32);
        break;
        break;
    case CeilDouble:
        inst.visitArg(0, func, Arg.Use, FP, 64);
        inst.visitArg(1, func, Arg.Def, FP, 64);
        break;
        break;
    case CeilFloat:
        inst.visitArg(0, func, Arg.Use, FP, 32);
        inst.visitArg(1, func, Arg.Def, FP, 32);
        break;
        break;
    case FloorDouble:
        inst.visitArg(0, func, Arg.Use, FP, 64);
        inst.visitArg(1, func, Arg.Def, FP, 64);
        break;
        break;
    case FloorFloat:
        inst.visitArg(0, func, Arg.Use, FP, 32);
        inst.visitArg(1, func, Arg.Def, FP, 32);
        break;
        break;
    case SqrtDouble:
        inst.visitArg(0, func, Arg.Use, FP, 64);
        inst.visitArg(1, func, Arg.Def, FP, 64);
        break;
        break;
    case SqrtFloat:
        inst.visitArg(0, func, Arg.Use, FP, 32);
        inst.visitArg(1, func, Arg.Def, FP, 32);
        break;
        break;
    case ConvertInt32ToDouble:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Def, FP, 64);
        break;
        break;
    case ConvertInt64ToDouble:
        inst.visitArg(0, func, Arg.Use, GP, 64);
        inst.visitArg(1, func, Arg.Def, FP, 64);
        break;
        break;
    case ConvertInt32ToFloat:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Def, FP, 32);
        break;
        break;
    case ConvertInt64ToFloat:
        inst.visitArg(0, func, Arg.Use, GP, 64);
        inst.visitArg(1, func, Arg.Def, FP, 32);
        break;
        break;
    case CountLeadingZeros32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.ZDef, GP, 32);
        break;
        break;
    case CountLeadingZeros64:
        inst.visitArg(0, func, Arg.Use, GP, 64);
        inst.visitArg(1, func, Arg.Def, GP, 64);
        break;
        break;
    case ConvertDoubleToFloat:
        inst.visitArg(0, func, Arg.Use, FP, 64);
        inst.visitArg(1, func, Arg.Def, FP, 32);
        break;
        break;
    case ConvertFloatToDouble:
        inst.visitArg(0, func, Arg.Use, FP, 32);
        inst.visitArg(1, func, Arg.Def, FP, 64);
        break;
        break;
    case Move:
        inst.visitArg(0, func, Arg.Use, GP, Ptr);
        inst.visitArg(1, func, Arg.Def, GP, Ptr);
        break;
        break;
    case Swap32:
        inst.visitArg(0, func, Arg.UseDef, GP, 32);
        inst.visitArg(1, func, Arg.UseDef, GP, 32);
        break;
        break;
    case Swap64:
        inst.visitArg(0, func, Arg.UseDef, GP, 64);
        inst.visitArg(1, func, Arg.UseDef, GP, 64);
        break;
        break;
    case Move32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.ZDef, GP, 32);
        break;
        break;
    case StoreZero32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        break;
        break;
    case SignExtend32ToPtr:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Def, GP, Ptr);
        break;
        break;
    case ZeroExtend8To32:
        inst.visitArg(0, func, Arg.Use, GP, 8);
        inst.visitArg(1, func, Arg.ZDef, GP, 32);
        break;
        break;
    case SignExtend8To32:
        inst.visitArg(0, func, Arg.Use, GP, 8);
        inst.visitArg(1, func, Arg.ZDef, GP, 32);
        break;
        break;
    case ZeroExtend16To32:
        inst.visitArg(0, func, Arg.Use, GP, 16);
        inst.visitArg(1, func, Arg.ZDef, GP, 32);
        break;
        break;
    case SignExtend16To32:
        inst.visitArg(0, func, Arg.Use, GP, 16);
        inst.visitArg(1, func, Arg.ZDef, GP, 32);
        break;
        break;
    case MoveFloat:
        inst.visitArg(0, func, Arg.Use, FP, 32);
        inst.visitArg(1, func, Arg.Def, FP, 32);
        break;
        break;
    case MoveDouble:
        inst.visitArg(0, func, Arg.Use, FP, 64);
        inst.visitArg(1, func, Arg.Def, FP, 64);
        break;
        break;
    case MoveZeroToDouble:
        inst.visitArg(0, func, Arg.Def, FP, 64);
        break;
        break;
    case Move64ToDouble:
        inst.visitArg(0, func, Arg.Use, GP, 64);
        inst.visitArg(1, func, Arg.Def, FP, 64);
        break;
        break;
    case Move32ToFloat:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Def, FP, 32);
        break;
        break;
    case MoveDoubleTo64:
        inst.visitArg(0, func, Arg.Use, FP, 64);
        inst.visitArg(1, func, Arg.Def, GP, 64);
        break;
        break;
    case MoveFloatTo32:
        inst.visitArg(0, func, Arg.Use, FP, 32);
        inst.visitArg(1, func, Arg.Def, GP, 32);
        break;
        break;
    case Load8:
        inst.visitArg(0, func, Arg.Use, GP, 8);
        inst.visitArg(1, func, Arg.ZDef, GP, 32);
        break;
        break;
    case Store8:
        inst.visitArg(0, func, Arg.Use, GP, 8);
        inst.visitArg(1, func, Arg.Def, GP, 8);
        break;
        break;
    case Load8SignedExtendTo32:
        inst.visitArg(0, func, Arg.Use, GP, 8);
        inst.visitArg(1, func, Arg.ZDef, GP, 32);
        break;
        break;
    case Load16:
        inst.visitArg(0, func, Arg.Use, GP, 16);
        inst.visitArg(1, func, Arg.ZDef, GP, 32);
        break;
        break;
    case Load16SignedExtendTo32:
        inst.visitArg(0, func, Arg.Use, GP, 16);
        inst.visitArg(1, func, Arg.ZDef, GP, 32);
        break;
        break;
    case Store16:
        inst.visitArg(0, func, Arg.Use, GP, 16);
        inst.visitArg(1, func, Arg.Def, GP, 16);
        break;
        break;
    case Compare32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 32);
        inst.visitArg(2, func, Arg.Use, GP, 32);
        inst.visitArg(3, func, Arg.ZDef, GP, 32);
        break;
        break;
    case Compare64:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 64);
        inst.visitArg(2, func, Arg.Use, GP, 64);
        inst.visitArg(3, func, Arg.ZDef, GP, 32);
        break;
        break;
    case Test32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 32);
        inst.visitArg(2, func, Arg.Use, GP, 32);
        inst.visitArg(3, func, Arg.ZDef, GP, 32);
        break;
        break;
    case Test64:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 64);
        inst.visitArg(2, func, Arg.Use, GP, 64);
        inst.visitArg(3, func, Arg.ZDef, GP, 32);
        break;
        break;
    case CompareDouble:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, FP, 64);
        inst.visitArg(2, func, Arg.Use, FP, 64);
        inst.visitArg(3, func, Arg.ZDef, GP, 32);
        break;
        break;
    case CompareFloat:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, FP, 32);
        inst.visitArg(2, func, Arg.Use, FP, 32);
        inst.visitArg(3, func, Arg.ZDef, GP, 32);
        break;
        break;
    case Branch8:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 8);
        inst.visitArg(2, func, Arg.Use, GP, 8);
        break;
        break;
    case Branch32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 32);
        inst.visitArg(2, func, Arg.Use, GP, 32);
        break;
        break;
    case Branch64:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 64);
        inst.visitArg(2, func, Arg.Use, GP, 64);
        break;
        break;
    case BranchTest8:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 8);
        inst.visitArg(2, func, Arg.Use, GP, 8);
        break;
        break;
    case BranchTest32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 32);
        inst.visitArg(2, func, Arg.Use, GP, 32);
        break;
        break;
    case BranchTest64:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 64);
        inst.visitArg(2, func, Arg.Use, GP, 64);
        break;
        break;
    case BranchDouble:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, FP, 64);
        inst.visitArg(2, func, Arg.Use, FP, 64);
        break;
        break;
    case BranchFloat:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, FP, 32);
        inst.visitArg(2, func, Arg.Use, FP, 32);
        break;
        break;
    case BranchAdd32:
        switch (inst.args.length) {
        case 4:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.Use, GP, 32);
            inst.visitArg(3, func, Arg.ZDef, GP, 32);
            break;
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.UseZDef, GP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case BranchAdd64:
        switch (inst.args.length) {
        case 4:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 64);
            inst.visitArg(2, func, Arg.Use, GP, 64);
            inst.visitArg(3, func, Arg.ZDef, GP, 64);
            break;
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 64);
            inst.visitArg(2, func, Arg.UseDef, GP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case BranchMul32:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.UseZDef, GP, 32);
            break;
        case 4:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.Use, GP, 32);
            inst.visitArg(3, func, Arg.ZDef, GP, 32);
            break;
        case 6:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.Use, GP, 32);
            inst.visitArg(3, func, Arg.Scratch, GP, 32);
            inst.visitArg(4, func, Arg.Scratch, GP, 32);
            inst.visitArg(5, func, Arg.ZDef, GP, 32);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case BranchMul64:
        switch (inst.args.length) {
        case 3:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 64);
            inst.visitArg(2, func, Arg.UseZDef, GP, 64);
            break;
        case 6:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 64);
            inst.visitArg(2, func, Arg.Use, GP, 64);
            inst.visitArg(3, func, Arg.Scratch, GP, 64);
            inst.visitArg(4, func, Arg.Scratch, GP, 64);
            inst.visitArg(5, func, Arg.ZDef, GP, 64);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case BranchSub32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 32);
        inst.visitArg(2, func, Arg.UseZDef, GP, 32);
        break;
        break;
    case BranchSub64:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 64);
        inst.visitArg(2, func, Arg.UseDef, GP, 64);
        break;
        break;
    case BranchNeg32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.UseZDef, GP, 32);
        break;
        break;
    case BranchNeg64:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.UseZDef, GP, 64);
        break;
        break;
    case MoveConditionally32:
        switch (inst.args.length) {
        case 5:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.Use, GP, 32);
            inst.visitArg(3, func, Arg.Use, GP, Ptr);
            inst.visitArg(4, func, Arg.UseDef, GP, Ptr);
            break;
        case 6:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.Use, GP, 32);
            inst.visitArg(3, func, Arg.Use, GP, Ptr);
            inst.visitArg(4, func, Arg.Use, GP, Ptr);
            inst.visitArg(5, func, Arg.Def, GP, Ptr);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case MoveConditionally64:
        switch (inst.args.length) {
        case 5:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 64);
            inst.visitArg(2, func, Arg.Use, GP, 64);
            inst.visitArg(3, func, Arg.Use, GP, Ptr);
            inst.visitArg(4, func, Arg.UseDef, GP, Ptr);
            break;
        case 6:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 64);
            inst.visitArg(2, func, Arg.Use, GP, 64);
            inst.visitArg(3, func, Arg.Use, GP, Ptr);
            inst.visitArg(4, func, Arg.Use, GP, Ptr);
            inst.visitArg(5, func, Arg.Def, GP, Ptr);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case MoveConditionallyTest32:
        switch (inst.args.length) {
        case 5:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.Use, GP, 32);
            inst.visitArg(3, func, Arg.Use, GP, Ptr);
            inst.visitArg(4, func, Arg.UseDef, GP, Ptr);
            break;
        case 6:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.Use, GP, 32);
            inst.visitArg(3, func, Arg.Use, GP, Ptr);
            inst.visitArg(4, func, Arg.Use, GP, Ptr);
            inst.visitArg(5, func, Arg.Def, GP, Ptr);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case MoveConditionallyTest64:
        switch (inst.args.length) {
        case 5:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 64);
            inst.visitArg(2, func, Arg.Use, GP, 64);
            inst.visitArg(3, func, Arg.Use, GP, Ptr);
            inst.visitArg(4, func, Arg.UseDef, GP, Ptr);
            break;
        case 6:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, GP, 32);
            inst.visitArg(2, func, Arg.Use, GP, 32);
            inst.visitArg(3, func, Arg.Use, GP, Ptr);
            inst.visitArg(4, func, Arg.Use, GP, Ptr);
            inst.visitArg(5, func, Arg.Def, GP, Ptr);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case MoveConditionallyDouble:
        switch (inst.args.length) {
        case 6:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, FP, 64);
            inst.visitArg(2, func, Arg.Use, FP, 64);
            inst.visitArg(3, func, Arg.Use, GP, Ptr);
            inst.visitArg(4, func, Arg.Use, GP, Ptr);
            inst.visitArg(5, func, Arg.Def, GP, Ptr);
            break;
        case 5:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, FP, 64);
            inst.visitArg(2, func, Arg.Use, FP, 64);
            inst.visitArg(3, func, Arg.Use, GP, Ptr);
            inst.visitArg(4, func, Arg.UseDef, GP, Ptr);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case MoveConditionallyFloat:
        switch (inst.args.length) {
        case 6:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, FP, 32);
            inst.visitArg(2, func, Arg.Use, FP, 32);
            inst.visitArg(3, func, Arg.Use, GP, Ptr);
            inst.visitArg(4, func, Arg.Use, GP, Ptr);
            inst.visitArg(5, func, Arg.Def, GP, Ptr);
            break;
        case 5:
            inst.visitArg(0, func, Arg.Use, GP, 32);
            inst.visitArg(1, func, Arg.Use, FP, 32);
            inst.visitArg(2, func, Arg.Use, FP, 32);
            inst.visitArg(3, func, Arg.Use, GP, Ptr);
            inst.visitArg(4, func, Arg.UseDef, GP, Ptr);
            break;
        default:
            throw new Error("Bad overload");
            break;
        }
        break;
    case MoveDoubleConditionally32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 32);
        inst.visitArg(2, func, Arg.Use, GP, 32);
        inst.visitArg(3, func, Arg.Use, FP, 64);
        inst.visitArg(4, func, Arg.Use, FP, 64);
        inst.visitArg(5, func, Arg.Def, FP, 64);
        break;
        break;
    case MoveDoubleConditionally64:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 64);
        inst.visitArg(2, func, Arg.Use, GP, 64);
        inst.visitArg(3, func, Arg.Use, FP, 64);
        inst.visitArg(4, func, Arg.Use, FP, 64);
        inst.visitArg(5, func, Arg.Def, FP, 64);
        break;
        break;
    case MoveDoubleConditionallyTest32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 32);
        inst.visitArg(2, func, Arg.Use, GP, 32);
        inst.visitArg(3, func, Arg.Use, FP, 64);
        inst.visitArg(4, func, Arg.Use, FP, 64);
        inst.visitArg(5, func, Arg.Def, FP, 64);
        break;
        break;
    case MoveDoubleConditionallyTest64:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, GP, 64);
        inst.visitArg(2, func, Arg.Use, GP, 64);
        inst.visitArg(3, func, Arg.Use, FP, 64);
        inst.visitArg(4, func, Arg.Use, FP, 64);
        inst.visitArg(5, func, Arg.Def, FP, 64);
        break;
        break;
    case MoveDoubleConditionallyDouble:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, FP, 64);
        inst.visitArg(2, func, Arg.Use, FP, 64);
        inst.visitArg(3, func, Arg.Use, FP, 64);
        inst.visitArg(4, func, Arg.Use, FP, 64);
        inst.visitArg(5, func, Arg.Def, FP, 64);
        break;
        break;
    case MoveDoubleConditionallyFloat:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        inst.visitArg(1, func, Arg.Use, FP, 32);
        inst.visitArg(2, func, Arg.Use, FP, 32);
        inst.visitArg(3, func, Arg.Use, FP, 64);
        inst.visitArg(4, func, Arg.Use, FP, 64);
        inst.visitArg(5, func, Arg.Def, FP, 64);
        break;
        break;
    case Jump:
        break;
        break;
    case Ret32:
        inst.visitArg(0, func, Arg.Use, GP, 32);
        break;
        break;
    case Ret64:
        inst.visitArg(0, func, Arg.Use, GP, 64);
        break;
        break;
    case RetFloat:
        inst.visitArg(0, func, Arg.Use, FP, 32);
        break;
        break;
    case RetDouble:
        inst.visitArg(0, func, Arg.Use, FP, 64);
        break;
        break;
    case Oops:
        break;
        break;
    case Shuffle:
        ShuffleCustom.forEachArg(inst, func);
        break;
    case Patch:
        PatchCustom.forEachArg(inst, func);
        break;
    case CCall:
        CCallCustom.forEachArg(inst, func);
        break;
    case ColdCCall:
        ColdCCallCustom.forEachArg(inst, func);
        break;
    default:
        throw "Bad opcode";
    }
}
function Inst_hasNonArgEffects(inst)
{
    switch (inst.opcode) {
    case Branch8:
    case Branch32:
    case Branch64:
    case BranchTest8:
    case BranchTest32:
    case BranchTest64:
    case BranchDouble:
    case BranchFloat:
    case BranchAdd32:
    case BranchAdd64:
    case BranchMul32:
    case BranchMul64:
    case BranchSub32:
    case BranchSub64:
    case BranchNeg32:
    case BranchNeg64:
    case Jump:
    case Ret32:
    case Ret64:
    case RetFloat:
    case RetDouble:
    case Oops:
        return true;
    case Shuffle:
        return ShuffleCustom.hasNonArgNonControlEffects(inst);
    case Patch:
        return PatchCustom.hasNonArgNonControlEffects(inst);
    case CCall:
        return CCallCustom.hasNonArgNonControlEffects(inst);
    case ColdCCall:
        return ColdCCallCustom.hasNonArgNonControlEffects(inst);
    default:
        return false;
    }
}
function opcodeCode(opcode)
{
    switch (opcode) {
    case AbsDouble:
        return 0
    case AbsFloat:
        return 1
    case Add16:
        return 2
    case Add32:
        return 3
    case Add64:
        return 4
    case Add8:
        return 5
    case AddDouble:
        return 6
    case AddFloat:
        return 7
    case And32:
        return 8
    case And64:
        return 9
    case AndDouble:
        return 10
    case AndFloat:
        return 11
    case Branch32:
        return 12
    case Branch64:
        return 13
    case Branch8:
        return 14
    case BranchAdd32:
        return 15
    case BranchAdd64:
        return 16
    case BranchDouble:
        return 17
    case BranchFloat:
        return 18
    case BranchMul32:
        return 19
    case BranchMul64:
        return 20
    case BranchNeg32:
        return 21
    case BranchNeg64:
        return 22
    case BranchSub32:
        return 23
    case BranchSub64:
        return 24
    case BranchTest32:
        return 25
    case BranchTest64:
        return 26
    case BranchTest8:
        return 27
    case CCall:
        return 28
    case CeilDouble:
        return 29
    case CeilFloat:
        return 30
    case ColdCCall:
        return 31
    case Compare32:
        return 32
    case Compare64:
        return 33
    case CompareDouble:
        return 34
    case CompareFloat:
        return 35
    case ConvertDoubleToFloat:
        return 36
    case ConvertFloatToDouble:
        return 37
    case ConvertInt32ToDouble:
        return 38
    case ConvertInt32ToFloat:
        return 39
    case ConvertInt64ToDouble:
        return 40
    case ConvertInt64ToFloat:
        return 41
    case CountLeadingZeros32:
        return 42
    case CountLeadingZeros64:
        return 43
    case Div32:
        return 44
    case Div64:
        return 45
    case DivDouble:
        return 46
    case DivFloat:
        return 47
    case FloorDouble:
        return 48
    case FloorFloat:
        return 49
    case Jump:
        return 50
    case Lea:
        return 51
    case Load16:
        return 52
    case Load16SignedExtendTo32:
        return 53
    case Load8:
        return 54
    case Load8SignedExtendTo32:
        return 55
    case Lshift32:
        return 56
    case Lshift64:
        return 57
    case Move:
        return 58
    case Move32:
        return 59
    case Move32ToFloat:
        return 60
    case Move64ToDouble:
        return 61
    case MoveConditionally32:
        return 62
    case MoveConditionally64:
        return 63
    case MoveConditionallyDouble:
        return 64
    case MoveConditionallyFloat:
        return 65
    case MoveConditionallyTest32:
        return 66
    case MoveConditionallyTest64:
        return 67
    case MoveDouble:
        return 68
    case MoveDoubleConditionally32:
        return 69
    case MoveDoubleConditionally64:
        return 70
    case MoveDoubleConditionallyDouble:
        return 71
    case MoveDoubleConditionallyFloat:
        return 72
    case MoveDoubleConditionallyTest32:
        return 73
    case MoveDoubleConditionallyTest64:
        return 74
    case MoveDoubleTo64:
        return 75
    case MoveFloat:
        return 76
    case MoveFloatTo32:
        return 77
    case MoveZeroToDouble:
        return 78
    case Mul32:
        return 79
    case Mul64:
        return 80
    case MulDouble:
        return 81
    case MulFloat:
        return 82
    case MultiplyAdd32:
        return 83
    case MultiplyAdd64:
        return 84
    case MultiplyNeg32:
        return 85
    case MultiplyNeg64:
        return 86
    case MultiplySub32:
        return 87
    case MultiplySub64:
        return 88
    case Neg32:
        return 89
    case Neg64:
        return 90
    case NegateDouble:
        return 91
    case Nop:
        return 92
    case Not32:
        return 93
    case Not64:
        return 94
    case Oops:
        return 95
    case Or32:
        return 96
    case Or64:
        return 97
    case Patch:
        return 98
    case Ret32:
        return 99
    case Ret64:
        return 100
    case RetDouble:
        return 101
    case RetFloat:
        return 102
    case Rshift32:
        return 103
    case Rshift64:
        return 104
    case Shuffle:
        return 105
    case SignExtend16To32:
        return 106
    case SignExtend32ToPtr:
        return 107
    case SignExtend8To32:
        return 108
    case SqrtDouble:
        return 109
    case SqrtFloat:
        return 110
    case Store16:
        return 111
    case Store8:
        return 112
    case StoreZero32:
        return 113
    case Sub32:
        return 114
    case Sub64:
        return 115
    case SubDouble:
        return 116
    case SubFloat:
        return 117
    case Swap32:
        return 118
    case Swap64:
        return 119
    case Test32:
        return 120
    case Test64:
        return 121
    case Urshift32:
        return 122
    case Urshift64:
        return 123
    case X86ConvertToDoubleWord32:
        return 124
    case X86ConvertToQuadWord64:
        return 125
    case X86Div32:
        return 126
    case X86Div64:
        return 127
    case Xor32:
        return 128
    case Xor64:
        return 129
    case XorDouble:
        return 130
    case XorFloat:
        return 131
    case ZeroExtend16To32:
        return 132
    case ZeroExtend8To32:
        return 133
    default:
        throw new Error("bad opcode");
    }
}

class Reg extends TmpBase {
    constructor(index, type, name, isCalleeSave)
    {
        super();
        this._index = index;
        this._type = type;
        this._name = name;
        this._isCalleeSave = !!isCalleeSave;
    }
    
    static fromReg(reg)
    {
        return reg;
    }
    
    get index() { return this._index; }
    get type() { return this._type; }
    get name() { return this._name; }
    get isCalleeSave() { return this._isCalleeSave; }
    
    get isReg() { return true; }
    
    hash()
    {
        if (this.isGP)
            return 1 + this._index;
        return -1 - this._index;
    }
    
    toString()
    {
        return `%${this._name}`;
    }
    
    static extract(arg)
    {
        if (arg.isReg)
            return arg.reg;
        return null;
    }
    
    static forEachFast(arg, func)
    {
        return arg.forEachTmpFast(tmp => {
            if (!tmp.isReg)
                return;
            return func(tmp);
        });
    }
    
    static forEach(arg, argRole, argType, argWidth, func)
    {
        return arg.forEachTmp(
            argRole, argType, argWidth,
            (tmp, role, type, width) => {
                if (!tmp.isReg)
                    return;
                return func(tmp, role, type, width);
            });
    }
}

{
    Reg.regs = [];
    function newReg(...args)
    {
        let result = new Reg(...args);
        Reg.regs.push(result);
        return result;
    }

    // Define X86_64 GPRs
    {
        let index = 0;
        function newGPR(name, isCalleeSave) { return newReg(index++, GP, name, isCalleeSave); }
        
        Reg.rax = newGPR("rax");
        Reg.rcx = newGPR("rcx");
        Reg.rdx = newGPR("rdx");
        Reg.rbx = newGPR("rbx", true);
        Reg.rsp = newGPR("rsp");
        Reg.rbp = newGPR("rbp", true);
        Reg.rsi = newGPR("rsi");
        Reg.rdi = newGPR("rdi");
        for (let i = 8; i <= 15; ++i)
            Reg[`r${i}`] = newGPR(`r${i}`, i >= 12);
    }

    // Define X86_64 FPRs.
    for (let i = 0; i <= 15; ++i)
        Reg[`xmm${i}`] = newReg(i, FP, `xmm${i}`);

    Reg.gprs = []
    Reg.fprs = []
    Reg.calleeSaveGPRs = []
    Reg.calleeSaveFPRs = []
    Reg.calleeSaves = []
    for (let reg of Reg.regs) {
        if (reg.isGP) {
            Reg.gprs.push(reg);
            if (reg.isCalleeSave)
                Reg.calleeSaveGPRs.push(reg);
        } else {
            Reg.fprs.push(reg);
            if (reg.isCalleeSave)
                Reg.calleeSaveFPRS.push(reg);
        }
        if (reg.isCalleeSave)
            Reg.calleeSaves.push(reg);
    }
    
    Reg.callFrameRegister = Reg.rbp;
    Reg.stackPointerRegister = Reg.rsp;
}

class StackSlot {
    constructor(index, byteSize, kind)
    {
        this._index = index;
        this._byteSize = byteSize;
        this._kind = kind;
    }
    
    get byteSize() { return this._byteSize; }
    get kind() { return this._kind; }
    
    get isLocked() { return this._kind == Locked; }
    get isSpill() { return this._kind == Spill; }
    
    get index() { return this._index; }

    ensureSize(size)
    {
        if (this._offsetFromFP)
            throw new Error("Stack slot already allocated");
        this._byteSize = Math.max(this._byteSize, size);
    }
    
    get alignment()
    {
        if (this._byteSize <= 1)
            return 1;
        if (this._byteSize <= 2)
            return 2;
        if (this._byteSize <= 4)
            return 4;
        return 8;
    }
    
    get offsetFromFP() { return this._offsetFromFP; }
    
    setOffsetFromFP(value) { this._offsetFromFP = value; }
    
    hash()
    {
        return ((this._kind == Spill ? 1 : 0) + this._byteSize * 3 + (this._offsetFromFP ? this._offsetFromFP * 7 : 0)) >>> 0;
    }
    
    toString()
    {
        return "" + (this.isSpill ? "spill" : "stack") + this._index + "<" + this._byteSize +
            (this._offsetFromFP ? ", offset = " + this._offsetFromFP : "") + ">";
    }
    
    static extract(arg)
    {
        if (arg.isStack)
            return arg.stackSlot;
        return null;
    }
    
    static forEachFast(arg, func)
    {
        if (!arg.isStack)
            return;
        
        let replacement;
        if (replacement = func(arg.stackSlot))
            return Arg.createStack(replacement, this._offset);
    }
    
    static forEach(arg, role, type, width, func)
    {
        if (!arg.isStack)
            return;
        
        let replacement;
        if (replacement = func(arg.stackSlot, role, type, width))
            return Arg.createStack(replacement, this._offset);
    }
}

class Tmp extends TmpBase {
    constructor(index, type)
    {
        super();
        this._index = index;
        this._type = type;
    }
    
    static fromReg(reg)
    {
        return reg;
    }
    
    get index() { return this._index; }
    get type() { return this._type; }
    
    get isReg() { return false; }
    
    hash()
    {
        if (isGP)
            return Reg.gprs[Reg.gprs.length - 1].hash() + 1 + this._index;
        return Reg.fprs[Reg.fprs.length - 1].hash() - 1 - this._index;
    }

    toString()
    {
        return "%" + (this.isGP ? "" : "f") + "tmp" + this._index;
    }
    
    static extract(arg)
    {
        if (arg.isTmp)
            return arg.tmp;
        return null;
    }

    static forEachFast(arg, func) { return arg.forEachTmpFast(func); }
    static forEach(arg, role, type, width, func) { return arg.forEachTmp(role, type, width, func); }
}

function isRepresentableAsInt32(value)
{
    return (value | 0) === value;
}

function addIndexed(list, cons, ...args)
{
    let result = new cons(list.length, ...args);
    list.push(result);
    return result;
}

const stackAlignmentBytes = 16;

function roundUpToMultipleOf(amount, value)
{
    return Math.ceil(value / amount) * amount;
}

function symbolName(symbol)
{
    let fullString = symbol.toString();
    return fullString.substring("Symbol(".length, fullString.length - ")".length);
}

function lowerSymbolName(symbol)
{
    return symbolName(symbol).toLowerCase();
}

function setToString(set)
{
    let result = "";
    for (let value of set) {
        if (result)
            result += ", ";
        result += value;
    }
    return result;
}

function mergeIntoSet(target, source)
{
    let didAdd = false;
    for (let value of source) {
        if (target.has(value))
            continue;
        target.add(value);
        didAdd = true;
    }
    return didAdd;
}

function nonEmptyRangesOverlap(leftMin, leftMax, rightMin, rightMax)
{
    if (leftMin >= leftMax)
        throw new Error("Bad left range");
    if (rightMin >= rightMax)
        throw new Error("Bad right range");
    
    if (leftMin <= rightMin && leftMax > rightMin)
        return true;
    if (rightMin <= leftMin && rightMax > leftMin)
        return true;
    return false;
}

function rangesOverlap(leftMin, leftMax, rightMin, rightMax)
{
    if (leftMin > leftMax)
        throw new Error("Bad left range");
    if (rightMin > rightMax)
        throw new Error("Bad right range");
    
    if (leftMin == leftMax)
        return false;
    if (rightMin == rightMax)
        return false;
    
    return nonEmptyRangesOverlap(leftMin, leftMax, rightMin, rightMax);
}

function removeAllMatching(array, func)
{
    let srcIndex = 0;
    let dstIndex = 0;
    while (srcIndex < array.length) {
        let value = array[srcIndex++];
        if (!func(value))
            array[dstIndex++] = value;
    }
    array.length = dstIndex;
}

function bubbleSort(array, lessThan)
{
    function swap(i, j)
    {
        var tmp = array[i];
        array[i] = array[j];
        array[j] = tmp;
    }
    
    let begin = 0;
    let end = array.length;
    for (;;) {
        let changed = false;
        
        function bubble(i, j)
        {
            if (lessThan(array[i], array[j])) {
                swap(i, j);
                changed = true;
            }
        }
    
        if (end < begin)
            throw new Error("Begin and end are messed up");
        
        let limit = end - begin;
        for (let i = limit; i-- > 1;)
            bubble(begin + i, begin + i - 1);
        if (!changed)
            return;
        
        // After one run, the first element in the list is guaranteed to be the smallest.
        begin++;
        
        // Now go in the other direction. This eliminates most sorting pathologies.
        changed = false;
        
        if (end < begin)
            throw new Error("Begin and end are messed up");
        
        limit = end - begin;
        for (let i = 1; i < limit; ++i)
            bubble(begin + i, begin + i - 1);
        if (!changed)
            return;
        
        // Now the last element is guaranteed to be the largest.
        end--;
    }
}

let currentTime;
if (this.performance && performance.now)
    currentTime = function() { return performance.now() };
else if (this.preciseTime)
    currentTime = function() { return preciseTime() * 1000; };
else
    currentTime = function() { return +new Date(); };


const ShuffleCustom = {
    forEachArg(inst, func)
    {
        var limit = Math.floor(inst.args.length / 3) * 3;
        for (let i = 0; i < limit; i += 3) {
            let src = inst.args[i + 0];
            let dst = inst.args[i + 1];
            let widthArg = inst.args[i + 2];
            let width = widthArg.width;
            let type = src.isGP && dst.isGP ? GP : FP;
            inst.visitArg(i + 0, func, Arg.Use, type, width);
            inst.visitArg(i + 1, func, Arg.Def, type, width);
            inst.visitArg(i + 2, func, Arg.Use, GP, 8);
        }
    },
    
    hasNonArgNonControlEffects(inst)
    {
        return false;
    }
};

const PatchCustom = {
    forEachArg(inst, func)
    {
        for (let i = 0; i < inst.args.length; ++i) {
            let {type, role, width} = inst.patchArgData[i];
            inst.visitArg(i, func, role, type, width);
        }
    },
    
    hasNonArgNonControlEffects(inst)
    {
        return inst.patchHasNonArgEffects;
    }
};

const CCallCustom = {
    forEachArg(inst, func)
    {
        let index = 0;
        inst.visitArg(index++, func, Arg.Use, GP, Ptr); // callee
        
        if (inst.cCallType != Void) {
            inst.visitArg(
                index++, func, Arg.Def, Arg.typeForB3Type(inst.cCallType),
                Arg.widthForB3Type(inst.cCallType));
        }
        
        for (let type of inst.cCallArgTypes) {
            inst.visitArg(
                index++, func, Arg.Use, Arg.typeForB3Type(type), Arg.widthForB3Type(type));
        }
    },
    
    hasNonArgNonControlEffects(inst)
    {
        return true;
    }
};

const ColdCCallCustom = {
    forEachArg(inst, func)
    {
        CCallCustom.forEachArg(
            inst,
            (arg, role, type, width) => {
                return func(arg, Arg.cooled(role), type, width);
            });
    },
    
    hasNonArgNonControlEffects(inst)
    {
        return true;
    }
};


class Liveness {
    constructor(thing, code)
    {
        this._thing = thing;
        this._code = code;
        
        this._liveAtHead = new Map();
        this._liveAtTail = new Map();
        
        for (let block of code) {
            this._liveAtHead.set(block, new Set());
            
            let liveAtTail = new Set();
            this._liveAtTail.set(block, liveAtTail);
            
            block.last.forEach(
                thing,
                (value, role, type, width) => {
                    if (Arg.isLateUse(role))
                        liveAtTail.add(value);
                });
        }
        
        let dirtyBlocks = new Set(code);
        
        let changed;
        do {
            changed = false;
            
            for (let blockIndex = code.size; blockIndex--;) {
                let block = code.at(blockIndex);
                if (!block)
                    continue;
                
                if (!dirtyBlocks.delete(block))
                    continue;
                
                let localCalc = this.localCalc(block);
                for (let instIndex = block.size; instIndex--;)
                    localCalc.execute(instIndex);
                
                // Handle the early def's of the first instruction.
                block.at(0).forEach(
                    thing,
                    (value, role, type, width) => {
                        if (Arg.isEarlyDef(role))
                            localCalc.liveSet.remove(value);
                    });
                
                let liveAtHead = this._liveAtHead.get(block);
                
                if (!mergeIntoSet(liveAtHead, localCalc.liveSet))
                    continue;
                
                for (let predecessor of block.predecessors) {
                    if (mergeIntoSet(this._liveAtTail.get(predecessor), liveAtHead)) {
                        dirtyBlocks.add(predecessor);
                        changed = true;
                    }
                }
            }
        } while (changed);
    }
    
    get thing() { return this._thing; }
    get code() { return this._code; }
    get liveAtHead() { return this._liveAtHead; }
    get liveAtTail() { return this._liveAtTail; }
    
    localCalc(block)
    {
        let liveness = this;
        class LocalCalc {
            constructor()
            {
                this._liveSet = new Set(liveness.liveAtTail.get(block));
            }
            
            get liveSet() { return this._liveSet; }
            
            execute(instIndex)
            {
                let inst = block.at(instIndex);
                
                // First handle the early defs of the next instruction.
                if (instIndex + 1 < block.size) {
                    block.at(instIndex + 1).forEach(
                        liveness.thing,
                        (value, role, type, width) => {
                            if (Arg.isEarlyDef(role))
                                this._liveSet.delete(value);
                        });
                }
                
                // Then handle defs.
                inst.forEach(
                    liveness.thing,
                    (value, role, type, width) => {
                        if (Arg.isLateDef(role))
                            this._liveSet.delete(value);
                    });
                
                // Then handle uses.
                inst.forEach(
                    liveness.thing,
                    (value, role, type, width) => {
                        if (Arg.isEarlyUse(role))
                            this._liveSet.add(value);
                    });
                
                // Finally handle the late uses of the previous instruction.
                if (instIndex - 1 >= 0) {
                    block.at(instIndex - 1).forEach(
                        liveness.thing,
                        (value, role, type, width) => {
                            if (Arg.isLateUse(role))
                                this._liveSet.add(value);
                        });
                }
            }
        }
        
        return new LocalCalc();
    }
}


class Insertion {
    constructor(index, element)
    {
        this._index = index;
        this._element = element;
    }
    
    get index() { return this._index; }
    get element() { return this._element; }
    
    lessThan(other)
    {
        return this._index < other._index;
    }
}

class InsertionSet {
    constructor()
    {
        this._insertions = []
    }
    
    appendInsertion(insertion)
    {
        this._insertions.push(insertion);
    }
    
    append(index, element)
    {
        this.appendInsertion(new Insertion(index, element));
    }
    
    execute(target)
    {
        // We bubble-sort because that's what the C++ code, and for the same reason as we do it:
        // the stdlib doesn't have a stable sort and mergesort is slower in the common case of the
        // array usually being sorted. This array is usually sorted.
        bubbleSort(this._insertions, (a, b) => (a.lessThan(b)));
        
        let numInsertions = this._insertions.length;
        if (!numInsertions)
            return 0;
        let originalTargetSize = target.length;
        target.length += numInsertions;
        let lastIndex = target.length;
        for (let indexInInsertions = numInsertions; indexInInsertions--;) {
            let insertion = this._insertions[indexInInsertions];
            if (indexInInsertions && insertion.index < this._insertions[indexInInsertions - 1].index)
                throw new Error("Insertions out of order");
            if (insertion.index > originalTargetSize)
                throw new Error("Out-of-bounds insertion");
            let firstIndex = insertion.index + indexInInsertions;
            let indexOffset = indexInInsertions + 1;
            for (let i = lastIndex; --i > firstIndex;)
                target[i] = target[i - indexOffset];
            target[firstIndex] = insertion.element;
            lastIndex = firstIndex;
        }
        this._insertions = [];
        return numInsertions;
    }
}


function allocateStack(code)
{
    if (code.frameSize)
        throw new Error("Frame size already determined");
    
    function attemptAssignment(slot, offsetFromFP, otherSlots)
    {
        if (offsetFromFP > 0)
            throw new Error("Expect negative offset");
        
        offsetFromFP = -roundUpToMultipleOf(slot.alignment, -offsetFromFP);
        
        for (let otherSlot of otherSlots) {
            if (!otherSlot.offsetFromFP)
                continue;
            let overlap = rangesOverlap(
                offsetFromFP,
                offsetFromFP + slot.byteSize,
                otherSlot.offsetFromFP,
                otherSlot.offsetFromFP + otherSlot.byteSize);
            if (overlap)
                return false;
        }
        
        slot.setOffsetFromFP(offsetFromFP);
        return true;
    }
    
    function assign(slot, otherSlots)
    {
        if (attemptAssignment(slot, -slot.byteSize, otherSlots))
            return;
        
        for (let otherSlot of otherSlots) {
            if (!otherSlot.offsetFromFP)
                continue;
            if (attemptAssignment(slot, otherSlot.offsetFromFP - slot.byteSize, otherSlots))
                return;
        }
        
        throw new Error("Assignment failed");
    }
    
    // Allocate all of the escaped slots in order. This is kind of a crazy algorithm to allow for
    // the possibility of stack slots being assigned frame offsets before we even get here.
    let assignedEscapedStackSlots = [];
    let escapedStackSlotsWorklist = [];
    for (let slot of code.stackSlots) {
        if (slot.isLocked) {
            if (slot.offsetFromFP)
                assignedEscapedStackSlots.push(slot);
            else
                escapedStackSlotsWorklist.push(slot);
        } else {
            if (slot.offsetFromFP)
                throw new Error("Offset already assigned");
        }
    }
    
    // This is a fairly espensive loop, but it's OK because we'll usually only have a handful of
    // escaped stack slots.
    while (escapedStackSlotsWorklist.length) {
        let slot = escapedStackSlotsWorklist.pop();
        assign(slot, assignedEscapedStackSlots);
        assignedEscapedStackSlots.push(slot);
    }
    
    // Now we handle the spill slots.
    let liveness = new Liveness(StackSlot, code);
    let interference = new Map();
    for (let slot of code.stackSlots)
        interference.set(slot, new Set());
    let slots = [];
    
    for (let block of code) {
        let localCalc = liveness.localCalc(block);
        
        function interfere(instIndex)
        {
            Inst.forEachDef(
                StackSlot, block.get(instIndex), block.get(instIndex + 1),
                (slot, role, type, width) => {
                    if (!slot.isSpill)
                        return;
                    
                    for (let otherSlot of localCalc.liveSet) {
                        interference.get(slot).add(otherSlot);
                        interference.get(otherSlot).add(slot);
                    }
                });
        }
        
        for (let instIndex = block.size; instIndex--;) {
            // Kill dead stores. For simplicity we say that a store is killable if it has only late
            // defs and those late defs are to things that are dead right now. We only do that
            // because that's the only kind of dead stack store we will see here.
            let inst = block.at(instIndex);
            if (!inst.hasNonArgEffects) {
                let ok = true;
                inst.forEachArg((arg, role, type, width) => {
                    if (Arg.isEarlyDef(role)) {
                        ok = false;
                        return;
                    }
                    if (!Arg.isLateDef(role))
                        return;
                    if (!arg.isStack) {
                        ok = false;
                        return;
                    }
                    
                    let slot = arg.stackSlot;
                    if (!slot.isSpill) {
                        ok = false;
                        return;
                    }
                    
                    if (localCalc.liveSet.has(slot)) {
                        ok = false;
                        return;
                    }
                });
                if (ok)
                    inst.clear();
            }
            
            interfere(instIndex);
            localCalc.execute(instIndex);
        }
        interfere(-1);
        
        removeAllMatching(block.insts, inst => inst.opcode == Nop);
    }
    
    // Now we assign stack locations. At its heart this algorithm is just first-fit. For each
    // StackSlot we just want to find the offsetFromFP that is closest to zero while ensuring no
    // overlap with other StackSlots that this overlaps with.
    for (let slot of code.stackSlots) {
        if (slot.offsetFromFP)
            continue;
        
        assign(slot, assignedEscapedStackSlots.concat(Array.from(interference.get(slot))));
    }
    
    // Figure out how much stack we're using for stack slots.
    let frameSizeForStackSlots = 0;
    for (let slot of code.stackSlots) {
        frameSizeForStackSlots = Math.max(
            frameSizeForStackSlots,
            -slot.offsetFromFP);
    }
    
    frameSizeForStackSlots = roundUpToMultipleOf(stackAlignmentBytes, frameSizeForStackSlots);

    // No we need to deduce how much argument area we need.
    for (let block of code) {
        for (let inst of block) {
            for (let arg of inst.args) {
                if (arg.isCallArg) {
                    // For now, we assume that we use 8 bytes of the call arg. But that's not
                    // such an awesome assumption.
                    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=150454
                    if (arg.offset < 0)
                        throw new Error("Did not expect negative offset for callArg");
                    code.requestCallArgAreaSize(arg.offset + 8);
                }
            }
        }
    }
    
    code.setFrameSize(frameSizeForStackSlots + code.callArgAreaSize);
    
    // Finally transform the code to use Addrs instead of StackSlots. This is a lossless
    // transformation since we can search the StackSlots array to figure out which StackSlot any
    // offset-from-FP refers to.

    // FIXME: This may produce addresses that aren't valid if we end up with a ginormous stack frame.
    // We would have to scavenge for temporaries if this happened. Fortunately, this case will be
    // extremely rare so we can do crazy things when it arises.
    // https://bugs.webkit.org/show_bug.cgi?id=152530
    
    let insertionSet = new InsertionSet();
    for (let block of code) {
        for (let instIndex = 0; instIndex < block.size; ++instIndex) {
            let inst = block.at(instIndex);
            inst.forEachArg((arg, role, type, width) => {
                function stackAddr(offset)
                {
                    return Arg.createStackAddr(offset, code.frameSize, width);
                }
                
                switch (arg.kind) {
                case Arg.Stack: {
                    let slot = arg.stackSlot;
                    if (Arg.isZDef(role)
                        && slot.isSpill
                        && slot.byteSize > Arg.bytes(width)) {
                        // Currently we only handle this simple case because it's the only one
                        // that arises: ZDef's are only 32-bit right now. So, when we hit these
                        // assertions it means that we need to implement those other kinds of
                        // zero fills.
                        if (slot.byteSize != 8) {
                            throw new Error(
                                `Bad spill slot size for ZDef: ${slot.byteSize}, width is ${width}`);
                        }
                        if (width != 32)
                            throw new Error("Bad width for ZDef");
                        
                        insertionSet.insert(
                            instIndex + 1,
                            new Inst(
                                StoreZero32,
                                [stackAddr(arg.offset + 4 + slot.offsetFromFP)]));
                    }
                    return stackAddr(arg.offset + slot.offsetFromFP);
                }
                case Arg.CallArg:
                    return stackAddr(arg.offset - code.frameSize);
                default:
                    break;
                }
            });
        }
        insertionSet.execute(block.insts);
    }
}
"use strict";
// Generated by Air::dumpAsJS from executeIteration#EVx8pJ in Octane/gbemu
function createPayloadGbemuExecuteIteration()
{
    let code = new Code();
    let bb0 = code.addBlock();
    let bb1 = code.addBlock();
    let bb2 = code.addBlock();
    let bb3 = code.addBlock();
    let bb4 = code.addBlock();
    let bb5 = code.addBlock();
    let bb6 = code.addBlock();
    let bb7 = code.addBlock();
    let bb8 = code.addBlock();
    let bb9 = code.addBlock();
    let bb10 = code.addBlock();
    let bb11 = code.addBlock();
    let bb12 = code.addBlock();
    let bb13 = code.addBlock();
    let bb14 = code.addBlock();
    let bb15 = code.addBlock();
    let bb16 = code.addBlock();
    let bb17 = code.addBlock();
    let bb18 = code.addBlock();
    let bb19 = code.addBlock();
    let bb20 = code.addBlock();
    let bb21 = code.addBlock();
    let bb22 = code.addBlock();
    let bb23 = code.addBlock();
    let bb24 = code.addBlock();
    let bb25 = code.addBlock();
    let bb26 = code.addBlock();
    let bb27 = code.addBlock();
    let bb28 = code.addBlock();
    let bb29 = code.addBlock();
    let bb30 = code.addBlock();
    let bb31 = code.addBlock();
    let bb32 = code.addBlock();
    let bb33 = code.addBlock();
    let bb34 = code.addBlock();
    let bb35 = code.addBlock();
    let bb36 = code.addBlock();
    let bb37 = code.addBlock();
    let bb38 = code.addBlock();
    let bb39 = code.addBlock();
    let bb40 = code.addBlock();
    let bb41 = code.addBlock();
    let bb42 = code.addBlock();
    let slot0 = code.addStackSlot(64, Locked);
    let slot1 = code.addStackSlot(8, Spill);
    let slot2 = code.addStackSlot(8, Spill);
    let slot3 = code.addStackSlot(8, Spill);
    let slot4 = code.addStackSlot(8, Spill);
    let slot5 = code.addStackSlot(8, Spill);
    let slot6 = code.addStackSlot(8, Spill);
    let slot7 = code.addStackSlot(8, Spill);
    let slot8 = code.addStackSlot(8, Spill);
    let slot9 = code.addStackSlot(8, Spill);
    let slot10 = code.addStackSlot(8, Spill);
    let slot11 = code.addStackSlot(8, Spill);
    let slot12 = code.addStackSlot(40, Locked);
    slot12.setOffsetFromFP(-40);
    let tmp190 = code.newTmp(GP);
    let tmp189 = code.newTmp(GP);
    let tmp188 = code.newTmp(GP);
    let tmp187 = code.newTmp(GP);
    let tmp186 = code.newTmp(GP);
    let tmp185 = code.newTmp(GP);
    let tmp184 = code.newTmp(GP);
    let tmp183 = code.newTmp(GP);
    let tmp182 = code.newTmp(GP);
    let tmp181 = code.newTmp(GP);
    let tmp180 = code.newTmp(GP);
    let tmp179 = code.newTmp(GP);
    let tmp178 = code.newTmp(GP);
    let tmp177 = code.newTmp(GP);
    let tmp176 = code.newTmp(GP);
    let tmp175 = code.newTmp(GP);
    let tmp174 = code.newTmp(GP);
    let tmp173 = code.newTmp(GP);
    let tmp172 = code.newTmp(GP);
    let tmp171 = code.newTmp(GP);
    let tmp170 = code.newTmp(GP);
    let tmp169 = code.newTmp(GP);
    let tmp168 = code.newTmp(GP);
    let tmp167 = code.newTmp(GP);
    let tmp166 = code.newTmp(GP);
    let tmp165 = code.newTmp(GP);
    let tmp164 = code.newTmp(GP);
    let tmp163 = code.newTmp(GP);
    let tmp162 = code.newTmp(GP);
    let tmp161 = code.newTmp(GP);
    let tmp160 = code.newTmp(GP);
    let tmp159 = code.newTmp(GP);
    let tmp158 = code.newTmp(GP);
    let tmp157 = code.newTmp(GP);
    let tmp156 = code.newTmp(GP);
    let tmp155 = code.newTmp(GP);
    let tmp154 = code.newTmp(GP);
    let tmp153 = code.newTmp(GP);
    let tmp152 = code.newTmp(GP);
    let tmp151 = code.newTmp(GP);
    let tmp150 = code.newTmp(GP);
    let tmp149 = code.newTmp(GP);
    let tmp148 = code.newTmp(GP);
    let tmp147 = code.newTmp(GP);
    let tmp146 = code.newTmp(GP);
    let tmp145 = code.newTmp(GP);
    let tmp144 = code.newTmp(GP);
    let tmp143 = code.newTmp(GP);
    let tmp142 = code.newTmp(GP);
    let tmp141 = code.newTmp(GP);
    let tmp140 = code.newTmp(GP);
    let tmp139 = code.newTmp(GP);
    let tmp138 = code.newTmp(GP);
    let tmp137 = code.newTmp(GP);
    let tmp136 = code.newTmp(GP);
    let tmp135 = code.newTmp(GP);
    let tmp134 = code.newTmp(GP);
    let tmp133 = code.newTmp(GP);
    let tmp132 = code.newTmp(GP);
    let tmp131 = code.newTmp(GP);
    let tmp130 = code.newTmp(GP);
    let tmp129 = code.newTmp(GP);
    let tmp128 = code.newTmp(GP);
    let tmp127 = code.newTmp(GP);
    let tmp126 = code.newTmp(GP);
    let tmp125 = code.newTmp(GP);
    let tmp124 = code.newTmp(GP);
    let tmp123 = code.newTmp(GP);
    let tmp122 = code.newTmp(GP);
    let tmp121 = code.newTmp(GP);
    let tmp120 = code.newTmp(GP);
    let tmp119 = code.newTmp(GP);
    let tmp118 = code.newTmp(GP);
    let tmp117 = code.newTmp(GP);
    let tmp116 = code.newTmp(GP);
    let tmp115 = code.newTmp(GP);
    let tmp114 = code.newTmp(GP);
    let tmp113 = code.newTmp(GP);
    let tmp112 = code.newTmp(GP);
    let tmp111 = code.newTmp(GP);
    let tmp110 = code.newTmp(GP);
    let tmp109 = code.newTmp(GP);
    let tmp108 = code.newTmp(GP);
    let tmp107 = code.newTmp(GP);
    let tmp106 = code.newTmp(GP);
    let tmp105 = code.newTmp(GP);
    let tmp104 = code.newTmp(GP);
    let tmp103 = code.newTmp(GP);
    let tmp102 = code.newTmp(GP);
    let tmp101 = code.newTmp(GP);
    let tmp100 = code.newTmp(GP);
    let tmp99 = code.newTmp(GP);
    let tmp98 = code.newTmp(GP);
    let tmp97 = code.newTmp(GP);
    let tmp96 = code.newTmp(GP);
    let tmp95 = code.newTmp(GP);
    let tmp94 = code.newTmp(GP);
    let tmp93 = code.newTmp(GP);
    let tmp92 = code.newTmp(GP);
    let tmp91 = code.newTmp(GP);
    let tmp90 = code.newTmp(GP);
    let tmp89 = code.newTmp(GP);
    let tmp88 = code.newTmp(GP);
    let tmp87 = code.newTmp(GP);
    let tmp86 = code.newTmp(GP);
    let tmp85 = code.newTmp(GP);
    let tmp84 = code.newTmp(GP);
    let tmp83 = code.newTmp(GP);
    let tmp82 = code.newTmp(GP);
    let tmp81 = code.newTmp(GP);
    let tmp80 = code.newTmp(GP);
    let tmp79 = code.newTmp(GP);
    let tmp78 = code.newTmp(GP);
    let tmp77 = code.newTmp(GP);
    let tmp76 = code.newTmp(GP);
    let tmp75 = code.newTmp(GP);
    let tmp74 = code.newTmp(GP);
    let tmp73 = code.newTmp(GP);
    let tmp72 = code.newTmp(GP);
    let tmp71 = code.newTmp(GP);
    let tmp70 = code.newTmp(GP);
    let tmp69 = code.newTmp(GP);
    let tmp68 = code.newTmp(GP);
    let tmp67 = code.newTmp(GP);
    let tmp66 = code.newTmp(GP);
    let tmp65 = code.newTmp(GP);
    let tmp64 = code.newTmp(GP);
    let tmp63 = code.newTmp(GP);
    let tmp62 = code.newTmp(GP);
    let tmp61 = code.newTmp(GP);
    let tmp60 = code.newTmp(GP);
    let tmp59 = code.newTmp(GP);
    let tmp58 = code.newTmp(GP);
    let tmp57 = code.newTmp(GP);
    let tmp56 = code.newTmp(GP);
    let tmp55 = code.newTmp(GP);
    let tmp54 = code.newTmp(GP);
    let tmp53 = code.newTmp(GP);
    let tmp52 = code.newTmp(GP);
    let tmp51 = code.newTmp(GP);
    let tmp50 = code.newTmp(GP);
    let tmp49 = code.newTmp(GP);
    let tmp48 = code.newTmp(GP);
    let tmp47 = code.newTmp(GP);
    let tmp46 = code.newTmp(GP);
    let tmp45 = code.newTmp(GP);
    let tmp44 = code.newTmp(GP);
    let tmp43 = code.newTmp(GP);
    let tmp42 = code.newTmp(GP);
    let tmp41 = code.newTmp(GP);
    let tmp40 = code.newTmp(GP);
    let tmp39 = code.newTmp(GP);
    let tmp38 = code.newTmp(GP);
    let tmp37 = code.newTmp(GP);
    let tmp36 = code.newTmp(GP);
    let tmp35 = code.newTmp(GP);
    let tmp34 = code.newTmp(GP);
    let tmp33 = code.newTmp(GP);
    let tmp32 = code.newTmp(GP);
    let tmp31 = code.newTmp(GP);
    let tmp30 = code.newTmp(GP);
    let tmp29 = code.newTmp(GP);
    let tmp28 = code.newTmp(GP);
    let tmp27 = code.newTmp(GP);
    let tmp26 = code.newTmp(GP);
    let tmp25 = code.newTmp(GP);
    let tmp24 = code.newTmp(GP);
    let tmp23 = code.newTmp(GP);
    let tmp22 = code.newTmp(GP);
    let tmp21 = code.newTmp(GP);
    let tmp20 = code.newTmp(GP);
    let tmp19 = code.newTmp(GP);
    let tmp18 = code.newTmp(GP);
    let tmp17 = code.newTmp(GP);
    let tmp16 = code.newTmp(GP);
    let tmp15 = code.newTmp(GP);
    let tmp14 = code.newTmp(GP);
    let tmp13 = code.newTmp(GP);
    let tmp12 = code.newTmp(GP);
    let tmp11 = code.newTmp(GP);
    let tmp10 = code.newTmp(GP);
    let tmp9 = code.newTmp(GP);
    let tmp8 = code.newTmp(GP);
    let tmp7 = code.newTmp(GP);
    let tmp6 = code.newTmp(GP);
    let tmp5 = code.newTmp(GP);
    let tmp4 = code.newTmp(GP);
    let tmp3 = code.newTmp(GP);
    let tmp2 = code.newTmp(GP);
    let tmp1 = code.newTmp(GP);
    let tmp0 = code.newTmp(GP);
    let ftmp7 = code.newTmp(FP);
    let ftmp6 = code.newTmp(FP);
    let ftmp5 = code.newTmp(FP);
    let ftmp4 = code.newTmp(FP);
    let ftmp3 = code.newTmp(FP);
    let ftmp2 = code.newTmp(FP);
    let ftmp1 = code.newTmp(FP);
    let ftmp0 = code.newTmp(FP);
    let inst;
    let arg;
    bb0.successors.push(new FrequentedBlock(bb2, Normal));
    bb0.successors.push(new FrequentedBlock(bb1, Normal));
    inst = new Inst(Move);
    arg = Arg.createBigImm(286904960, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbp, 16);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbp);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Scratch, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbp, 40);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(2, -65536);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 5);
    inst.args.push(arg);
    arg = Arg.createImm(21);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 8});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 8});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createAddr(Reg.rbx, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(286506544, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot10, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(286455168, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot4, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(287131344, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot6, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(10);
    inst.args.push(arg);
    arg = Arg.createStack(slot3, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(286474592, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot2, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(287209728, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot11, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(0, -65536);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(287112728, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot8, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(0, 65536);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot9, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(287112720, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(286506192, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot7, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(Equal);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createImm(862);
    inst.args.push(arg);
    bb0.append(inst);
    bb1.successors.push(new FrequentedBlock(bb41, Normal));
    bb1.successors.push(new FrequentedBlock(bb3, Normal));
    bb1.predecessors.push(bb0);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createImm(881);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb1.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 224);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb1.append(inst);
    inst = new Inst(BranchTest32);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb1.append(inst);
    bb2.successors.push(new FrequentedBlock(bb41, Normal));
    bb2.successors.push(new FrequentedBlock(bb3, Normal));
    bb2.predecessors.push(bb0);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 224);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb2.append(inst);
    inst = new Inst(BranchTest32);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb2.append(inst);
    bb3.successors.push(new FrequentedBlock(bb5, Normal));
    bb3.successors.push(new FrequentedBlock(bb4, Normal));
    bb3.predecessors.push(bb1);
    bb3.predecessors.push(bb40);
    bb3.predecessors.push(bb39);
    bb3.predecessors.push(bb2);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb3.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rsi, -1144);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb3.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(Equal);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    bb3.append(inst);
    bb4.successors.push(new FrequentedBlock(bb6, Normal));
    bb4.successors.push(new FrequentedBlock(bb7, Normal));
    bb4.predecessors.push(bb3);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(Equal);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createImm(2);
    inst.args.push(arg);
    bb4.append(inst);
    bb5.successors.push(new FrequentedBlock(bb6, Normal));
    bb5.predecessors.push(bb3);
    inst = new Inst(Move);
    arg = Arg.createImm(7);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 232);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 256);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 248);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(And32);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(And32);
    arg = Arg.createImm(31);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 240);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Jump);
    bb5.append(inst);
    bb6.successors.push(new FrequentedBlock(bb7, Normal));
    bb6.predecessors.push(bb4);
    bb6.predecessors.push(bb5);
    inst = new Inst(Add32);
    arg = Arg.createImm(-1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rsi, -1144);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(Jump);
    bb6.append(inst);
    bb7.successors.push(new FrequentedBlock(bb8, Normal));
    bb7.successors.push(new FrequentedBlock(bb9, Normal));
    bb7.predecessors.push(bb4);
    bb7.predecessors.push(bb6);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 240);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb7.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(GreaterThan);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createImm(0);
    inst.args.push(arg);
    bb7.append(inst);
    bb8.successors.push(new FrequentedBlock(bb9, Normal));
    bb8.predecessors.push(bb7);
    inst = new Inst(Move);
    arg = Arg.createBigImm(286455168, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb8.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(286455168, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb8.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    bb8.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    bb8.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    bb8.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.rcx);
    inst.extraClobberedRegs.add(Reg.rdx);
    inst.extraClobberedRegs.add(Reg.rsi);
    inst.extraClobberedRegs.add(Reg.rdi);
    inst.extraClobberedRegs.add(Reg.r8);
    inst.extraClobberedRegs.add(Reg.r9);
    inst.extraClobberedRegs.add(Reg.r10);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.xmm0);
    inst.extraClobberedRegs.add(Reg.xmm1);
    inst.extraClobberedRegs.add(Reg.xmm2);
    inst.extraClobberedRegs.add(Reg.xmm3);
    inst.extraClobberedRegs.add(Reg.xmm4);
    inst.extraClobberedRegs.add(Reg.xmm5);
    inst.extraClobberedRegs.add(Reg.xmm6);
    inst.extraClobberedRegs.add(Reg.xmm7);
    inst.extraClobberedRegs.add(Reg.xmm8);
    inst.extraClobberedRegs.add(Reg.xmm9);
    inst.extraClobberedRegs.add(Reg.xmm10);
    inst.extraClobberedRegs.add(Reg.xmm11);
    inst.extraClobberedRegs.add(Reg.xmm12);
    inst.extraClobberedRegs.add(Reg.xmm13);
    inst.extraClobberedRegs.add(Reg.xmm14);
    inst.extraClobberedRegs.add(Reg.xmm15);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb8.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb8.append(inst);
    inst = new Inst(Jump);
    bb8.append(inst);
    bb9.successors.push(new FrequentedBlock(bb12, Normal));
    bb9.successors.push(new FrequentedBlock(bb10, Normal));
    bb9.predecessors.push(bb7);
    bb9.predecessors.push(bb8);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 304);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    bb9.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 128);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb9.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.r8, 0);
    inst.args.push(arg);
    arg = Arg.createImm(80);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb9.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.r8, 8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb9.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rax, -8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb9.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb9.append(inst);
    inst = new Inst(Move);
    arg = Arg.createIndex(Reg.rax, Reg.rsi, 8, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb9.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb9.append(inst);
    inst = new Inst(MoveConditionallyTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createImm(-1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb9.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb9.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rcx, 5);
    inst.args.push(arg);
    arg = Arg.createImm(23);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 8});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 8});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb9.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rcx, 24);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb9.append(inst);
    inst = new Inst(Branch64);
    arg = Arg.createRelCond(Equal);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createStack(slot7, 0);
    inst.args.push(arg);
    bb9.append(inst);
    bb10.successors.push(new FrequentedBlock(bb11, Normal));
    bb10.successors.push(new FrequentedBlock(bb13, Normal));
    bb10.predecessors.push(bb9);
    inst = new Inst(Branch64);
    arg = Arg.createRelCond(Equal);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createStack(slot10, 0);
    inst.args.push(arg);
    bb10.append(inst);
    bb11.successors.push(new FrequentedBlock(bb14, Normal));
    bb11.predecessors.push(bb10);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot0, 0);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 344);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rdi, 0);
    inst.args.push(arg);
    arg = Arg.createImm(502);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb11.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rdi, 16);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rdi, 24);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb11.append(inst);
    inst = new Inst(Load8);
    arg = Arg.createIndex(Reg.rsi, Reg.rax, 1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Jump);
    bb11.append(inst);
    bb12.successors.push(new FrequentedBlock(bb14, Normal));
    bb12.predecessors.push(bb9);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot0, 0);
    inst.args.push(arg);
    bb12.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 336);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb12.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 456);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb12.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    bb12.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rdi, 0);
    inst.args.push(arg);
    arg = Arg.createImm(502);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb12.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rdi, 16);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb12.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rdi, 24);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb12.append(inst);
    inst = new Inst(Load8);
    arg = Arg.createIndex(Reg.rsi, Reg.rax, 1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb12.append(inst);
    inst = new Inst(Jump);
    bb12.append(inst);
    bb13.predecessors.push(bb10);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb13.append(inst);
    inst = new Inst(Oops);
    bb13.append(inst);
    bb14.successors.push(new FrequentedBlock(bb15, Normal));
    bb14.successors.push(new FrequentedBlock(bb16, Normal));
    bb14.predecessors.push(bb11);
    bb14.predecessors.push(bb12);
    inst = new Inst(Add32);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb14.append(inst);
    inst = new Inst(ZeroExtend16To32);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb14.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb14.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 128);
    inst.args.push(arg);
    bb14.append(inst);
    inst = new Inst(BranchTest32);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 216);
    inst.args.push(arg);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    bb14.append(inst);
    bb15.predecessors.push(bb14);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb15.append(inst);
    inst = new Inst(Oops);
    bb15.append(inst);
    bb16.successors.push(new FrequentedBlock(bb18, Normal));
    bb16.successors.push(new FrequentedBlock(bb17, Normal));
    bb16.predecessors.push(bb14);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, -1752);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rdx, 16);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rdx, 24);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb16.append(inst);
    inst = new Inst(Load8);
    arg = Arg.createIndex(Reg.rax, Reg.rcx, 1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 272);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(287112720, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rax, 0);
    inst.args.push(arg);
    arg = Arg.createImm(80);
    inst.args.push(arg);
    arg = Arg.createBigImm(287112720, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(287112728, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rax, -8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createIndex(Reg.rax, Reg.rcx, 8, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(MoveConditionallyTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createImm(-1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createImm(2);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(287112720, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createCallArg(32);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    arg = Arg.createCallArg(32);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.rcx);
    inst.extraClobberedRegs.add(Reg.rdx);
    inst.extraClobberedRegs.add(Reg.rsi);
    inst.extraClobberedRegs.add(Reg.rdi);
    inst.extraClobberedRegs.add(Reg.r8);
    inst.extraClobberedRegs.add(Reg.r9);
    inst.extraClobberedRegs.add(Reg.r10);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.xmm0);
    inst.extraClobberedRegs.add(Reg.xmm1);
    inst.extraClobberedRegs.add(Reg.xmm2);
    inst.extraClobberedRegs.add(Reg.xmm3);
    inst.extraClobberedRegs.add(Reg.xmm4);
    inst.extraClobberedRegs.add(Reg.xmm5);
    inst.extraClobberedRegs.add(Reg.xmm6);
    inst.extraClobberedRegs.add(Reg.xmm7);
    inst.extraClobberedRegs.add(Reg.xmm8);
    inst.extraClobberedRegs.add(Reg.xmm9);
    inst.extraClobberedRegs.add(Reg.xmm10);
    inst.extraClobberedRegs.add(Reg.xmm11);
    inst.extraClobberedRegs.add(Reg.xmm12);
    inst.extraClobberedRegs.add(Reg.xmm13);
    inst.extraClobberedRegs.add(Reg.xmm14);
    inst.extraClobberedRegs.add(Reg.xmm15);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb16.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rdx, -1088);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 272);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 280);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Rshift32);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb16.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rdx, -1088);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rdx, -88);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rdx, -1176);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb16.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rcx, 0);
    inst.args.push(arg);
    arg = Arg.createImm(80);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rcx, 8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rax, -8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb16.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createIndex(Reg.rax, Reg.rdx, 8, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(MoveConditionallyTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createImm(-1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb16.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rax, 5);
    inst.args.push(arg);
    arg = Arg.createImm(23);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 8});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 8});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createImm(2);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createCallArg(32);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    arg = Arg.createCallArg(32);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.rcx);
    inst.extraClobberedRegs.add(Reg.rdx);
    inst.extraClobberedRegs.add(Reg.rsi);
    inst.extraClobberedRegs.add(Reg.rdi);
    inst.extraClobberedRegs.add(Reg.r8);
    inst.extraClobberedRegs.add(Reg.r9);
    inst.extraClobberedRegs.add(Reg.r10);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.xmm0);
    inst.extraClobberedRegs.add(Reg.xmm1);
    inst.extraClobberedRegs.add(Reg.xmm2);
    inst.extraClobberedRegs.add(Reg.xmm3);
    inst.extraClobberedRegs.add(Reg.xmm4);
    inst.extraClobberedRegs.add(Reg.xmm5);
    inst.extraClobberedRegs.add(Reg.xmm6);
    inst.extraClobberedRegs.add(Reg.xmm7);
    inst.extraClobberedRegs.add(Reg.xmm8);
    inst.extraClobberedRegs.add(Reg.xmm9);
    inst.extraClobberedRegs.add(Reg.xmm10);
    inst.extraClobberedRegs.add(Reg.xmm11);
    inst.extraClobberedRegs.add(Reg.xmm12);
    inst.extraClobberedRegs.add(Reg.xmm13);
    inst.extraClobberedRegs.add(Reg.xmm14);
    inst.extraClobberedRegs.add(Reg.xmm15);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb16.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 272);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 280);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Rshift32);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rsi, -1048);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    bb16.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rsi, -1048);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rsi, -1072);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb16.append(inst);
    inst = new Inst(Branch64);
    arg = Arg.createRelCond(Below);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    bb16.append(inst);
    bb17.successors.push(new FrequentedBlock(bb19, Normal));
    bb17.predecessors.push(bb16);
    inst = new Inst(ConvertInt32ToDouble);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    bb17.append(inst);
    inst = new Inst(Jump);
    bb17.append(inst);
    bb18.successors.push(new FrequentedBlock(bb19, Normal));
    bb18.predecessors.push(bb16);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb18.append(inst);
    inst = new Inst(Move64ToDouble);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    bb18.append(inst);
    inst = new Inst(Jump);
    bb18.append(inst);
    bb19.successors.push(new FrequentedBlock(bb20, Normal));
    bb19.successors.push(new FrequentedBlock(bb32, Normal));
    bb19.predecessors.push(bb17);
    bb19.predecessors.push(bb18);
    inst = new Inst(ConvertInt32ToDouble);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    bb19.append(inst);
    inst = new Inst(AddDouble);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    bb19.append(inst);
    inst = new Inst(MoveDoubleTo64);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb19.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(0, 65536);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb19.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb19.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rsi, -1072);
    inst.args.push(arg);
    bb19.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rsi, -1080);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb19.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb19.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb19.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rsi, -1080);
    inst.args.push(arg);
    bb19.append(inst);
    inst = new Inst(BranchTest32);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rsi, -1104);
    inst.args.push(arg);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    bb19.append(inst);
    bb20.successors.push(new FrequentedBlock(bb21, Normal));
    bb20.successors.push(new FrequentedBlock(bb32, Normal));
    bb20.predecessors.push(bb19);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rsi, -1096);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb20.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb20.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb20.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rsi, -1096);
    inst.args.push(arg);
    bb20.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rsi, -1112);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    bb20.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(GreaterThanOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    bb20.append(inst);
    bb21.successors.push(new FrequentedBlock(bb23, Normal));
    bb21.predecessors.push(bb20);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 344);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    bb21.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.r12, 0);
    inst.args.push(arg);
    arg = Arg.createImm(502);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb21.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.r12, 16);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb21.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createAddr(Reg.r12, 24);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r9);
    inst.args.push(arg);
    bb21.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(BelowOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r9);
    inst.args.push(arg);
    arg = Arg.createImm(65286);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb21.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 232);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    bb21.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 256);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    bb21.append(inst);
    inst = new Inst(Jump);
    bb21.append(inst);
    bb22.successors.push(new FrequentedBlock(bb23, Normal));
    bb22.predecessors.push(bb30);
    bb22.predecessors.push(bb31);
    bb22.predecessors.push(bb29);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb22.append(inst);
    inst = new Inst(Jump);
    bb22.append(inst);
    bb23.successors.push(new FrequentedBlock(bb25, Normal));
    bb23.successors.push(new FrequentedBlock(bb24, Normal));
    bb23.predecessors.push(bb21);
    bb23.predecessors.push(bb22);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.UseZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb23.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rsi, -1096);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Load8);
    arg = Arg.createAddr(Reg.rdi, 65285);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.UseZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    bb23.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(BelowOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r9);
    inst.args.push(arg);
    arg = Arg.createImm(65285);
    inst.args.push(arg);
    bb23.append(inst);
    bb24.successors.push(new FrequentedBlock(bb26, Normal));
    bb24.successors.push(new FrequentedBlock(bb30, Normal));
    bb24.predecessors.push(bb23);
    inst = new Inst(Store8);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rdi, 65285);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(Equal);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createImm(256);
    inst.args.push(arg);
    bb24.append(inst);
    bb25.successors.push(new FrequentedBlock(bb26, Normal));
    bb25.successors.push(new FrequentedBlock(bb30, Normal));
    bb25.predecessors.push(bb23);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(Equal);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createImm(256);
    inst.args.push(arg);
    bb25.append(inst);
    bb26.successors.push(new FrequentedBlock(bb28, Normal));
    bb26.successors.push(new FrequentedBlock(bb27, Normal));
    bb26.predecessors.push(bb24);
    bb26.predecessors.push(bb25);
    inst = new Inst(Load8);
    arg = Arg.createAddr(Reg.rdi, 65286);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb26.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(BelowOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r9);
    inst.args.push(arg);
    arg = Arg.createImm(65285);
    inst.args.push(arg);
    bb26.append(inst);
    bb27.successors.push(new FrequentedBlock(bb28, Normal));
    bb27.predecessors.push(bb26);
    inst = new Inst(Store8);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rdi, 65285);
    inst.args.push(arg);
    bb27.append(inst);
    inst = new Inst(Jump);
    bb27.append(inst);
    bb28.successors.push(new FrequentedBlock(bb29, Normal));
    bb28.successors.push(new FrequentedBlock(bb31, Normal));
    bb28.predecessors.push(bb26);
    bb28.predecessors.push(bb27);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 248);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb28.append(inst);
    inst = new Inst(Or32);
    arg = Arg.createImm(4);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb28.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    bb28.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    bb28.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 248);
    inst.args.push(arg);
    bb28.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    bb28.append(inst);
    inst = new Inst(BranchTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    bb28.append(inst);
    bb29.successors.push(new FrequentedBlock(bb22, Normal));
    bb29.successors.push(new FrequentedBlock(bb32, Normal));
    bb29.predecessors.push(bb28);
    inst = new Inst(And32);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb29.append(inst);
    inst = new Inst(And32);
    arg = Arg.createImm(31);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb29.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb29.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 240);
    inst.args.push(arg);
    bb29.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(GreaterThanOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    bb29.append(inst);
    bb30.successors.push(new FrequentedBlock(bb22, Normal));
    bb30.successors.push(new FrequentedBlock(bb32, Normal));
    bb30.predecessors.push(bb24);
    bb30.predecessors.push(bb25);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(GreaterThanOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    bb30.append(inst);
    bb31.successors.push(new FrequentedBlock(bb22, Normal));
    bb31.successors.push(new FrequentedBlock(bb32, Normal));
    bb31.predecessors.push(bb28);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(GreaterThanOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    bb31.append(inst);
    bb32.successors.push(new FrequentedBlock(bb33, Normal));
    bb32.successors.push(new FrequentedBlock(bb34, Normal));
    bb32.predecessors.push(bb19);
    bb32.predecessors.push(bb20);
    bb32.predecessors.push(bb30);
    bb32.predecessors.push(bb31);
    bb32.predecessors.push(bb29);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rsi, -1120);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb32.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(GreaterThan);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createImm(0);
    inst.args.push(arg);
    bb32.append(inst);
    bb33.predecessors.push(bb32);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb33.append(inst);
    inst = new Inst(Oops);
    bb33.append(inst);
    bb34.successors.push(new FrequentedBlock(bb36, Normal));
    bb34.successors.push(new FrequentedBlock(bb35, Normal));
    bb34.predecessors.push(bb32);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 136);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Branch64);
    arg = Arg.createRelCond(Below);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    bb34.append(inst);
    bb35.successors.push(new FrequentedBlock(bb37, Normal));
    bb35.successors.push(new FrequentedBlock(bb38, Normal));
    bb35.predecessors.push(bb34);
    inst = new Inst(ConvertInt32ToDouble);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    bb35.append(inst);
    inst = new Inst(BranchDouble);
    arg = Arg.createDoubleCond(DoubleGreaterThanOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    bb35.append(inst);
    bb36.successors.push(new FrequentedBlock(bb37, Normal));
    bb36.successors.push(new FrequentedBlock(bb38, Normal));
    bb36.predecessors.push(bb34);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb36.append(inst);
    inst = new Inst(Move64ToDouble);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    bb36.append(inst);
    inst = new Inst(BranchDouble);
    arg = Arg.createDoubleCond(DoubleGreaterThanOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    bb36.append(inst);
    bb37.successors.push(new FrequentedBlock(bb38, Normal));
    bb37.predecessors.push(bb35);
    bb37.predecessors.push(bb36);
    inst = new Inst(Move);
    arg = Arg.createBigImm(286474592, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb37.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(286474592, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb37.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    bb37.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    bb37.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    bb37.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.rcx);
    inst.extraClobberedRegs.add(Reg.rdx);
    inst.extraClobberedRegs.add(Reg.rsi);
    inst.extraClobberedRegs.add(Reg.rdi);
    inst.extraClobberedRegs.add(Reg.r8);
    inst.extraClobberedRegs.add(Reg.r9);
    inst.extraClobberedRegs.add(Reg.r10);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.xmm0);
    inst.extraClobberedRegs.add(Reg.xmm1);
    inst.extraClobberedRegs.add(Reg.xmm2);
    inst.extraClobberedRegs.add(Reg.xmm3);
    inst.extraClobberedRegs.add(Reg.xmm4);
    inst.extraClobberedRegs.add(Reg.xmm5);
    inst.extraClobberedRegs.add(Reg.xmm6);
    inst.extraClobberedRegs.add(Reg.xmm7);
    inst.extraClobberedRegs.add(Reg.xmm8);
    inst.extraClobberedRegs.add(Reg.xmm9);
    inst.extraClobberedRegs.add(Reg.xmm10);
    inst.extraClobberedRegs.add(Reg.xmm11);
    inst.extraClobberedRegs.add(Reg.xmm12);
    inst.extraClobberedRegs.add(Reg.xmm13);
    inst.extraClobberedRegs.add(Reg.xmm14);
    inst.extraClobberedRegs.add(Reg.xmm15);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb37.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb37.append(inst);
    inst = new Inst(Jump);
    bb37.append(inst);
    bb38.successors.push(new FrequentedBlock(bb39, Normal));
    bb38.successors.push(new FrequentedBlock(bb40, Normal));
    bb38.predecessors.push(bb35);
    bb38.predecessors.push(bb37);
    bb38.predecessors.push(bb36);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 0);
    inst.args.push(arg);
    arg = Arg.createImm(881);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb38.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb38.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rdx, -1824);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb38.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb38.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.UseZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    bb38.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb38.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb38.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rdx, -1824);
    inst.args.push(arg);
    bb38.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rdx, -1832);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb38.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(GreaterThan);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb38.append(inst);
    bb39.successors.push(new FrequentedBlock(bb42, Normal));
    bb39.successors.push(new FrequentedBlock(bb3, Normal));
    bb39.predecessors.push(bb38);
    inst = new Inst(Move);
    arg = Arg.createBigImm(286474592, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(286474592, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.rcx);
    inst.extraClobberedRegs.add(Reg.rdx);
    inst.extraClobberedRegs.add(Reg.rsi);
    inst.extraClobberedRegs.add(Reg.rdi);
    inst.extraClobberedRegs.add(Reg.r8);
    inst.extraClobberedRegs.add(Reg.r9);
    inst.extraClobberedRegs.add(Reg.r10);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.xmm0);
    inst.extraClobberedRegs.add(Reg.xmm1);
    inst.extraClobberedRegs.add(Reg.xmm2);
    inst.extraClobberedRegs.add(Reg.xmm3);
    inst.extraClobberedRegs.add(Reg.xmm4);
    inst.extraClobberedRegs.add(Reg.xmm5);
    inst.extraClobberedRegs.add(Reg.xmm6);
    inst.extraClobberedRegs.add(Reg.xmm7);
    inst.extraClobberedRegs.add(Reg.xmm8);
    inst.extraClobberedRegs.add(Reg.xmm9);
    inst.extraClobberedRegs.add(Reg.xmm10);
    inst.extraClobberedRegs.add(Reg.xmm11);
    inst.extraClobberedRegs.add(Reg.xmm12);
    inst.extraClobberedRegs.add(Reg.xmm13);
    inst.extraClobberedRegs.add(Reg.xmm14);
    inst.extraClobberedRegs.add(Reg.xmm15);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb39.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb39.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 224);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Or32);
    arg = Arg.createImm(2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 224);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(287131344, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(287131344, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(287209728, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.rcx);
    inst.extraClobberedRegs.add(Reg.rdx);
    inst.extraClobberedRegs.add(Reg.rsi);
    inst.extraClobberedRegs.add(Reg.rdi);
    inst.extraClobberedRegs.add(Reg.r8);
    inst.extraClobberedRegs.add(Reg.r9);
    inst.extraClobberedRegs.add(Reg.r10);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.xmm0);
    inst.extraClobberedRegs.add(Reg.xmm1);
    inst.extraClobberedRegs.add(Reg.xmm2);
    inst.extraClobberedRegs.add(Reg.xmm3);
    inst.extraClobberedRegs.add(Reg.xmm4);
    inst.extraClobberedRegs.add(Reg.xmm5);
    inst.extraClobberedRegs.add(Reg.xmm6);
    inst.extraClobberedRegs.add(Reg.xmm7);
    inst.extraClobberedRegs.add(Reg.xmm8);
    inst.extraClobberedRegs.add(Reg.xmm9);
    inst.extraClobberedRegs.add(Reg.xmm10);
    inst.extraClobberedRegs.add(Reg.xmm11);
    inst.extraClobberedRegs.add(Reg.xmm12);
    inst.extraClobberedRegs.add(Reg.xmm13);
    inst.extraClobberedRegs.add(Reg.xmm14);
    inst.extraClobberedRegs.add(Reg.xmm15);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb39.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb39.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 224);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb39.append(inst);
    inst = new Inst(BranchTest32);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb39.append(inst);
    bb40.successors.push(new FrequentedBlock(bb42, Normal));
    bb40.successors.push(new FrequentedBlock(bb3, Normal));
    bb40.predecessors.push(bb38);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 224);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb40.append(inst);
    inst = new Inst(BranchTest32);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb40.append(inst);
    bb41.predecessors.push(bb1);
    bb41.predecessors.push(bb2);
    inst = new Inst(Move);
    arg = Arg.createImm(10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb41.append(inst);
    inst = new Inst(Ret64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb41.append(inst);
    bb42.predecessors.push(bb40);
    bb42.predecessors.push(bb39);
    inst = new Inst(Move);
    arg = Arg.createImm(10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb42.append(inst);
    inst = new Inst(Ret64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb42.append(inst);
    return code;
}
"use strict";
// Generated by Air::dumpAsJS from gaussianBlur#A8vcYg in Kraken/imaging-gaussian-blur
function createPayloadImagingGaussianBlurGaussianBlur()
{
    let code = new Code();
    let bb0 = code.addBlock();
    let bb1 = code.addBlock();
    let bb2 = code.addBlock();
    let bb3 = code.addBlock();
    let bb4 = code.addBlock();
    let bb5 = code.addBlock();
    let bb6 = code.addBlock();
    let bb7 = code.addBlock();
    let bb8 = code.addBlock();
    let bb9 = code.addBlock();
    let bb10 = code.addBlock();
    let bb11 = code.addBlock();
    let bb12 = code.addBlock();
    let bb13 = code.addBlock();
    let bb14 = code.addBlock();
    let bb15 = code.addBlock();
    let bb16 = code.addBlock();
    let bb17 = code.addBlock();
    let bb18 = code.addBlock();
    let bb19 = code.addBlock();
    let bb20 = code.addBlock();
    let bb21 = code.addBlock();
    let bb22 = code.addBlock();
    let bb23 = code.addBlock();
    let bb24 = code.addBlock();
    let bb25 = code.addBlock();
    let bb26 = code.addBlock();
    let bb27 = code.addBlock();
    let bb28 = code.addBlock();
    let bb29 = code.addBlock();
    let bb30 = code.addBlock();
    let bb31 = code.addBlock();
    let bb32 = code.addBlock();
    let bb33 = code.addBlock();
    let bb34 = code.addBlock();
    let bb35 = code.addBlock();
    let bb36 = code.addBlock();
    let slot0 = code.addStackSlot(40, Locked);
    let slot1 = code.addStackSlot(8, Spill);
    let slot2 = code.addStackSlot(8, Spill);
    let slot3 = code.addStackSlot(4, Spill);
    let slot4 = code.addStackSlot(8, Spill);
    let slot5 = code.addStackSlot(8, Spill);
    let slot6 = code.addStackSlot(40, Locked);
    slot6.setOffsetFromFP(-40);
    let tmp141 = code.newTmp(GP);
    let tmp140 = code.newTmp(GP);
    let tmp139 = code.newTmp(GP);
    let tmp138 = code.newTmp(GP);
    let tmp137 = code.newTmp(GP);
    let tmp136 = code.newTmp(GP);
    let tmp135 = code.newTmp(GP);
    let tmp134 = code.newTmp(GP);
    let tmp133 = code.newTmp(GP);
    let tmp132 = code.newTmp(GP);
    let tmp131 = code.newTmp(GP);
    let tmp130 = code.newTmp(GP);
    let tmp129 = code.newTmp(GP);
    let tmp128 = code.newTmp(GP);
    let tmp127 = code.newTmp(GP);
    let tmp126 = code.newTmp(GP);
    let tmp125 = code.newTmp(GP);
    let tmp124 = code.newTmp(GP);
    let tmp123 = code.newTmp(GP);
    let tmp122 = code.newTmp(GP);
    let tmp121 = code.newTmp(GP);
    let tmp120 = code.newTmp(GP);
    let tmp119 = code.newTmp(GP);
    let tmp118 = code.newTmp(GP);
    let tmp117 = code.newTmp(GP);
    let tmp116 = code.newTmp(GP);
    let tmp115 = code.newTmp(GP);
    let tmp114 = code.newTmp(GP);
    let tmp113 = code.newTmp(GP);
    let tmp112 = code.newTmp(GP);
    let tmp111 = code.newTmp(GP);
    let tmp110 = code.newTmp(GP);
    let tmp109 = code.newTmp(GP);
    let tmp108 = code.newTmp(GP);
    let tmp107 = code.newTmp(GP);
    let tmp106 = code.newTmp(GP);
    let tmp105 = code.newTmp(GP);
    let tmp104 = code.newTmp(GP);
    let tmp103 = code.newTmp(GP);
    let tmp102 = code.newTmp(GP);
    let tmp101 = code.newTmp(GP);
    let tmp100 = code.newTmp(GP);
    let tmp99 = code.newTmp(GP);
    let tmp98 = code.newTmp(GP);
    let tmp97 = code.newTmp(GP);
    let tmp96 = code.newTmp(GP);
    let tmp95 = code.newTmp(GP);
    let tmp94 = code.newTmp(GP);
    let tmp93 = code.newTmp(GP);
    let tmp92 = code.newTmp(GP);
    let tmp91 = code.newTmp(GP);
    let tmp90 = code.newTmp(GP);
    let tmp89 = code.newTmp(GP);
    let tmp88 = code.newTmp(GP);
    let tmp87 = code.newTmp(GP);
    let tmp86 = code.newTmp(GP);
    let tmp85 = code.newTmp(GP);
    let tmp84 = code.newTmp(GP);
    let tmp83 = code.newTmp(GP);
    let tmp82 = code.newTmp(GP);
    let tmp81 = code.newTmp(GP);
    let tmp80 = code.newTmp(GP);
    let tmp79 = code.newTmp(GP);
    let tmp78 = code.newTmp(GP);
    let tmp77 = code.newTmp(GP);
    let tmp76 = code.newTmp(GP);
    let tmp75 = code.newTmp(GP);
    let tmp74 = code.newTmp(GP);
    let tmp73 = code.newTmp(GP);
    let tmp72 = code.newTmp(GP);
    let tmp71 = code.newTmp(GP);
    let tmp70 = code.newTmp(GP);
    let tmp69 = code.newTmp(GP);
    let tmp68 = code.newTmp(GP);
    let tmp67 = code.newTmp(GP);
    let tmp66 = code.newTmp(GP);
    let tmp65 = code.newTmp(GP);
    let tmp64 = code.newTmp(GP);
    let tmp63 = code.newTmp(GP);
    let tmp62 = code.newTmp(GP);
    let tmp61 = code.newTmp(GP);
    let tmp60 = code.newTmp(GP);
    let tmp59 = code.newTmp(GP);
    let tmp58 = code.newTmp(GP);
    let tmp57 = code.newTmp(GP);
    let tmp56 = code.newTmp(GP);
    let tmp55 = code.newTmp(GP);
    let tmp54 = code.newTmp(GP);
    let tmp53 = code.newTmp(GP);
    let tmp52 = code.newTmp(GP);
    let tmp51 = code.newTmp(GP);
    let tmp50 = code.newTmp(GP);
    let tmp49 = code.newTmp(GP);
    let tmp48 = code.newTmp(GP);
    let tmp47 = code.newTmp(GP);
    let tmp46 = code.newTmp(GP);
    let tmp45 = code.newTmp(GP);
    let tmp44 = code.newTmp(GP);
    let tmp43 = code.newTmp(GP);
    let tmp42 = code.newTmp(GP);
    let tmp41 = code.newTmp(GP);
    let tmp40 = code.newTmp(GP);
    let tmp39 = code.newTmp(GP);
    let tmp38 = code.newTmp(GP);
    let tmp37 = code.newTmp(GP);
    let tmp36 = code.newTmp(GP);
    let tmp35 = code.newTmp(GP);
    let tmp34 = code.newTmp(GP);
    let tmp33 = code.newTmp(GP);
    let tmp32 = code.newTmp(GP);
    let tmp31 = code.newTmp(GP);
    let tmp30 = code.newTmp(GP);
    let tmp29 = code.newTmp(GP);
    let tmp28 = code.newTmp(GP);
    let tmp27 = code.newTmp(GP);
    let tmp26 = code.newTmp(GP);
    let tmp25 = code.newTmp(GP);
    let tmp24 = code.newTmp(GP);
    let tmp23 = code.newTmp(GP);
    let tmp22 = code.newTmp(GP);
    let tmp21 = code.newTmp(GP);
    let tmp20 = code.newTmp(GP);
    let tmp19 = code.newTmp(GP);
    let tmp18 = code.newTmp(GP);
    let tmp17 = code.newTmp(GP);
    let tmp16 = code.newTmp(GP);
    let tmp15 = code.newTmp(GP);
    let tmp14 = code.newTmp(GP);
    let tmp13 = code.newTmp(GP);
    let tmp12 = code.newTmp(GP);
    let tmp11 = code.newTmp(GP);
    let tmp10 = code.newTmp(GP);
    let tmp9 = code.newTmp(GP);
    let tmp8 = code.newTmp(GP);
    let tmp7 = code.newTmp(GP);
    let tmp6 = code.newTmp(GP);
    let tmp5 = code.newTmp(GP);
    let tmp4 = code.newTmp(GP);
    let tmp3 = code.newTmp(GP);
    let tmp2 = code.newTmp(GP);
    let tmp1 = code.newTmp(GP);
    let tmp0 = code.newTmp(GP);
    let ftmp74 = code.newTmp(FP);
    let ftmp73 = code.newTmp(FP);
    let ftmp72 = code.newTmp(FP);
    let ftmp71 = code.newTmp(FP);
    let ftmp70 = code.newTmp(FP);
    let ftmp69 = code.newTmp(FP);
    let ftmp68 = code.newTmp(FP);
    let ftmp67 = code.newTmp(FP);
    let ftmp66 = code.newTmp(FP);
    let ftmp65 = code.newTmp(FP);
    let ftmp64 = code.newTmp(FP);
    let ftmp63 = code.newTmp(FP);
    let ftmp62 = code.newTmp(FP);
    let ftmp61 = code.newTmp(FP);
    let ftmp60 = code.newTmp(FP);
    let ftmp59 = code.newTmp(FP);
    let ftmp58 = code.newTmp(FP);
    let ftmp57 = code.newTmp(FP);
    let ftmp56 = code.newTmp(FP);
    let ftmp55 = code.newTmp(FP);
    let ftmp54 = code.newTmp(FP);
    let ftmp53 = code.newTmp(FP);
    let ftmp52 = code.newTmp(FP);
    let ftmp51 = code.newTmp(FP);
    let ftmp50 = code.newTmp(FP);
    let ftmp49 = code.newTmp(FP);
    let ftmp48 = code.newTmp(FP);
    let ftmp47 = code.newTmp(FP);
    let ftmp46 = code.newTmp(FP);
    let ftmp45 = code.newTmp(FP);
    let ftmp44 = code.newTmp(FP);
    let ftmp43 = code.newTmp(FP);
    let ftmp42 = code.newTmp(FP);
    let ftmp41 = code.newTmp(FP);
    let ftmp40 = code.newTmp(FP);
    let ftmp39 = code.newTmp(FP);
    let ftmp38 = code.newTmp(FP);
    let ftmp37 = code.newTmp(FP);
    let ftmp36 = code.newTmp(FP);
    let ftmp35 = code.newTmp(FP);
    let ftmp34 = code.newTmp(FP);
    let ftmp33 = code.newTmp(FP);
    let ftmp32 = code.newTmp(FP);
    let ftmp31 = code.newTmp(FP);
    let ftmp30 = code.newTmp(FP);
    let ftmp29 = code.newTmp(FP);
    let ftmp28 = code.newTmp(FP);
    let ftmp27 = code.newTmp(FP);
    let ftmp26 = code.newTmp(FP);
    let ftmp25 = code.newTmp(FP);
    let ftmp24 = code.newTmp(FP);
    let ftmp23 = code.newTmp(FP);
    let ftmp22 = code.newTmp(FP);
    let ftmp21 = code.newTmp(FP);
    let ftmp20 = code.newTmp(FP);
    let ftmp19 = code.newTmp(FP);
    let ftmp18 = code.newTmp(FP);
    let ftmp17 = code.newTmp(FP);
    let ftmp16 = code.newTmp(FP);
    let ftmp15 = code.newTmp(FP);
    let ftmp14 = code.newTmp(FP);
    let ftmp13 = code.newTmp(FP);
    let ftmp12 = code.newTmp(FP);
    let ftmp11 = code.newTmp(FP);
    let ftmp10 = code.newTmp(FP);
    let ftmp9 = code.newTmp(FP);
    let ftmp8 = code.newTmp(FP);
    let ftmp7 = code.newTmp(FP);
    let ftmp6 = code.newTmp(FP);
    let ftmp5 = code.newTmp(FP);
    let ftmp4 = code.newTmp(FP);
    let ftmp3 = code.newTmp(FP);
    let ftmp2 = code.newTmp(FP);
    let ftmp1 = code.newTmp(FP);
    let ftmp0 = code.newTmp(FP);
    let inst;
    let arg;
    bb0.successors.push(new FrequentedBlock(bb2, Normal));
    bb0.successors.push(new FrequentedBlock(bb1, Rare));
    inst = new Inst(Move);
    arg = Arg.createBigImm(144305904, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbp, 16);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbp);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Scratch, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(142547168, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(142547184, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(142547192, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(142547200, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(142547208, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(142547216, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(142547224, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(142547232, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(142547240, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rdi, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(0, -65536);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move64ToDouble);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(BranchDouble);
    arg = Arg.createDoubleCond(DoubleEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    bb0.append(inst);
    bb1.successors.push(new FrequentedBlock(bb2, Normal));
    bb1.predecessors.push(bb0);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(Below);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb1.append(inst);
    inst = new Inst(ConvertInt32ToDouble);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    bb1.append(inst);
    inst = new Inst(Jump);
    bb1.append(inst);
    bb2.successors.push(new FrequentedBlock(bb4, Normal));
    bb2.successors.push(new FrequentedBlock(bb3, Rare));
    bb2.predecessors.push(bb0);
    bb2.predecessors.push(bb1);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(Below);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb2.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(Below);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb2.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(Below);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb2.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    bb2.append(inst);
    inst = new Inst(Move64ToDouble);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    bb2.append(inst);
    inst = new Inst(BranchDouble);
    arg = Arg.createDoubleCond(DoubleEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    bb2.append(inst);
    bb3.successors.push(new FrequentedBlock(bb4, Normal));
    bb3.predecessors.push(bb2);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(Below);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb3.append(inst);
    inst = new Inst(ConvertInt32ToDouble);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    bb3.append(inst);
    inst = new Inst(Jump);
    bb3.append(inst);
    bb4.successors.push(new FrequentedBlock(bb6, Normal));
    bb4.successors.push(new FrequentedBlock(bb5, Rare));
    bb4.predecessors.push(bb2);
    bb4.predecessors.push(bb3);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Move64ToDouble);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(BranchDouble);
    arg = Arg.createDoubleCond(DoubleEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    bb4.append(inst);
    bb5.successors.push(new FrequentedBlock(bb6, Normal));
    bb5.predecessors.push(bb4);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(Below);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb5.append(inst);
    inst = new Inst(ConvertInt32ToDouble);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Jump);
    bb5.append(inst);
    bb6.successors.push(new FrequentedBlock(bb8, Normal));
    bb6.successors.push(new FrequentedBlock(bb7, Rare));
    bb6.predecessors.push(bb4);
    bb6.predecessors.push(bb5);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(Below);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb6.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(Move64ToDouble);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(BranchDouble);
    arg = Arg.createDoubleCond(DoubleEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    bb6.append(inst);
    bb7.successors.push(new FrequentedBlock(bb8, Normal));
    bb7.predecessors.push(bb6);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(Below);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb7.append(inst);
    inst = new Inst(ConvertInt32ToDouble);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    bb7.append(inst);
    inst = new Inst(Jump);
    bb7.append(inst);
    bb8.successors.push(new FrequentedBlock(bb10, Normal));
    bb8.successors.push(new FrequentedBlock(bb9, Rare));
    bb8.predecessors.push(bb6);
    bb8.predecessors.push(bb7);
    inst = new Inst(Move);
    arg = Arg.createBigImm(117076488, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    bb8.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.r8, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r9);
    inst.args.push(arg);
    bb8.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.r9);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    bb8.append(inst);
    inst = new Inst(Move64ToDouble);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm6);
    inst.args.push(arg);
    bb8.append(inst);
    inst = new Inst(BranchDouble);
    arg = Arg.createDoubleCond(DoubleEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm6);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm6);
    inst.args.push(arg);
    bb8.append(inst);
    bb9.successors.push(new FrequentedBlock(bb10, Normal));
    bb9.predecessors.push(bb8);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(Below);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r9);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r9);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb9.append(inst);
    inst = new Inst(ConvertInt32ToDouble);
    arg = Arg.createTmp(Reg.r9);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm6);
    inst.args.push(arg);
    bb9.append(inst);
    inst = new Inst(Jump);
    bb9.append(inst);
    bb10.successors.push(new FrequentedBlock(bb18, Normal));
    bb10.predecessors.push(bb8);
    bb10.predecessors.push(bb9);
    inst = new Inst(Move);
    arg = Arg.createBigImm(144506584, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb10.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rdi, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r9);
    inst.args.push(arg);
    bb10.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createAddr(Reg.r9, -8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    bb10.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(144506544, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb10.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rdi, 0);
    inst.args.push(arg);
    arg = Arg.createImm(80);
    inst.args.push(arg);
    arg = Arg.createBigImm(144506544, 1);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb10.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(144506552, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb10.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rdi, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb10.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createStack(slot2, 0);
    inst.args.push(arg);
    bb10.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb10.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createAddr(Reg.rdi, -8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb10.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createStack(slot3, 0);
    inst.args.push(arg);
    bb10.append(inst);
    inst = new Inst(MoveZeroToDouble);
    arg = Arg.createTmp(Reg.xmm7);
    inst.args.push(arg);
    bb10.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(10);
    inst.args.push(arg);
    arg = Arg.createStack(slot4, 0);
    inst.args.push(arg);
    bb10.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(2, -65536);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    bb10.append(inst);
    inst = new Inst(Jump);
    bb10.append(inst);
    bb11.successors.push(new FrequentedBlock(bb13, Normal));
    bb11.predecessors.push(bb35);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Jump);
    bb11.append(inst);
    bb12.successors.push(new FrequentedBlock(bb13, Normal));
    bb12.predecessors.push(bb34);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb12.append(inst);
    inst = new Inst(Jump);
    bb12.append(inst);
    bb13.successors.push(new FrequentedBlock(bb15, Normal));
    bb13.predecessors.push(bb11);
    bb13.predecessors.push(bb12);
    inst = new Inst(Move);
    arg = Arg.createImm(-6);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb13.append(inst);
    inst = new Inst(MoveDouble);
    arg = Arg.createTmp(Reg.xmm7);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    bb13.append(inst);
    inst = new Inst(MoveDouble);
    arg = Arg.createTmp(Reg.xmm7);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    bb13.append(inst);
    inst = new Inst(MoveDouble);
    arg = Arg.createTmp(Reg.xmm7);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    bb13.append(inst);
    inst = new Inst(MoveDouble);
    arg = Arg.createTmp(Reg.xmm7);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    bb13.append(inst);
    inst = new Inst(Jump);
    bb13.append(inst);
    bb14.successors.push(new FrequentedBlock(bb15, Normal));
    bb14.predecessors.push(bb31);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb14.append(inst);
    inst = new Inst(Jump);
    bb14.append(inst);
    bb15.successors.push(new FrequentedBlock(bb28, Normal));
    bb15.successors.push(new FrequentedBlock(bb16, Normal));
    bb15.predecessors.push(bb13);
    bb15.predecessors.push(bb14);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    bb15.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(LessThan);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createImm(0);
    inst.args.push(arg);
    bb15.append(inst);
    bb16.successors.push(new FrequentedBlock(bb29, Normal));
    bb16.successors.push(new FrequentedBlock(bb17, Normal));
    bb16.predecessors.push(bb15);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(GreaterThanOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createImm(267);
    inst.args.push(arg);
    bb16.append(inst);
    bb17.successors.push(new FrequentedBlock(bb18, Normal));
    bb17.predecessors.push(bb16);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb17.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(-6);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb17.append(inst);
    inst = new Inst(Jump);
    bb17.append(inst);
    bb18.successors.push(new FrequentedBlock(bb20, Normal));
    bb18.successors.push(new FrequentedBlock(bb19, Rare));
    bb18.predecessors.push(bb10);
    bb18.predecessors.push(bb17);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    bb18.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    bb18.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createImm(400);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    bb18.append(inst);
    inst = new Inst(BranchTest32);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    bb18.append(inst);
    bb19.successors.push(new FrequentedBlock(bb20, Normal));
    bb19.predecessors.push(bb18);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(LessThan);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createImm(0);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    bb19.append(inst);
    inst = new Inst(Jump);
    bb19.append(inst);
    bb20.successors.push(new FrequentedBlock(bb22, Normal));
    bb20.predecessors.push(bb18);
    bb20.predecessors.push(bb19);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb20.append(inst);
    inst = new Inst(Rshift32);
    arg = Arg.createImm(31);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb20.append(inst);
    inst = new Inst(Add32);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb20.append(inst);
    inst = new Inst(Xor32);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb20.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(LessThan);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createImm(0);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    bb20.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createStack(slot3, 0);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    bb20.append(inst);
    inst = new Inst(Move);
    arg = Arg.createStack(slot2, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb20.append(inst);
    inst = new Inst(Move);
    arg = Arg.createIndex(Reg.rsi, Reg.rdi, 8, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb20.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb20.append(inst);
    inst = new Inst(MoveConditionallyTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createImm(-1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb20.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    bb20.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rdi, 0);
    inst.args.push(arg);
    arg = Arg.createImm(79);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    bb20.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rdi, 8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    bb20.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createAddr(Reg.r12, -8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    bb20.append(inst);
    inst = new Inst(Jump);
    bb20.append(inst);
    bb21.successors.push(new FrequentedBlock(bb22, Normal));
    bb21.predecessors.push(bb27);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb21.append(inst);
    inst = new Inst(Jump);
    bb21.append(inst);
    bb22.successors.push(new FrequentedBlock(bb25, Normal));
    bb22.successors.push(new FrequentedBlock(bb23, Normal));
    bb22.predecessors.push(bb20);
    bb22.predecessors.push(bb21);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    bb22.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(LessThan);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createImm(0);
    inst.args.push(arg);
    bb22.append(inst);
    bb23.successors.push(new FrequentedBlock(bb26, Normal));
    bb23.successors.push(new FrequentedBlock(bb24, Normal));
    bb23.predecessors.push(bb22);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(GreaterThanOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createImm(400);
    inst.args.push(arg);
    bb23.append(inst);
    bb24.successors.push(new FrequentedBlock(bb27, Normal));
    bb24.predecessors.push(bb23);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    bb24.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createImm(4);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    bb24.append(inst);
    inst = new Inst(Add32);
    arg = Arg.createImm(3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb24.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb24.append(inst);
    inst = new Inst(MoveDouble);
    arg = Arg.createIndex(Reg.r9, Reg.r15, 8, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(Rshift32);
    arg = Arg.createImm(31);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(Add32);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(Xor32);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(LessThan);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createImm(0);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb24.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb24.append(inst);
    inst = new Inst(MoveDouble);
    arg = Arg.createIndex(Reg.r12, Reg.rbx, 8, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm4);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(MulDouble);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm4);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(AddDouble);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(Add32);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(MulDouble);
    arg = Arg.createIndex(Reg.r9, Reg.rsi, 8, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm4);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(AddDouble);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(Add32);
    arg = Arg.createImm(2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(MulDouble);
    arg = Arg.createIndex(Reg.r9, Reg.r15, 8, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm4);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(AddDouble);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(MulDouble);
    arg = Arg.createIndex(Reg.r9, Reg.r14, 8, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm4);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm4);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(AddDouble);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm4);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    bb24.append(inst);
    inst = new Inst(Jump);
    bb24.append(inst);
    bb25.successors.push(new FrequentedBlock(bb27, Normal));
    bb25.predecessors.push(bb22);
    inst = new Inst(Jump);
    bb25.append(inst);
    bb26.successors.push(new FrequentedBlock(bb27, Normal));
    bb26.predecessors.push(bb23);
    inst = new Inst(Jump);
    bb26.append(inst);
    bb27.successors.push(new FrequentedBlock(bb21, Normal));
    bb27.successors.push(new FrequentedBlock(bb30, Normal));
    bb27.predecessors.push(bb24);
    bb27.predecessors.push(bb26);
    bb27.predecessors.push(bb25);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb27.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.UseZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    bb27.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(LessThan);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createImm(7);
    inst.args.push(arg);
    bb27.append(inst);
    bb28.successors.push(new FrequentedBlock(bb31, Normal));
    bb28.predecessors.push(bb15);
    inst = new Inst(Jump);
    bb28.append(inst);
    bb29.successors.push(new FrequentedBlock(bb31, Normal));
    bb29.predecessors.push(bb16);
    inst = new Inst(Jump);
    bb29.append(inst);
    bb30.successors.push(new FrequentedBlock(bb31, Normal));
    bb30.predecessors.push(bb27);
    inst = new Inst(Move);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb30.append(inst);
    inst = new Inst(Jump);
    bb30.append(inst);
    bb31.successors.push(new FrequentedBlock(bb14, Normal));
    bb31.successors.push(new FrequentedBlock(bb32, Normal));
    bb31.predecessors.push(bb30);
    bb31.predecessors.push(bb29);
    bb31.predecessors.push(bb28);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb31.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.UseZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    bb31.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(LessThan);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createImm(7);
    inst.args.push(arg);
    bb31.append(inst);
    bb32.successors.push(new FrequentedBlock(bb34, Normal));
    bb32.successors.push(new FrequentedBlock(bb33, Rare));
    bb32.predecessors.push(bb31);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createImm(400);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    bb32.append(inst);
    inst = new Inst(BranchTest32);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb32.append(inst);
    bb33.successors.push(new FrequentedBlock(bb34, Normal));
    bb33.predecessors.push(bb32);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(LessThan);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createImm(0);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    bb33.append(inst);
    inst = new Inst(Jump);
    bb33.append(inst);
    bb34.successors.push(new FrequentedBlock(bb12, Normal));
    bb34.successors.push(new FrequentedBlock(bb35, Normal));
    bb34.predecessors.push(bb32);
    bb34.predecessors.push(bb33);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    bb34.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createImm(4);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    bb34.append(inst);
    inst = new Inst(DivDouble);
    arg = Arg.createTmp(Reg.xmm6);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    bb34.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createDoubleCond(DoubleNotEqualOrUnordered);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    bb34.append(inst);
    inst = new Inst(MoveDouble);
    arg = Arg.createTmp(Reg.xmm1);
    inst.args.push(arg);
    arg = Arg.createIndex(Reg.r9, Reg.rsi, 8, 0);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.UseZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    bb34.append(inst);
    inst = new Inst(DivDouble);
    arg = Arg.createTmp(Reg.xmm6);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Add32);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Add32);
    arg = Arg.createImm(3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    bb34.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    bb34.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createDoubleCond(DoubleNotEqualOrUnordered);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    bb34.append(inst);
    inst = new Inst(MoveDouble);
    arg = Arg.createTmp(Reg.xmm2);
    inst.args.push(arg);
    arg = Arg.createIndex(Reg.r9, Reg.rdi, 8, 0);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Add32);
    arg = Arg.createImm(2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(DivDouble);
    arg = Arg.createTmp(Reg.xmm6);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createDoubleCond(DoubleNotEqualOrUnordered);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    bb34.append(inst);
    inst = new Inst(MoveDouble);
    arg = Arg.createTmp(Reg.xmm3);
    inst.args.push(arg);
    arg = Arg.createIndex(Reg.r9, Reg.rsi, 8, 0);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(DivDouble);
    arg = Arg.createTmp(Reg.xmm6);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createDoubleCond(DoubleNotEqualOrUnordered);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: FP, width: 64});
    bb34.append(inst);
    inst = new Inst(MoveDouble);
    arg = Arg.createTmp(Reg.xmm5);
    inst.args.push(arg);
    arg = Arg.createIndex(Reg.r9, Reg.rax, 8, 0);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.UseZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    bb34.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(LessThan);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createImm(400);
    inst.args.push(arg);
    bb34.append(inst);
    bb35.successors.push(new FrequentedBlock(bb11, Normal));
    bb35.successors.push(new FrequentedBlock(bb36, Normal));
    bb35.predecessors.push(bb34);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb35.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.UseZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    bb35.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(LessThan);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createImm(267);
    inst.args.push(arg);
    bb35.append(inst);
    bb36.predecessors.push(bb35);
    inst = new Inst(Move);
    arg = Arg.createBigImm(144506576, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb36.append(inst);
    inst = new Inst(Ret64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb36.append(inst);
    return code;
}
"use strict";
// Generated by Air::dumpAsJS from #ACLj8C in Air.js
function createPayloadAirJSACLj8C()
{
    let code = new Code();
    let bb0 = code.addBlock();
    let bb1 = code.addBlock();
    let bb2 = code.addBlock();
    let bb3 = code.addBlock();
    let bb4 = code.addBlock();
    let bb5 = code.addBlock();
    let bb6 = code.addBlock();
    let bb7 = code.addBlock();
    let bb8 = code.addBlock();
    let bb9 = code.addBlock();
    let bb10 = code.addBlock();
    let bb11 = code.addBlock();
    let bb12 = code.addBlock();
    let bb13 = code.addBlock();
    let bb14 = code.addBlock();
    let bb15 = code.addBlock();
    let slot0 = code.addStackSlot(160, Locked);
    let slot1 = code.addStackSlot(8, Spill);
    let slot2 = code.addStackSlot(8, Spill);
    let slot3 = code.addStackSlot(8, Spill);
    let slot4 = code.addStackSlot(40, Locked);
    slot4.setOffsetFromFP(-40);
    let tmp61 = code.newTmp(GP);
    let tmp60 = code.newTmp(GP);
    let tmp59 = code.newTmp(GP);
    let tmp58 = code.newTmp(GP);
    let tmp57 = code.newTmp(GP);
    let tmp56 = code.newTmp(GP);
    let tmp55 = code.newTmp(GP);
    let tmp54 = code.newTmp(GP);
    let tmp53 = code.newTmp(GP);
    let tmp52 = code.newTmp(GP);
    let tmp51 = code.newTmp(GP);
    let tmp50 = code.newTmp(GP);
    let tmp49 = code.newTmp(GP);
    let tmp48 = code.newTmp(GP);
    let tmp47 = code.newTmp(GP);
    let tmp46 = code.newTmp(GP);
    let tmp45 = code.newTmp(GP);
    let tmp44 = code.newTmp(GP);
    let tmp43 = code.newTmp(GP);
    let tmp42 = code.newTmp(GP);
    let tmp41 = code.newTmp(GP);
    let tmp40 = code.newTmp(GP);
    let tmp39 = code.newTmp(GP);
    let tmp38 = code.newTmp(GP);
    let tmp37 = code.newTmp(GP);
    let tmp36 = code.newTmp(GP);
    let tmp35 = code.newTmp(GP);
    let tmp34 = code.newTmp(GP);
    let tmp33 = code.newTmp(GP);
    let tmp32 = code.newTmp(GP);
    let tmp31 = code.newTmp(GP);
    let tmp30 = code.newTmp(GP);
    let tmp29 = code.newTmp(GP);
    let tmp28 = code.newTmp(GP);
    let tmp27 = code.newTmp(GP);
    let tmp26 = code.newTmp(GP);
    let tmp25 = code.newTmp(GP);
    let tmp24 = code.newTmp(GP);
    let tmp23 = code.newTmp(GP);
    let tmp22 = code.newTmp(GP);
    let tmp21 = code.newTmp(GP);
    let tmp20 = code.newTmp(GP);
    let tmp19 = code.newTmp(GP);
    let tmp18 = code.newTmp(GP);
    let tmp17 = code.newTmp(GP);
    let tmp16 = code.newTmp(GP);
    let tmp15 = code.newTmp(GP);
    let tmp14 = code.newTmp(GP);
    let tmp13 = code.newTmp(GP);
    let tmp12 = code.newTmp(GP);
    let tmp11 = code.newTmp(GP);
    let tmp10 = code.newTmp(GP);
    let tmp9 = code.newTmp(GP);
    let tmp8 = code.newTmp(GP);
    let tmp7 = code.newTmp(GP);
    let tmp6 = code.newTmp(GP);
    let tmp5 = code.newTmp(GP);
    let tmp4 = code.newTmp(GP);
    let tmp3 = code.newTmp(GP);
    let tmp2 = code.newTmp(GP);
    let tmp1 = code.newTmp(GP);
    let tmp0 = code.newTmp(GP);
    let inst;
    let arg;
    bb0.successors.push(new FrequentedBlock(bb1, Normal));
    bb0.successors.push(new FrequentedBlock(bb15, Normal));
    inst = new Inst(Move);
    arg = Arg.createBigImm(276424800, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbp, 16);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbp);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Scratch, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbp, 72);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbp, 64);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbp, 56);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbp, 48);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(2, -65536);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbp, 24);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, 16);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(0, -65536);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rcx, 32);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rcx, 40);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(276327648, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.r8, 5);
    inst.args.push(arg);
    arg = Arg.createImm(21);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 8});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 8});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.r12, 0);
    inst.args.push(arg);
    arg = Arg.createImm(372);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.r12, 8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, -40);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(276321024, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot0, 72);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createStack(slot0, 64);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createStack(slot0, 56);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createStack(slot0, 48);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createStack(slot0, 40);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Xor64);
    arg = Arg.createImm(6);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(-2);
    inst.args.push(arg);
    arg = Arg.createStack(slot2, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(-2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r9);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r9);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createStack(slot3, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(BranchTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    bb1.successors.push(new FrequentedBlock(bb3, Normal));
    bb1.successors.push(new FrequentedBlock(bb2, Normal));
    bb1.predecessors.push(bb0);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.r8, 0);
    inst.args.push(arg);
    arg = Arg.createImm(468);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb1.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.r8, 16);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb1.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(276741160, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb1.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rcx, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb1.append(inst);
    inst = new Inst(Branch64);
    arg = Arg.createRelCond(Equal);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rax, 8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb1.append(inst);
    bb2.predecessors.push(bb1);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb2.append(inst);
    inst = new Inst(Oops);
    bb2.append(inst);
    bb3.successors.push(new FrequentedBlock(bb4, Normal));
    bb3.successors.push(new FrequentedBlock(bb7, Normal));
    bb3.predecessors.push(bb1);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.r8, 24);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    bb3.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 5);
    inst.args.push(arg);
    arg = Arg.createImm(23);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 8});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 8});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb3.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(275739616, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb3.append(inst);
    inst = new Inst(Branch64);
    arg = Arg.createRelCond(Equal);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 24);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb3.append(inst);
    bb4.successors.push(new FrequentedBlock(bb5, Normal));
    bb4.successors.push(new FrequentedBlock(bb6, Normal));
    bb4.predecessors.push(bb3);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot0, 0);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 16);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, 32);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb4.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createStack(slot0, 32);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createStack(slot0, 24);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createStack(slot0, 16);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createStack(slot0, 8);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(276645872, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(276646496, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createImm(2);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createCallArg(32);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    arg = Arg.createCallArg(32);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.rcx);
    inst.extraClobberedRegs.add(Reg.rdx);
    inst.extraClobberedRegs.add(Reg.rsi);
    inst.extraClobberedRegs.add(Reg.rdi);
    inst.extraClobberedRegs.add(Reg.r8);
    inst.extraClobberedRegs.add(Reg.r9);
    inst.extraClobberedRegs.add(Reg.r10);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.xmm0);
    inst.extraClobberedRegs.add(Reg.xmm1);
    inst.extraClobberedRegs.add(Reg.xmm2);
    inst.extraClobberedRegs.add(Reg.xmm3);
    inst.extraClobberedRegs.add(Reg.xmm4);
    inst.extraClobberedRegs.add(Reg.xmm5);
    inst.extraClobberedRegs.add(Reg.xmm6);
    inst.extraClobberedRegs.add(Reg.xmm7);
    inst.extraClobberedRegs.add(Reg.xmm8);
    inst.extraClobberedRegs.add(Reg.xmm9);
    inst.extraClobberedRegs.add(Reg.xmm10);
    inst.extraClobberedRegs.add(Reg.xmm11);
    inst.extraClobberedRegs.add(Reg.xmm12);
    inst.extraClobberedRegs.add(Reg.xmm13);
    inst.extraClobberedRegs.add(Reg.xmm14);
    inst.extraClobberedRegs.add(Reg.xmm15);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb4.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb4.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Xor64);
    arg = Arg.createImm(6);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(-2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb4.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb4.append(inst);
    inst = new Inst(BranchTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb4.append(inst);
    bb5.successors.push(new FrequentedBlock(bb8, Normal));
    bb5.predecessors.push(bb4);
    inst = new Inst(Move);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateUse, type: GP, width: 64});
    bb5.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb5.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb5.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rcx, 0);
    inst.args.push(arg);
    arg = Arg.createImm(419);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb5.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(276168608, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createImm(2);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createCallArg(32);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    arg = Arg.createCallArg(32);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.rcx);
    inst.extraClobberedRegs.add(Reg.rdx);
    inst.extraClobberedRegs.add(Reg.rsi);
    inst.extraClobberedRegs.add(Reg.rdi);
    inst.extraClobberedRegs.add(Reg.r8);
    inst.extraClobberedRegs.add(Reg.r9);
    inst.extraClobberedRegs.add(Reg.r10);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.xmm0);
    inst.extraClobberedRegs.add(Reg.xmm1);
    inst.extraClobberedRegs.add(Reg.xmm2);
    inst.extraClobberedRegs.add(Reg.xmm3);
    inst.extraClobberedRegs.add(Reg.xmm4);
    inst.extraClobberedRegs.add(Reg.xmm5);
    inst.extraClobberedRegs.add(Reg.xmm6);
    inst.extraClobberedRegs.add(Reg.xmm7);
    inst.extraClobberedRegs.add(Reg.xmm8);
    inst.extraClobberedRegs.add(Reg.xmm9);
    inst.extraClobberedRegs.add(Reg.xmm10);
    inst.extraClobberedRegs.add(Reg.xmm11);
    inst.extraClobberedRegs.add(Reg.xmm12);
    inst.extraClobberedRegs.add(Reg.xmm13);
    inst.extraClobberedRegs.add(Reg.xmm14);
    inst.extraClobberedRegs.add(Reg.xmm15);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb5.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb5.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Jump);
    bb5.append(inst);
    bb6.successors.push(new FrequentedBlock(bb8, Normal));
    bb6.predecessors.push(bb4);
    inst = new Inst(Move);
    arg = Arg.createImm(10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(Jump);
    bb6.append(inst);
    bb7.successors.push(new FrequentedBlock(bb12, Normal));
    bb7.successors.push(new FrequentedBlock(bb9, Normal));
    bb7.predecessors.push(bb3);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb7.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    bb7.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createImm(5);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    bb7.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(10);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    bb7.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createCallArg(32);
    inst.args.push(arg);
    bb7.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createCallArg(40);
    inst.args.push(arg);
    bb7.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createCallArg(48);
    inst.args.push(arg);
    bb7.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createCallArg(56);
    inst.args.push(arg);
    bb7.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    arg = Arg.createCallArg(32);
    inst.args.push(arg);
    arg = Arg.createCallArg(40);
    inst.args.push(arg);
    arg = Arg.createCallArg(48);
    inst.args.push(arg);
    arg = Arg.createCallArg(56);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.rcx);
    inst.extraClobberedRegs.add(Reg.rdx);
    inst.extraClobberedRegs.add(Reg.rsi);
    inst.extraClobberedRegs.add(Reg.rdi);
    inst.extraClobberedRegs.add(Reg.r8);
    inst.extraClobberedRegs.add(Reg.r9);
    inst.extraClobberedRegs.add(Reg.r10);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.xmm0);
    inst.extraClobberedRegs.add(Reg.xmm1);
    inst.extraClobberedRegs.add(Reg.xmm2);
    inst.extraClobberedRegs.add(Reg.xmm3);
    inst.extraClobberedRegs.add(Reg.xmm4);
    inst.extraClobberedRegs.add(Reg.xmm5);
    inst.extraClobberedRegs.add(Reg.xmm6);
    inst.extraClobberedRegs.add(Reg.xmm7);
    inst.extraClobberedRegs.add(Reg.xmm8);
    inst.extraClobberedRegs.add(Reg.xmm9);
    inst.extraClobberedRegs.add(Reg.xmm10);
    inst.extraClobberedRegs.add(Reg.xmm11);
    inst.extraClobberedRegs.add(Reg.xmm12);
    inst.extraClobberedRegs.add(Reg.xmm13);
    inst.extraClobberedRegs.add(Reg.xmm14);
    inst.extraClobberedRegs.add(Reg.xmm15);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb7.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb7.append(inst);
    inst = new Inst(BranchTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    bb7.append(inst);
    bb8.successors.push(new FrequentedBlock(bb13, Normal));
    bb8.successors.push(new FrequentedBlock(bb10, Normal));
    bb8.predecessors.push(bb6);
    bb8.predecessors.push(bb5);
    inst = new Inst(BranchTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    bb8.append(inst);
    bb9.successors.push(new FrequentedBlock(bb11, Normal));
    bb9.predecessors.push(bb7);
    inst = new Inst(Jump);
    bb9.append(inst);
    bb10.successors.push(new FrequentedBlock(bb11, Normal));
    bb10.predecessors.push(bb8);
    inst = new Inst(Jump);
    bb10.append(inst);
    bb11.predecessors.push(bb9);
    bb11.predecessors.push(bb10);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(Below);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rax, 5);
    inst.args.push(arg);
    arg = Arg.createImm(20);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 8});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 8});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb11.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb11.append(inst);
    inst = new Inst(Oops);
    bb11.append(inst);
    bb12.successors.push(new FrequentedBlock(bb14, Normal));
    bb12.predecessors.push(bb7);
    inst = new Inst(Jump);
    bb12.append(inst);
    bb13.successors.push(new FrequentedBlock(bb14, Normal));
    bb13.predecessors.push(bb8);
    inst = new Inst(Jump);
    bb13.append(inst);
    bb14.predecessors.push(bb12);
    bb14.predecessors.push(bb13);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb14.append(inst);
    inst = new Inst(And64);
    arg = Arg.createImm(-9);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb14.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createImm(2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb14.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb14.append(inst);
    inst = new Inst(Ret64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb14.append(inst);
    bb15.predecessors.push(bb0);
    inst = new Inst(Move);
    arg = Arg.createImm(10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb15.append(inst);
    inst = new Inst(Ret64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb15.append(inst);
    return code;
}
"use strict";
// Generated by Air::dumpAsJS from scanIdentifier#EPcFQe in Octane/typescript
function createPayloadTypescriptScanIdentifier()
{
    let code = new Code();
    let bb0 = code.addBlock();
    let bb1 = code.addBlock();
    let bb2 = code.addBlock();
    let bb3 = code.addBlock();
    let bb4 = code.addBlock();
    let bb5 = code.addBlock();
    let bb6 = code.addBlock();
    let bb7 = code.addBlock();
    let bb8 = code.addBlock();
    let bb9 = code.addBlock();
    let bb10 = code.addBlock();
    let bb11 = code.addBlock();
    let bb12 = code.addBlock();
    let bb13 = code.addBlock();
    let bb14 = code.addBlock();
    let bb15 = code.addBlock();
    let bb16 = code.addBlock();
    let bb17 = code.addBlock();
    let bb18 = code.addBlock();
    let bb19 = code.addBlock();
    let bb20 = code.addBlock();
    let bb21 = code.addBlock();
    let bb22 = code.addBlock();
    let bb23 = code.addBlock();
    let bb24 = code.addBlock();
    let bb25 = code.addBlock();
    let bb26 = code.addBlock();
    let bb27 = code.addBlock();
    let bb28 = code.addBlock();
    let bb29 = code.addBlock();
    let bb30 = code.addBlock();
    let bb31 = code.addBlock();
    let bb32 = code.addBlock();
    let bb33 = code.addBlock();
    let bb34 = code.addBlock();
    let slot0 = code.addStackSlot(56, Locked);
    let slot1 = code.addStackSlot(8, Spill);
    let slot2 = code.addStackSlot(8, Spill);
    let slot3 = code.addStackSlot(8, Spill);
    let slot4 = code.addStackSlot(8, Spill);
    let slot5 = code.addStackSlot(4, Spill);
    let slot6 = code.addStackSlot(8, Spill);
    let slot7 = code.addStackSlot(8, Spill);
    let slot8 = code.addStackSlot(8, Spill);
    let slot9 = code.addStackSlot(40, Locked);
    slot9.setOffsetFromFP(-40);
    let tmp98 = code.newTmp(GP);
    let tmp97 = code.newTmp(GP);
    let tmp96 = code.newTmp(GP);
    let tmp95 = code.newTmp(GP);
    let tmp94 = code.newTmp(GP);
    let tmp93 = code.newTmp(GP);
    let tmp92 = code.newTmp(GP);
    let tmp91 = code.newTmp(GP);
    let tmp90 = code.newTmp(GP);
    let tmp89 = code.newTmp(GP);
    let tmp88 = code.newTmp(GP);
    let tmp87 = code.newTmp(GP);
    let tmp86 = code.newTmp(GP);
    let tmp85 = code.newTmp(GP);
    let tmp84 = code.newTmp(GP);
    let tmp83 = code.newTmp(GP);
    let tmp82 = code.newTmp(GP);
    let tmp81 = code.newTmp(GP);
    let tmp80 = code.newTmp(GP);
    let tmp79 = code.newTmp(GP);
    let tmp78 = code.newTmp(GP);
    let tmp77 = code.newTmp(GP);
    let tmp76 = code.newTmp(GP);
    let tmp75 = code.newTmp(GP);
    let tmp74 = code.newTmp(GP);
    let tmp73 = code.newTmp(GP);
    let tmp72 = code.newTmp(GP);
    let tmp71 = code.newTmp(GP);
    let tmp70 = code.newTmp(GP);
    let tmp69 = code.newTmp(GP);
    let tmp68 = code.newTmp(GP);
    let tmp67 = code.newTmp(GP);
    let tmp66 = code.newTmp(GP);
    let tmp65 = code.newTmp(GP);
    let tmp64 = code.newTmp(GP);
    let tmp63 = code.newTmp(GP);
    let tmp62 = code.newTmp(GP);
    let tmp61 = code.newTmp(GP);
    let tmp60 = code.newTmp(GP);
    let tmp59 = code.newTmp(GP);
    let tmp58 = code.newTmp(GP);
    let tmp57 = code.newTmp(GP);
    let tmp56 = code.newTmp(GP);
    let tmp55 = code.newTmp(GP);
    let tmp54 = code.newTmp(GP);
    let tmp53 = code.newTmp(GP);
    let tmp52 = code.newTmp(GP);
    let tmp51 = code.newTmp(GP);
    let tmp50 = code.newTmp(GP);
    let tmp49 = code.newTmp(GP);
    let tmp48 = code.newTmp(GP);
    let tmp47 = code.newTmp(GP);
    let tmp46 = code.newTmp(GP);
    let tmp45 = code.newTmp(GP);
    let tmp44 = code.newTmp(GP);
    let tmp43 = code.newTmp(GP);
    let tmp42 = code.newTmp(GP);
    let tmp41 = code.newTmp(GP);
    let tmp40 = code.newTmp(GP);
    let tmp39 = code.newTmp(GP);
    let tmp38 = code.newTmp(GP);
    let tmp37 = code.newTmp(GP);
    let tmp36 = code.newTmp(GP);
    let tmp35 = code.newTmp(GP);
    let tmp34 = code.newTmp(GP);
    let tmp33 = code.newTmp(GP);
    let tmp32 = code.newTmp(GP);
    let tmp31 = code.newTmp(GP);
    let tmp30 = code.newTmp(GP);
    let tmp29 = code.newTmp(GP);
    let tmp28 = code.newTmp(GP);
    let tmp27 = code.newTmp(GP);
    let tmp26 = code.newTmp(GP);
    let tmp25 = code.newTmp(GP);
    let tmp24 = code.newTmp(GP);
    let tmp23 = code.newTmp(GP);
    let tmp22 = code.newTmp(GP);
    let tmp21 = code.newTmp(GP);
    let tmp20 = code.newTmp(GP);
    let tmp19 = code.newTmp(GP);
    let tmp18 = code.newTmp(GP);
    let tmp17 = code.newTmp(GP);
    let tmp16 = code.newTmp(GP);
    let tmp15 = code.newTmp(GP);
    let tmp14 = code.newTmp(GP);
    let tmp13 = code.newTmp(GP);
    let tmp12 = code.newTmp(GP);
    let tmp11 = code.newTmp(GP);
    let tmp10 = code.newTmp(GP);
    let tmp9 = code.newTmp(GP);
    let tmp8 = code.newTmp(GP);
    let tmp7 = code.newTmp(GP);
    let tmp6 = code.newTmp(GP);
    let tmp5 = code.newTmp(GP);
    let tmp4 = code.newTmp(GP);
    let tmp3 = code.newTmp(GP);
    let tmp2 = code.newTmp(GP);
    let tmp1 = code.newTmp(GP);
    let tmp0 = code.newTmp(GP);
    let inst;
    let arg;
    bb0.successors.push(new FrequentedBlock(bb5, Normal));
    bb0.successors.push(new FrequentedBlock(bb4, Normal));
    inst = new Inst(Move);
    arg = Arg.createBigImm(177329888, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbp, 16);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbp);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Scratch, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbp, 40);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(2, -65536);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 5);
    inst.args.push(arg);
    arg = Arg.createImm(21);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 8});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 8});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 0);
    inst.args.push(arg);
    arg = Arg.createImm(2540);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 72);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Compare32);
    arg = Arg.createRelCond(Equal);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createImm(92);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(154991936, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(NotEqual);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rcx, 0);
    inst.args.push(arg);
    arg = Arg.createImm(80);
    inst.args.push(arg);
    arg = Arg.createBigImm(154991936, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(154991944, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rcx, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createAddr(Reg.r12, -8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb0.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createIndex(Reg.r12, Reg.rax, 8, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(MoveConditionallyTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createImm(-1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Xor64);
    arg = Arg.createImm(6);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(-2);
    inst.args.push(arg);
    arg = Arg.createStack(slot2, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(-2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createStack(slot1, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(129987312, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot4, 0);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(108418352, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(0, -65536);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb0.append(inst);
    inst = new Inst(BranchTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb0.append(inst);
    bb1.predecessors.push(bb6);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb1.append(inst);
    inst = new Inst(Oops);
    bb1.append(inst);
    bb2.predecessors.push(bb23);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb2.append(inst);
    inst = new Inst(Oops);
    bb2.append(inst);
    bb3.predecessors.push(bb32);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb3.append(inst);
    inst = new Inst(Oops);
    bb3.append(inst);
    bb4.predecessors.push(bb0);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb4.append(inst);
    inst = new Inst(Oops);
    bb4.append(inst);
    bb5.successors.push(new FrequentedBlock(bb8, Normal));
    bb5.successors.push(new FrequentedBlock(bb6, Rare));
    bb5.predecessors.push(bb0);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 56);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, -24);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.r10, 16);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb5.append(inst);
    inst = new Inst(BranchTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb5.append(inst);
    bb6.successors.push(new FrequentedBlock(bb1, Rare));
    bb6.successors.push(new FrequentedBlock(bb7, Normal));
    bb6.predecessors.push(bb5);
    inst = new Inst(Move32);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbp, 36);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createStack(slot8, 0);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createStack(slot7, 0);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createStack(slot6, 0);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rbp);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraClobberedRegs.add(Reg.rcx);
    inst.extraClobberedRegs.add(Reg.rsi);
    inst.extraClobberedRegs.add(Reg.rdi);
    inst.extraClobberedRegs.add(Reg.r8);
    inst.extraClobberedRegs.add(Reg.r9);
    inst.extraClobberedRegs.add(Reg.r10);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.xmm1);
    inst.extraClobberedRegs.add(Reg.xmm2);
    inst.extraClobberedRegs.add(Reg.xmm3);
    inst.extraClobberedRegs.add(Reg.xmm4);
    inst.extraClobberedRegs.add(Reg.xmm5);
    inst.extraClobberedRegs.add(Reg.xmm6);
    inst.extraClobberedRegs.add(Reg.xmm7);
    inst.extraClobberedRegs.add(Reg.xmm8);
    inst.extraClobberedRegs.add(Reg.xmm9);
    inst.extraClobberedRegs.add(Reg.xmm10);
    inst.extraClobberedRegs.add(Reg.xmm11);
    inst.extraClobberedRegs.add(Reg.xmm12);
    inst.extraClobberedRegs.add(Reg.xmm13);
    inst.extraClobberedRegs.add(Reg.xmm14);
    inst.extraClobberedRegs.add(Reg.xmm15);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb6.append(inst);
    inst = new Inst(Move);
    arg = Arg.createStack(slot8, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(Move);
    arg = Arg.createStack(slot7, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(Move);
    arg = Arg.createStack(slot6, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(129987312, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb6.append(inst);
    inst = new Inst(BranchTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rcx, 0);
    inst.args.push(arg);
    arg = Arg.createImm(-1);
    inst.args.push(arg);
    bb6.append(inst);
    bb7.successors.push(new FrequentedBlock(bb11, Normal));
    bb7.predecessors.push(bb6);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, 8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb7.append(inst);
    inst = new Inst(Jump);
    bb7.append(inst);
    bb8.successors.push(new FrequentedBlock(bb11, Normal));
    bb8.predecessors.push(bb5);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, 8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb8.append(inst);
    inst = new Inst(Jump);
    bb8.append(inst);
    bb9.successors.push(new FrequentedBlock(bb11, Normal));
    bb9.predecessors.push(bb15);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r9);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb9.append(inst);
    inst = new Inst(Jump);
    bb9.append(inst);
    bb10.successors.push(new FrequentedBlock(bb11, Normal));
    bb10.predecessors.push(bb18);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r9);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb10.append(inst);
    inst = new Inst(Jump);
    bb10.append(inst);
    bb11.successors.push(new FrequentedBlock(bb12, Normal));
    bb11.successors.push(new FrequentedBlock(bb16, Normal));
    bb11.predecessors.push(bb7);
    bb11.predecessors.push(bb10);
    bb11.predecessors.push(bb9);
    bb11.predecessors.push(bb8);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r9);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 40);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.UseZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    bb11.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 40);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 32);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(Overflow);
    inst.args.push(arg);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.UseZDef, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.LateColdUse, type: GP, width: 32});
    bb11.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 32);
    inst.args.push(arg);
    bb11.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(LessThan);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb11.append(inst);
    bb12.successors.push(new FrequentedBlock(bb13, Normal));
    bb12.successors.push(new FrequentedBlock(bb14, Normal));
    bb12.predecessors.push(bb11);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.r10, 12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb12.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.r10, 16);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb12.append(inst);
    inst = new Inst(BranchTest32);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rax, 16);
    inst.args.push(arg);
    arg = Arg.createImm(8);
    inst.args.push(arg);
    bb12.append(inst);
    bb13.successors.push(new FrequentedBlock(bb15, Normal));
    bb13.predecessors.push(bb12);
    inst = new Inst(Load8);
    arg = Arg.createIndex(Reg.r9, Reg.rdx, 1, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb13.append(inst);
    inst = new Inst(Jump);
    bb13.append(inst);
    bb14.successors.push(new FrequentedBlock(bb15, Normal));
    bb14.predecessors.push(bb12);
    inst = new Inst(Load16);
    arg = Arg.createIndex(Reg.r9, Reg.rdx, 2, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb14.append(inst);
    inst = new Inst(Jump);
    bb14.append(inst);
    bb15.successors.push(new FrequentedBlock(bb9, Normal));
    bb15.successors.push(new FrequentedBlock(bb17, Normal));
    bb15.predecessors.push(bb14);
    bb15.predecessors.push(bb13);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb15.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb15.append(inst);
    inst = new Inst(Add64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb15.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbx, 72);
    inst.args.push(arg);
    bb15.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createRelCond(AboveOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r8);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb15.append(inst);
    inst = new Inst(Move);
    arg = Arg.createIndex(Reg.r12, Reg.rax, 8, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb15.append(inst);
    inst = new Inst(MoveConditionallyTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createImm(-1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb15.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb15.append(inst);
    inst = new Inst(Xor64);
    arg = Arg.createImm(6);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb15.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(-2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb15.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb15.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb15.append(inst);
    inst = new Inst(BranchTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb15.append(inst);
    bb16.predecessors.push(bb11);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb16.append(inst);
    inst = new Inst(Oops);
    bb16.append(inst);
    bb17.successors.push(new FrequentedBlock(bb18, Normal));
    bb17.successors.push(new FrequentedBlock(bb19, Normal));
    bb17.predecessors.push(bb15);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(GreaterThanOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createImm(48);
    inst.args.push(arg);
    bb17.append(inst);
    bb18.successors.push(new FrequentedBlock(bb10, Normal));
    bb18.successors.push(new FrequentedBlock(bb19, Normal));
    bb18.predecessors.push(bb17);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(LessThanOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createImm(57);
    inst.args.push(arg);
    bb18.append(inst);
    bb19.successors.push(new FrequentedBlock(bb20, Normal));
    bb19.successors.push(new FrequentedBlock(bb21, Normal));
    bb19.predecessors.push(bb17);
    bb19.predecessors.push(bb18);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(GreaterThanOrEqual);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createImm(128);
    inst.args.push(arg);
    bb19.append(inst);
    bb20.predecessors.push(bb19);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb20.append(inst);
    inst = new Inst(Oops);
    bb20.append(inst);
    bb21.successors.push(new FrequentedBlock(bb22, Normal));
    bb21.successors.push(new FrequentedBlock(bb23, Normal));
    bb21.predecessors.push(bb19);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    bb21.append(inst);
    inst = new Inst(Branch32);
    arg = Arg.createRelCond(Equal);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createImm(92);
    inst.args.push(arg);
    bb21.append(inst);
    bb22.predecessors.push(bb21);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createStack(slot5, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb22.append(inst);
    inst = new Inst(Oops);
    bb22.append(inst);
    bb23.successors.push(new FrequentedBlock(bb2, Rare));
    bb23.successors.push(new FrequentedBlock(bb24, Normal));
    bb23.predecessors.push(bb21);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rbx, 48);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(155021568, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createImm(3);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r10);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createCallArg(32);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r11);
    inst.args.push(arg);
    arg = Arg.createCallArg(40);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    arg = Arg.createCallArg(32);
    inst.args.push(arg);
    arg = Arg.createCallArg(40);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.rcx);
    inst.extraClobberedRegs.add(Reg.rdx);
    inst.extraClobberedRegs.add(Reg.rsi);
    inst.extraClobberedRegs.add(Reg.rdi);
    inst.extraClobberedRegs.add(Reg.r8);
    inst.extraClobberedRegs.add(Reg.r9);
    inst.extraClobberedRegs.add(Reg.r10);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.xmm0);
    inst.extraClobberedRegs.add(Reg.xmm1);
    inst.extraClobberedRegs.add(Reg.xmm2);
    inst.extraClobberedRegs.add(Reg.xmm3);
    inst.extraClobberedRegs.add(Reg.xmm4);
    inst.extraClobberedRegs.add(Reg.xmm5);
    inst.extraClobberedRegs.add(Reg.xmm6);
    inst.extraClobberedRegs.add(Reg.xmm7);
    inst.extraClobberedRegs.add(Reg.xmm8);
    inst.extraClobberedRegs.add(Reg.xmm9);
    inst.extraClobberedRegs.add(Reg.xmm10);
    inst.extraClobberedRegs.add(Reg.xmm11);
    inst.extraClobberedRegs.add(Reg.xmm12);
    inst.extraClobberedRegs.add(Reg.xmm13);
    inst.extraClobberedRegs.add(Reg.xmm14);
    inst.extraClobberedRegs.add(Reg.xmm15);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(155041288, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, 0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.rax, -1336);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createAddr(Reg.r13, 24);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createStack(slot0, 0);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createImm(2);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbp, 36);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(108356304, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createStack(slot3, 0);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rbp);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraClobberedRegs.add(Reg.rcx);
    inst.extraClobberedRegs.add(Reg.rsi);
    inst.extraClobberedRegs.add(Reg.rdi);
    inst.extraClobberedRegs.add(Reg.r8);
    inst.extraClobberedRegs.add(Reg.r9);
    inst.extraClobberedRegs.add(Reg.r10);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.xmm1);
    inst.extraClobberedRegs.add(Reg.xmm2);
    inst.extraClobberedRegs.add(Reg.xmm3);
    inst.extraClobberedRegs.add(Reg.xmm4);
    inst.extraClobberedRegs.add(Reg.xmm5);
    inst.extraClobberedRegs.add(Reg.xmm6);
    inst.extraClobberedRegs.add(Reg.xmm7);
    inst.extraClobberedRegs.add(Reg.xmm8);
    inst.extraClobberedRegs.add(Reg.xmm9);
    inst.extraClobberedRegs.add(Reg.xmm10);
    inst.extraClobberedRegs.add(Reg.xmm11);
    inst.extraClobberedRegs.add(Reg.xmm12);
    inst.extraClobberedRegs.add(Reg.xmm13);
    inst.extraClobberedRegs.add(Reg.xmm14);
    inst.extraClobberedRegs.add(Reg.xmm15);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb23.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(129987312, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb23.append(inst);
    inst = new Inst(BranchTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rcx, 0);
    inst.args.push(arg);
    arg = Arg.createImm(-1);
    inst.args.push(arg);
    bb23.append(inst);
    bb24.successors.push(new FrequentedBlock(bb25, Normal));
    bb24.successors.push(new FrequentedBlock(bb26, Normal));
    bb24.predecessors.push(bb23);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r13);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb24.append(inst);
    inst = new Inst(BranchTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    bb24.append(inst);
    bb25.successors.push(new FrequentedBlock(bb27, Normal));
    bb25.successors.push(new FrequentedBlock(bb26, Normal));
    bb25.predecessors.push(bb24);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb25.append(inst);
    inst = new Inst(And64);
    arg = Arg.createImm(-9);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb25.append(inst);
    inst = new Inst(Branch64);
    arg = Arg.createRelCond(Equal);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createImm(2);
    inst.args.push(arg);
    bb25.append(inst);
    bb26.successors.push(new FrequentedBlock(bb29, Normal));
    bb26.successors.push(new FrequentedBlock(bb28, Normal));
    bb26.predecessors.push(bb24);
    bb26.predecessors.push(bb25);
    inst = new Inst(BranchTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    bb26.append(inst);
    bb27.successors.push(new FrequentedBlock(bb30, Normal));
    bb27.predecessors.push(bb25);
    inst = new Inst(Move);
    arg = Arg.createImm(2);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb27.append(inst);
    inst = new Inst(Jump);
    bb27.append(inst);
    bb28.successors.push(new FrequentedBlock(bb32, Normal));
    bb28.predecessors.push(bb26);
    inst = new Inst(Jump);
    bb28.append(inst);
    bb29.successors.push(new FrequentedBlock(bb30, Normal));
    bb29.predecessors.push(bb26);
    inst = new Inst(Jump);
    bb29.append(inst);
    bb30.successors.push(new FrequentedBlock(bb34, Normal));
    bb30.successors.push(new FrequentedBlock(bb31, Normal));
    bb30.predecessors.push(bb29);
    bb30.predecessors.push(bb27);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb30.append(inst);
    inst = new Inst(And64);
    arg = Arg.createImm(-9);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb30.append(inst);
    inst = new Inst(Branch64);
    arg = Arg.createRelCond(Equal);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createImm(2);
    inst.args.push(arg);
    bb30.append(inst);
    bb31.successors.push(new FrequentedBlock(bb32, Normal));
    bb31.predecessors.push(bb30);
    inst = new Inst(Jump);
    bb31.append(inst);
    bb32.successors.push(new FrequentedBlock(bb3, Rare));
    bb32.successors.push(new FrequentedBlock(bb33, Normal));
    bb32.predecessors.push(bb28);
    bb32.predecessors.push(bb31);
    inst = new Inst(Move32);
    arg = Arg.createImm(3);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rbp, 36);
    inst.args.push(arg);
    bb32.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(154991632, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb32.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    bb32.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    bb32.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rbp);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    bb32.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createBigImm(108356304, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.xmm0);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rsi);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rdx);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraClobberedRegs.add(Reg.rcx);
    inst.extraClobberedRegs.add(Reg.rsi);
    inst.extraClobberedRegs.add(Reg.rdi);
    inst.extraClobberedRegs.add(Reg.r8);
    inst.extraClobberedRegs.add(Reg.r9);
    inst.extraClobberedRegs.add(Reg.r10);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.xmm1);
    inst.extraClobberedRegs.add(Reg.xmm2);
    inst.extraClobberedRegs.add(Reg.xmm3);
    inst.extraClobberedRegs.add(Reg.xmm4);
    inst.extraClobberedRegs.add(Reg.xmm5);
    inst.extraClobberedRegs.add(Reg.xmm6);
    inst.extraClobberedRegs.add(Reg.xmm7);
    inst.extraClobberedRegs.add(Reg.xmm8);
    inst.extraClobberedRegs.add(Reg.xmm9);
    inst.extraClobberedRegs.add(Reg.xmm10);
    inst.extraClobberedRegs.add(Reg.xmm11);
    inst.extraClobberedRegs.add(Reg.xmm12);
    inst.extraClobberedRegs.add(Reg.xmm13);
    inst.extraClobberedRegs.add(Reg.xmm14);
    inst.extraClobberedRegs.add(Reg.xmm15);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: FP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb32.append(inst);
    inst = new Inst(Move);
    arg = Arg.createBigImm(129987312, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rcx);
    inst.args.push(arg);
    bb32.append(inst);
    inst = new Inst(BranchTest64);
    arg = Arg.createResCond(NonZero);
    inst.args.push(arg);
    arg = Arg.createAddr(Reg.rcx, 0);
    inst.args.push(arg);
    arg = Arg.createImm(-1);
    inst.args.push(arg);
    bb32.append(inst);
    bb33.predecessors.push(bb32);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb33.append(inst);
    inst = new Inst(Ret64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb33.append(inst);
    bb34.predecessors.push(bb30);
    inst = new Inst(Move);
    arg = Arg.createBigImm(153835296, 1);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Move32);
    arg = Arg.createImm(3);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Move);
    arg = Arg.createTmp(Reg.r12);
    inst.args.push(arg);
    arg = Arg.createCallArg(32);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Move);
    arg = Arg.createImm(6);
    inst.args.push(arg);
    arg = Arg.createCallArg(40);
    inst.args.push(arg);
    bb34.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    arg = Arg.createCallArg(8);
    inst.args.push(arg);
    arg = Arg.createCallArg(16);
    inst.args.push(arg);
    arg = Arg.createCallArg(24);
    inst.args.push(arg);
    arg = Arg.createCallArg(32);
    inst.args.push(arg);
    arg = Arg.createCallArg(40);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r15);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.r14);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.extraEarlyClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.rcx);
    inst.extraClobberedRegs.add(Reg.rdx);
    inst.extraClobberedRegs.add(Reg.rsi);
    inst.extraClobberedRegs.add(Reg.rdi);
    inst.extraClobberedRegs.add(Reg.r8);
    inst.extraClobberedRegs.add(Reg.r9);
    inst.extraClobberedRegs.add(Reg.r10);
    inst.extraClobberedRegs.add(Reg.r11);
    inst.extraClobberedRegs.add(Reg.xmm0);
    inst.extraClobberedRegs.add(Reg.xmm1);
    inst.extraClobberedRegs.add(Reg.xmm2);
    inst.extraClobberedRegs.add(Reg.xmm3);
    inst.extraClobberedRegs.add(Reg.xmm4);
    inst.extraClobberedRegs.add(Reg.xmm5);
    inst.extraClobberedRegs.add(Reg.xmm6);
    inst.extraClobberedRegs.add(Reg.xmm7);
    inst.extraClobberedRegs.add(Reg.xmm8);
    inst.extraClobberedRegs.add(Reg.xmm9);
    inst.extraClobberedRegs.add(Reg.xmm10);
    inst.extraClobberedRegs.add(Reg.xmm11);
    inst.extraClobberedRegs.add(Reg.xmm12);
    inst.extraClobberedRegs.add(Reg.xmm13);
    inst.extraClobberedRegs.add(Reg.xmm14);
    inst.extraClobberedRegs.add(Reg.xmm15);
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Def, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 32});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    bb34.append(inst);
    inst = new Inst(Patch);
    arg = Arg.createSpecial();
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rbx);
    inst.args.push(arg);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    inst.patchHasNonArgEffects = true;
    inst.extraEarlyClobberedRegs = new Set();
    inst.extraClobberedRegs = new Set();
    inst.patchArgData = [];
    inst.patchArgData.push({role: Arg.Use, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    inst.patchArgData.push({role: Arg.ColdUse, type: GP, width: 64});
    bb34.append(inst);
    inst = new Inst(Ret64);
    arg = Arg.createTmp(Reg.rax);
    inst.args.push(arg);
    bb34.append(inst);
    return code;
}

class AirBenchmark {
    constructor(verbose = 0)
    {
        this._verbose = verbose;
        
        this._payloads = [
            {generate: createPayloadGbemuExecuteIteration, earlyHash: 632653144, lateHash: 372715518},
            {generate: createPayloadImagingGaussianBlurGaussianBlur, earlyHash: 3677819581, lateHash: 1252116304},
            {generate: createPayloadTypescriptScanIdentifier, earlyHash: 1914852601, lateHash: 837339551},
            {generate: createPayloadAirJSACLj8C, earlyHash: 1373599940, lateHash: 3981283600}
        ];
    }
    
    runIteration()
    {
        for (let payload of this._payloads) {
            // Sadly about 17% of our time is in generate. I don't think that's really avoidable,
            // and I don't mind testing VMs' ability to run such "data definition" code quickly. I
            // would not have expected it to be so slow from first principles!
            let code = payload.generate();
            
            if (this._verbose) {
                print("Before allocateStack:");
                print(code);
            }
            
            let hash = code.hash();
            if (hash != payload.earlyHash)
                throw new Error(`Wrong early hash for ${payload.generate.name}: ${hash}`);
            
            allocateStack(code);
            
            if (this._verbose) {
                print("After allocateStack:");
                print(code);
            }

            hash = code.hash();
            if (hash != payload.lateHash)
                throw new Error(`Wrong late hash for ${payload.generate.name}: ${hash}`);
        }
    }
}

function runBenchmark()
{
    const verbose = 0;
    const numIterations = 5;
    
    let before = currentTime();
    let benchmark = new AirBenchmark(verbose);
    
    for (let iteration = 0; iteration < numIterations; ++iteration)
        benchmark.runIteration();
    
    let after = currentTime();
    return after - before;
}
runBenchmark();
