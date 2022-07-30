/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#import "config.h"

#import "AssignmentStatement.h"
#import "IdentifierExpression.h"
#import "Lexer.h"
#import "LiteralExpressions.h"
#import "Parser.h"
#import "ReturnStatement.h"
#import "StructureAccess.h"
#import "CallableExpression.h"
#import "BinaryExpression.h"
#import "WGSL.h"
#import <wtf/DataLog.h>
#import <XCTest/XCTest.h>

@interface FullModuleTests : XCTestCase

@end

@implementation FullModuleTests

- (void) setUp {
    [super setUp];
    [self setContinueAfterFailure:NO];
}

- (void) testSomething {
    String source =
    "@vertex\n"
    "fn main(@builtin(vertex_index) VertexIndex : u32)\n"
    "    -> @builtin(position) vec4<f32> {\n"
    "    var pos = array<vec2<f32>, 3>(\n"
    "        vec2<f32>(0.5, 0.5),\n"
    "        vec2<f32>(0.5, 0.5),\n"
    "        vec2<f32>(0.5, 0.5));\n"
    "\n"
    "    return vec4<f32>(pos[VertexIndex], 0.0, 1.0);\n"
    "}"_s;
    
    auto shader = WGSL::parseLChar(source);
    
    XCTAssert(shader);

    XCTAssert(shader->directives().isEmpty());

    XCTAssert(shader->structs().isEmpty());

    XCTAssert(shader->globalVars().isEmpty());

    XCTAssert(shader->functions().size() == 1);

    // Check function main()
    WGSL::AST::FunctionDecl& func = shader->functions()[0];
    
    // Check attributes
    {
        XCTAssert(func.attributes().size() == 1);
        XCTAssert(func.attributes()[0]->isStage());
        const auto& stageAttrib = downcast<WGSL::AST::StageAttribute>(func.attributes()[0].get());
        XCTAssert(stageAttrib.stage() == WGSL::AST::StageAttribute::Stage::Vertex);
    }
    
    // Check function name.
    XCTAssert(func.name() == "main"_s);
    
    // Check number of parameters.
    XCTAssert(func.parameters().size() == 1);
    
    // Check information of parameter 0.
    {
        auto& param = func.parameters()[0].get();
        
        // Check attributes count.
        XCTAssert(param.attributes().size() == 1);

        // Check attribute 0 (@builtin(vertex_index))
        const auto& paramAttrib = param.attributes()[0].get();
        XCTAssert(paramAttrib.isBuiltin());
        auto& builtinAttrib = downcast<WGSL::AST::BuiltinAttribute>(paramAttrib);
        XCTAssert(builtinAttrib.name() == "vertex_index"_s);

        // Check parameter name.
        XCTAssert(param.name() == "VertexIndex"_s);

        // Check parameter type
        auto& paramType = param.type();
        XCTAssert(paramType.isNamed());
        auto& namedParamType = downcast<WGSL::AST::NamedType>(paramType);
        XCTAssert(namedParamType.name() == "u32"_s);
    }

    // Check return type and attributes
    {
        // Check if function returns something.
        XCTAssert(func.maybeReturnType());
        auto& returnType = *func.maybeReturnType();

        // Check return attribute is @builtin(position)
        XCTAssert(func.returnAttributes().size() == 1);
        const auto& returnAttrib = func.returnAttributes()[0].get();
        XCTAssert(returnAttrib.isBuiltin());
        const auto& builtinAttrib = downcast<WGSL::AST::BuiltinAttribute>(returnAttrib);
        XCTAssert(builtinAttrib.name() == "position"_s);

        // Check return type is a vector.
        XCTAssert(returnType.isVec());
        auto& vecReturnType = downcast<WGSL::AST::VecType>(returnType);

        // Check vector size is 4.
        XCTAssert(vecReturnType.size() == 4);

        // Check element type is f32
        const auto& vecElementType = vecReturnType.elementType();
        XCTAssert(vecElementType.isNamed());
        const auto& vecElementTypeAsNamed = downcast<WGSL::AST::NamedType>(vecElementType);
        XCTAssert(vecElementTypeAsNamed.name() == "f32"_s);
    }

    XCTAssert(func.body().statements().size() == 2);
    // Check function statement 0 (variable statement)
    {
        auto& stmt = func.body().statements()[0].get();
        XCTAssert(stmt.isVariable());
        const auto& variableStmt = downcast<WGSL::AST::VariableStatement>(stmt);
        const auto& variableDecl = variableStmt.decl();

        XCTAssert(variableDecl.kind() == WGSL::AST::VariableDecl::Kind::Var);
        XCTAssert(variableDecl.name() == "pos"_s);
        XCTAssert(variableDecl.attributes().isEmpty());
        XCTAssert(!variableDecl.maybeQualifier());
        XCTAssert(!variableDecl.maybeTypeDecl());

        // Check initialization expression.
        {
            XCTAssert(variableDecl.maybeInitializer());
            const auto& expr = *variableDecl.maybeInitializer();
            XCTAssert(expr.isCallableExpression());
            const auto& callExpr = downcast<WGSL::AST::CallableExpression>(expr);

            // Check type of callable expression is array<vec2<f32>, 3>
            {
                const auto& type = callExpr.typeDecl();

                XCTAssert(type.isArray());
                const auto& arrayDecl = downcast<WGSL::AST::ArrayType>(type);
                
                const auto& elementType = arrayDecl.elementType();
                XCTAssert(elementType.isVec());
                const auto& vecElementType = downcast<WGSL::AST::VecType>(elementType);
                XCTAssert(vecElementType.size() == 2);
                {
                    const auto& type = vecElementType.elementType();
                    XCTAssert(type.isNamed());
                    const auto& namedType = downcast<WGSL::AST::NamedType>(type);
                    XCTAssert(namedType.name() == "f32"_s);
                }
                
                XCTAssert(arrayDecl.maybeSizeExpression());
                const auto& sizeExpr = *arrayDecl.maybeSizeExpression();
                XCTAssert(sizeExpr.isAbstractIntLiteral());
                const auto& sizeLiteral = downcast<WGSL::AST::AbstractIntLiteral>(sizeExpr);
                XCTAssert(sizeLiteral.value() == 3);
            }
            
            const auto& callArgs = callExpr.arguments();
            XCTAssert(callArgs.size() == 3);
            
            // All arguments should be of type vec2<f32>
            for (const auto& arg : callArgs) {
                XCTAssert(is<WGSL::AST::CallableExpression>(arg.get()));
                const auto& callExpr = downcast<WGSL::AST::CallableExpression>(arg.get());

                XCTAssert(is<WGSL::AST::VecType>(callExpr.typeDecl()));
                const auto& vecTarget = downcast<WGSL::AST::VecType>(callExpr.typeDecl());
                XCTAssert(vecTarget.size() == 2);
                {
                    const auto& type = vecTarget.elementType();
                    XCTAssert(type.isNamed());
                    const auto& namedType = downcast<WGSL::AST::NamedType>(type);
                    XCTAssert(namedType.name() == "f32"_s);
                }

                for (const auto& vecArg : callExpr.arguments()) {
                    XCTAssert(is<WGSL::AST::AbstractFloatLiteral>(vecArg.get()));
                    const auto& floatLiteral = downcast<WGSL::AST::AbstractFloatLiteral>(vecArg.get());
                    XCTAssert(floatLiteral.value() == 0.5);
                }
            }
        }
    }
    {
        auto& stmt = func.body().statements()[1].get();
        XCTAssert(stmt.isReturn());
        auto& returnStmt = downcast<WGSL::AST::ReturnStatement>(stmt);

        XCTAssert(returnStmt.maybeExpression());
        auto& returnExpr = *returnStmt.maybeExpression();

        XCTAssert(returnExpr.isCallableExpression());
        const auto& callExpr = downcast<WGSL::AST::CallableExpression>(returnExpr);

        // Check type of callable expression is vec4<f32>
        {
            const auto& type = callExpr.typeDecl();
            XCTAssert(type.isVec());
            const auto& vec = downcast<WGSL::AST::VecType>(type);
            XCTAssert(vec.size() == 4);
            {
                const auto& type = vec.elementType();
                XCTAssert(type.isNamed());
                const auto& namedType = downcast<WGSL::AST::NamedType>(type);
                XCTAssert(namedType.name() == "f32"_s);
            }
        }
        
        const auto& callArgs = callExpr.arguments();
        XCTAssert(callArgs.size() == 3);
        
        // Check argument 0 ("pos[VertexIndex]")
        {
            const auto& expr = callArgs[0].get();
            XCTAssert(expr.isBinaryExpression());
            const auto& subscriptExpr = downcast<WGSL::AST::BinaryExpression>(expr);
            
            // Check correct operator
            XCTAssert(subscriptExpr.op() == WGSL::AST::BinaryExpression::Op::Subscript);

            // Check left side.
            const auto& lhsExpr = subscriptExpr.lhs();
            XCTAssert(lhsExpr.isIdentifier());
            const auto& lhsIdent = downcast<WGSL::AST::IdentifierExpression>(lhsExpr);
            XCTAssert(lhsIdent.identifier() == "pos"_s);

            // Check right side.
            const auto& rhsExpr = subscriptExpr.rhs();
            XCTAssert(rhsExpr.isIdentifier());
            const auto& rhsIdent = downcast<WGSL::AST::IdentifierExpression>(rhsExpr);
            XCTAssert(rhsIdent.identifier() == "VertexIndex"_s);

        }
        // Check argument 1 ("0.0")
        {
            const auto& expr = callArgs[1].get();
            XCTAssert(expr.isAbstractFloatLiteral());
            const auto& literal = downcast<WGSL::AST::AbstractFloatLiteral>(expr);
            XCTAssert(literal.value() == 0.0);
        }

        // Check argument 2 ("1.0")
        {
            const auto& expr = callArgs[2].get();
            XCTAssert(expr.isAbstractFloatLiteral());
            const auto& literal = downcast<WGSL::AST::AbstractFloatLiteral>(expr);
            XCTAssert(literal.value() == 1.0);
        }
    }
}

- (void)testParsingStruct {
//    auto shader = WGSL::parseLChar("struct B {\n"
//                                   "    a: i32;\n"
//                                   "}"_s);
//
//    if (!shader.has_value())
//        dataLogLn(shader.error());
//    XCTAssert(shader.has_value());
//    XCTAssert(!shader->directives().size());
//    XCTAssert(shader->structs().size() == 1);
//    XCTAssert(shader->globalVars().size() == 0);
//    XCTAssert(shader->functions().size() == 0);
//    WGSL::AST::StructDecl& str = shader->structs()[0];
//    XCTAssert(str.name() == "B"_s);
//    XCTAssert(str.attributes().isEmpty());
//    XCTAssert(str.members().size() == 1);
//    XCTAssert(str.members()[0]->attributes().isEmpty());
//    XCTAssert(str.members()[0]->name() == "a"_s);
//    XCTAssert(str.members()[0]->type().isNamed());
//    WGSL::AST::NamedType& memberType = downcast<WGSL::AST::NamedType>(str.members()[0]->type());
//    XCTAssert(memberType.name() == "i32"_s);
}

- (void)testParsingGlobalVariable {
//    auto shader = WGSL::parseLChar("@group(0) @binding(0)\n"
//                                   "var<storage, read_write> x: B;\n"_s);
//    if (!shader.has_value())
//        dataLogLn(shader.error());
//    XCTAssert(shader.has_value());
//    XCTAssert(!shader->directives().size());
//    XCTAssert(shader->structs().size() == 0);
//    XCTAssert(shader->globalVars().size() == 1);
//    XCTAssert(shader->functions().size() == 0);
//    WGSL::AST::GlobalVariableDecl& var = shader->globalVars()[0];
//    XCTAssert(var.attributes().size() == 2);
//    XCTAssert(var.attributes()[0]->isGroup());
//    XCTAssert(downcast<WGSL::AST::GroupAttribute>(var.attributes()[0].get()).group() == 0);
//    XCTAssert(var.attributes()[1]->isBinding());
//    XCTAssert(downcast<WGSL::AST::BindingAttribute>(var.attributes()[1].get()).binding() == 0);
//    XCTAssert(var.name() == "x"_s);
//    XCTAssert(var.maybeQualifier());
//    XCTAssert(var.maybeQualifier()->storageClass() == WGSL::AST::StorageClass::Storage);
//    XCTAssert(var.maybeQualifier()->accessMode() == WGSL::AST::AccessMode::ReadWrite);
//    XCTAssert(var.maybeTypeDecl());
//    XCTAssert(var.maybeTypeDecl()->isNamed());
//    WGSL::AST::NamedType& namedType = downcast<WGSL::AST::NamedType>(*var.maybeTypeDecl());
//    XCTAssert(namedType.name() == "B"_s);
//    XCTAssert(!var.maybeInitializer());
//}
//
//- (void)testParsingFunctionDecl {
//    auto shader = WGSL::parseLChar("@compute\n"
//                                   "fn main() {\n"
//                                   "    x.a = 42i;\n"
//                                   "}"_s);
//    if (!shader.has_value())
//        dataLogLn(shader.error());
//    XCTAssert(shader.has_value());
//    XCTAssert(!shader->directives().size());
//    XCTAssert(shader->structs().size() == 0);
//    XCTAssert(shader->globalVars().size() == 0);
//    XCTAssert(shader->functions().size() == 1);
//    WGSL::AST::FunctionDecl& func = shader->functions()[0];
//    XCTAssert(func.attributes().size() == 1);
//    XCTAssert(func.attributes()[0]->isStage());
//    XCTAssert(downcast<WGSL::AST::StageAttribute>(func.attributes()[0].get()).stage() == WGSL::AST::StageAttribute::Stage::Compute);
//    XCTAssert(func.name() == "main"_s);
//    XCTAssert(func.parameters().size() == 0);
//    XCTAssert(func.returnAttributes().isEmpty());
//    XCTAssert(func.maybeReturnType() == nullptr);
//    XCTAssert(func.body().statements().size() == 1);
//    XCTAssert(func.body().statements()[0]->isAssignment());
//    WGSL::AST::AssignmentStatement& stmt = downcast<WGSL::AST::AssignmentStatement>(func.body().statements()[0].get());
//    XCTAssert(stmt.maybeLhs());
//    XCTAssert(stmt.maybeLhs()->isStructureAccess());
//    WGSL::AST::StructureAccess& structAccess = downcast<WGSL::AST::StructureAccess>(*stmt.maybeLhs());
//    XCTAssert(structAccess.base()->isIdentifier());
//    WGSL::AST::IdentifierExpression base = downcast<WGSL::AST::IdentifierExpression>(structAccess.base().get());
//    XCTAssert(base.identifier() == "x"_s);
//    XCTAssert(structAccess.fieldName() == "a"_s);
//    XCTAssert(stmt.rhs().isInt32Literal());
//    WGSL::AST::Int32Literal& rhs = downcast<WGSL::AST::Int32Literal>(stmt.rhs());
//    XCTAssert(rhs.value() == 42);
}

- (void)testTrivialGraphicsShader {
//    auto shader = WGSL::parseLChar("@vertex\n"
//                                   "fn vertexShader(@location(0) x: vec4<f32>) -> @builtin(position) vec4<f32> {\n"
//                                   "    return x;\n"
//                                   "}\n\n"
//                                   "@fragment\n"
//                                   "fn fragmentShader() -> @location(0) vec4<f32> {\n"
//                                   "    return vec4<f32>(0.4, 0.4, 0.8, 1.0);\n"
//                                   "}"_s);
//    if (!shader.has_value())
//        dataLogLn(shader.error());
//    XCTAssert(shader.has_value());
//    XCTAssert(!shader->directives().size());
//    XCTAssert(shader->structs().size() == 0);
//    XCTAssert(shader->globalVars().size() == 0);
//    XCTAssert(shader->functions().size() == 2);
//
//    {
//        WGSL::AST::FunctionDecl& func = shader->functions()[0];
//        XCTAssert(func.attributes().size() == 1);
//        XCTAssert(func.attributes()[0]->isStage());
//        XCTAssert(downcast<WGSL::AST::StageAttribute>(func.attributes()[0].get()).stage() == WGSL::AST::StageAttribute::Stage::Vertex);
//        XCTAssert(func.name() == "vertexShader"_s);
//        XCTAssert(func.parameters().size() == 1);
//        XCTAssert(func.parameters()[0]->name() == "x"_s);
//        XCTAssert(func.parameters()[0]->attributes().size() == 1);
//        XCTAssert(func.parameters()[0]->attributes()[0]->isLocation());
//        XCTAssert(downcast<WGSL::AST::LocationAttribute>(func.parameters()[0]->attributes()[0].get()).location() == 0);
//        XCTAssert(func.parameters()[0]->type().isParameterized());
//        WGSL::AST::ParameterizedType& paramType = downcast<WGSL::AST::ParameterizedType>(func.parameters()[0]->type());
//        XCTAssert(paramType.base() == WGSL::AST::ParameterizedType::Base::Vec4);
//        XCTAssert(paramType.elementType().isNamed());
//        XCTAssert(downcast<WGSL::AST::NamedType>(paramType.elementType()).name() == "f32"_s);
//        XCTAssert(func.returnAttributes().size() == 1);
//        XCTAssert(func.returnAttributes()[0]->isBuiltin());
//        XCTAssert(downcast<WGSL::AST::BuiltinAttribute>(func.returnAttributes()[0].get()).name() == "position"_s);
//        XCTAssert(func.maybeReturnType());
//        XCTAssert(func.maybeReturnType()->isParameterized());
//        XCTAssert(func.body().statements().size() == 1);
//        XCTAssert(func.body().statements()[0]->isReturn());
//        WGSL::AST::ReturnStatement& stmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0].get());
//        XCTAssert(stmt.maybeExpression());
//        XCTAssert(stmt.maybeExpression()->isIdentifier());
//    }
//
//    {
//        WGSL::AST::FunctionDecl& func = shader->functions()[1];
//        XCTAssert(func.attributes().size() == 1);
//        XCTAssert(func.attributes()[0]->isStage());
//        XCTAssert(downcast<WGSL::AST::StageAttribute>(func.attributes()[0].get()).stage() == WGSL::AST::StageAttribute::Stage::Fragment);
//        XCTAssert(func.name() == "fragmentShader"_s);
//        XCTAssert(func.parameters().size() == 0);
//        XCTAssert(func.returnAttributes().size() == 1);
//        XCTAssert(func.returnAttributes()[0]->isLocation());
//        XCTAssert(downcast<WGSL::AST::LocationAttribute>(func.returnAttributes()[0].get()).location() == 0);
//        XCTAssert(func.maybeReturnType());
//        XCTAssert(func.maybeReturnType()->isParameterized());
//        XCTAssert(func.body().statements().size() == 1);
//        XCTAssert(func.body().statements()[0]->isReturn());
//        WGSL::AST::ReturnStatement& stmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0].get());
//        XCTAssert(stmt.maybeExpression());
//        XCTAssert(stmt.maybeExpression()->isTypeConversion());
//        WGSL::AST::TypeConversion& expr = downcast<WGSL::AST::TypeConversion>(*stmt.maybeExpression());
//        XCTAssert(expr.typeDecl()->isParameterized());
//        XCTAssert(expr.arguments().size() == 4);
//        XCTAssert(expr.arguments()[0]->isFloat32Literal());
//        XCTAssert(expr.arguments()[1]->isFloat32Literal());
//        XCTAssert(expr.arguments()[2]->isFloat32Literal());
//        XCTAssert(expr.arguments()[3]->isFloat32Literal());
//    }
}

@end
