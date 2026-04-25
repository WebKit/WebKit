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

