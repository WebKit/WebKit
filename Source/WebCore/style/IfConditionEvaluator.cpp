/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "IfConditionEvaluator.h"

#include "CSSSupportsParser.h"
#include "CSSTokenizer.h"
#include "CommonAtomStrings.h"
#include "ContainerQueryFeatures.h"
#include "ContainerQueryParser.h"
#include "Document.h"
#include "DocumentView.h"
#include "MediaQueryEvaluator.h"
#include "MediaQueryParser.h"
#include "RenderElement.h"
#include "StyleBuilder.h"

namespace WebCore {
namespace Style {

auto IfConditionEvaluator::evaluate(CSSParserTokenRange branchCondition) -> Result
{
    // Evaluates <boolean-expr[ <if-test> ]> using the container query parser. The else keyword (the
    // other <if-condition> alternative) is resolved earlier, in substituteIfArgumentGrammar.
    // <if-test> =
    //   supports( [ <ident> : <declaration-value> ] | <supports-condition> ) |
    //   media( <media-feature> | <media-condition> ) |
    //   style( <style-query> )
    // https://drafts.csswg.org/css-values-5/#typedef-if-condition

    // Unsupported features (like size queries) evaluate to Unknown.
    auto parserContext = MediaQueryParserContext { m_context };
    auto condition = CQ::ContainerQueryParser::consumeCondition(branchCondition, parserContext);
    if (!condition || !branchCondition.atEnd())
        return Result::Invalid;

    auto& state = m_styleBuilder.state();
    MQ::FeatureEvaluationContext evaluationContext {
        state.document(),
        state.cssToLengthConversionData(),
        nullptr
    };

    return evaluateCondition(*condition, evaluationContext) == MQ::EvaluationResult::True ? Result::True : Result::False;
}

MQ::EvaluationResult IfConditionEvaluator::evaluateQueryInParens(const MQ::QueryInParens& queryInParens, const MQ::FeatureEvaluationContext& context) const
{
    // media() and supports() functions fall through to GeneralEnclosed during parsing. The generic
    // evaluator treats those as unknown, so evaluate them here. Everything else uses the base logic.
    // FIXME: Parse these into dedicated media/supports FeatureSchemas that carry the argument tokens.
    // That avoids re-tokenizing the serialized argument text below, and the schema pointer would
    // identify the function (like style()) instead of comparing names here.
    if (auto* enclosed = std::get_if<MQ::GeneralEnclosed>(&queryInParens)) {
        if (equalLettersIgnoringASCIICase(enclosed->name, "media"_s)) {
            // Wrap in parens so bare features like "max-width: 1px" parse as media conditions.
            auto wrappedText = makeString('(', enclosed->text, ')');
            CSSTokenizer tokenizer(wrappedText);
            auto mediaQuery = MQ::MediaQueryParser::parseCondition(tokenizer.tokenRange(), m_context);
            if (!mediaQuery || !mediaQuery->condition)
                return MQ::EvaluationResult::Unknown;

            auto& document = m_styleBuilder.state().document();
            CheckedPtr view = document.view();
            MQ::MediaQueryEvaluator evaluator { view ? view->mediaType() : screenAtom(), document };
            return evaluator.evaluate(*mediaQuery) ? MQ::EvaluationResult::True : MQ::EvaluationResult::False;
        }

        if (equalLettersIgnoringASCIICase(enclosed->name, "supports"_s)) {
            auto result = CSSSupportsParser::supportsCondition(enclosed->text, m_context, CSSSupportsParser::ParsingMode::AllowBareDeclarationAndGeneralEnclosed);
            if (result == CSSSupportsParser::Invalid)
                return MQ::EvaluationResult::Unknown;
            return result == CSSSupportsParser::Supported ? MQ::EvaluationResult::True : MQ::EvaluationResult::False;
        }
    }

    return MQ::GenericMediaQueryEvaluator<IfConditionEvaluator>::evaluateQueryInParens(queryInParens, context);
}

MQ::EvaluationResult IfConditionEvaluator::evaluateFeature(const MQ::Feature& feature, const MQ::FeatureEvaluationContext& context) const
{
    // The only feature valid in an <if-test> is style(). The container query parser also accepts size
    // and scroll-state features. Treat those as unknown rather than evaluating them.
    if (feature.schema != &CQ::Features::style())
        return MQ::EvaluationResult::Unknown;

    // Force resolution of the queried custom property. Range queries comparing
    // literals (e.g. style(5 > 3)) have no custom-property name to resolve.
    if (!feature.name.isNull())
        m_styleBuilder.applyCustomProperty(feature.name);

    return feature.schema->evaluate(feature, context);
}

} // namespace Style
} // namespace WebCore
