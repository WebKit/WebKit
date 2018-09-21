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

// FIXME: Rather than being a separate function this should instead happen in the Checker phase that annotates
// each Node instance with a "type" property. https://bugs.webkit.org/show_bug.cgi?id=189611
function typeOf(node) {
    class TypeVisitor extends Visitor {
        visitAnonymousVariable(node)
        {
            return node.type.visit(this);
        }

        visitArrayRefType(node)
        {
            return node;
        }

        visitArrayType(node)
        {
            return node;
        }

        visitAssignment(node)
        {
            return node.type.visit(this);
        }

        visitCallExpression(node)
        {
            return node.func.visit(this);
        }

        visitCommaExpression(node)
        {
            return node.list[node.list.length - 1].visit(this);
        }

        visitConvertPtrToArrayRefExpression(node)
        {
            const ptrType = typeOf(node.lValue);
            return new ArrayRefType(node.origin, ptrType.addressSpace, ptrType.elementType);
        }

        visitDotExpression(node)
        {
            return node.struct.fieldMap.get(node.fieldName).type.visit(this);
        }

        visitElementalType(node)
        {
            return node;
        }

        visitEnumType(node)
        {
            return node;
        }

        visitField(node)
        {
            return node.type.visit(this);
        }

        visitFunc(node)
        {
            return node.returnType.visit(this);
        }

        visitFuncDef(node)
        {
            return node.returnType.visit(this);
        }

        visitFuncParameter(node)
        {
            return node.type.visit(this);
        }

        visitFunctionLikeBlock(node)
        {
            return node.returnType.visit(this);
        }

        visitGenericLiteralType(node)
        {
            return typeOf(node.type);
        }

        visitIdentityExpression(node)
        {
            return node.target.visit(this);
        }

        visitIndexExpression(node)
        {
            return node.array.elementType.visit(this);
        }

        visitLogicalExpression(node)
        {
            throw new Error("FIXME: Implement TypeVisitor.visitLogicalExpression");
        }

        visitLogicalNot(node)
        {
            return node.operand.visit(this);
        }

        visitMakeArrayRefExpression(node)
        {
            return node.type.visit(this);
        }

        visitMakePtrExpression(node)
        {
            return new PtrType(node.origin, "thread", typeOf(node.lValue));
        }

        visitMatrixType(node)
        {
            return node;
        }

        visitNativeFunc(node)
        {
            return node.returnType.visit(this);
        }

        visitNativeFuncInstance(node)
        {
            return node.returnType.visit(this);
        }

        visitNativeType(node)
        {
            return node;
        }

        visitNativeTypeInstance(node)
        {
            return node;
        }

        visitNullLiteral(node)
        {
            return node.type.visit(this);
        }

        visitNullType(node)
        {
            return node;
        }

        visitPtrType(node)
        {
            return node;
        }

        visitReadModifyWriteExpression(node)
        {
            throw new Error("FIXME: Implement TypeVisitor.visitReadModifyWriteExpression");
        }

        visitReferenceType(node)
        {
            return node;
        }

        visitStructType(node)
        {
            return node;
        }

        visitTypeDef(node)
        {
            return node.type.visit(this);
        }

        visitTypeRef(node)
        {
            return node.type.visit(this);
        }

        visitVariableDecl(node)
        {
            return node.type.visit(this);
        }

        visitVariableRef(node)
        {
            return node.variable.type.visit(this);
        }

        visitVectorType(node)
        {
            return node;
        }

        visitBlock(node)
        {
            throw new Error("Block has no type");
        }

        visitBoolLiteral(node)
        {
            throw new Error("BoolLiteral has no type");
        }

        visitBreak(node)
        {
            throw new Error("Break has no type");
        }

        visitConstexprTypeParameter(node)
        {
            throw new Error("ConstexprTypeParameter has no type");
        }

        visitContinue(node)
        {
            throw new Error("Continue has no type");
        }

        visitDereferenceExpression(node)
        {
            return node.type.visit(this);
        }

        visitDoWhileLoop(node)
        {
            throw new Error("DoWhileLoop has no type");
        }

        visitEnumLiteral(node)
        {
            throw new Error("EnumLiteral has no type");
        }

        visitEnumMember(node)
        {
            throw new Error("EnumMember has no type");
        }

        visitForLoop(node)
        {
            throw new Error("ForLoop has no type");
        }
        visitGenericLiteral(node)
        {
            return node.type.visit(this);
        }
        visitIfStatement(node)
        {
            throw new Error("IfStatement has no type");
        }

        visitProgram(node)
        {
            throw new Error("Program has no type");
        }

        visitProtocolDecl(node)
        {
            throw new Error("ProtocolDecl has no type");
        }

        visitProtocolFuncDecl(node)
        {
            throw new Error("ProtocolFuncDecl has no type");
        }

        visitProtocolRef(node)
        {
            throw new Error("ProtocolRef has no type");
        }

        visitReturn(node)
        {
            throw new Error("Return has no type");
        }

        visitSwitchCase(node)
        {
            throw new Error("SwitchCase has no type");
        }

        visitSwitchStatement(node)
        {
            throw new Error("SwitchStatement has no type");
        }

        visitTrapStatement(node)
        {
            throw new Error("TrapStatement has no type");
        }

        visitWhileLoop(node)
        {
            throw new Error("WhileLoop has no type");
        }
    }
    return node.visit(new TypeVisitor());
}