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

namespace WGSL {

class AttributeValidator : public AST::Visitor {
public:
    AttributeValidator(ShaderModule&);

    std::optional<FailedCheck> validate();

    void visit(AST::Function&) override;
    void visit(AST::Parameter&) override;
    void visit(AST::Variable&) override;
    void visit(AST::Structure&) override;
    void visit(AST::StructureMember&) override;

private:
    bool parseBuiltin(AST::Function*, std::optional<Builtin>&, AST::Attribute&);
    bool parseInterpolate(std::optional<AST::Interpolation>&, AST::Attribute&);
    bool parseInvariant(bool&, AST::Attribute&);
    bool parseLocation(AST::Function*, std::optional<unsigned>&, AST::Attribute&, const Type*);

    void validateInterpolation(const SourceSpan&, const std::optional<AST::Interpolation>&, const std::optional<unsigned>&);
    void validateInvariant(const SourceSpan&, const std::optional<Builtin>&, bool);

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
                error(attribute.span(), "@must_use can only be applied to functions that return a value");
            set(attribute.span(), function.m_mustUse);
            continue;
        }

        if (is<AST::StageAttribute>(attribute)) {
            update(attribute.span(), function.m_stage, downcast<AST::StageAttribute>(attribute).stage());
            continue;
        }

        if (is<AST::WorkgroupSizeAttribute>(attribute)) {
            auto& workgroupSize = downcast<AST::WorkgroupSizeAttribute>(attribute).workgroupSize();
            const auto& check = [&](AST::Expression* dimension) {
                if (!dimension)
                    return;
                auto value = dimension->constantValue();
                if (!value.has_value())
                    return;
                if (value->integerValue() < 1)
                    error(dimension->span(), "@workgroup_size argument must be at least 1");
            };
            check(workgroupSize.x);
            check(workgroupSize.y);
            check(workgroupSize.z);
            update(attribute.span(), function.m_workgroupSize, workgroupSize);
            continue;
        }

        error(attribute.span(), "invalid attribute for function declaration");
    }
    if (function.workgroupSize().has_value() && (!function.stage().has_value() || *function.stage() != ShaderStage::Compute))
        error(function.span(), "@workgroup_size must only be applied to compute shader entry point function");

    for (auto& attribute : function.returnAttributes()) {
        if (parseBuiltin(&function, function.m_returnTypeBuiltin, attribute))
            continue;

        if (parseInterpolate(function.m_returnTypeInterpolation, attribute))
            continue;

        if (parseInvariant(function.m_returnTypeInvariant, attribute))
            continue;

        if (parseLocation(&function, function.m_returnTypeLocation, attribute, function.maybeReturnType()->inferredType()))
            continue;

        error(attribute.span(), "invalid attribute for function return type");
    }

    validateInterpolation(function.maybeReturnType()->span(), function.returnTypeInterpolation(), function.returnTypeLocation());
    validateInvariant(function.maybeReturnType()->span(), function.returnTypeBuiltin(), function.returnTypeInvariant());

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

        error(attribute.span(), "invalid attribute for function parameter");
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
        if (is<AST::BindingAttribute>(attribute)) {
            if (!isResource)
                error(attribute.span(), "@binding attribute must only be applied to resource variables");
            // https://gpuweb.github.io/cts/standalone/?q=webgpu:shader,validation,parse,attribute:expressions:value=%22override%22;attribute=%22binding%22
            auto& constantValue = downcast<AST::BindingAttribute>(attribute).binding().constantValue();
            if (!constantValue) {
                error(attribute.span(), "@binding attribute must only be applied to resource variables");
                continue;
            }

            auto bindingValue = constantValue->integerValue();
            if (bindingValue < 0)
                error(attribute.span(), "@binding value must be non-negative");
            else
                update(attribute.span(), variable.m_binding, static_cast<unsigned>(bindingValue));
            continue;
        }

        if (is<AST::GroupAttribute>(attribute)) {
            if (!isResource)
                error(attribute.span(), "@group attribute must only be applied to resource variables");
            // https://gpuweb.github.io/cts/standalone/?q=webgpu:shader,validation,parse,attribute:expressions:value=%22override%22;attribute=%22binding%22
            auto& constantValue = downcast<AST::GroupAttribute>(attribute).group().constantValue();
            if (!constantValue) {
                error(attribute.span(), "@group attribute must only be applied to resource variables");
                continue;
            }
            auto groupValue = constantValue->integerValue();
            if (groupValue < 0)
                error(attribute.span(), "@group value must be non-negative");
            else
                update(attribute.span(), variable.m_group, static_cast<unsigned>(groupValue));
            continue;
        }

        if (is<AST::IdAttribute>(attribute)) {
            auto& idExpression = downcast<AST::IdAttribute>(attribute).value();
            if (variable.flavor() != AST::VariableFlavor::Override || !satisfies(variable.storeType(), Constraints::Scalar))
                error(attribute.span(), "@id attribute must only be applied to override variables of scalar type");
            // https://gpuweb.github.io/cts/standalone/?q=webgpu:shader,validation,parse,attribute:expressions:value=%22override%22;attribute=%22binding%22
            auto& constantValue = idExpression.constantValue();
            if (!constantValue) {
                error(attribute.span(), "@id attribute must only be applied to override variables of scalar type");
                continue;
            }
            auto idValue = constantValue->integerValue();
            if (idValue < 0)
                error(attribute.span(), "@id value must be non-negative");
            else {
                auto uintIdValue = static_cast<unsigned>(idValue);
                if (m_shaderModule.containsOverride(uintIdValue))
                    error(attribute.span(), "@id value must be unique");
                else {
                    update(attribute.span(), variable.m_id, uintIdValue);
                    m_shaderModule.addOverride(uintIdValue);
                }
            }
            continue;
        }

        error(attribute.span(), "invalid attribute for variable declaration");
    }

    if (isResource && (!variable.m_group || !variable.m_binding))
        error(variable.span(), "resource variables require @group and @binding attributes");
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

    unsigned previousSize = 0;
    unsigned alignment = 0;
    unsigned size = 0;
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

        auto offset = WTF::roundUpToMultipleOf(*fieldAlignment, size);
        member.m_offset = offset;

        alignment = std::max(alignment, *fieldAlignment);
        size = offset + *fieldSize;

        if (previousMember)
            previousMember->m_padding = offset - previousSize;

        previousMember = &member;
        previousSize = offset + typeSize;
    }
    auto finalSize = WTF::roundUpToMultipleOf(alignment, size);
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

        if (is<AST::SizeAttribute>(attribute)) {
            // FIXME: check that the member type must have creation-fixed footprint.
            m_hasSizeOrAlignmentAttributes = true;
            // https://gpuweb.github.io/cts/standalone/?q=webgpu:shader,validation,parse,attribute:expressions:value=%22override%22;*
            auto& constantValue = downcast<AST::SizeAttribute>(attribute).size().constantValue();
            if (!constantValue) {
                error(attribute.span(), "@size constant value is not found");
                continue;
            }
            auto sizeValue = constantValue->integerValue();
            if (sizeValue < 0)
                error(attribute.span(), "@size value must be non-negative");
            else if (m_errors.isEmpty() && sizeValue < member.type().inferredType()->size()) {
                // We can't call Type::size() if we already have errors, as we might
                // try to read the size of a struct, which we will not have computed
                // if we already encountered errors
                error(attribute.span(), "@size value must be at least the byte-size of the type of the member");
            }
            update(attribute.span(), member.m_size, static_cast<unsigned>(sizeValue));
            continue;
        }

        if (is<AST::AlignAttribute>(attribute)) {
            m_hasSizeOrAlignmentAttributes = true;
            // https://gpuweb.github.io/cts/standalone/?q=webgpu:shader,validation,parse,attribute:expressions:value=%22override%22;attribute=%22align%22
            auto constantValue = downcast<AST::AlignAttribute>(attribute).alignment().constantValue();
            if (!constantValue) {
                error(attribute.span(), "@align constant value does not exist");
                continue;
            }
            auto alignmentValue = constantValue->integerValue();
            auto isPowerOf2 = !(alignmentValue & (alignmentValue - 1));
            if (alignmentValue < 0)
                error(attribute.span(), "@align value must be non-negative");
            else if (!isPowerOf2)
                error(attribute.span(), "@align value must be a power of two");
            // FIXME: validate that alignment is a multiple of RequiredAlignOf(T,C)
            update(attribute.span(), member.m_alignment, static_cast<unsigned>(alignmentValue));
            continue;
        }

        error(attribute.span(), "invalid attribute for structure member");
    }

    validateInterpolation(member.span(), member.interpolation(), member.location());
    validateInvariant(member.span(), member.builtin(), member.invariant());

    AST::Visitor::visit(member);
}

bool AttributeValidator::parseBuiltin(AST::Function* function, std::optional<Builtin>& builtin, AST::Attribute& attribute)
{
    if (!is<AST::BuiltinAttribute>(attribute))
        return false;
    if (function && !function->stage())
        error(attribute.span(), "@builtin is not valid for non-entry point function types");
    update(attribute.span(), builtin, downcast<AST::BuiltinAttribute>(attribute).builtin());
    return true;
}

bool AttributeValidator::parseInterpolate(std::optional<AST::Interpolation>& interpolation, AST::Attribute& attribute)
{
    if (!is<AST::InterpolateAttribute>(attribute))
        return false;
    update(attribute.span(), interpolation, downcast<AST::InterpolateAttribute>(attribute).interpolation());
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
    if (!is<AST::LocationAttribute>(attribute))
        return false;
    if (function && !function->stage())
        error(attribute.span(), "@location is not valid for non-entry point function types");
    else if (function && *function->stage() == ShaderStage::Compute)
        error(attribute.span(), "@location may not be used in the compute shader stage");

    bool isNumeric = satisfies(declarationType, Constraints::Number);
    bool isNumericVector = false;
    if (!isNumeric) {
        if (auto* vectorType = std::get_if<Types::Vector>(declarationType))
            isNumericVector = satisfies(vectorType->element, Constraints::Number);
    }
    if (!isNumeric && !isNumericVector)
        error(attribute.span(), "@location must only be applied to declarations of numeric scalar or numeric vector type");

    auto& constantValue = downcast<AST::LocationAttribute>(attribute).location().constantValue();
    // https://gpuweb.github.io/cts/standalone/?q=webgpu:shader,validation,parse,attribute:expressions:value=%22override%22;*
    if (!constantValue) {
        error(attribute.span(), "@location constant value is missing");
        return false;
    }
    auto locationValue = constantValue->integerValue();
    if (locationValue < 0)
        error(attribute.span(), "@location value must be non-negative");
    else
        update(attribute.span(), location, static_cast<unsigned>(locationValue));
    return true;
}

void AttributeValidator::validateInterpolation(const SourceSpan& span, const std::optional<AST::Interpolation>& interpolation, const std::optional<unsigned>& location)
{
    if (interpolation && !location)
        error(span, "@interpolate is only allowed on declarations that have a @location attribute");
}

void AttributeValidator::validateInvariant(const SourceSpan& span, const std::optional<Builtin>& builtin, bool invariant)
{
    if (invariant && (!builtin || *builtin != Builtin::Position))
        error(span, "@invariant is only allowed on declarations that have a @builtin(position) attribute");
}

template<typename T>
void AttributeValidator::update(const SourceSpan& span, std::optional<T>& destination, const T& source)
{
    if (destination.has_value())
        error(span, "duplicate attribute");
    else
        destination = source;
}

void AttributeValidator::set(const SourceSpan& span, bool& destination)
{
    if (destination)
        error(span, "duplicate attribute");
    else
        destination = true;
}

template<typename... Arguments>
void AttributeValidator::error(const SourceSpan& span, Arguments&&... arguments)
{
    m_errors.append({ makeString(std::forward<Arguments>(arguments)...), span });
}

std::optional<FailedCheck> validateAttributes(ShaderModule& shaderModule)
{
    return AttributeValidator(shaderModule).validate();
}

} // namespace WGSL
