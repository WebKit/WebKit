/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

class Inliner extends Rewriter {
    constructor(program, func, visiting)
    {
        super();
        this._program = program;
        this._visiting = visiting;
    }
    
    visitCallExpression(node)
    {
        let result = super.visitCallExpression(node);
        if (result.nativeFuncInstance)
            return result;
        return this._visiting.doVisit(node.func, () => {
            let func = this._program.funcInstantiator.getUnique(result.func, result.actualTypeArguments);
            if (func.isNative)
                throw new Error("Unexpected native func: " + func);
            _inlineFunction(this._program, func, this._visiting);
            let resultingBlock = new FunctionLikeBlock(
                result.origin, func.returnType, result.argumentList, func.parameters, func.body);
            resultingBlock.returnEPtr = result.resultEPtr;
            return resultingBlock;
        });
    }
}

