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

function insertTrapParameter(program)
{
    const functionVisitedOrder = [];
    const entryPointFunctions = new Set();
    class FindEntryPoints extends Visitor {
        visitFuncDef(node)
        {
            if (node.isEntryPoint && !entryPointFunctions.has(node)) {
                entryPointFunctions.add(node);
                functionVisitedOrder.push(node);
            }
        }
    }
    program.visit(new FindEntryPoints());

    const functionsCalledByEntryPoints = new Set();
    class FindFunctionsCalledByEntryPoints extends Visitor {
        visitCallExpression(node)
        {
            super.visitCallExpression(node);
            if (node.func instanceof FuncDef && !functionsCalledByEntryPoints.has(node.func)) {
                functionsCalledByEntryPoints.add(node.func);
                functionVisitedOrder.push(node.func);
                node.func.visit(this);
            }
        }
    }
    for (let entryPoint of entryPointFunctions)
        entryPoint.visit(new FindFunctionsCalledByEntryPoints());

    for (let entryPoint of entryPointFunctions) {
        const trapVariable = new VariableDecl(entryPoint.origin, null, program.types.get("bool"), new BoolLiteral(entryPoint.origin, true));
        entryPoint.body.statements.unshift(trapVariable);
        entryPoint._trapPointerExpression = new MakePtrExpression(entryPoint.origin, VariableRef.wrap(trapVariable));
    }

    for (let func of functionsCalledByEntryPoints) {
        const trapParameter = new FuncParameter(func.origin, null, new PtrType(func.origin, "thread", TypeRef.wrap(program.types.get("bool"))));
        func.parameters.push(trapParameter);
        func._trapPointerExpression = VariableRef.wrap(trapParameter);
    }

    const visitedFunctions = new Set();
    class UpdateCallExpressions extends Visitor {
        constructor(caller)
        {
            super();
            this._caller = caller;
            if (!visitedFunctions.has(this._caller)) {
                visitedFunctions.add(this._caller);
                caller.visit(this);
            }
        }

        visitCallExpression(node)
        {
            super.visitCallExpression(node);
            if (node.func instanceof FuncDef) {
                node.argumentList.push(this._caller._trapPointerExpression);
                new UpdateCallExpressions(node.func);
            }
        }
    }

    for (let entryPoint of entryPointFunctions)
        new UpdateCallExpressions(entryPoint);
}
