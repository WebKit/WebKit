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

class TypeDefResolver extends Visitor {
    constructor()
    {
        super();
        this._visiting = new VisitingSet();
    }
    
    visitTypeRef(node)
    {
        this._visiting.doVisit(node, () => {
            for (let typeArgument of node.typeArguments)
                typeArgument.visit(this);
            if (node.type instanceof TypeDef) {
                let unificationContext = new UnificationContext(node.type.typeParameters);
                if (node.typeArguments.length != node.type.typeParameters.length)
                    throw new Error("argument/parameter mismatch (should have been caught earlier)");
                for (let i = 0; i < node.typeArguments.length; ++i)
                    node.typeArguments[i].unify(unificationContext, node.type.typeParameters[i]);
                let verificationResult = unificationContext.verify();
                if (!verificationResult.result)
                    throw new WTypeError(node.origin.originString, "Type reference to a type definition violates protocol constraints: " + verificationResult.reason);
                
                let newType = node.type.type.substituteToUnification(node.type.typeParameters, unificationContext);
                newType.visit(this);
                node.setTypeAndArguments(newType, []);
            }
        });
    }
}
