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

class StatementCloner extends Rewriter {
    visitFuncDef(node)
    {
        let typeParameters = node.typeParameters.map(typeParameter => typeParameter.visit(this));
        let result = new FuncDef(
            node.origin, node.name,
            node.returnType.visit(this),
            typeParameters,
            node.parameters.map(parameter => parameter.visit(this)),
            node.body.visit(this),
            node.isCast, node.shaderType);
        result.isRestricted = node.isRestricted;
        return result;
    }
    
    visitNativeFunc(node)
    {
        let result = new NativeFunc(
            node.origin, node.name,
            node.returnType.visit(this),
            node.typeParameters.map(typeParameter => typeParameter.visit(this)),
            node.parameters.map(parameter => parameter.visit(this)),
            node.isCast, node.shaderType);
        result.isRestricted = node.isRestricted;
        return result;
    }
    
    visitNativeType(node)
    {
        return new NativeType(
            node.origin, node.name, node.typeParameters.map(typeParameter => typeParameter.visit(this)));
    }
    
    visitTypeDef(node)
    {
        return new TypeDef(
            node.origin, node.name,
            node.typeParameters.map(typeParameter => typeParameter.visit(this)),
            node.type.visit(this));
    }
    
    visitStructType(node)
    {
        let result = new StructType(
            node.origin, node.name,
            node.typeParameters.map(typeParameter => typeParameter.visit(this)));
        for (let field of node.fields)
            result.add(field.visit(this));
        return result;
    }
    
    visitConstexprTypeParameter(node)
    {
        return new ConstexprTypeParameter(node.origin, node.name, node.type.visit(this));
    }
    
    visitProtocolDecl(node)
    {
        let result = new ProtocolDecl(node.origin, node.name);
        for (let protocol of node.extends)
            result.addExtends(protocol.visit(this));
        for (let signature of node.signatures)
            result.add(signature.visit(this));
        return result;
    }

    visitTypeVariable(node)
    {
        return new TypeVariable(node.origin, node.name, Node.visit(node.protocol, this));
    }
    
    visitProtocolRef(node)
    {
        return new ProtocolRef(node.origin, node.name);
    }
    
    visitBoolLiteral(node)
    {
        return new BoolLiteral(node.origin, node.value);
    }
    
    visitTypeOrVariableRef(node)
    {
        return new TypeOrVariableRef(node.origin, node.name);
    }
    
    visitEnumType(node)
    {
        let result = new EnumType(node.origin, node.name, node.baseType.visit(this));
        for (let member of node.members)
            result.add(member);
        return result;
    }
}

