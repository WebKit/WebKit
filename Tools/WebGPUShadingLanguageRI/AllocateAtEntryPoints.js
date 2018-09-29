/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.

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

function gatherEntryPoints(program)
{
    const entryPoints = new Set();
    for (let [name, funcDefs] of program.functions) {
        for (let funcDef of funcDefs) {
            if (funcDef.isEntryPoint)
                entryPoints.add(funcDef);
        }
    }
    return entryPoints;
}

function gatherCallees(funcDefs)
{
    const calleeSet = new Set();
    class FindFunctionsThatAreCalled extends Visitor
    {
        visitCallExpression(node)
        {
            super.visitCallExpression(node);
            if (node.func instanceof FuncDef && !calleeSet.has(node.func)) {
                calleeSet.add(node.func);
                node.func.visit(this);
            }
        }
    }
    const visitor = new FindFunctionsThatAreCalled();
    for (let funcDef of funcDefs)
        funcDef.visit(visitor);
    return calleeSet;
}

function gatherVariablesAndFunctionParameters(funcDefs)
{
    const varsAndParams = new Set();
    class VarAndParamVisitor extends Visitor {

        constructor(isEntryPoint)
        {
            super();
            this._isEntryPoint = isEntryPoint;
        }

        visitFuncParameter(node)
        {
            if (!this._isEntryPoint)
                varsAndParams.add(node);
        }

        visitVariableDecl(node)
        {
            varsAndParams.add(node);
        }
    }
    for (let func of funcDefs)
        func.visit(new VarAndParamVisitor(func.isEntryPoint));
    return varsAndParams;
}

function createGlobalStructTypeAndVarToFieldMap(origin, allVariablesAndFunctionParameters, functionsThatAreCalledByEntryPoints)
{
    const globalStructType = new StructType(origin, null);
    let counter = 0;
    const varToFieldMap = new Map();

    for (let varOrParam of allVariablesAndFunctionParameters) {
        const fieldName = `field${counter++}_${varOrParam.name}`;
        globalStructType.add(new Field(varOrParam.origin, fieldName, varOrParam.type));
        varToFieldMap.set(varOrParam, fieldName);
    }

    for (let func of functionsThatAreCalledByEntryPoints) {
        if (func.returnType.name !== "void") {
            const fieldName = `field${counter++}_return_${func.name}`;
            globalStructType.add(new Field(func.origin, fieldName, func.returnType));
            func.returnFieldName = fieldName;
        }
    }

    return [ globalStructType, varToFieldMap ];
}

function allocateAtEntryPoints(program)
{
    const entryPoints = gatherEntryPoints(program);
    const allCallees = gatherCallees(entryPoints);
    const allExecutedFunctions = new Set([...entryPoints, ...allCallees]);
    const allVariablesAndFunctionParameters = gatherVariablesAndFunctionParameters(allExecutedFunctions);

    if (!allVariablesAndFunctionParameters.size)
        return;

    const anyEntryPoint = entryPoints.values().next().value;
    const [ globalStructType, varToFieldMap ] = createGlobalStructTypeAndVarToFieldMap(anyEntryPoint.origin, allVariablesAndFunctionParameters, allCallees);
    program.add(globalStructType);
    // All other struct accessors will have been synthesized at this point (they have to be synthesized earlier for call resolution).
    synthesizeStructAccessorsForStructType(program, globalStructType);

    const globalStructTypeRef = TypeRef.wrap(globalStructType);
    const ptrToGlobalStructType = new PtrType(anyEntryPoint.origin, "thread", globalStructTypeRef);
    const ptrToGlobalStructTypeRef = TypeRef.wrap(ptrToGlobalStructType);

    const funcNewParameterMapping = new Map();
    function updateFunction(func)
    {
        class UpdateFunctions extends Rewriter {
            constructor()
            {
                super();
                if (func.isEntryPoint)
                    this._addVariableDeclaration();
                else
                    this._reconfigureParameters();
                func.body = func._body.visit(this);
            }

            _addVariableDeclaration()
            {
                this._variableDecl = new VariableDecl(func.origin, "global struct", globalStructTypeRef, null);
                this.makeGlobalStructVariableRef = () => new MakePtrExpression(func.origin, VariableRef.wrap(this._variableDecl));
                func.body.statements.unshift(this._variableDecl);
            }

            _reconfigureParameters()
            {
                const funcParam = new FuncParameter(func.origin, null, ptrToGlobalStructTypeRef);
                this.makeGlobalStructVariableRef = () =>  VariableRef.wrap(funcParam);
                funcNewParameterMapping.set(func, [ funcParam ]);
            }

            _callExpressionForFieldName(node, fieldName)
            {
                if (!fieldName)
                    throw new Error("Field name was null");
                const functionName = `operator&.${fieldName}`;
                const possibleAndOverloads = program.globalNameContext.get(Func, functionName);
                const callExpressionResolution = CallExpression.resolve(node.origin, possibleAndOverloads, functionName, [ this.makeGlobalStructVariableRef() ], [ ptrToGlobalStructTypeRef ]);
                return callExpressionResolution.call;
            }

            _dereferencedCallExpressionForFieldName(node, resultType, fieldName)
            {
                const derefExpr = new DereferenceExpression(node.origin, this._callExpressionForFieldName(node, fieldName), node.type, "thread");
                derefExpr.type = resultType;
                return derefExpr;
            }

            visitVariableRef(node)
            {
                if (!varToFieldMap.has(node.variable))
                    return super.visitVariableRef(node);
                return this._dereferencedCallExpressionForFieldName(node, node.variable.type, varToFieldMap.get(node.variable));
            }

            visitVariableDecl(node)
            {
                if (node == this._variableDecl)
                    return node;
                else if (node.initializer) {
                    if (!node.type)
                        throw new Error(`${node} doesn't have a type`);
                    return new Assignment(node.origin, this._dereferencedCallExpressionForFieldName(node, node.type, varToFieldMap.get(node)), node.initializer.visit(this), node.type);
                }
                return new Block(node.origin);
            }

            visitCallExpression(node)
            {
                node = super.visitCallExpression(node);
                if (node.func instanceof FuncDef) {
                    let exprs = [];
                    const anonymousVariableAssignments = [];
                    for (let i = node.argumentList.length; i--;) {
                        const type = node.func.parameters[i].type;
                        if (!type)
                            throw new Error(`${node.func.parameters[i]} has no type`);
                        const anonymousVariable = new AnonymousVariable(node.origin, type);
                        exprs.push(anonymousVariable);
                        exprs.push(new Assignment(node.origin, VariableRef.wrap(anonymousVariable), node.argumentList[i], type));
                        anonymousVariableAssignments.push(new Assignment(node.origin, this._dereferencedCallExpressionForFieldName(node.func.parameters[i], node.func.parameters[i].type, varToFieldMap.get(node.func.parameters[i])), VariableRef.wrap(anonymousVariable), type));
                    }
                    exprs = exprs.concat(anonymousVariableAssignments);
                    exprs.push(node);

                    if (node.func.returnType.name !== "void")
                        exprs.push(this._dereferencedCallExpressionForFieldName(node.func, node.func.returnType, node.func.returnFieldName));

                    node.argumentList = [ this.makeGlobalStructVariableRef() ];
                    return new CommaExpression(node.origin, exprs);
                }

                return node;
            }

            visitReturn(node)
            {
                node = super.visitReturn(node);
                if (!func.isEntryPoint && node.value) {
                    return new CommaExpression(node.origin, [
                        new Assignment(node.origin, this._dereferencedCallExpressionForFieldName(func, func.returnType, func.returnFieldName), node.value, func.returnType),
                        new Return(node.origin) ]);
                } else
                    return node;
            }
        }
        func.visit(new UpdateFunctions());
    }

    for (let funcDef of allExecutedFunctions)
        updateFunction(funcDef);

    for (let [func, newParameters] of funcNewParameterMapping)
        func.parameters = newParameters;

    for (let func of allCallees)
        func._returnType = TypeRef.wrap(program.types.get("void"));

    class UpdateCallExpressionReturnTypes extends Visitor {
        visitCallExpression(node)
        {
            super.visitCallExpression(node);
            if (node.func instanceof FuncDef)
                node._returnType = node.resultType = TypeRef.wrap(program.types.get("void"));
        }
    }
    const updateCallExpressionVisitor = new UpdateCallExpressionReturnTypes();
    for (let func of allExecutedFunctions)
        func.visit(updateCallExpressionVisitor);
}
