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
class Directive;

class Attribute;
class AlignAttribute;
class BindingAttribute;
class BuiltinAttribute;
class ConstAttribute;
class GroupAttribute;
class IdAttribute;
class InterpolateAttribute;
class InvariantAttribute;
class LocationAttribute;
class SizeAttribute;
class StageAttribute;
class WorkgroupSizeAttribute;

class Expression;
class AbstractFloatLiteral;
class AbstractIntegerLiteral;
class BinaryExpression;
class BitcastExpression;
class BoolLiteral;
class CallExpression;
class FieldAccessExpression;
class Float32Literal;
class IdentifierExpression;
class IdentityExpression;
class IndexAccessExpression;
class PointerDereferenceExpression;
class Signed32Literal;
class UnaryExpression;
class Unsigned32Literal;

class Function;

class Identifier;

class Statement;
class AssignmentStatement;
class BreakStatement;
class CompoundAssignmentStatement;
class CompoundStatement;
class ContinueStatement;
class DecrementIncrementStatement;
class DiscardStatement;
class ForStatement;
class IfStatement;
class LoopStatement;
class PhonyAssignmentStatement;
class ReturnStatement;
class StaticAssertStatement;
class SwitchStatement;
class VariableStatement;
class WhileStatement;

class Structure;
class StructureMember;

class TypeAlias;

class TypeName;
class ArrayTypeName;
class NamedTypeName;
class ParameterizedTypeName;
class StructTypeName;
class ReferenceTypeName;

class Value;
class ConstantValue;
class OverrideValue;
class LetValue;
class ParameterValue;

class Variable;
class VariableQualifier;

enum class AccessMode : uint8_t;
enum class ParameterRole : uint8_t;
enum class StorageClass : uint8_t;
enum class StructureRole : uint8_t;

} // namespace WGSL::AST
