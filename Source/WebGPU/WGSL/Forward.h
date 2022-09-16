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

class AbstractFloatLiteral;
class AbstractIntLiteral;
class ArrayAccess;
class ArrayType;
class AssignmentStatement;
class Attribute;
class BindingAttribute;
class BoolLiteral;
class BuiltinAttribute;
class CallableExpression;
class CompoundStatement;
class Decl;
class Expression;
class Float32Literal;
class FunctionDecl;
class GlobalDirective;
class GroupAttribute;
class IdentifierExpression;
class Int32Literal;
class LocationAttribute;
class NamedType;
class Parameter;
class ParameterizedType;
class ReturnStatement;
class ShaderModule;
class StageAttribute;
class Statement;
class StructDecl;
class StructMember;
class StructureAccess;
class TypeDecl;
class Uint32Literal;
class UnaryExpression;
class VariableDecl;
class VariableQualifier;
class VariableStatement;

} // namespace AST

} // namespace WGSL
