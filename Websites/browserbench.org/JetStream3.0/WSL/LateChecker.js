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

class LateChecker extends Visitor {
    visitReferenceType(node)
    {
        if (node.addressSpace == "thread")
            return;
        
        if (!node.elementType.isPrimitive)
            throw new WTypeError(node.origin.originString, "Illegal pointer to non-primitive type: " + node);
    }
    
    _checkShaderType(node)
    {
        // FIXME: Tighten these checks. For now, we should only accept int32, uint32, float32, and float64.
        let assertPrimitive = type => {
            if (!type.isPrimitive)
                throw new WTypeError(node.origin.originString, "Shader signature cannot include non-primitive type: " + type);
        }
        assertPrimitive(node.returnType);
        if (!(node.returnType.unifyNode instanceof StructType))
            throw new WTypeError(node.origin.originString, "Vertex shader " + node.name + " must return a struct.");
        switch (node.shaderType) {
        case "vertex":
            for (let parameter of node.parameters) {
                if (parameter.type.unifyNode instanceof StructType)
                    assertPrimitive(parameter.type);
                else if (!parameter.type.unifyNode.isArrayRef)
                    throw new WTypeError(node.origin.originString, node.name + " accepts a parameter " + parameter.name + " which isn't a struct and isn't an ArrayRef.");
            }
            break;
        case "fragment":
            for (let parameter of node.parameters) {
                if (parameter.name == "stageIn") {
                    if (!(parameter.type.unifyNode instanceof StructType))
                        throw new WTypeError(node.origin.originString, "Fragment entry points' stageIn parameter (of " + node.name + ") must be a struct type.");
                    assertPrimitive(parameter.type);
                } else {
                    if (!parameter.type.unifyNode.isArrayRef)
                        throw new WTypeError(node.origin.originString, "Fragment entry point's " + parameter.name + " parameter is not an array reference.");
                }
            }
            break;
        default:
            throw new Error("Bad shader type: " + node.shaderType);
        }
    }
    
    visitFuncDef(node)
    {
        if (node.shaderType)
            this._checkShaderType(node);
        super.visitFuncDef(node);
    }
}

