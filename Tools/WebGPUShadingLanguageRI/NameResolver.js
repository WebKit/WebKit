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

// This only resolves names. After this phase runs, all names will be resolved. This means that all
// XYZRef objects now know who they resolve to. This does not yet allow type analysis, because there
// are still TypeDefs. This throws type errors for failed name resolutions and for things that are
// more convenient to check here than in the Checker. This usually involves things that need to be
// checked before TypeRefToTypeDefSkipper.
class NameResolver extends Visitor {
    // It's totally OK to instantiate this with zero arguments (i.e. passing undefined for both of
    // them) if you're creating a NameResolver to visit a Program.
    constructor(nameContext)
    {
        super();
        this._nameContext = nameContext;
    }
    
    visitProgram(node)
    {
        let nameContext = new NameContext(this._nameContext);
        nameContext.program = node;
        nameContext.recognizeIntrinsics();
        nameContext.handleDefining();
        node.intrinsics = nameContext.intrinsics;
        for (let statement of node.topLevelStatements)
            nameContext.add(statement);
        let checker = new NameResolver(nameContext);
        for (let statement of node.topLevelStatements)
            nameContext.doStatement(statement, () => statement.visit(checker));
        node.globalNameContext = nameContext;
    }
    
    _visitTypeParametersAndBuildNameContext(node)
    {
        let nameContext = new NameContext(this._nameContext);
        for (let typeParameter of node.typeParameters) {
            nameContext.add(typeParameter);
            typeParameter.visit(this);
        }
        return nameContext;
    }
    
    visitFunc(node)
    {
        let checker = new NameResolver(this._visitTypeParametersAndBuildNameContext(node));
        node.returnType.visit(checker);
        for (let parameter of node.parameters)
            parameter.visit(checker);
    }
    
    visitFuncDef(node)
    {
        let contextWithTypeParameters = this._visitTypeParametersAndBuildNameContext(node);
        let checkerWithTypeParameters = new NameResolver(contextWithTypeParameters);
        node.returnType.visit(checkerWithTypeParameters);
        let contextWithParameters = new NameContext(contextWithTypeParameters);
        for (let parameter of node.parameters) {
            parameter.visit(checkerWithTypeParameters);
            contextWithParameters.add(parameter);
        }
        let checkerWithParameters = new NameResolver(contextWithParameters);
        node.body.visit(checkerWithParameters);
    }
    
    visitBlock(node)
    {
        let checker = new NameResolver(new NameContext(this._nameContext));
        for (let statement of node.statements)
            statement.visit(checker);
    }

    visitIfStatement(node)
    {
        node.conditional.visit(this);
        // The bodies might not be Blocks, so we need to explicitly give them a new context.
        node.body.visit(new NameResolver(new NameContext(this._nameContext)));
        if (node.elseBody)
            node.elseBody.visit(new NameResolver(new NameContext(this._nameContext)));
    }

    visitWhileLoop(node)
    {
        node.conditional.visit(this);
        // The bodies might not be Blocks, so we need to explicitly give them a new context.
        node.body.visit(new NameResolver(new NameContext(this._nameContext)));
    }

    visitDoWhileLoop(node)
    {
        // The bodies might not be Blocks, so we need to explicitly give them a new context.
        node.body.visit(new NameResolver(new NameContext(this._nameContext)));
        node.conditional.visit(this);
    }
    
    visitProtocolDecl(node)
    {
        let nameContext = new NameContext(this._nameContext);
        nameContext.add(node.typeVariable);
        let checker = new NameResolver(nameContext);
        for (let signature of node.signatures)
            signature.visit(checker);
    }
    
    visitProtocolRef(node)
    {
        let result = this._nameContext.get(Protocol, node.name);
        if (!result)
            throw new WTypeError(node.origin.originString, "Could not find protocol named " + node.name);
        node.protocolDecl = result;
    }
    
    visitProtocolFuncDecl(node)
    {
        this.visitFunc(node);
        let funcs = this._nameContext.get(Func, node.name);
        if (!funcs)
            throw new WTypeError(node.origin.originString, "Cannot find any functions named " + node.na,e);
        node.possibleOverloads = funcs;
    }
    
    visitTypeDef(node)
    {
        let nameContext = new NameContext(this._nameContext);
        for (let typeParameter of node.typeParameters) {
            typeParameter.visit(this);
            nameContext.add(typeParameter);
        }
        let checker = new NameResolver(nameContext);
        node.type.visit(checker);
    }
    
    visitStructType(node)
    {
        let nameContext = new NameContext(this._nameContext);
        for (let typeParameter of node.typeParameters) {
            typeParameter.visit(this);
            nameContext.add(typeParameter);
        }
        let checker = new NameResolver(nameContext);
        for (let field of node.fields)
            field.visit(checker);
    }
    
    _resolveTypeArguments(typeArguments)
    {
        for (let i = 0; i < typeArguments.length; ++i) {
            let typeArgument = typeArguments[i];
            if (typeArgument instanceof TypeOrVariableRef) {
                let thing = this._nameContext.get(NotFunc, typeArgument.name);
                if (!thing)
                    new WTypeError(typeArgument.origin.originString, "Could not find type or variable named " + typeArgument.name);
                if (thing instanceof Value) {
                    typeArguments[i] = new VariableRef(typeArgument.origin, typeArgument.name);
                } else
                    typeArguments[i] = new TypeRef(typeArgument.origin, typeArgument.name, []);
            }
            
            if (typeArgument[i] instanceof Value
                && !typeArgument[i].isConstexpr)
                throw new WTypeError(typeArgument[i].origin.originString, "Expected constexpr");
        }
    }
    
    visitTypeRef(node)
    {
        this._resolveTypeArguments(node.typeArguments);
        
        let type = this._nameContext.get(Type, node.name);
        if (!type)
            throw new WTypeError(node.origin.originString, "Could not find type named " + node.name);
        if (!this._nameContext.isDefined(type))
            throw new WTypeError(node.origin.originString, "Illegal forward use of type named " + node.name);
        node.type = type;
        
        if (type.typeParameters.length != node.typeArguments.length)
            throw new WTypeError(node.origin.originString, "Wrong number of type arguments (passed " + node.typeArguments.length + ", expected " + type.typeParameters.length + ")");
        for (let i = 0; i < type.typeParameters.length; ++i) {
            let parameterIsType = type.typeParameters[i] instanceof TypeVariable;
            let argumentIsType = node.typeArguments[i] instanceof Type;
            node.typeArguments[i].visit(this);
            if (parameterIsType && !argumentIsType)
                throw new WTypeError(node.origin.originString, "Expected type, but got value at argument #" + i);
            if (!parameterIsType && argumentIsType)
                throw new WTypeError(node.origin.originString, "Expected value, but got type at argument #" + i);
        }

        super.visitTypeRef(node);
    }
    
    visitReferenceType(node)
    {
        let nameContext = new NameContext(this._nameContext);
        nameContext.defineAll();
        node.elementType.visit(new NameResolver(nameContext));
    }
    
    visitVariableDecl(node)
    {
        this._nameContext.add(node);
        node.type.visit(this);
        if (node.initializer)
            node.initializer.visit(this);
    }
    
    visitVariableRef(node)
    {
        let result = this._nameContext.get(Value, node.name);
        if (!result)
            throw new WTypeError(node.origin.originString, "Could not find variable named " + node.name);
        node.variable = result;
    }
    
    visitReturn(node)
    {
        node.func = this._nameContext.currentStatement;
        super.visitReturn(node);
    }
    
    visitCallExpression(node)
    {
        this._resolveTypeArguments(node.typeArguments);
        
        let funcs = this._nameContext.get(Func, node.name);
        if (funcs)
            node.possibleOverloads = funcs;
        else {
            let type = this._nameContext.get(Type, node.name);
            if (!type)
                throw new WTypeError(node.origin.originString, "Cannot find any function or type named " + node.name);
            node.becomeCast(type);
            node.possibleOverloads = this._nameContext.get(Func, "operator cast");
            if (!node.possibleOverloads)
                throw new WTypeError(node.origin.originString, "Cannot find any operator cast implementations in cast to " + type);
        }
        
        super.visitCallExpression(node);
    }
}

