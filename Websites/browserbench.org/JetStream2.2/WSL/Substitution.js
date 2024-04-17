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

class Substitution extends Rewriter {
    constructor(parameters, argumentList)
    {
        super();
        if (parameters.length != argumentList.length)
            throw new Error("Parameters and arguments are mismatched");
        this._map = new Map();
        for (let i = 0; i < parameters.length; ++i)
            this._map.set(parameters[i], argumentList[i]);
    }
    
    get map() { return this._map; }
    
    visitTypeRef(node)
    {
        let replacement = this._map.get(node.type);
        if (replacement) {
            if (node.typeArguments.length)
                throw new Error("Unexpected type arguments on type variable");
            let result = replacement.visit(new AutoWrapper());
            return result;
        }
        
        let result = super.visitTypeRef(node);
        return result;
    }

    visitVariableRef(node)
    {
        let replacement = this._map.get(node.variable);
        if (replacement)
            return replacement.visit(new AutoWrapper());
        
        return super.visitVariableRef(node);
    }
}

