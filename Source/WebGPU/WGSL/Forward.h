/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

namespace WGSL {

namespace AST {

#pragma mark -
#pragma mark Shader Module
class ShaderModule;
class GlobalDirective;

#pragma mark -
#pragma mark Attribute
class Attribute;
class BindingAttribute;
class BuiltinAttribute;
class GroupAttribute;
class LocationAttribute;
class StageAttribute;

#pragma mark -
#pragma mark Declaration
class Declaration;
class FunctionDeclaration;
class StructDeclaration;
class StructMember;
class VariableDeclaration;
class VariableQualifier;

#pragma mark -
#pragma mark Expression
class Expression;
class BoolLiteral;
class Int32Literal;
class Uint32Literal;
class Float32Literal;
class AbstractIntLiteral;
class AbstractFloatLiteral;
class IdentifierExpression;
class ArrayAccess;
class StructureAccess;
class CallableExpression;
class UnaryExpression;

class Parameter;

#pragma mark -
#pragma mark Statement
class Statement;
class AssignmentStatement;
class CompoundStatement;
class ReturnStatement;
class VariableStatement;

#pragma mark -
#pragma mark Types
class TypeReference;
class ArrayTypeReference;
class NamedTypeReference;
class ParameterizedTypeReference;

} // namespace AST

} // namespace WGSL
