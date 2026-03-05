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
#include "StyleCalculationTree+Conversion.h"

#include "CSSCalcExecutor.h"
#include "CSSCalcRandomCachingKey.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcTree+Evaluation.h"
#include "CSSCalcTree+Mappings.h"
#include "CSSCalcTree+Simplification.h"
#include "CSSCalcTree+Traversal.h"
#include "CSSCalcTree.h"
#include "CSSPrimitiveNumericCategory.h"
#include "CSSUnevaluatedCalc.h"
#include "RenderStyle+GettersInlines.h"
#include "StyleBuilderState.h"
#include "StyleCalculationTree.h"
#include "StyleLengthResolution.h"
#include "StyleZoomPrimitivesInlines.h"
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {
namespace Style {
namespace Calculation {

struct ToCSSConversionOptions {
    CSSCalc::CanonicalDimension::Dimension canonicalDimension;
    CSSCalc::SimplificationOptions simplification;
    const RenderStyle& style;
};

struct ToStyleConversionOptions {
    CSSCalc::EvaluationOptions evaluation;
};

static auto toCSS(const Random::Fixed&, const ToCSSConversionOptions&) -> CSSCalc::Random::Sharing;
static auto toCSS(const CSS::Keyword::None&, const ToCSSConversionOptions&) -> CSS::Keyword::None;
static auto toCSS(const ChildOrNone&, const ToCSSConversionOptions&) -> CSSCalc::ChildOrNone;
static auto toCSS(const Children&, const ToCSSConversionOptions&) -> CSSCalc::Children;
static auto toCSS(const std::optional<Child>&, const ToCSSConversionOptions&) -> std::optional<CSSCalc::Child>;
static auto toCSS(const Child&, const ToCSSConversionOptions&) -> CSSCalc::Child;
static auto toCSS(const Number&, const ToCSSConversionOptions&) -> CSSCalc::Child;
static auto toCSS(const Percentage&, const ToCSSConversionOptions&) -> CSSCalc::Child;
static auto toCSS(const Dimension&, const ToCSSConversionOptions&) -> CSSCalc::Child;
static auto toCSS(const IndirectNode<Blend>&, const ToCSSConversionOptions&) -> CSSCalc::Child;
template<typename CalculationOp> auto toCSS(const IndirectNode<CalculationOp>&, const ToCSSConversionOptions&) -> CSSCalc::Child;

static auto toStyle(const CSSCalc::Random::Sharing&, const ToStyleConversionOptions&) -> Random::Fixed;
static auto toStyle(const std::optional<CSSCalc::Child>&, const ToStyleConversionOptions&) -> std::optional<Child>;
static auto toStyle(const CSS::Keyword::None&, const ToStyleConversionOptions&) -> CSS::Keyword::None;
static auto toStyle(const CSSCalc::ChildOrNone&, const ToStyleConversionOptions&) -> ChildOrNone;
static auto toStyle(const CSSCalc::Children&, const ToStyleConversionOptions&) -> Children;
static auto toStyle(const CSSCalc::Child&, const ToStyleConversionOptions&) -> Child;
static auto toStyle(const CSSCalc::Number&, const ToStyleConversionOptions&) -> Child;
static auto toStyle(const CSSCalc::Percentage&, const ToStyleConversionOptions&) -> Child;
static auto toStyle(const CSSCalc::CanonicalDimension&, const ToStyleConversionOptions&) -> Child;
static auto toStyle(const CSSCalc::NonCanonicalDimension&, const ToStyleConversionOptions&) -> Child;
static auto toStyle(const CSSCalc::Symbol&, const ToStyleConversionOptions&) -> Child;
static auto toStyle(const CSSCalc::SiblingCount&, const ToStyleConversionOptions&) -> Child;
static auto toStyle(const CSSCalc::SiblingIndex&, const ToStyleConversionOptions&) -> Child;
static auto toStyle(const CSSCalc::IndirectNode<CSSCalc::Anchor>&, const ToStyleConversionOptions&) -> Child;
static auto toStyle(const CSSCalc::IndirectNode<CSSCalc::AnchorSize>&, const ToStyleConversionOptions&) -> Child;
template<typename Op> auto toStyle(const CSSCalc::IndirectNode<Op>&, const ToStyleConversionOptions&) -> Child;

static CSSCalc::CanonicalDimension::Dimension NODELETE determineCanonicalDimension(CSS::Category category)
{
    switch (category) {
    case CSS::Category::LengthPercentage:
        return CSSCalc::CanonicalDimension::Dimension::Length;

    case CSS::Category::AnglePercentage:
        return CSSCalc::CanonicalDimension::Dimension::Angle;

    case CSS::Category::Integer:
    case CSS::Category::Number:
    case CSS::Category::Percentage:
    case CSS::Category::Length:
    case CSS::Category::Angle:
    case CSS::Category::Time:
    case CSS::Category::Frequency:
    case CSS::Category::Resolution:
    case CSS::Category::Flex:
        break;
    }

    ASSERT_NOT_REACHED();
    return CSSCalc::CanonicalDimension::Dimension::Length;
}

// MARK: - From

CSSCalc::Random::Sharing toCSS(const Random::Fixed& randomFixed, const ToCSSConversionOptions&)
{
    return CSSCalc::Random::SharingFixed { randomFixed.baseValue };
}

CSS::Keyword::None NODELETE toCSS(const CSS::Keyword::None& none, const ToCSSConversionOptions&)
{
    return none;
}

CSSCalc::ChildOrNone toCSS(const ChildOrNone& root, const ToCSSConversionOptions& options)
{
    return WTF::switchOn(root, [&](const auto& root) { return CSSCalc::ChildOrNone { toCSS(root, options) }; });
}

CSSCalc::Children toCSS(const Children& children, const ToCSSConversionOptions& options)
{
    return WTF::map(children.value, [&](const auto& child) -> CSSCalc::Child { return toCSS(child, options); });
}

std::optional<CSSCalc::Child> toCSS(const std::optional<Child>& root, const ToCSSConversionOptions& options)
{
    if (root)
        return toCSS(*root, options);
    return std::nullopt;
}

CSSCalc::Child toCSS(const Child& root, const ToCSSConversionOptions& options)
{
    return WTF::switchOn(root, [&](const auto& root) { return toCSS(root, options); });
}

CSSCalc::Child toCSS(const Number& number, const ToCSSConversionOptions&)
{
    return CSSCalc::makeChild(CSSCalc::Number { .value = number.value });
}

CSSCalc::Child toCSS(const Percentage& percentage, const ToCSSConversionOptions& options)
{
    return CSSCalc::makeChild(CSSCalc::Percentage { .value = percentage.value, .hint = CSSCalc::Type::determinePercentHint(options.simplification.category) });
}

CSSCalc::Child toCSS(const Dimension& root, const ToCSSConversionOptions& options)
{
    switch (options.canonicalDimension) {
    case CSSCalc::CanonicalDimension::Dimension::Length:
        return CSSCalc::makeChild(CSSCalc::CanonicalDimension { .value = Style::adjustFloatForAbsoluteZoom(root.value, options.style), .dimension = options.canonicalDimension });

    case CSSCalc::CanonicalDimension::Dimension::Angle:
    case CSSCalc::CanonicalDimension::Dimension::Time:
    case CSSCalc::CanonicalDimension::Dimension::Frequency:
    case CSSCalc::CanonicalDimension::Dimension::Resolution:
    case CSSCalc::CanonicalDimension::Dimension::Flex:
        break;
    }

    return CSSCalc::makeChild(CSSCalc::CanonicalDimension { .value = root.value, .dimension = options.canonicalDimension });
}

CSSCalc::Child toCSS(const IndirectNode<Blend>& root, const ToCSSConversionOptions& options)
{
    // FIXME: (http://webkit.org/b/122036) Create a CSSCalc::Tree equivalent of Blend.

    auto createBlendHalf = [](const auto& child, const auto& options, auto progress) -> CSSCalc::Child {
        auto product = multiply(
            toCSS(child, options),
            CSSCalc::makeChild(CSSCalc::Number { .value = progress })
        );

        if (auto replacement = CSSCalc::simplify(product, options.simplification))
            return WTF::move(*replacement);

        auto type = toType(product);
        return CSSCalc::makeChild(WTF::move(product), *type);
    };

    auto sum = add(
        createBlendHalf(root->from, options, 1 - root->progress),
        createBlendHalf(root->to, options, root->progress)
    );

    if (auto replacement = simplify(sum, options.simplification))
        return WTF::move(*replacement);

    auto type = CSSCalc::toType(sum);
    return CSSCalc::makeChild(WTF::move(sum), *type);
}

template<typename CalculationOp> CSSCalc::Child toCSS(const IndirectNode<CalculationOp>& root, const ToCSSConversionOptions& options)
{
    using CalcOp = CSSCalc::ToCalcTreeOp<CalculationOp>;

    auto op = WTF::apply([&](const auto& ...x) { return CalcOp { toCSS(x, options)... }; } , *root);

    if (auto replacement = CSSCalc::simplify(op, options.simplification))
        return WTF::move(*replacement);

    auto type = toType(op);
    return CSSCalc::makeChild(WTF::move(op), *type);
}

// MARK: - To.

auto toStyle(const CSSCalc::Random::Sharing& randomSharing, const ToStyleConversionOptions& options) -> Random::Fixed
{
    ASSERT(options.evaluation.conversionData);
    ASSERT(options.evaluation.conversionData->styleBuilderState());

    return WTF::switchOn(randomSharing,
        [&](const CSSCalc::Random::SharingOptions& sharingOptions) -> Random::Fixed {
            if (!sharingOptions.elementShared.has_value()) {
                ASSERT(options.evaluation.conversionData->styleBuilderState()->element());
            }

            auto baseValue = protect(options.evaluation.conversionData->styleBuilderState())->lookupCSSRandomBaseValue(
                sharingOptions.identifier,
                sharingOptions.elementShared
            );

            return Random::Fixed { baseValue };
        },
        [&](const CSSCalc::Random::SharingFixed& sharingFixed) -> Random::Fixed {
            return WTF::switchOn(sharingFixed.value,
                [&](const CSS::Number<CSS::ClosedUnitRange>::Raw& raw) -> Random::Fixed {
                    return Random::Fixed { raw.value };
                },
                [&](const CSS::Number<CSS::ClosedUnitRange>::Calc& calc) -> Random::Fixed {
                    return Random::Fixed { calc.evaluate(CSS::Category::Number, *protect(options.evaluation.conversionData->styleBuilderState())) };
                }
            );
        }
    );
}

std::optional<Child> toStyle(const std::optional<CSSCalc::Child>& optionalChild, const ToStyleConversionOptions& options)
{
    if (optionalChild)
        return toStyle(*optionalChild, options);
    return std::nullopt;
}

CSS::Keyword::None NODELETE toStyle(const CSS::Keyword::None& none, const ToStyleConversionOptions&)
{
    return none;
}

ChildOrNone toStyle(const CSSCalc::ChildOrNone& root, const ToStyleConversionOptions& options)
{
    return WTF::switchOn(root, [&](const auto& root) { return ChildOrNone { toStyle(root, options) }; });
}

Children toStyle(const CSSCalc::Children& children, const ToStyleConversionOptions& options)
{
    return WTF::map(children, [&](const auto& child) { return toStyle(child, options); });
}

Child toStyle(const CSSCalc::Child& root, const ToStyleConversionOptions& options)
{
    return WTF::switchOn(root, [&](const auto& root) { return toStyle(root, options); });
}

Child toStyle(const CSSCalc::Number& root, const ToStyleConversionOptions&)
{
    return number(root.value);
}

Child toStyle(const CSSCalc::Percentage& root, const ToStyleConversionOptions&)
{
    return percentage(root.value);
}

Child toStyle(const CSSCalc::CanonicalDimension& root, const ToStyleConversionOptions& options)
{
    ASSERT(options.evaluation.conversionData);

    switch (root.dimension) {
    case CSSCalc::CanonicalDimension::Dimension::Length:
        return dimension(Style::computeNonCalcLengthDouble(root.value, CSS::LengthUnit::Px, *options.evaluation.conversionData));

    case CSSCalc::CanonicalDimension::Dimension::Angle:
    case CSSCalc::CanonicalDimension::Dimension::Time:
    case CSSCalc::CanonicalDimension::Dimension::Frequency:
    case CSSCalc::CanonicalDimension::Dimension::Resolution:
    case CSSCalc::CanonicalDimension::Dimension::Flex:
        break;
    }

    return dimension(root.value);
}

Child toStyle(const CSSCalc::NonCanonicalDimension&, const ToStyleConversionOptions&)
{
    ASSERT_NOT_REACHED("Non-canonical numeric values are not supported in the Tree");
    return number(0);
}

Child toStyle(const CSSCalc::Symbol&, const ToStyleConversionOptions&)
{
    ASSERT_NOT_REACHED("Unevaluated symbols are not supported in the Tree");
    return number(0);
}

Child toStyle(const CSSCalc::SiblingCount&, const ToStyleConversionOptions&)
{
    ASSERT_NOT_REACHED("Unevaluated sibling-count() functions are not supported in the Tree");
    return number(0);
}

Child toStyle(const CSSCalc::SiblingIndex&, const ToStyleConversionOptions&)
{
    ASSERT_NOT_REACHED("Unevaluated sibling-index() functions are not supported in the Tree");
    return number(0);
}

Child toStyle(const CSSCalc::IndirectNode<CSSCalc::Anchor>&, const ToStyleConversionOptions&)
{
    ASSERT_NOT_REACHED("Unevaluated anchor() functions are not supported in the Tree");
    return number(0);
}

Child toStyle(const CSSCalc::IndirectNode<CSSCalc::AnchorSize>&, const ToStyleConversionOptions&)
{
    ASSERT_NOT_REACHED("Unevaluated anchor-size() functions are not supported in the Tree");
    return number(0);
}

template<typename Op> Child toStyle(const CSSCalc::IndirectNode<Op>& root, const ToStyleConversionOptions& options)
{
    using CalculationOp = CSSCalc::ToCalculationTreeOp<Op>;

    return makeChild(WTF::apply([&](const auto& ...x) { return CalculationOp { toStyle(x, options)... }; } , *root));
}

// MARK: - Exposed functions

CSSCalc::Tree toCSS(const Tree& tree, const ToCSSOptions& toCSSOptions)
{
    auto conversionOptions = ToCSSConversionOptions {
        .canonicalDimension = determineCanonicalDimension(toCSSOptions.category),
        .simplification = CSSCalc::SimplificationOptions {
            .category = toCSSOptions.category,
            .range = toCSSOptions.range,
            .conversionData = std::nullopt,
            .symbolTable = { },
            .allowZeroValueLengthRemovalFromSum = true,
        },
        .style = toCSSOptions.style,
    };

    auto root = toCSS(tree.root, conversionOptions);
    auto type = CSSCalc::getType(root);

    return CSSCalc::Tree {
        .root = WTF::move(root),
        .type = type,
        .stage = CSSCalc::Stage::Computed,
    };
}

Tree toStyle(const CSSCalc::Tree& tree, const ToStyleOptions& toStyleOptions)
{
    ASSERT(toStyleOptions.category == CSS::Category::LengthPercentage || toStyleOptions.category == CSS::Category::AnglePercentage);

    auto simplificationOptions = CSSCalc::SimplificationOptions {
        .category = toStyleOptions.category,
        .range = toStyleOptions.range,
        .conversionData = toStyleOptions.conversionData,
        .symbolTable = toStyleOptions.symbolTable,
        .allowZeroValueLengthRemovalFromSum = true,
    };
    auto simplifiedTree = CSSCalc::copyAndSimplify(tree, simplificationOptions);

    auto conversionOptions = ToStyleConversionOptions {
        .evaluation = CSSCalc::EvaluationOptions {
            .category = toStyleOptions.category,
            .range = toStyleOptions.range,
            .conversionData = toStyleOptions.conversionData,
            .symbolTable = toStyleOptions.symbolTable,
        }
    };
    return Tree { toStyle(simplifiedTree.root, conversionOptions) };
}

} // namespace Calculation
} // namespace Style
} // namespace WebCore
