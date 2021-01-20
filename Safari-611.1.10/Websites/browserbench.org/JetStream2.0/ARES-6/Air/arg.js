/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

