/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSCalcTree+CalculationValue.h"

#include "CSSCalcRandomCachingKey.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcTree+Evaluation.h"
#include "CSSCalcTree+Mappings.h"
#include "CSSCalcTree+Simplification.h"
#include "CSSCalcTree+Traversal.h"
#include "CSSCalcTree.h"
#include "CSSUnevaluatedCalc.h"
#include "CalculationCategory.h"
#include "CalculationExecutor.h"
#include "CalculationTree.h"
#include "CalculationValue.h"
#include "RenderStyleInlines.h"
#include "StyleBuilderState.h"
#include "StyleLengthResolution.h"
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {
namespace CSSCalc {

struct FromConversionOptions {
    CanonicalDimension::Dimension canonicalDimension;
    SimplificationOptions simplification;
    const RenderStyle& style;
};

struct ToConversionOptions {
    EvaluationOptions evaluation;
};

static auto fromCalculationValue(const Calculation::Random::Fixed&, const FromConversionOptions&) -> Random::Sharing;
static auto fromCalculationValue(const Calculation::None&, const FromConversionOptions&) -> CSS::Keyword::None;
static auto fromCalculationValue(const Calculation::ChildOrNone&, const FromConversionOptions&) -> ChildOrNone;
static auto fromCalculationValue(const Calculation::Children&, const FromConversionOptions&) -> Children;
static auto fromCalculationValue(const std::optional<Calculation::Child>&, const FromConversionOptions&) -> std::optional<Child>;
static auto fromCalculationValue(const Calculation::Child&, const FromConversionOptions&) -> Child;
static auto fromCalculationValue(const Calculation::Number&, const FromConversionOptions&) -> Child;
static auto fromCalculationValue(const Calculation::Percentage&, const FromConversionOptions&) -> Child;
static auto fromCalculationValue(const Calculation::Dimension&, const FromConversionOptions&) -> Child;
static auto fromCalculationValue(const Calculation::IndirectNode<Calculation::Blend>&, const FromConversionOptions&) -> Child;
template<typename CalculationOp> auto fromCalculationValue(const Calculation::IndirectNode<CalculationOp>&, const FromConversionOptions&) -> Child;

static auto toCalculationValue(const Random::Sharing&, const ToConversionOptions&) -> Calculation::Random::Fixed;
static auto toCalculationValue(const std::optional<Child>&, const ToConversionOptions&) -> std::optional<Calculation::Child>;
static auto toCalculationValue(const CSS::Keyword::None&, const ToConversionOptions&) -> Calculation::None;
static auto toCalculationValue(const ChildOrNone&, const ToConversionOptions&) -> Calculation::ChildOrNone;
static auto toCalculationValue(const Children&, const ToConversionOptions&) -> Calculation::Children;
static auto toCalculationValue(const Child&, const ToConversionOptions&) -> Calculation::Child;
static auto toCalculationValue(const Number&, const ToConversionOptions&) -> Calculation::Child;
static auto toCalculationValue(const Percentage&, const ToConversionOptions&) -> Calculation::Child;
static auto toCalculationValue(const CanonicalDimension&, const ToConversionOptions&) -> Calculation::Child;
static auto toCalculationValue(const NonCanonicalDimension&, const ToConversionOptions&) -> Calculation::Child;
static auto toCalculationValue(const Symbol&, const ToConversionOptions&) -> Calculation::Child;
static auto toCalculationValue(const SiblingCount&, const ToConversionOptions&) -> Calculation::Child;
static auto toCalculationValue(const SiblingIndex&, const ToConversionOptions&) -> Calculation::Child;
static auto toCalculationValue(const IndirectNode<Anchor>&, const ToConversionOptions&) -> Calculation::Child;
static auto toCalculationValue(const IndirectNode<AnchorSize>&, const ToConversionOptions&) -> Calculation::Child;
template<typename Op> auto toCalculationValue(const IndirectNode<Op>&, const ToConversionOptions&) -> Calculation::Child;

static CanonicalDimension::Dimension determineCanonicalDimension(Calculation::Category category)
{
    switch (category) {
    case Calculation::Category::LengthPercentage:
        return CanonicalDimension::Dimension::Length;

    case Calculation::Category::AnglePercentage:
        return CanonicalDimension::Dimension::Angle;

    case Calculation::Category::Integer:
    case Calculation::Category::Number:
    case Calculation::Category::Percentage:
    case Calculation::Category::Length:
    case Calculation::Category::Angle:
    case Calculation::Category::Time:
    case Calculation::Category::Frequency:
    case Calculation::Category::Resolution:
    case Calculation::Category::Flex:
        break;
    }

    ASSERT_NOT_REACHED();
    return CanonicalDimension::Dimension::Length;
}

// MARK: - From

Random::Sharing fromCalculationValue(const Calculation::Random::Fixed& randomFixed, const FromConversionOptions&)
{
    return Random::SharingFixed { randomFixed.baseValue };
}

CSS::Keyword::None fromCalculationValue(const Calculation::None&, const FromConversionOptions&)
{
    return CSS::Keyword::None { };
}

ChildOrNone fromCalculationValue(const Calculation::ChildOrNone& root, const FromConversionOptions& options)
{
    return WTF::switchOn(root, [&](const auto& root) { return ChildOrNone { fromCalculationValue(root, options) }; });
}

Children fromCalculationValue(const Calculation::Children& children, const FromConversionOptions& options)
{
    return WTF::map(children.value, [&](const auto& child) -> Child { return fromCalculationValue(child, options); });
}

std::optional<Child> fromCalculationValue(const std::optional<Calculation::Child>& root, const FromConversionOptions& options)
{
    if (root)
        return fromCalculationValue(*root, options);
    return std::nullopt;
}

Child fromCalculationValue(const Calculation::Child& root, const FromConversionOptions& options)
{
    return WTF::switchOn(root, [&](const auto& root) { return fromCalculationValue(root, options); });
}

Child fromCalculationValue(const Calculation::Number& number, const FromConversionOptions&)
{
    return makeChild(Number { .value = number.value });
}

Child fromCalculationValue(const Calculation::Percentage& percentage, const FromConversionOptions& options)
{
    return makeChild(Percentage { .value = percentage.value, .hint = Type::determinePercentHint(options.simplification.category) });
}

Child fromCalculationValue(const Calculation::Dimension& root, const FromConversionOptions& options)
{
    switch (options.canonicalDimension) {
    case CanonicalDimension::Dimension::Length:
        return makeChild(CanonicalDimension { .value = adjustFloatForAbsoluteZoom(root.value, options.style), .dimension = options.canonicalDimension });

    case CanonicalDimension::Dimension::Angle:
    case CanonicalDimension::Dimension::Time:
    case CanonicalDimension::Dimension::Frequency:
    case CanonicalDimension::Dimension::Resolution:
    case CanonicalDimension::Dimension::Flex:
        break;
    }

    return makeChild(CanonicalDimension { .value = root.value, .dimension = options.canonicalDimension });
}

Child fromCalculationValue(const Calculation::IndirectNode<Calculation::Blend>& root, const FromConversionOptions& options)
{
    // FIXME: (http://webkit.org/b/122036) Create a CSSCalc::Tree equivalent of Calculation::Blend.

    auto createBlendHalf = [](const auto& child, const auto& options, auto progress) -> Child {
        auto product = multiply(
            fromCalculationValue(child, options),
            makeChild(Number { .value = progress })
        );

        if (auto replacement = simplify(product, options.simplification))
            return WTFMove(*replacement);

        auto type = toType(product);
        return makeChild(WTFMove(product), *type);
    };

    auto sum = add(
        createBlendHalf(root->from, options, 1 - root->progress),
        createBlendHalf(root->to, options, root->progress)
    );

    if (auto replacement = simplify(sum, options.simplification))
        return WTFMove(*replacement);

    auto type = toType(sum);
    return makeChild(WTFMove(sum), *type);
}

template<typename CalculationOp> Child fromCalculationValue(const Calculation::IndirectNode<CalculationOp>& root, const FromConversionOptions& options)
{
    using CalcOp = ToCalcTreeOp<CalculationOp>;

    auto op = WTF::apply([&](const auto& ...x) { return CalcOp { fromCalculationValue(x, options)... }; } , *root);

    if (auto replacement = simplify(op, options.simplification))
        return WTFMove(*replacement);

    auto type = toType(op);
    return makeChild(WTFMove(op), *type);
}

// MARK: - To.

auto toCalculationValue(const Random::Sharing& randomSharing, const ToConversionOptions& options) -> Calculation::Random::Fixed
{
    ASSERT(options.evaluation.conversionData);
    ASSERT(options.evaluation.conversionData->styleBuilderState());

    return WTF::switchOn(randomSharing,
        [&](const Random::SharingOptions& sharingOptions) -> Calculation::Random::Fixed {
            if (!sharingOptions.elementShared.has_value()) {
                ASSERT(options.evaluation.conversionData->styleBuilderState()->element());
            }

            auto baseValue = options.evaluation.conversionData->protectedStyleBuilderState()->lookupCSSRandomBaseValue(
                sharingOptions.identifier,
                sharingOptions.elementShared
            );

            return Calculation::Random::Fixed { baseValue };
        },
        [&](const Random::SharingFixed& sharingFixed) -> Calculation::Random::Fixed {
            return WTF::switchOn(sharingFixed.value,
                [&](const CSS::Number<CSS::ClosedUnitRange>::Raw& raw) -> Calculation::Random::Fixed {
                    return Calculation::Random::Fixed { raw.value };
                },
                [&](const CSS::Number<CSS::ClosedUnitRange>::Calc& calc) -> Calculation::Random::Fixed {
                    return Calculation::Random::Fixed { calc.evaluate(Calculation::Category::Number, *options.evaluation.conversionData->protectedStyleBuilderState()) };
                }
            );
        }
    );
}

std::optional<Calculation::Child> toCalculationValue(const std::optional<Child>& optionalChild, const ToConversionOptions& options)
{
    if (optionalChild)
        return toCalculationValue(*optionalChild, options);
    return std::nullopt;
}

Calculation::None toCalculationValue(const CSS::Keyword::None&, const ToConversionOptions&)
{
    return Calculation::None { };
}

Calculation::ChildOrNone toCalculationValue(const ChildOrNone& root, const ToConversionOptions& options)
{
    return WTF::switchOn(root, [&](const auto& root) { return Calculation::ChildOrNone { toCalculationValue(root, options) }; });
}

Calculation::Children toCalculationValue(const Children& children, const ToConversionOptions& options)
{
    return WTF::map(children, [&](const auto& child) { return toCalculationValue(child, options); });
}

Calculation::Child toCalculationValue(const Child& root, const ToConversionOptions& options)
{
    return WTF::switchOn(root, [&](const auto& root) { return toCalculationValue(root, options); });
}

Calculation::Child toCalculationValue(const Number& root, const ToConversionOptions&)
{
    return Calculation::number(root.value);
}

Calculation::Child toCalculationValue(const Percentage& root, const ToConversionOptions&)
{
    return Calculation::percentage(root.value);
}

Calculation::Child toCalculationValue(const CanonicalDimension& root, const ToConversionOptions& options)
{
    ASSERT(options.evaluation.conversionData);

    switch (root.dimension) {
    case CanonicalDimension::Dimension::Length:
        return Calculation::dimension(Style::computeNonCalcLengthDouble(root.value, CSS::LengthUnit::Px, *options.evaluation.conversionData));

    case CanonicalDimension::Dimension::Angle:
    case CanonicalDimension::Dimension::Time:
    case CanonicalDimension::Dimension::Frequency:
    case CanonicalDimension::Dimension::Resolution:
    case CanonicalDimension::Dimension::Flex:
        break;
    }

    return Calculation::dimension(root.value);
}

Calculation::Child toCalculationValue(const NonCanonicalDimension&, const ToConversionOptions&)
{
    ASSERT_NOT_REACHED("Non-canonical numeric values are not supported in the Calculation::Tree");
    return Calculation::number(0);
}

Calculation::Child toCalculationValue(const Symbol&, const ToConversionOptions&)
{
    ASSERT_NOT_REACHED("Unevaluated symbols are not supported in the Calculation::Tree");
    return Calculation::number(0);
}

Calculation::Child toCalculationValue(const SiblingCount&, const ToConversionOptions&)
{
    ASSERT_NOT_REACHED("Unevaluated sibling-count() functions are not supported in the Calculation::Tree");
    return Calculation::number(0);
}

Calculation::Child toCalculationValue(const SiblingIndex&, const ToConversionOptions&)
{
    ASSERT_NOT_REACHED("Unevaluated sibling-index() functions are not supported in the Calculation::Tree");
    return Calculation::number(0);
}

Calculation::Child toCalculationValue(const IndirectNode<Anchor>&, const ToConversionOptions&)
{
    ASSERT_NOT_REACHED("Unevaluated anchor() functions are not supported in the Calculation::Tree");
    return Calculation::number(0);
}

Calculation::Child toCalculationValue(const IndirectNode<AnchorSize>&, const ToConversionOptions&)
{
    ASSERT_NOT_REACHED("Unevaluated anchor-size() functions are not supported in the Calculation::Tree");
    return Calculation::number(0);
}

template<typename Op> Calculation::Child toCalculationValue(const IndirectNode<Op>& root, const ToConversionOptions& options)
{
    using CalculationOp = ToCalculationTreeOp<Op>;

    return Calculation::makeChild(WTF::apply([&](const auto& ...x) { return CalculationOp { toCalculationValue(x, options)... }; } , *root));
}

// MARK: - Exposed functions

Tree fromCalculationValue(const CalculationValue& calculationValue, const RenderStyle& style)
{
    auto category = calculationValue.category();
    auto range = calculationValue.range();

    auto conversionOptions = FromConversionOptions {
        .canonicalDimension = determineCanonicalDimension(category),
        .simplification = SimplificationOptions {
            .category = category,
            .range = { range.min, range.max },
            .conversionData = std::nullopt,
            .symbolTable = { },
            .allowZeroValueLengthRemovalFromSum = true,
        },
        .style = style,
    };

    auto root = fromCalculationValue(calculationValue.tree().root, conversionOptions);
    auto type = getType(root);

    return Tree {
        .root = WTFMove(root),
        .type = type,
        .stage = CSSCalc::Stage::Computed,
    };
}

Ref<CalculationValue> toCalculationValue(const Tree& tree, const EvaluationOptions& options)
{
    ASSERT(options.category == Calculation::Category::LengthPercentage || options.category == Calculation::Category::AnglePercentage);

    auto category = options.category;
    auto range = options.range;

    auto simplificationOptions = SimplificationOptions {
        .category = category,
        .range = range,
        .conversionData = options.conversionData,
        .symbolTable = options.symbolTable,
        .allowZeroValueLengthRemovalFromSum = true,
    };
    auto simplifiedTree = copyAndSimplify(tree, simplificationOptions);

    auto conversionOptions = ToConversionOptions {
        .evaluation = options
    };
    auto root = toCalculationValue(simplifiedTree.root, conversionOptions);

    return CalculationValue::create(
        category,
        Calculation::Range { range.min, range.max },
        Calculation::Tree { WTFMove(root) }
    );
}

} // namespace CSSCalc
} // namespace WebCore
