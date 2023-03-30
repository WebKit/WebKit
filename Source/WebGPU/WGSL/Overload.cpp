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
#include "Overload.h"

#include "Types.h"
#include <wtf/DataLog.h>

namespace WGSL {

static constexpr bool shouldDumpOverloadDebugInformation = false;
static constexpr ASCIILiteral logPrefix = "> overload: "_s;

template<typename... Arguments>
inline void log(Arguments&&... arguments)
{
    if (shouldDumpOverloadDebugInformation)
        dataLog(logPrefix, std::forward<Arguments>(arguments)...);
}

template<typename... Arguments>
inline void logLn(Arguments&&... arguments)
{
    if (shouldDumpOverloadDebugInformation)
        dataLogLn(logPrefix, std::forward<Arguments>(arguments)...);
}

struct ViableOverload {
    const OverloadCandidate* candidate;
    FixedVector<unsigned> ranks;
    Type* result;
};

class OverloadResolver {
public:
    OverloadResolver(TypeStore&, const Vector<OverloadCandidate>&, const Vector<Type*>& valueArguments, const Vector<Type*>& typeArguments, unsigned numberOfTypeSubstitutions, unsigned numberOfValueSubstitutions);

    Type* resolve();

private:
    static constexpr unsigned s_noConversion = std::numeric_limits<unsigned>::max();

    FixedVector<std::optional<ViableOverload>> considerCandidates();
    std::optional<ViableOverload> considerCandidate(const OverloadCandidate&);
    unsigned calculateRank(const AbstractType&, Type*);
    unsigned conversionRank(Type*, Type*) const;
    unsigned conversionRankImpl(Type*, Type*) const;

    bool unify(const AbstractType&, Type*);
    bool assign(TypeVariable, Type*);
    Type* resolve(TypeVariable) const;
    Type* materialize(const AbstractType&) const;

    bool unify(const AbstractValue&, unsigned);
    void assign(NumericVariable, unsigned);
    std::optional<unsigned> resolve(NumericVariable) const;
    unsigned materialize(const AbstractValue&) const;

    TypeStore& m_types;
    const Vector<OverloadCandidate>& m_candidates;
    const Vector<Type*>& m_valueArguments;
    const Vector<Type*>& m_typeArguments;
    FixedVector<Type*> m_typeSubstitutions;
    FixedVector<std::optional<unsigned>> m_numericSubstitutions;
};

OverloadResolver::OverloadResolver(TypeStore& types, const Vector<OverloadCandidate>& candidates, const Vector<Type*>& valueArguments, const Vector<Type*>& typeArguments, unsigned numberOfTypeSubstitutions, unsigned numberOfValueSubstitutions)
    : m_types(types)
    , m_candidates(candidates)
    , m_valueArguments(valueArguments)
    , m_typeArguments(typeArguments)
    , m_typeSubstitutions(numberOfTypeSubstitutions)
    , m_numericSubstitutions(numberOfValueSubstitutions)
{
}

Type* OverloadResolver::resolve()
{
    auto candidates = considerCandidates();
    std::optional<ViableOverload> selectedCandidate = std::nullopt;

    for (auto& candidate : candidates) {
        if (!candidate.has_value())
            continue;

        if (!selectedCandidate.has_value()) {
            std::exchange(selectedCandidate, candidate);
            continue;
        }
        ASSERT(selectedCandidate->ranks.size() == candidate->ranks.size());

        bool isBetterOrEqual = true;
        bool hasStrictlyBetter = false;
        for (unsigned i = 0; i < candidate->ranks.size(); ++i) {
            if (candidate->ranks[i] > selectedCandidate->ranks[i]) {
                isBetterOrEqual = false;
                break;
            }
            if (candidate->ranks[i] < selectedCandidate->ranks[i])
                hasStrictlyBetter = true;
        }
        if (isBetterOrEqual && hasStrictlyBetter)
            std::exchange(selectedCandidate, candidate);
    }

    if (!selectedCandidate.has_value()) {
        logLn("no suitable overload found");
        return nullptr;
    }

    logLn("selected overload: ", *selectedCandidate->candidate);
    logLn("materialized result type: ", *selectedCandidate->result);

    return selectedCandidate->result;
}

Type* OverloadResolver::materialize(const AbstractType& abstractType) const
{
    return WTF::switchOn(abstractType,
        [&](Type* type) -> Type* {
            return type;
        },
        [&](TypeVariable variable) -> Type* {
            return resolve(variable);
        },
        [&](const AbstractVector& vector) -> Type* {
            if (auto* element = materialize(vector.element)) {
                auto size = materialize(vector.size);
                return m_types.vectorType(element, size);
            }
            return nullptr;
        },
        [&](const AbstractMatrix& matrix) -> Type* {
            if (auto* element = materialize(matrix.element)) {
                auto columns = materialize(matrix.columns);
                auto rows = materialize(matrix.rows);
                return m_types.matrixType(element, columns, rows);
            }
            return nullptr;
        });
}

unsigned OverloadResolver::materialize(const AbstractValue& abstractValue) const
{
    return WTF::switchOn(abstractValue,
        [&](unsigned value) -> unsigned {
            return value;
        },
        [&](NumericVariable variable) -> unsigned {
            std::optional<unsigned> resolvedValue = resolve(variable);
            ASSERT(resolvedValue.has_value());
            return *resolvedValue;
        });
}

FixedVector<std::optional<ViableOverload>> OverloadResolver::considerCandidates()
{
    FixedVector<std::optional<ViableOverload>> candidates(m_candidates.size());
    for (unsigned i = 0; i < m_candidates.size(); ++i)
        candidates[i] = considerCandidate(m_candidates[i]);
    return candidates;
}

std::optional<ViableOverload> OverloadResolver::considerCandidate(const OverloadCandidate& candidate)
{
    if (candidate.parameters.size() != m_valueArguments.size())
        return std::nullopt;

    if (m_typeArguments.size() > candidate.typeVariables.size())
        return std::nullopt;

    m_typeSubstitutions.fill(nullptr);
    m_numericSubstitutions.fill(std::nullopt);

    for (unsigned i = 0; i < m_typeArguments.size(); ++i) {
        if (!assign(candidate.typeVariables[i], m_typeArguments[i]))
            return std::nullopt;
    }

    logLn("Considering overload: ", candidate);

    ViableOverload viableOverload { &candidate, FixedVector<unsigned>(m_valueArguments.size()), nullptr };
    for (unsigned i = 0; i < m_valueArguments.size(); ++i) {
        auto& parameter = candidate.parameters[i];
        auto* argument = m_valueArguments[i];
        logLn("matching parameter #", i, " '", parameter, "' with argument '", *argument, "'");
        if (!unify(parameter, argument)) {
            logLn("rejected on parameter #", i);
            return std::nullopt;
        }
    }
    for (unsigned i = 0; i < m_valueArguments.size(); ++i) {
        auto& parameter = candidate.parameters[i];
        auto* argument = m_valueArguments[i];
        auto rank = calculateRank(parameter, argument);
        ASSERT(rank != s_noConversion);
        viableOverload.ranks[i] = rank;
    }

    viableOverload.result = materialize(candidate.result);
    if (!viableOverload.result)
        return std::nullopt;

    if (shouldDumpOverloadDebugInformation) {
        log("found a viable candidate '", candidate, "' materialized as '(");
        bool first = true;
        for (auto& parameter : candidate.parameters) {
            if (!first)
                dataLog(", ");
            first = false;
            dataLog(*materialize(parameter));
        }
        dataLog(") -> ", *viableOverload.result, "'");
        dataLogLn();
    }


    return { WTFMove(viableOverload) };
}

unsigned OverloadResolver::calculateRank(const AbstractType& parameter, Type* argumentType)
{
    if (auto* variable = std::get_if<TypeVariable>(&parameter)) {
        auto* resolvedType = resolve(*variable);
        ASSERT(resolvedType);
        return conversionRank(argumentType, resolvedType);
    }

    if (auto* vectorParameter = std::get_if<AbstractVector>(&parameter)) {
        auto& vectorArgument = std::get<Types::Vector>(*argumentType);
        return calculateRank(vectorParameter->element, vectorArgument.element);
    }

    if (auto* matrixParameter = std::get_if<AbstractMatrix>(&parameter)) {
        auto& matrixArgument = std::get<Types::Matrix>(*argumentType);
        return calculateRank(matrixParameter->element, matrixArgument.element);
    }

    auto* parameterType = std::get<Type*>(parameter);
    return conversionRank(argumentType, parameterType);
}

bool OverloadResolver::unify(const AbstractType& parameter, Type* argumentType)
{
    logLn("unify parameter type '", parameter, "' with argument '", *argumentType, "'");
    if (auto* variable = std::get_if<TypeVariable>(&parameter)) {
        auto* resolvedType = resolve(*variable);
        if (!resolvedType)
            return assign(*variable, argumentType);

        logLn("resolved '", *variable, "' to '", *resolvedType, "'");

        // Consider the following:
        // + :: (T, T) -> T
        // 1 + 1u
        //
        // We first unify `Var(T)` with `Type(AbstractInt)`, and assign `T` to `AbstractInt`
        // Next, we unify `Var(T) with `Type(u32)`, we look up `T => AbstractInt`,
        //   and promote `T` to `u32`.
        //
        // A few more examples to illustrate it:
        //
        // vec3 :: (T, T, T) -> T
        // vec3(1, 1.0, 1.0f) -> vec3<f32>
        // 1) unify(Var(T), Type(AbstractInt); T => AbstractInt (assign)
        // 2) unify(Var(T), Type(AbstractFloat); T => AbstractFloat (promote T)
        // 3) unify(Var(T), Type(f32); T => f32 (promote T again)
        //
        // vec3 :: (T, T, T) -> T
        // vec3(1.0, 1, 1.0f) -> vec3<f32>
        // 1) unify(Var(T), Type(AbstractFloat); T => AbstractFloat (assign)
        // 2) unify(Var(T), Type(AbstractInt); T => AbstractFloat (convert the argument to AbstractInt)
        // 3) unify(Var(T), Type(f32); T => f32 (promote T)
        //
        // vec3 :: (T, T, T) -> T
        // vec3(1.0, 1u, 1.0f) -> vec3<f32>
        // 1) unify(Var(T), Type(AbstractInt); T => AbstractFloat (assign)
        // 2) unify(Var(T), Type(u32); Failed! Can't unify AbstractFloat and u32
        auto variablePromotionRank = conversionRank(resolvedType, argumentType);
        auto argumentConversionRank = conversionRank(argumentType, resolvedType);
        logLn("variablePromotionRank: ", variablePromotionRank, ", argumentConversionRank: ", argumentConversionRank);
        if (variablePromotionRank < argumentConversionRank)
            return assign(*variable, argumentType);

        return argumentConversionRank != s_noConversion;
    }

    if (auto* vectorParameter = std::get_if<AbstractVector>(&parameter)) {
        auto* vectorArgument = std::get_if<Types::Vector>(argumentType);
        if (!vectorArgument)
            return false;
        if (!unify(vectorParameter->element, vectorArgument->element))
            return false;
        return unify(vectorParameter->size, vectorArgument->size);
    }

    if (auto* matrixParameter = std::get_if<AbstractMatrix>(&parameter)) {
        auto* matrixArgument = std::get_if<Types::Matrix>(argumentType);
        if (!matrixArgument)
            return false;
        if (!unify(matrixParameter->element, matrixArgument->element))
            return false;
        if (!unify(matrixParameter->columns, matrixArgument->columns))
            return false;
        return unify(matrixParameter->rows, matrixArgument->rows);
    }

    auto* parameterType = std::get<Type*>(parameter);
    return conversionRank(argumentType, parameterType) != s_noConversion;
}

bool OverloadResolver::unify(const AbstractValue& parameter, unsigned argumentValue)
{
    if (auto* parameterValue = std::get_if<unsigned>(&parameter))
        return *parameterValue == argumentValue;

    auto variable = std::get<NumericVariable>(parameter);
    auto resolvedValue = resolve(variable);
    if (!resolvedValue.has_value()) {
        assign(variable, argumentValue);
        return true;
    }
    return *resolvedValue == argumentValue;
}

bool OverloadResolver::assign(TypeVariable variable, Type* type)
{
    logLn("assign ", variable, " => ", *type);
    if (variable.constraints) {
        auto* primitive = std::get_if<Types::Primitive>(type);
        if (!primitive)
            return false;

        switch (primitive->kind) {
        case Types::Primitive::AbstractInt:
            if (variable.constraints < TypeVariable::AbstractInt)
                return false;
            if (variable.constraints & TypeVariable::AbstractInt)
                break;
            if (variable.constraints & TypeVariable::I32) {
                type = m_types.i32Type();
                break;
            }
            if (variable.constraints & TypeVariable::U32) {
                type = m_types.u32Type();
                break;
            }
            if (variable.constraints & TypeVariable::AbstractFloat) {
                type = m_types.abstractFloatType();
                break;
            }
            if (variable.constraints & TypeVariable::F32) {
                type = m_types.f32Type();
                break;
            }
            if (variable.constraints & TypeVariable::F16) {
                // FIXME: Add F16 support
                // https://bugs.webkit.org/show_bug.cgi?id=254668
                RELEASE_ASSERT_NOT_REACHED();
            }
            RELEASE_ASSERT_NOT_REACHED();
        case Types::Primitive::AbstractFloat:
            if (variable.constraints < TypeVariable::AbstractFloat)
                return false;
            if (variable.constraints & TypeVariable::AbstractFloat)
                break;
            if (variable.constraints & TypeVariable::F32) {
                type = m_types.f32Type();
                break;
            }
            if (variable.constraints & TypeVariable::F16) {
                // FIXME: Add F16 support
                // https://bugs.webkit.org/show_bug.cgi?id=254668
                RELEASE_ASSERT_NOT_REACHED();
            }
            RELEASE_ASSERT_NOT_REACHED();
        case Types::Primitive::I32:
            if (!(variable.constraints & TypeVariable::I32))
                return false;
            break;
        case Types::Primitive::U32:
            if (!(variable.constraints & TypeVariable::U32))
                return false;
            break;
        case Types::Primitive::F32:
            if (!(variable.constraints & TypeVariable::F32))
                return false;
            break;
        // FIXME: Add F16 support
        // https://bugs.webkit.org/show_bug.cgi?id=254668
        case Types::Primitive::Bool:
            if (!(variable.constraints & TypeVariable::Bool))
                return false;
            break;

        case Types::Primitive::Void:
        case Types::Primitive::Sampler:
            return false;
        }
    }
    m_typeSubstitutions[variable.id] = type;
    return true;
}

void OverloadResolver::assign(NumericVariable variable, unsigned value)
{
    logLn("assign ", variable, " => ", value);
    m_numericSubstitutions[variable.id] = { value };
}

Type* OverloadResolver::resolve(TypeVariable variable) const
{
    return m_typeSubstitutions[variable.id];
}

std::optional<unsigned> OverloadResolver::resolve(NumericVariable variable) const
{
    return m_numericSubstitutions[variable.id];
}

unsigned OverloadResolver::conversionRank(Type* from, Type* to) const
{
    auto rank = conversionRankImpl(from, to);
    logLn("conversionRank(from: ", *from, ", to: ", *to, ") = ", rank);
    return rank;
}

constexpr unsigned primitivePair(Types::Primitive::Kind first, Types::Primitive::Kind second)
{
    static_assert(sizeof(Types::Primitive::Kind) == 1);
    return static_cast<unsigned>(first) << 8 | second;
}

// https://www.w3.org/TR/WGSL/#conversion-rank
unsigned OverloadResolver::conversionRankImpl(Type* from, Type* to) const
{
    using namespace WGSL::Types;

    if (from == to)
        return 0;

    if (std::holds_alternative<Bottom>(*from))
        return 0;

    // FIXME: refs should also return 0

    if (auto* fromPrimitive = std::get_if<Primitive>(from)) {
        auto* toPrimitive = std::get_if<Primitive>(to);
        if (!toPrimitive)
            return s_noConversion;

        switch (primitivePair(fromPrimitive->kind, toPrimitive->kind)) {
        case primitivePair(Primitive::AbstractFloat, Primitive::F32):
            return 1;
        // FIXME: AbstractFloat to f16 should return 2
        case primitivePair(Primitive::AbstractInt, Primitive::I32):
            return 3;
        case primitivePair(Primitive::AbstractInt, Primitive::U32):
            return 4;
        case primitivePair(Primitive::AbstractInt, Primitive::AbstractFloat):
            return 5;
        case primitivePair(Primitive::AbstractInt, Primitive::F32):
            return 6;
        // FIXME: AbstractInt to f16 should return 7
        default:
            return s_noConversion;
        }
    }

    if (auto* fromVector = std::get_if<Vector>(from)) {
        auto* toVector = std::get_if<Vector>(to);
        if (!toVector)
            return s_noConversion;
        if (fromVector->size != toVector->size)
            return s_noConversion;
        return conversionRank(fromVector->element, toVector->element);
    }

    if (auto* fromMatrix = std::get_if<Matrix>(from)) {
        auto* toMatrix = std::get_if<Matrix>(to);
        if (!toMatrix)
            return s_noConversion;
        if (fromMatrix->columns != toMatrix->columns)
            return s_noConversion;
        if (fromMatrix->rows != toMatrix->rows)
            return s_noConversion;
        return conversionRank(fromMatrix->element, toMatrix->element);
    }

    if (auto* fromArray = std::get_if<Array>(from)) {
        auto* toArray = std::get_if<Array>(to);
        if (!toArray)
            return s_noConversion;
        if (fromArray->size != toArray->size)
            return s_noConversion;
        return conversionRank(fromArray->element, toArray->element);
    }

    // FIXME: add the abstract result conversion rules
    return s_noConversion;
}

Type* resolveOverloads(TypeStore& types, const Vector<OverloadCandidate>& candidates, const Vector<Type*>& valueArguments, const Vector<Type*>& typeArguments)
{

    unsigned numberOfTypeSubstitutions = 0;
    unsigned numberOfValueSubstitutions = 0;
    for (const auto& candidate : candidates) {
        numberOfTypeSubstitutions = std::max(numberOfTypeSubstitutions, static_cast<unsigned>(candidate.typeVariables.size()));
        numberOfValueSubstitutions = std::max(numberOfValueSubstitutions, static_cast<unsigned>(candidate.numericVariables.size()));
    }
    OverloadResolver resolver(types, candidates, valueArguments, typeArguments, numberOfTypeSubstitutions, numberOfValueSubstitutions);
    return resolver.resolve();
}

} // namespace WGSL

namespace WTF {

void printInternal(PrintStream& out, const WGSL::NumericVariable& variable)
{
    out.print("val", variable.id);
}

void printInternal(PrintStream& out, const WGSL::AbstractValue& value)
{
    WTF::switchOn(value,
        [&](unsigned value) {
            out.print(value);
        },
        [&](WGSL::NumericVariable variable) {
            printInternal(out, variable);
        });
}

void printInternal(PrintStream& out, const WGSL::TypeVariable& variable)
{
    out.print("type", variable.id);
}

void printInternal(PrintStream& out, const WGSL::AbstractType& type)
{
    WTF::switchOn(type,
        [&](WGSL::Type* type) {
            printInternal(out, *type);
        },
        [&](WGSL::TypeVariable variable) {
            printInternal(out, variable);
        },
        [&](const WGSL::AbstractVector& vector) {
            out.print("vector<");
            printInternal(out, vector.element);
            out.print(", ");
            printInternal(out, vector.size);
            out.print(">");
        },
        [&](const WGSL::AbstractMatrix& matrix) {
            out.print("matrix<");
            printInternal(out, matrix.element);
            out.print(", ");
            printInternal(out, matrix.columns);
            out.print(", ");
            printInternal(out, matrix.rows);
            out.print(">");
        });
}

void printInternal(PrintStream& out, const WGSL::OverloadCandidate& candidate)
{
    if (candidate.typeVariables.size() || candidate.numericVariables.size()) {
        bool first = true;
        out.print("<");
        for (auto& typeVariable : candidate.typeVariables) {
            if (!first)
                out.print(", ");
            first = false;
            printInternal(out, typeVariable);
        }
        for (auto& numericVariable : candidate.numericVariables) {
            if (!first)
                out.print(", ");
            first = false;
            printInternal(out, numericVariable);
        }
        out.print(">");
    }
    out.print("(");
    bool first = true;
    for (auto& parameter : candidate.parameters) {
        if (!first)
            out.print(", ");
        first = false;
        printInternal(out, parameter);
    }
    out.print(") -> ");
    printInternal(out, candidate.result);
}

} // namespace WTF
