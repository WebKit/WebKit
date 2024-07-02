/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "AttributeValidator.h"

#include "AST.h"
#include "ASTVisitor.h"
#include "Constraints.h"
#include "WGSLShaderModule.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/text/MakeString.h>

namespace WGSL {

enum class Direction : uint8_t {
    Input,
    Output,
};

class AttributeValidator : public AST::Visitor {
public:
    AttributeValidator(ShaderModule&);

    std::optional<FailedCheck> validate();
    std::optional<FailedCheck> validateIO();

    void visit(AST::Function&) override;
    void visit(AST::Parameter&) override;
    void visit(AST::Variable&) override;
    void visit(AST::Structure&) override;
    void visit(AST::StructureMember&) override;
    void visit(AST::CompoundStatement&) override;

private:
    bool parseBuiltin(AST::Function*, std::optional<Builtin>&, AST::Attribute&);
    bool parseInterpolate(std::optional<AST::Interpolation>&, AST::Attribute&);
    bool parseInvariant(bool&, AST::Attribute&);
    bool parseLocation(AST::Function*, std::optional<unsigned>&, AST::Attribute&, const Type*);

    void validateInterpolation(const SourceSpan&, const std::optional<AST::Interpolation>&, const std::optional<unsigned>&);
    void validateInvariant(const SourceSpan&, const std::optional<Builtin>&, bool);

    using Builtins = HashSet<Builtin, WTF::IntHash<Builtin>, WTF::StrongEnumHashTraits<Builtin>>;
    using Locations = HashSet<uint32_t, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    void validateBuiltinIO(const SourceSpan&, const Type*, ShaderStage, Builtin, Direction, Builtins&);
    void validateLocationIO(const SourceSpan&, const Type*, ShaderStage, unsigned, Locations&);
    void validateStructIO(ShaderStage, const Types::Struct&, Direction, Builtins&, Locations&);

    template<typename T>
    void update(const SourceSpan&, std::optional<T>&, const T&);
    void set(const SourceSpan&, bool&);

    template<typename... Arguments>
    void error(const SourceSpan&, Arguments&&...);

    AST::Function* m_currentFunction { nullptr };
    ShaderModule& m_shaderModule;
    Vector<Error> m_errors;
    bool m_hasSizeOrAlignmentAttributes { false };
};

AttributeValidator::AttributeValidator(ShaderModule& shaderModule)
    : m_shaderModule(shaderModule)
{
}

std::optional<FailedCheck> AttributeValidator::validate()
{
    AST::Visitor::visit(m_shaderModule);

    if (m_errors.isEmpty())
        return std::nullopt;
    return FailedCheck { WTFMove(m_errors), { } };
}

void AttributeValidator::visit(AST::Function& function)
{
    for (auto& attribute : function.attributes()) {
        if (is<AST::MustUseAttribute>(attribute)) {
            if (!function.maybeReturnType())
                error(attribute.span(), "@must_use can only be applied to functions that return a value"_s);
            set(attribute.span(), function.m_mustUse);
            continue;
        }

        if (auto* stageAttribute = dynamicDowncast<AST::StageAttribute>(attribute)) {
            update(attribute.span(), function.m_stage, stageAttribute->stage());
            continue;
        }

        if (auto* workgroupSizeAttribute = dynamicDowncast<AST::WorkgroupSizeAttribute>(attribute)) {
            auto& workgroupSize = workgroupSizeAttribute->workgroupSize();
            const auto& check = [&](AST::Expression* dimension) {
                if (!dimension)
                    return;
                auto value = dimension->constantValue();
                if (!value.has_value())
                    return;
                if (value->integerValue() < 1)
                    error(dimension->span(), "@workgroup_size argument must be at least 1"_s);
            };
            check(workgroupSize.x);
            check(workgroupSize.y);
            check(workgroupSize.z);
            update(attribute.span(), function.m_workgroupSize, workgroupSize);
            continue;
        }

        error(attribute.span(), "invalid attribute for function declaration"_s);
    }
    if (function.workgroupSize().has_value() && (!function.stage().has_value() || *function.stage() != ShaderStage::Compute))
        error(function.span(), "@workgroup_size must only be applied to compute shader entry point function"_s);

    for (auto& attribute : function.returnAttributes()) {
        if (parseBuiltin(&function, function.m_returnTypeBuiltin, attribute))
            continue;

        if (parseInterpolate(function.m_returnTypeInterpolation, attribute))
            continue;

        if (parseInvariant(function.m_returnTypeInvariant, attribute))
            continue;

        if (parseLocation(&function, function.m_returnTypeLocation, attribute, function.maybeReturnType()->inferredType()))
            continue;

        error(attribute.span(), "invalid attribute for function return type"_s);
    }

    if (function.maybeReturnType()) {
        validateInterpolation(function.maybeReturnType()->span(), function.returnTypeInterpolation(), function.returnTypeLocation());
        validateInvariant(function.maybeReturnType()->span(), function.returnTypeBuiltin(), function.returnTypeInvariant());
    }

    m_currentFunction = &function;
    AST::Visitor::visit(function);
    m_currentFunction = nullptr;
}

void AttributeValidator::visit(AST::Parameter& parameter)
{
    for (auto& attribute : parameter.attributes()) {
        if (parseBuiltin(m_currentFunction, parameter.m_builtin, attribute))
            continue;

        if (parseInterpolate(parameter.m_interpolation, attribute))
            continue;

        if (parseInvariant(parameter.m_invariant, attribute))
            continue;

        if (parseLocation(m_currentFunction, parameter.m_location, attribute, parameter.typeName().inferredType()))
            continue;

        error(attribute.span(), "invalid attribute for function parameter"_s);
    }

    validateInterpolation(parameter.span(), parameter.interpolation(), parameter.location());
    validateInvariant(parameter.span(), parameter.builtin(), parameter.invariant());

    AST::Visitor::visit(parameter);
}

void AttributeValidator::visit(AST::Variable& variable)
{
    bool isResource = [&]() -> bool {
        auto addressSpace = variable.addressSpace();
        if (!addressSpace.has_value())
            return false;
        switch (*addressSpace) {
        case AddressSpace::Handle:
        case AddressSpace::Storage:
        case AddressSpace::Uniform:
            return true;
        case AddressSpace::Function:
        case AddressSpace::Private:
        case AddressSpace::Workgroup:
            return false;
        }
    }();

    for (auto& attribute : variable.attributes()) {
        if (auto* bindingAttribute = dynamicDowncast<AST::BindingAttribute>(attribute)) {
            if (!isResource)
                error(attribute.span(), "@binding attribute must only be applied to resource variables"_s);
            // https://gpuweb.github.io/cts/standalone/?q=webgpu:shader,validation,parse,attribute:expressions:value=%22override%22;attribute=%22binding%22
            auto& constantValue = bindingAttribute->binding().constantValue();
            if (!constantValue) {
                error(attribute.span(), "@binding attribute must only be applied to resource variables"_s);
                continue;
            }

            auto bindingValue = constantValue->integerValue();
            if (bindingValue < 0)
                error(attribute.span(), "@binding value must be non-negative"_s);
            else
                update(attribute.span(), variable.m_binding, static_cast<unsigned>(bindingValue));
            continue;
        }

        if (auto* groupAttribute = dynamicDowncast<AST::GroupAttribute>(attribute)) {
            if (!isResource)
                error(attribute.span(), "@group attribute must only be applied to resource variables"_s);
            // https://gpuweb.github.io/cts/standalone/?q=webgpu:shader,validation,parse,attribute:expressions:value=%22override%22;attribute=%22binding%22
            auto& constantValue = groupAttribute->group().constantValue();
            if (!constantValue) {
                error(attribute.span(), "@group attribute must only be applied to resource variables"_s);
                continue;
            }
            auto groupValue = constantValue->integerValue();
            if (groupValue < 0)
                error(attribute.span(), "@group value must be non-negative"_s);
            else
                update(attribute.span(), variable.m_group, static_cast<unsigned>(groupValue));
            continue;
        }

        if (auto* idAttribute = dynamicDowncast<AST::IdAttribute>(attribute)) {
            auto& idExpression = idAttribute->value();
            if (variable.flavor() != AST::VariableFlavor::Override)
                error(attribute.span(), "@id attribute must only be applied to override variables"_s);
            else
                RELEASE_ASSERT(satisfies(variable.storeType(), Constraints::Scalar));

            // https://gpuweb.github.io/cts/standalone/?q=webgpu:shader,validation,parse,attribute:expressions:value=%22override%22;attribute=%22binding%22
            auto& constantValue = idExpression.constantValue();
            RELEASE_ASSERT(constantValue);

            auto idValue = constantValue->integerValue();
            if (idValue < 0)
                error(attribute.span(), "@id value must be non-negative"_s);
            else if (idValue > std::numeric_limits<uint16_t>::max())
                error(attribute.span(), "@id value must be between 0 and 65535"_s);
            else {
                auto uintIdValue = static_cast<unsigned>(idValue);
                if (m_shaderModule.containsOverride(uintIdValue))
                    error(attribute.span(), "@id value must be unique"_s);
                else {
                    update(attribute.span(), variable.m_id, uintIdValue);
                    m_shaderModule.addOverride(uintIdValue);
                }
            }
            continue;
        }

        error(attribute.span(), "invalid attribute for variable declaration"_s);
    }

    if (isResource && (!variable.m_group || !variable.m_binding))
        error(variable.span(), "resource variables require @group and @binding attributes"_s);
}

void AttributeValidator::visit(AST::Structure& structure)
{
    AST::Visitor::visit(structure);

    // Bail as we will stop the compilation after this pass, so the computed
    // properties of the struct will never be read, and the size and alignment
    // for the struct members might be invalid.
    if (m_errors.size())
        return;

    structure.m_hasSizeOrAlignmentAttributes = std::exchange(m_hasSizeOrAlignmentAttributes, false);

    CheckedUint32 previousSize = 0;
    unsigned alignment = 0;
    CheckedUint32 size = 0;
    AST::StructureMember* previousMember = nullptr;
    for (auto& member : structure.members()) {
        auto* type = member.type().inferredType();
        auto fieldAlignment = member.m_alignment;
        if (!fieldAlignment) {
            fieldAlignment = type->alignment();
            member.m_alignment = fieldAlignment;
        }

        auto typeSize = type->size();
        auto fieldSize = member.m_size;
        if (!fieldSize) {
            fieldSize = typeSize;
            member.m_size = fieldSize;
        }

        unsigned currentSize = UNLIKELY(size.hasOverflowed()) ? std::numeric_limits<unsigned>::max() : size.value();
        unsigned offset;
        if (UNLIKELY(size.hasOverflowed()))
            offset = currentSize;
        else {
            CheckedUint32 checkedOffset = WTF::roundUpToMultipleOf(*fieldAlignment, static_cast<uint64_t>(currentSize));
            offset = UNLIKELY(checkedOffset.hasOverflowed()) ? std::numeric_limits<unsigned>::max() : checkedOffset.value();
        }

        member.m_offset = offset;

        alignment = std::max(alignment, *fieldAlignment);
        size = offset;
        size += *fieldSize;
        if (UNLIKELY(size.hasOverflowed()))
            size = currentSize;

        if (previousMember)
            previousMember->m_padding = offset - previousSize;

        previousMember = &member;

        previousSize = offset;
        previousSize += typeSize;
        if (UNLIKELY(previousSize.hasOverflowed()))
            previousSize = currentSize;
    }
    unsigned finalSize;
    if (UNLIKELY(size.hasOverflowed()))
        finalSize = std::numeric_limits<unsigned>::max();
    else {
        CheckedUint32 checkedFinalSize = WTF::roundUpToMultipleOf(alignment, static_cast<uint64_t>(size.value()));
        finalSize = UNLIKELY(checkedFinalSize.hasOverflowed()) ? std::numeric_limits<unsigned>::max() : checkedFinalSize.value();
    }
    previousMember->m_padding = finalSize - previousSize;
    structure.m_alignment = alignment;
    structure.m_size = finalSize;
}

void AttributeValidator::visit(AST::StructureMember& member)
{
    for (auto& attribute : member.attributes()) {
        if (parseBuiltin(nullptr, member.m_builtin, attribute))
            continue;

        if (parseInterpolate(member.m_interpolation, attribute))
            continue;

        if (parseInvariant(member.m_invariant, attribute))
            continue;

        if (parseLocation(nullptr, member.m_location, attribute, member.type().inferredType()))
            continue;

        if (auto* sizeAttribute = dynamicDowncast<AST::SizeAttribute>(attribute)) {
            // FIXME: check that the member type must have creation-fixed footprint.
            m_hasSizeOrAlignmentAttributes = true;
            // https://gpuweb.github.io/cts/standalone/?q=webgpu:shader,validation,parse,attribute:expressions:value=%22override%22;*
            auto& constantValue = sizeAttribute->size().constantValue();
            if (!constantValue) {
                error(attribute.span(), "@size constant value is not found"_s);
                continue;
            }
            auto sizeValue = constantValue->integerValue();
            if (sizeValue < 0)
                error(attribute.span(), "@size value must be non-negative"_s);
            else if (m_errors.isEmpty() && sizeValue < member.type().inferredType()->size()) {
                // We can't call Type::size() if we already have errors, as we might
                // try to read the size of a struct, which we will not have computed
                // if we already encountered errors
                error(attribute.span(), "@size value must be at least the byte-size of the type of the member"_s);
            }
            update(attribute.span(), member.m_size, static_cast<unsigned>(sizeValue));
            continue;
        }

        if (auto* alignAttribute = dynamicDowncast<AST::AlignAttribute>(attribute)) {
            m_hasSizeOrAlignmentAttributes = true;
            // https://gpuweb.github.io/cts/standalone/?q=webgpu:shader,validation,parse,attribute:expressions:value=%22override%22;attribute=%22align%22
            auto constantValue = alignAttribute->alignment().constantValue();
            if (!constantValue) {
                error(attribute.span(), "@align constant value does not exist"_s);
                continue;
            }
            auto alignmentValue = constantValue->integerValue();
            auto isPowerOf2 = !(alignmentValue & (alignmentValue - 1));
            if (alignmentValue < 1)
                error(attribute.span(), "@align value must be positive"_s);
            else if (!isPowerOf2)
                error(attribute.span(), "@align value must be a power of two"_s);
            // FIXME: validate that alignment is a multiple of RequiredAlignOf(T,C)
            update(attribute.span(), member.m_alignment, static_cast<unsigned>(alignmentValue));
            continue;
        }

        error(attribute.span(), "invalid attribute for structure member"_s);
    }

    validateInterpolation(member.span(), member.interpolation(), member.location());
    validateInvariant(member.span(), member.builtin(), member.invariant());

    AST::Visitor::visit(member);
}

void AttributeValidator::visit(AST::CompoundStatement& statement)
{
    for (auto& attribute : statement.attributes()) {
        if (!is<AST::DiagnosticAttribute>(attribute))
            error(attribute.span(), "invalid attribute for compound statement"_s);
    }
    AST::Visitor::visit(statement);
}

bool AttributeValidator::parseBuiltin(AST::Function* function, std::optional<Builtin>& builtin, AST::Attribute& attribute)
{
    auto* builtinAttribute = dynamicDowncast<AST::BuiltinAttribute>(attribute);
    if (!builtinAttribute)
        return false;
    if (function && !function->stage())
        error(attribute.span(), "@builtin is not valid for non-entry point function types"_s);
    update(attribute.span(), builtin, builtinAttribute->builtin());
    return true;
}

bool AttributeValidator::parseInterpolate(std::optional<AST::Interpolation>& interpolation, AST::Attribute& attribute)
{
    auto* interpolateAttribute = dynamicDowncast<AST::InterpolateAttribute>(attribute);
    if (!interpolateAttribute)
        return false;
    update(attribute.span(), interpolation, interpolateAttribute->interpolation());
    return true;
}

bool AttributeValidator::parseInvariant(bool& invariant, AST::Attribute& attribute)
{
    if (!is<AST::InvariantAttribute>(attribute))
        return false;
    set(attribute.span(), invariant);
    return true;
}

bool AttributeValidator::parseLocation(AST::Function* function, std::optional<unsigned>& location, AST::Attribute& attribute, const Type* declarationType)
{
    auto* locationAttribute = dynamicDowncast<AST::LocationAttribute>(attribute);
    if (!locationAttribute)
        return false;
    if (function && !function->stage())
        error(attribute.span(), "@location is not valid for non-entry point function types"_s);
    else if (function && *function->stage() == ShaderStage::Compute)
        error(attribute.span(), "@location may not be used in the compute shader stage"_s);

    bool isNumeric = satisfies(declarationType, Constraints::Number);
    bool isNumericVector = false;
    if (!isNumeric) {
        if (auto* vectorType = std::get_if<Types::Vector>(declarationType))
            isNumericVector = satisfies(vectorType->element, Constraints::Number);
    }
    if (!isNumeric && !isNumericVector)
        error(attribute.span(), "@location must only be applied to declarations of numeric scalar or numeric vector type"_s);

    auto& constantValue = locationAttribute->location().constantValue();
    // https://gpuweb.github.io/cts/standalone/?q=webgpu:shader,validation,parse,attribute:expressions:value=%22override%22;*
    if (!constantValue) {
        error(attribute.span(), "@location constant value is missing"_s);
        return false;
    }
    auto locationValue = constantValue->integerValue();
    if (locationValue < 0)
        error(attribute.span(), "@location value must be non-negative"_s);
    else
        update(attribute.span(), location, static_cast<unsigned>(locationValue));
    return true;
}

void AttributeValidator::validateInterpolation(const SourceSpan& span, const std::optional<AST::Interpolation>& interpolation, const std::optional<unsigned>& location)
{
    if (interpolation && !location)
        error(span, "@interpolate is only allowed on declarations that have a @location attribute"_s);
}

void AttributeValidator::validateInvariant(const SourceSpan& span, const std::optional<Builtin>& builtin, bool invariant)
{
    if (invariant && (!builtin || *builtin != Builtin::Position))
        error(span, "@invariant is only allowed on declarations that have a @builtin(position) attribute"_s);
}


template<typename T>
void AttributeValidator::update(const SourceSpan& span, std::optional<T>& destination, const T& source)
{
    if (destination.has_value())
        error(span, "duplicate attribute"_s);
    else
        destination = source;
}

void AttributeValidator::set(const SourceSpan& span, bool& destination)
{
    if (destination)
        error(span, "duplicate attribute"_s);
    else
        destination = true;
}

template<typename... Arguments>
void AttributeValidator::error(const SourceSpan& span, Arguments&&... arguments)
{
    m_errors.append({ makeString(std::forward<Arguments>(arguments)...), span });
}

std::optional<FailedCheck> AttributeValidator::validateIO()
{
    for (auto& entryPoint : m_shaderModule.callGraph().entrypoints()) {
        auto& function = entryPoint.function;
        Builtins builtins;
        Locations locations;
        for (auto& parameter : function.parameters()) {
            const auto& span = parameter.span();
            const auto* type = parameter.typeName().inferredType();

            if (auto builtin = parameter.builtin()) {
                validateBuiltinIO(span, type, entryPoint.stage, *builtin, Direction::Input, builtins);
                continue;
            }

            if (auto location = parameter.location()) {
                validateLocationIO(span, type, entryPoint.stage, *location, locations);
                continue;
            }

            if (auto* structType = std::get_if<Types::Struct>(type)) {
                validateStructIO(entryPoint.stage, *structType, Direction::Input, builtins, locations);
                continue;
            }

            error(span, "missing entry point IO attribute on parameter"_s);
        }

        if (!function.maybeReturnType()) {
            if (entryPoint.stage == ShaderStage::Vertex)
                error(function.span(), "a vertex shader must include the 'position' builtin in its return type"_s);
            continue;
        }

        builtins.clear();
        locations.clear();
        const auto& span = function.maybeReturnType()->span();
        const auto* type = function.maybeReturnType()->inferredType();

        if (auto builtin = function.returnTypeBuiltin())
            validateBuiltinIO(span, type, entryPoint.stage, *builtin, Direction::Output, builtins);
        else if (auto location = function.returnTypeLocation())
            validateLocationIO(span, type, entryPoint.stage, *location, locations);
        else if (auto* structType = std::get_if<Types::Struct>(type))
            validateStructIO(entryPoint.stage, *structType, Direction::Output, builtins, locations);
        else {
            error(span, "missing entry point IO attribute on return type"_s);
            continue;
        }

        if (entryPoint.stage == ShaderStage::Vertex && !builtins.contains(Builtin::Position))
            error(span, "a vertex shader must include the 'position' builtin in its return type"_s);
    }

    if (m_errors.isEmpty())
        return std::nullopt;
    return FailedCheck { WTFMove(m_errors), { } };
}

void AttributeValidator::validateBuiltinIO(const SourceSpan& span, const Type* type, ShaderStage stage, Builtin builtin, Direction direction, Builtins& builtins)
{


#define TYPE_CHECK(__type) \
    type != m_shaderModule.types().__type##Type(), *m_shaderModule.types().__type##Type()

#define VEC_CHECK(__count, __elementType) \
    auto* vector = std::get_if<Types::Vector>(type); !vector || vector->size != __count || vector->element != m_shaderModule.types().__elementType##Type(), "vec" #__count "<" #__elementType ">"_s

#define CASE_(__case, __typeCheck, __type) \
case Builtin::__case: \
    if (__typeCheck)  { \
        error(span, "store type of @builtin("_s, toString(Builtin::__case), ") must be '"_s, __type, '\''); \
        return; \
    } \

#define CASE(__case, __typeCheck, __stage, __direction) \
    CASE_(__case, __typeCheck); \
    if (stage != ShaderStage::__stage || direction != Direction::__direction) { \
        error(span, "@builtin("_s, toString(Builtin::__case), ") cannot be used for "_s, toString(stage), " shader "_s, direction == Direction::Input ? "input"_s : "output"_s); \
        return; \
    } \
    break;

#define CASE2(__case, __typeCheck, __stage1, __direction1, __stage2, __direction2) \
    CASE_(__case, __typeCheck); \
    if ((stage != ShaderStage::__stage1 || direction != Direction::__direction1) && (stage != ShaderStage::__stage2 || direction != Direction::__direction2)) { \
        error(span, "@builtin("_s, toString(Builtin::__case), ") cannot be used for "_s, toString(stage), " shader "_s, direction == Direction::Input ? "input"_s : "output"_s); \
        return; \
    } \
    break;

    switch (builtin) {
        CASE(FragDepth, TYPE_CHECK(f32), Fragment, Output)
        CASE(FrontFacing, TYPE_CHECK(bool), Fragment, Input)
        CASE(GlobalInvocationId, VEC_CHECK(3, u32), Compute, Input)
        CASE(InstanceIndex, TYPE_CHECK(u32), Vertex, Input)
        CASE(LocalInvocationId, VEC_CHECK(3, u32), Compute, Input)
        CASE(LocalInvocationIndex, TYPE_CHECK(u32), Compute, Input)
        CASE(NumWorkgroups, VEC_CHECK(3, u32), Compute, Input)
        CASE(SampleIndex, TYPE_CHECK(u32), Fragment, Input)
        CASE(VertexIndex, TYPE_CHECK(u32), Vertex, Input)
        CASE(WorkgroupId, VEC_CHECK(3, u32), Compute, Input)
        CASE2(SampleMask, TYPE_CHECK(u32), Fragment, Input, Fragment, Output)
        CASE2(Position, VEC_CHECK(4, f32), Vertex, Output, Fragment, Input)
    }

    auto result = builtins.add(builtin);
    if (!result.isNewEntry)
        error(span, "@builtin("_s, toString(builtin), ") appears multiple times as pipeline input"_s);
}

void AttributeValidator::validateLocationIO(const SourceSpan& span, const Type* type, ShaderStage stage, unsigned location, Locations& locations)
{
    if (stage == ShaderStage::Compute) {
        error(span, "@location cannot be used by compute shaders"_s);
        return;
    }

    if (!satisfies(type, Constraints::Number)) {
        auto* vector = std::get_if<Types::Vector>(type);
        if (!vector || !satisfies(vector->element, Constraints::Number)) {
            error(span, "cannot apply @location to declaration of type '"_s, *type, '\'');
            return;
        }
    }

    auto result = locations.add(location);
    if (!result.isNewEntry)
        error(span, "@location("_s, location, ") appears multiple times"_s);
}

void AttributeValidator::validateStructIO(ShaderStage stage, const Types::Struct& structType, Direction direction, Builtins& builtins, Locations& locations)
{
    for (auto& member : structType.structure.members()) {
        const auto& span = member.span();
        const auto* type = member.type().inferredType();

        if (auto builtin = member.builtin()) {
            validateBuiltinIO(span, type, stage, *builtin, direction, builtins);
            continue;
        }

        if (auto location = member.location()) {
            validateLocationIO(span, type, stage, *location, locations);
            continue;
        }

        if (auto* structType = std::get_if<Types::Struct>(member.type().inferredType())) {
            error(span, "nested structures cannot be used for entry point IO"_s);
            continue;
        }

        error(span, "missing entry point IO attribute"_s);
    }
}

std::optional<FailedCheck> validateAttributes(ShaderModule& shaderModule)
{
    return AttributeValidator(shaderModule).validate();
}

std::optional<FailedCheck> validateIO(ShaderModule& shaderModule)
{
    return AttributeValidator(shaderModule).validateIO();
}

} // namespace WGSL
