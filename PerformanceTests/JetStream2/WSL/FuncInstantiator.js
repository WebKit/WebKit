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

class FuncInstantiator {
    constructor(program)
    {
        this._program = program;
        this._instances = new Map();
    }
    
    // Returns a Func object that uniquely identifies a particular system of type arguments. You must
    // intantiate things with concrete types, because this code casually assumes this. Note that this
    // will return a different func from `func` no matter what. This ensures that we can use the
    // returned func for rewrites that instantiate types without destroying our ability to do overload
    // resolutions on the original Program.
    getUnique(func, typeArguments)
    {
        class FindTypeVariable extends Visitor {
            visitTypeRef(node)
            {
                for (let typeArgument of node.typeArguments)
                    typeArgument.visit(this);
            }
            
            visitTypeVariable(node) {
                throw new Error("Unexpected type variable: " + node + " when instantiating " + func + " with arguments " + typeArguments);
            }
        }
        for (let typeArgument of typeArguments)
            typeArgument.visit(new FindTypeVariable());
        
        let instances = this._instances.get(func);
        if (!instances)
            this._instances.set(func, instances = []);
        
        for (let instance of instances) {
            let ok = true;
            for (let i = instance.typeArguments.length; i--;) {
                if (!instance.typeArguments[i].equals(typeArguments[i])) {
                    ok = false;
                    break;
                }
            }
            if (!ok)
                continue;
            return instance.func;
        }
        
        let thisInstantiator = this;
        
        class InstantiationSubstitution extends Substitution {
            visitCallExpression(node)
            {
                let result = super.visitCallExpression(node);
                
                // We may have to re-resolve the function call, if it was a call to a protocol
                // signature.
                if (result.func instanceof ProtocolFuncDecl) {
                    let overload = resolveOverloadImpl(result.possibleOverloads, result.typeArguments, result.argumentTypes, result.returnType);
                    if (!overload.func)
                        throw new Error("Could not resolve protocol signature function call during instantiation: " + result.func + (overload.failures.length ? "; tried:\n" + overload.failures.join("\n") : ""));
                    result.resolveToOverload(overload);
                }
                
                if (result.func.isNative)
                    result.nativeFuncInstance = thisInstantiator.getUnique(result.func, result.actualTypeArguments);
                
                return result;
            }
        }
        
        let substitution = new InstantiationSubstitution(func.typeParameters, typeArguments);

        class InstantiationInstantiateImmediates extends InstantiateImmediates {
            visitCallExpression(node)
            {
                // We need to preserve certain things that would have instantiated, but that we cannot
                // instantiate without breaking chain-instantiations (generic function calls generic
                // function so therefore the instantiated generic function must still have the original
                // (uninstantiated) types to instantiate the generic function that it calls).
                let result = new CallExpression(
                    node.origin, node.name, node.typeArguments,
                    node.argumentList.map(argument => Node.visit(argument, this)));
                result = this.processDerivedCallData(node, result);
                
                result.argumentTypes = Array.from(node.argumentTypes);
                if (node.isCast)
                    result.setCastData(node.returnType);
                result.actualTypeArguments = Array.from(node.actualTypeArguments);
                
                return result;
            }
        }

        let instantiateImmediates = new InstantiationInstantiateImmediates();
        
        class Instantiate {
            visitFuncDef(func)
            {
                let returnType = func.returnType.visit(substitution);
                returnType = returnType.visit(instantiateImmediates);
                let parameters = func.parameters.map(parameter => parameter.visit(substitution));
                parameters = parameters.map(parameter => parameter.visit(instantiateImmediates));
                let body = func.body.visit(substitution);
                body = body.visit(instantiateImmediates);
                return new FuncDef(
                    func.origin, func.name, returnType, [], parameters, body, func.isCast,
                    func.shaderType);
            }
            
            visitNativeFunc(func)
            {
                return new NativeFuncInstance(
                    func,
                    func.returnType.visit(substitution).visit(instantiateImmediates),
                    func.parameters.map(parameter => parameter.visit(substitution).visit(instantiateImmediates)),
                    func.isCast,
                    func.shaderType,
                    func.instantiateImplementation(substitution));
            }
        }
        let resultingFunc = func.visit(new Instantiate());
        resultingFunc.uninstantiatedReturnType = func.returnType.visit(substitution);
        let instance = {func: resultingFunc, typeArguments};
        instances.push(instance);
        return resultingFunc;
    }
}

