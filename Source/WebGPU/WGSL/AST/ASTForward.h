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

namespace WGSL::AST {

class ShaderModule;
class GlobalDirective;

class Attribute;
class BindingAttribute;
class BuiltinAttribute;
class GroupAttribute;
class LocationAttribute;
class StageAttribute;

class Decl;
class FunctionDecl;
class StructDecl;
class VariableDecl;

class Expression;
class AbstractFloatLiteral;
class AbstractIntLiteral;
class ArrayAccess;
class BoolLiteral;
class CallableExpression;
class Float32Literal;
class IdentifierExpression;
class Int32Literal;
class StructureAccess;
class Uint32Literal;
class UnaryExpression;

class Statement;
class AssignmentStatement;
class CompoundStatement;
class ReturnStatement;
class VariableStatement;

class TypeDecl;
class ArrayType;
class NamedType;
class ParameterizedType;

class Parameter;
class StructMember;
class VariableQualifier;

enum class AccessMode : uint8_t;
enum class StorageClass : uint8_t;

} // namespace WGSL::AST
