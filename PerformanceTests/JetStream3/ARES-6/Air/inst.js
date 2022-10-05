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

