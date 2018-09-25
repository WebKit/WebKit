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

class MSLStatementEmitter extends Visitor {

    constructor(program, funcMangler, typeUnifier, func, paramMap, debugName, typeAttributes)
    {
        super();
        this._program = program;
        this._funcMangler = funcMangler;
        this._typeUnifier = typeUnifier;
        this._func = func;
        this._nameMap = paramMap;
        this._debugName = debugName;
        this._typeAttributes = typeAttributes;
        this._counter = 0;
        this._indentLevel = 0;
        this._lines = [];

        this._loopConditionVariableStack = [];
        this._loopConditionNodeStack = [];
        this._loopIncrementNodeStack = [];

        this._actualResult = this._func.visit(this);
    }

    _emitTrap()
    {
        // No need to update this if in an entry point, we can just return zero.
        if (!this._func.isEntryPoint)
            this._add(`*(${this._func._trapPointerExpression.visit(this)}) = false;`);
        this._emitTrapReturn();
    }

    _emitTrapReturn()
    {
        if (this._func.returnType.equals(this._program.intrinsics.void))
            this._add('return;');
        else {
            const id = this._fresh();
            this._add(`${this._typeUnifier.uniqueTypeId(this._func.returnType)} ${id};`);
            this._zeroInitialize(this._func.returnType, id);
            this._add(`return ${id};`);
        }
    }

    _zeroInitialize(type, variableName, allowComment = true)
    {
        const emitter = this;

        class ZeroInitializer extends Visitor {
            visitNativeType(node)
            {
                if (node.name == "bool")
                    emitter._add(`${variableName} = false;`);
                else
                    emitter._add(`${variableName} = 0;`);
            }

            visitPtrType(node)
            {
                emitter._add(`${variableName} = nullptr;`);
            }

            visitArrayType(node)
            {
                for (let i = 0; i < node.numElements.value; i++)
                    emitter._zeroInitialize(node.elementType, `${variableName}[${i}]`, false);
            }

            visitArrayRefType(node)
            {
                emitter._add(`${variableName}.ptr = nullptr;`);
                emitter._add(`${variableName}.length = 0;`);
            }

            visitEnumType(node)
            {
                emitter._zeroInitialize(node.baseType, variableName, false);
            }

            visitStructType(node)
            {
                const typeAttributes = emitter._typeAttributes.attributesForType(node);

                for (let field of node.fields) {
                    const fieldName = typeAttributes.mangledFieldName(field.name);
                    emitter._zeroInitialize(field.type, `${variableName}.${fieldName}`, false);
                }
            }

            visitVectorType(node)
            {
                const elementNames = [ "x", "y", "z", "w" ];
                for (let i = 0; i < node.numElementsValue; i++)
                    emitter._add(`${variableName}.${elementNames[i]} = 0;`);
            }

            visitMatrixType(node)
            {
                for (let i = 0; i < node.numRowsValue; i++) {
                    for (let j = 0; j < node.numRowsValue; j++)
                        emitter._zeroInitialize(node.elementType, `${variableName}[${i}][${j}]`, false);
                }
            }

            visitTypeRef(node)
            {
                node.type.visit(this);
            }
        }
        if (allowComment)
            this._add(`// Zero initialization of ${variableName}`);
        Node.visit(type, new ZeroInitializer());
    }

    get actualResult()
    {
        return this._actualResult;
    }

    get lines()
    {
        return this._lines;
    }

    indentedSource()
    {
        return this._lines.map(line => "    " + line + "\n").join("");
    }

    get _loopConditionVariable()
    {
        return this._loopConditionVariableStack[this._loopConditionVariableStack.length - 1];
    }

    get _loopCondition()
    {
        return this._loopConditionNodeStack[this._loopConditionNodeStack.length - 1];
    }

    get _loopIncrement()
    {
        return this._loopIncrementNodeStack[this._loopIncrementNodeStack.length - 1];
    }

    _add(linesString)
    {
        for (let line of linesString.split('\n')) {
            for (let i = 0; i < this._indentLevel; i++)
                line = "    " + line;
            this._lines.push(line);
        }
    }

    _addLines(lines)
    {
        for (let line of lines)
            this._add(line);
    }

    _fresh()
    {
        return `V${this._counter++}`;
    }

    _indent(closure)
    {
        this._indentLevel++;
        const result = closure();
        this._indentLevel--;
        return result;
    }

    // Atomic statements.

    visitBreak(node)
    {
        this._add("break;")
    }

    visitContinue(node)
    {
        // This is necessary because for loops are compiled as while loops, so we need to do
        // the loop increment and condition check before returning to the beginning of the loop.
        this._emitLoopBodyEnd();
        this._add("continue;");
    }

    // Basic compound statements.

    visitBlock(block)
    {
        for (let statement of block.statements) {
            this._add(`// ${this._debugName} @ ${statement.origin.originString}`);
            statement.visit(this);
        }
    }

    visitReturn(node)
    {
        if (node.value)
            this._add(`return ${node.value.visit(this)};`);
        else
            this._add(`return;`);
    }

    _visitAndIndent(node)
    {
        return this._indent(() => node.visit(this));
    }

    visitIfStatement(node)
    {
        let condition = node.conditional.visit(this);
        this._add(`if (${condition}) {`);
        this._visitAndIndent(node.body);
        if (node.elseBody) {
            this._add(`} else {`);
            this._visitAndIndent(node.elseBody);
            this._add('}');
        } else
            this._add('}');
    }

    visitIdentityExpression(node)
    {
        return node.target.visit(this);
    }

    visitLogicalNot(node)
    {
        const type = typeOf(node);
        let expr = Node.visit(node.operand, this);
        let id = this._fresh();
        this._add(`${this._typeUnifier.uniqueTypeId(type)} ${id};`);
        this._add(`${id} = !(${expr});`);
        return id;
    }

    visitDereferenceExpression(node)
    {
        const ptr = node.ptr.visit(this);
        this._add(`if (!${ptr}) {`);
        this._indent(() => this._emitTrap());
        this._add('}');
        const id = this._fresh();
        this._add(`${this._typeUnifier.uniqueTypeId(node.type)} ${id};`);
        this._emitMemCopy(node.type, id, `(*${ptr})`);
        return id;
    }

    _isOperatorAnder(node)
    {
        const anderRegex = /^operator\&\.(.*?)$/;
        return node instanceof NativeFunc && anderRegex.test(node.name);
    }

    _isOperatorGetter(node)
    {
        const getterRegex = /^operator\.(.*?)$/;
        return node instanceof NativeFunc && getterRegex.test(node.name);
    }

    _isOperatorIndexer(node)
    {
        return node instanceof NativeFunc && node.name == "operator&[]";
    }

    _isOperatorSetter(node)
    {
        const setterRegex = /^operator\.(.*?)=$/;
        return node instanceof NativeFunc && setterRegex.test(node.name)
    }

    _isOperatorCast(node)
    {
        return node instanceof NativeFunc && node.name == "operator cast";
    }

    _isUnaryOperator(node)
    {
        const operatorRegex = /^operator\~$/;
        return node instanceof NativeFunc && operatorRegex.test(node.name);
    }

    _isBinaryOperator(node)
    {
        const operatorRegex = /operator(\+|\-|\*|\/|\^|\&|\||\&\&|\|\||\<\<|\>\>|\<|\<\=|\>|\>\=|\=\=|\!\=)$/;
        return node instanceof NativeFunc && operatorRegex.test(node.name);
    }

    _isOperatorValue(node)
    {
        return node instanceof NativeFunc && node.name == "operator.value";
    }

    _isOperatorLength(node)
    {
        return node instanceof NativeFunc && node.name == "operator.length";
    }

    _extractOperatorName(node)
    {
        return node.name.substring("operator".length);
    }

    visitVariableDecl(node)
    {
        let id = this._fresh();
        this._nameMap.set(node, id);

        this._add(`${this._typeUnifier.uniqueTypeId(node.type)} ${id}; // ${node}`);

        if (node.initializer) {
            let expr = node.initializer.visit(this);
            this._emitMemCopy(typeOf(node), id, expr);
        } else
            this._zeroInitialize(node.type, id);
    }

    visitVariableRef(node)
    {
        if (!this._nameMap.has(node.variable))
            throw new Error(`${node.variable} not found in this function's (${this._func.name}) variable map`);
        return this._nameMap.get(node.variable);
    }

    visitMakeArrayRefExpression(node)
    {
        const elemName = this._emitAsLValue(node.lValue);
        const arrayType = typeOf(node.lValue);
        const id = this._fresh();
        this._add(`${this._typeUnifier.uniqueTypeId(node.type)} ${id};`);
        this._add(`${id}.length = ${node.numElements.value};`);
        if (arrayType.isArray)
            this._add(`${id}.ptr = ${elemName};`);
        else
            this._add(`${id}.ptr = &(${elemName});`);
        return id;
    }

    visitConvertPtrToArrayRefExpression(node)
    {
        const lValue = this._emitAsLValue(node.lValue);
        const type = typeOf(node);
        const id = this._fresh();
        this._add(`${this._typeUnifier.uniqueTypeId(type)} ${id};`);
        this._add(`${id}.length = 1;`);
        this._add(`${id}.ptr = ${lValue};`);
        return id;
    }

    visitAnonymousVariable(node)
    {
        let id = this._fresh();
        this._nameMap.set(node, id);
        this._add(`${this._typeUnifier.uniqueTypeId(node.type)} ${id}; // ${node.name}`);
        this._zeroInitialize(node.type, id);
        return id;
    }

    visitAssignment(node)
    {
        const lhs = this._emitAsLValue(node.lhs);
        const rhs = node.rhs.visit(this);
        this._emitMemCopy(typeOf(node), lhs, rhs);
        return lhs;
    }

    _emitMemCopy(type, dest, src)
    {
        type = typeOf(type);
        if (type instanceof ArrayType) {
            for (let i = 0; i < type.numElementsValue; i++)
                this._emitMemCopy(typeOf(type.elementType), `${dest}[${i}]`, `${src}[${i}]`);
        } else {
            this._add(`// ${type}`);
            this._add(`${dest} = ${src};`);
        }
    }

    visitCommaExpression(node)
    {
        let result;
        for (let expr of node.list)
            result = Node.visit(expr, this);
        return result;
    }

    visitCallExpression(node)
    {
        const args = [];
        for (let i = node.argumentList.length; i--;)
            args.unshift(node.argumentList[i].visit(this));

        let resultVariable;
        if (node.func.returnType.name !== "void") {
            resultVariable = this._fresh();
            this._add(`// Result variable for call ${node.func.name}(${args.join(", ")})`);
            this._add(`${this._typeUnifier.uniqueTypeId(node.resultType)} ${resultVariable};`);
        }

        if (node.func instanceof FuncDef) {
            const mangledCallName = this._funcMangler.mangle(node.func);
            const callString = `${mangledCallName}(${args.join(", ")})`;
            if (resultVariable)
                this._add(`${resultVariable} = ${callString};`);
            else
                this._add(`${callString};`);

            this._add(`if (!(*(${this._func._trapPointerExpression.visit(this)}))) {`);
            this._indent(() => this._emitTrapReturn());
            this._add("}");
        } else
            this._makeNativeFunctionCall(node.func, resultVariable, args);

        return resultVariable;
    }

    _makeNativeFunctionCall(node, resultVariable, args)
    {
        if (!(node instanceof NativeFunc))
            throw new Error(`${node} should be a native function.`);

        if (this._isOperatorAnder(node))
            this._emitOperatorAnder(node, resultVariable, args);
        else if (node.implementationData instanceof BuiltinVectorGetter)
            this._emitBuiltinVectorGetter(node, resultVariable, args);
        else if (node.implementationData instanceof BuiltinVectorSetter)
            this._emitBuiltinVectorSetter(node, resultVariable, args);
        else if (node.implementationData instanceof BuiltinMatrixGetter)
            this._emitBuiltinMatrixGetter(node, resultVariable, args);
        else if (node.implementationData instanceof BuiltinMatrixSetter)
            this._emitBuiltinMatrixSetter(node, resultVariable, args);
        else if (this._isOperatorValue(node))
            this._add(`${resultVariable} = ${args[0]};`);
        else if (this._isOperatorLength(node))
            this._emitOperatorLength(node, resultVariable, args);
        else if (this._isOperatorSetter(node))
            this._emitOperatorSetter(node, resultVariable, args);
        else if (this._isOperatorGetter(node))
            this._emitOperatorGetter(node, resultVariable, args);
        else if (this._isOperatorIndexer(node))
            this._emitOperatorIndexer(node, resultVariable, args);
        else if (this._isOperatorCast(node))
            this._emitOperatorCast(node, resultVariable, args);
        else if (this._isUnaryOperator(node))
            this._add(`${resultVariable} = ${this._extractOperatorName(node)}(${args[0]});`);
        else if (this._isBinaryOperator(node))
            this._add(`${resultVariable} = ${args[0]} ${this._extractOperatorName(node)} ${args[1]};`);
        else
            this._add(mslNativeFunctionCall(node, resultVariable, args));

        return resultVariable;
    }

    _emitCallToMetalFunction(name, result, args)
    {
        this._add(`${result} = ${name}(${args.join(", ")});`);
    }

    _emitBuiltinVectorGetter(node, result, args)
    {
        this._add(`${result} = ${args[0]}.${node.implementationData.elementName};`);
    }

    _emitBuiltinVectorSetter(node, result, args)
    {
        this._add(`${result} = ${args[0]};`);
        this._add(`${result}.${node.implementationData.elementName} = ${args[1]};`);
    }

    _emitBuiltinMatrixGetter(node, result, args)
    {
        this._add(`if (${args[1]} >= ${node.implementationData.height}) {`);
        this._indent(() => this._emitTrap());
        this._add('}');
        this._add(`${result} = ${args[0]}[${args[1]}];`);
    }

    _emitBuiltinMatrixSetter(node, result, args)
    {
        this._add(`if (${args[1]} >= ${node.implementationData.height}) {`);
        this._indent(() => this._emitTrap());
        this._add('}');
        this._add(`${result} = ${args[0]};`);
        this._add(`${result}[${args[1]}] = ${args[2]};`);
    }

    _emitOperatorLength(node, result, args)
    {
        const paramType = typeOf(node.parameters[0]);
        if (paramType instanceof ArrayRefType)
            this._add(`${result} = ${args[0]}.length;`);
        else if (paramType instanceof ArrayType)
            this._add(`${result} = ${paramType.numElements.visit(this)};`);
        else
            throw new Error(`Unhandled paramter type ${paramType} for operator.length`);
    }

    _emitOperatorCast(node, result, args)
    {
        const retType = this._typeUnifier.uniqueTypeId(node.returnType);
        this._add(`${result} = ${retType}(${args.join(", ")});`);
    }

    _emitOperatorIndexer(node, result, args)
    {
        if (!(typeOf(node.parameters[0]) instanceof ArrayRefType))
            throw new Error(`Support for operator&[] ${node.parameters[0]} not implemented.`);

        this._add(`if (${args[1]} >= ${args[0]}.length) {`);
        this._indent(() => this._emitTrap());
        this._add(`}`);
        this._add(`// Operator indexer`);
        this._add(`${result} = &(${args[0]}.ptr[${args[1]}]);`);
    }

    _emitOperatorAnder(node, result, args)
    {
        const type = node.implementationData.structType;
        const field = this._typeAttributes.attributesForType(type).mangledFieldName(node.implementationData.name);
        this._add(`${result} = &((${args[0]})->${field});`);
    }

    _emitOperatorGetter(node, result, args)
    {
        const type = node.implementationData.type;
        const field = this._typeAttributes.attributesForType(type).mangledFieldName(node.implementationData.name);
        this._add(`${result} = ${args[0]}.${field};`);
    }

    _emitOperatorSetter(node, result, args)
    {
        // FIXME: Currently no WHLSL source produces structs that only have getters/setters rather than the ander,
        // and the ander is always preferred over the getter and setter. Therefore this code is untested.
        const type = node.implementationData.type;
        const field = this._typeAttributes.attributesForType(type).mangledFieldName(node.implementationData.name);
        this._add(`${args[0]}.${field} = ${args[1]};`);
        this._add(`${result} = ${args[0]}`);
    }

    visitMakePtrExpression(node)
    {
        if (node.target instanceof DereferenceExpression)
            return this._emitAsLValue(node.target.ptr);
        return `&(${this._emitAsLValue(node.lValue)})`;
    }

    // Loop code generation. Loops all follow the same style where they declare a conditional variable (boolean)
    // which is checked on each iteration (all loops are compiled to a while loop or do/while loop). The check is
    // emitted in _emitLoopCondition. For loops additionally have _emitLoopBodyEnd for the increment on each iteration.

    visitForLoop(node)
    {
        this._emitAsLoop(node.condition, node.increment, (conditionVar) => {
            Node.visit(node.initialization, this);
            this._emitLoopCondition();
            this._add(`while (${conditionVar}) {`);
            this._indent(() => {
                node.body.visit(this);
                this._emitLoopBodyEnd();
            });
            this._add('}');
        });
    }

    visitWhileLoop(node)
    {
        this._emitAsLoop(node.conditional, null, (conditionVar) => {
            this._add(`while (${conditionVar}) {`);
            this._indent(() => {
                node.body.visit(this);
                this._emitLoopCondition();
            });
            this._add('}');
        });
    }

    visitDoWhileLoop(node)
    {
        this._emitAsLoop(node.conditional, null, conditionVar => {
            this._add("do {");
            this._indent(() => {
                node.body.visit(this);
                this._emitLoopBodyEnd();
            });
            this._add(`} while (${conditionVar});`);
        });
    }

    _emitAsLoop(conditional, increment, emitLoopBody)
    {
        const conditionVar = this._fresh();
        this._loopConditionVariableStack.push(conditionVar);
        this._loopConditionNodeStack.push(conditional);
        this._loopIncrementNodeStack.push(increment);
        this._add(`bool ${conditionVar} = true;`);

        emitLoopBody(conditionVar);

        this._loopIncrementNodeStack.pop();
        this._loopConditionVariableStack.pop();
        this._loopConditionNodeStack.pop();
    }

    _emitLoopBodyEnd()
    {
        this._emitLoopIncrement();
        this._emitLoopCondition();
    }

    _emitLoopIncrement()
    {
        Node.visit(this._loopIncrement, this);
    }

    _emitLoopCondition()
    {
        if (this._loopCondition) {
            const conditionResult = this._loopCondition.visit(this);
            this._add(`${this._loopConditionVariable} = ${conditionResult};`);
        }
    }

    visitSwitchStatement(node)
    {
        const caseValueEmitter = new MSLConstexprEmitter();

        let switchValue = Node.visit(node.value, this);
        this._add(`switch (${switchValue}) {`);
        this._indent(() => {
            for (let i = 0; i < node.switchCases.length; i++) {
                let switchCase = node.switchCases[i];
                if (!switchCase.isDefault)
                    this._add(`case ${switchCase.value.visit(caseValueEmitter)}: {`);
                else
                    this._add("default: {");
                this._visitAndIndent(switchCase.body);
                this._add("}");
            }
        });
        this._add("}");
    }

    visitSwitchCase(node)
    {
        throw new Error(`MSLStatementEmitter.visitSwitchCase called; switch statements should be fully handled by MSLStatementEmitter.visitSwitchStatement.`);
    }

    visitBoolLiteral(node)
    {
        const fresh = this._fresh();
        this._add(`bool ${fresh} = ${node.value};`);
        return fresh;
    }

    visitEnumLiteral(node)
    {
        return node.member.value.visit(this);
    }

    visitGenericLiteral(node)
    {
        const fresh = this._fresh();
        this._add(`${this._typeUnifier.uniqueTypeId(node.type)} ${fresh};`);
        this._add(`${fresh} = ${node.value};`);
        return fresh;
    }

    visitNullLiteral(node)
    {
        return `nullptr`;
    }

    visitDotExpression(node)
    {
        throw new Error(`MSLStatementEmitter.visitDotExpression called; ${node} should have been converted to an operator function call.`);
    }

    visitTrapStatement(node)
    {
        this._emitTrap();
    }

    visitLogicalExpression(node)
    {
        const result = this._fresh();
        this._add(`// Result variable for ${node.text}`);
        this._add(`bool ${result};`);
        if (node.text == "&&") {
            const left = node.left.visit(this);
            this._add(`if (!${left}) {`)
            this._indent(() => this._add(`${result} = false;`));
            this._add('} else {');
            this._indent(() => this._add(`${result} = ${node.right.visit(this)};`));
            this._add('}');
        } else if (node.text == "||") {
            const left = node.left.visit(this);
            this._add(`${result} = ${left};`);
            this._add(`if (!${result}) {`);
            this._indent(() => this._add(`${result} = ${node.right.visit(this)};`));
            this._add("}");
        } else
            throw new Error(`Unrecognized logical expression ${node.text}`);
        return result;
    }

    visitTernaryExpression(node)
    {
        // FIXME: If each of the three expressions isn't a compound expression or statement then just use ternary syntax.
        let resultType = typeOf(node.bodyExpression);
        let resultVar = this._fresh();
        this._add(`${this._typeUnifier.uniqueTypeId(resultType)} ${resultVar};`);
        const predicate = node.predicate.visit(this);
        this._add(`if (${predicate}) {`);
        this._indent(() => {
            const bodyExpression = node.bodyExpression.visit(this);
            this._emitMemCopy(resultType, resultVar, bodyExpression);
        });
        this._add("} else {");
        this._indent(() => {
            const elseExpression = node.elseExpression.visit(this);
            this._emitMemCopy(resultType, resultVar, elseExpression);
        });
        this._add("}");

        return resultVar;
    }

    visitIndexExpression(node)
    {
        throw new Error("MSLStatementEmitter.visitIndexExpression is not implemented; IndexExpression should not appear in the AST for Metal codegen.");
    }

    visitNativeFunc(node)
    {
        throw new Error("MSLStatementEmitter.visitNativeFunc is not implemented; NativeFunction is not compiled by the Metal codegen.");
    }

    visitNativeTypeInstance(node)
    {
        throw new Error("MSLStatementEmitter.visitNativeTypeInstance is not implemented.");
    }

    visitReadModifyWriteExpression(node)
    {
        throw new Error("MSLStatementEmitter.visitReadModifyWriteExpression is not implemented; it should have been transformed out the tree in earlier stage.");
    }

    _emitAsLValue(node)
    {
        const lOrRValueEmitter = this;
        class LValueVisitor extends Visitor
        {
            visitDereferenceExpression(node)
            {
                const target = node.ptr.visit(lOrRValueEmitter);
                lOrRValueEmitter._add(`if (!(${target})) {`);
                lOrRValueEmitter._indent(() => lOrRValueEmitter._emitTrap());
                lOrRValueEmitter._add("}");
                return `(*(${target}))`;
            }

            visitVariableRef(node)
            {
                return node.visit(lOrRValueEmitter);
            }

            visitIdentityExpression(node)
            {
                return node.target.visit(this);
            }

            visitCallExpression(node)
            {
                // FIXME: Is this comment actually true???
                // Only occurs for @(...) expressions.
                return node.visit(lOrRValueEmitter);
            }

            visitMakePtrExpression(node)
            {
                if (node.lValue instanceof DereferenceExpression)
                    return node.lValue.ptr.visit(lOrRValueEmitter);
            }
        }
        const result = node.visit(new LValueVisitor());
        if (!result)
            throw new Error(`${node} not a valid l-value.`);
        return result;
    }
}