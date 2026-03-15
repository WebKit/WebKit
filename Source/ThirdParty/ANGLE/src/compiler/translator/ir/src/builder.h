//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Bridge to IR builder in Rust.  If ANGLE_IR is disabled, all functions are no-op; otherwise it
// converts between AST-based enums and types to IR's types.  Once migration to IR is complete,
// ANGLE could directly use the IR types to avoid most of this sort of conversion.
//

#ifndef COMPILER_TRANSLATOR_IR_SRC_BUILDER_H_
#define COMPILER_TRANSLATOR_IR_SRC_BUILDER_H_

#include <GLSLANG/ShaderVars.h>

#include "common/PackedEnums.h"
#include "common/span.h"
#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/Common.h"
#include "compiler/translator/ImmutableString.h"
#include "compiler/translator/Operator_autogen.h"

#ifdef ANGLE_IR
#    include "compiler/translator/ir/src/builder.rs.h"
#endif

namespace sh
{
class TField;
class TSymbol;

class TType;

class TConstantUnion;

namespace ir
{
enum class DeclarationSource
{
    // The variable be declared to IR is an internal built-in variable
    Internal,
    // The variable be declared to IR is declared by the shader, including when a built-in is
    // redeclared.
    Shader,
};

#ifdef ANGLE_IR
using IR         = rust::Box<ffi::IR>;
using TypeId     = ffi::TypeId;
using ConstantId = ffi::ConstantId;
using VariableId = ffi::VariableId;
using FunctionId = ffi::FunctionId;

static constexpr TypeId kInvalidTypeId = TypeId{0xFFFF'FFFF};
inline bool IsTypeIdValid(TypeId id)
{
    return id.id != kInvalidTypeId.id;
}

static constexpr VariableId kInvalidVariableId = VariableId{0xFFFF'FFFF};
inline bool IsVariableIdValid(VariableId id)
{
    return id.id != kInvalidVariableId.id;
}

class Builder
{
  public:
    Builder(gl::ShaderType shaderType, const ShCompileOptions &options);
    static IR destroy(Builder &&builder);

    void onError() { mHasError = true; }

    TypeId getBasicTypeId(TBasicType basicType, uint32_t primarySize, uint32_t secondarySize);
    TypeId getStructTypeId(const ImmutableString &name,
                           const angle::Span<const TField *const> &fields,
                           const angle::Span<TypeId> &fieldTypeIds,
                           bool isInterfaceBlock,
                           bool isBuiltIn,
                           bool isAtGlobalScope);
    TypeId getArrayTypeId(TypeId elementTypeId, const angle::Span<const unsigned int> &arraySizes);

    void setEarlyFragmentTests(bool value);
    void setAdvancedBlendEquations(uint32_t value);
    void setTcsVertices(uint32_t value);
    void setTesPrimitive(TLayoutTessEvaluationType value);
    void setTesVertexSpacing(TLayoutTessEvaluationType value);
    void setTesOrdering(TLayoutTessEvaluationType value);
    void setTesPointMode(TLayoutTessEvaluationType value);
    void setGsPrimitiveIn(TLayoutPrimitiveType value);
    void setGsPrimitiveOut(TLayoutPrimitiveType value);
    void setGsInvocations(uint32_t value);
    void setGsMaxVertices(uint32_t value);
    TypeId sizeUnsizedArrayType(TypeId typeId, uint32_t arraySize);

    VariableId declareInterfaceVariable(const ImmutableString &name,
                                        TypeId typeId,
                                        const TType &type,
                                        DeclarationSource source);
    VariableId declareTempVariable(const ImmutableString &name, TypeId typeId, const TType &type);
    void markVariableInvariant(VariableId id);
    void markVariablePrecise(VariableId id);
    void initialize(VariableId id);

    FunctionId newFunction(const ImmutableString &name,
                           const angle::Span<VariableId> &params,
                           const angle::Span<TQualifier> &paramDirections,
                           TypeId returnTypeId,
                           const TType &returnType);
    void updateFunctionParamNames(FunctionId id,
                                  const angle::Span<ImmutableString> &paramNames,
                                  const angle::Span<VariableId> &paramIdsOut);
    VariableId declareFunctionParam(const ImmutableString &name,
                                    TypeId typeId,
                                    const TType &type,
                                    TQualifier direction);
    void beginFunction(FunctionId id);
    void endFunction();

    void beginIfTrueBlock();
    void endIfTrueBlock();
    void beginIfFalseBlock();
    void endIfFalseBlock();
    void endIf();

    void beginTernaryTrueExpression();
    void endTernaryTrueExpression(TBasicType basicType);
    void beginTernaryFalseExpression();
    void endTernaryFalseExpression(TBasicType basicType);
    void endTernary(TBasicType basicType);

    void beginShortCircuitOr();
    void endShortCircuitOr();
    void beginShortCircuitAnd();
    void endShortCircuitAnd();

    void beginLoopCondition();
    void endLoopCondition();
    void endLoopContinue();
    void endLoop();
    void beginDoLoop();
    void beginDoLoopCondition();
    void endDoLoop();

    void beginSwitch();
    void beginCase();
    void beginDefault();
    void endSwitch();

    void branchDiscard();
    void branchReturn();
    void branchReturnValue();
    void branchBreak();
    void branchContinue();

    uint32_t popArraySize();
    void endStatementWithValue();
    void pushConstantFloat(float value);
    void pushConstantInt(int32_t value);
    void pushConstantUint(uint32_t value);
    void pushConstantBool(bool value);
    void pushConstantYuvCscStandard(TYuvCscStandardEXT value);
    void pushVariable(VariableId id);
    void callFunction(FunctionId id);
    void vectorComponent(uint32_t component);
    void vectorComponentMulti(const angle::Span<uint32_t> &components);
    void index();
    void structField(uint32_t fieldIndex);
    void construct(TypeId typeId, size_t argCount);
    void onGlClipDistanceSized(VariableId id, uint32_t length);
    void onGlCullDistanceSized(VariableId id, uint32_t length);
    void arrayLength();

    // Everything else that has a TOperator.
    void builtIn(TOperator op, size_t argCount);

  private:
    rust::Box<ffi::BuilderWrapper> mBuilder;
    // If the shader fails validation, stop generating the IR to avoid crashing on unexpected input.
    // This is the simpler alternative to replacing existing elements with fake ones that would have
    // passed validation.
    bool mHasError = false;
};

#else   // ANGLE_IR
using IR         = void *;
using TypeId     = uint8_t;
using ConstantId = uint8_t;
using VariableId = uint8_t;
using FunctionId = uint8_t;

static constexpr TypeId kInvalidTypeId = 0;
inline bool IsTypeIdValid(TypeId id)
{
    return true;
}

static constexpr VariableId kInvalidVariableId = 0;
inline bool IsVariableIdValid(VariableId id)
{
    return true;
}

class Builder
{
  public:
    Builder(gl::ShaderType shaderType, const ShCompileOptions &options) {}
    static IR destroy(Builder &&builder) { return nullptr; }

    void onError() {}

    TypeId getBasicTypeId(TBasicType basicType, uint32_t primarySize, uint32_t secondarySize)
    {
        return 0;
    }
    TypeId getStructTypeId(const ImmutableString &name,
                           const angle::Span<const TField *const> &fields,
                           const angle::Span<TypeId> &fieldTypeIds,
                           bool isInterfaceBlock,
                           bool isBuiltIn,
                           bool isAtGlobalScope)
    {
        return 0;
    }
    TypeId getArrayTypeId(TypeId elementTypeId, const angle::Span<const unsigned int> &arraySizes)
    {
        return 0;
    }

    void setEarlyFragmentTests(bool value) {}
    void setAdvancedBlendEquations(uint32_t value) {}
    void setTcsVertices(uint32_t value) {}
    void setTesPrimitive(TLayoutTessEvaluationType value) {}
    void setTesVertexSpacing(TLayoutTessEvaluationType value) {}
    void setTesOrdering(TLayoutTessEvaluationType value) {}
    void setTesPointMode(TLayoutTessEvaluationType value) {}
    void setGsPrimitiveIn(TLayoutPrimitiveType value) {}
    void setGsPrimitiveOut(TLayoutPrimitiveType value) {}
    void setGsInvocations(uint32_t value) {}
    void setGsMaxVertices(uint32_t value) {}
    TypeId sizeUnsizedArrayType(TypeId typeId, uint32_t arraySize) { return typeId; }

    VariableId declareInterfaceVariable(const ImmutableString &name,
                                        TypeId typeId,
                                        const TType &type,
                                        DeclarationSource source)
    {
        return 0;
    }
    VariableId declareTempVariable(const ImmutableString &name, TypeId typeId, const TType &type)
    {
        return 0;
    }
    void markVariableInvariant(VariableId id) {}
    void markVariablePrecise(VariableId id) {}
    void initialize(VariableId id) {}

    FunctionId newFunction(const ImmutableString &name,
                           const angle::Span<VariableId> &params,
                           const angle::Span<TQualifier> &paramDirections,
                           TypeId returnTypeId,
                           const TType &returnType)
    {
        return 0;
    }
    void updateFunctionParamNames(FunctionId id,
                                  const angle::Span<ImmutableString> &paramNames,
                                  const angle::Span<VariableId> &paramIdsOut)
    {}
    VariableId declareFunctionParam(const ImmutableString &name,
                                    TypeId typeId,
                                    const TType &type,
                                    TQualifier direction)
    {
        return 0;
    }
    void beginFunction(FunctionId id) {}
    void endFunction() {}

    void beginIfTrueBlock() {}
    void endIfTrueBlock() {}
    void beginIfFalseBlock() {}
    void endIfFalseBlock() {}
    void endIf() {}

    void beginTernaryTrueExpression() {}
    void endTernaryTrueExpression(TBasicType) {}
    void beginTernaryFalseExpression() {}
    void endTernaryFalseExpression(TBasicType) {}
    void endTernary(TBasicType) {}

    void beginShortCircuitOr() {}
    void endShortCircuitOr() {}
    void beginShortCircuitAnd() {}
    void endShortCircuitAnd() {}

    void beginLoopCondition() {}
    void endLoopCondition() {}
    void endLoopContinue() {}
    void endLoop() {}
    void beginDoLoop() {}
    void beginDoLoopCondition() {}
    void endDoLoop() {}

    void beginSwitch() {}
    void beginCase() {}
    void beginDefault() {}
    void endSwitch() {}

    void branchDiscard() {}
    void branchReturn() {}
    void branchReturnValue() {}
    void branchBreak() {}
    void branchContinue() {}

    uint32_t popArraySize() { return 0; }
    void endStatementWithValue() {}
    void pushConstantFloat(float value) {}
    void pushConstantInt(int32_t value) {}
    void pushConstantUint(uint32_t value) {}
    void pushConstantBool(bool value) {}
    void pushConstantYuvCscStandard(TYuvCscStandardEXT value) {}
    void pushVariable(VariableId id) {}
    void callFunction(FunctionId id) {}
    void vectorComponent(uint32_t component) {}
    void vectorComponentMulti(const angle::Span<uint32_t> &components) {}
    void index() {}
    void structField(uint32_t fieldIndex) {}
    void construct(TypeId typeId, size_t argCount) {}
    void onGlClipDistanceSized(VariableId id, uint32_t length) {}
    void onGlCullDistanceSized(VariableId id, uint32_t length) {}
    void arrayLength() {}
    void builtIn(TOperator op, size_t argCount) {}
};
#endif  // ANGLE_IR
}  // namespace ir
}  // namespace sh

#ifdef ANGLE_IR
inline bool operator==(sh::ir::TypeId one, sh::ir::TypeId other)
{
    return one.id == other.id;
}

inline bool operator!=(sh::ir::TypeId one, sh::ir::TypeId other)
{
    return !(one == other);
}
#endif

#endif  // COMPILER_TRANSLATOR_IR_SRC_BUILDER_H_
