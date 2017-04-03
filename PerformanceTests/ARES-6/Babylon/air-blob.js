/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
