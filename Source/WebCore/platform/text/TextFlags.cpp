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
#include "TextFlags.h"

#include "FontCascade.h"
#include <functional>
#include <numeric>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF::TextStream& operator<<(TextStream& ts, ExpansionBehavior::Behavior behavior)
{
    switch (behavior) {
    case ExpansionBehavior::Behavior::Forbid: ts << "forbid"; break;
    case ExpansionBehavior::Behavior::Allow: ts << "allow"; break;
    case ExpansionBehavior::Behavior::Force: ts << "force"; break;
    }
    return ts;
}

WTF::TextStream& operator<<(TextStream& ts, ExpansionBehavior expansionBehavior)
{
    ts << expansionBehavior.left;
    ts << ' ';
    ts << expansionBehavior.right;
    return ts;
}

WTF::TextStream& operator<<(TextStream& ts, Kerning kerning)
{
    switch (kerning) {
    case Kerning::Auto: ts << "auto"; break;
    case Kerning::Normal: ts << "normal"; break;
    case Kerning::NoShift: ts << "no-shift"; break;
    }
    return ts;
}

template <typename T> using BinOp = std::function<T(T, T)>;

template <typename T>
T reduce(const std::vector<T>& vec, const BinOp<T>& binOp, std::optional<T> initial = { })
{
    if (vec.empty())
        return initial ? *initial : T { };

    auto accumulator = initial ? *initial : vec[0];
    auto start = initial ? 0 : 1;
    for (size_t i = start ; i < vec.size() ; i++)
        accumulator = binOp(accumulator, vec[i]);

    return accumulator;
}

WTF::TextStream& operator<<(TextStream& ts, FontVariantAlternates alternates)
{
    if (alternates.isNormal())
        ts << "normal";
    else {
        auto values = alternates.values();
        std::vector<String> result;
        if (values.stylistic)
            result.push_back("stylistic(" + *values.stylistic + ")");
        if (values.historicalForms)
            result.push_back("historical-forms"_s);
        if (values.styleset)
            result.push_back("styleset(" + *values.styleset + ")");
        if (values.characterVariant)
            result.push_back("character-variant(" + *values.characterVariant + ")");
        if (values.swash)
            result.push_back("swash(" + *values.swash + ")");
        if (values.ornaments)
            result.push_back("ornaments(" + *values.ornaments + ")");
        if (values.annotation)
            result.push_back("annotation(" + *values.annotation + ")");
        
        BinOp<String> addWithSpace = [](auto a, auto b) { 
            return a + " " + b;
        };
        ts << reduce(result, addWithSpace);
    }
    return ts;
}

void add(Hasher& hasher, const FontVariantAlternatesValues& key)
{
    add(hasher, key.historicalForms, key.stylistic, key.styleset, key.characterVariant, key.swash, key.ornaments, key.annotation);
}

void add(Hasher& hasher, const FontVariantAlternates& key)
{
    add(hasher, key.m_val.index());
    if (key.isNormal())
        add(hasher, 0);
    else
        add(hasher, key.values());
}

WTF::TextStream& operator<<(TextStream& ts, FontVariantPosition position)
{
    switch (position) {
    case FontVariantPosition::Normal: ts << "normal"; break;
    case FontVariantPosition::Subscript: ts << "subscript"; break;
    case FontVariantPosition::Superscript: ts << "superscript"; break;
    }
    return ts;
}

WTF::TextStream& operator<<(TextStream& ts, FontVariantCaps caps)
{
    switch (caps) {
    case FontVariantCaps::Normal: ts << "normal"; break;
    case FontVariantCaps::Small: ts << "small"; break;
    case FontVariantCaps::AllSmall: ts << "all-small"; break;
    case FontVariantCaps::Petite: ts << "petite"; break;
    case FontVariantCaps::AllPetite: ts << "all-petite"; break;
    case FontVariantCaps::Unicase: ts << "unicase"; break;
    case FontVariantCaps::Titling: ts << "titling"; break;
    }
    return ts;
}

FeaturesMap computeFeatureSettingsFromVariants(const FontVariantSettings& variantSettings)
{
    FeaturesMap features;
    
    switch (variantSettings.commonLigatures) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        features.set(fontFeatureTag("liga"), 1);
        features.set(fontFeatureTag("clig"), 1);
        break;
    case FontVariantLigatures::No:
        features.set(fontFeatureTag("liga"), 0);
        features.set(fontFeatureTag("clig"), 0);
        break;
    }

    switch (variantSettings.discretionaryLigatures) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        features.set(fontFeatureTag("dlig"), 1);
        break;
    case FontVariantLigatures::No:
        features.set(fontFeatureTag("dlig"), 0);
        break;
    }

    switch (variantSettings.historicalLigatures) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        features.set(fontFeatureTag("hlig"), 1);
        break;
    case FontVariantLigatures::No:
        features.set(fontFeatureTag("hlig"), 0);
        break;
    }

    switch (variantSettings.contextualAlternates) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        features.set(fontFeatureTag("calt"), 1);
        break;
    case FontVariantLigatures::No:
        features.set(fontFeatureTag("calt"), 0);
        break;
    }

    switch (variantSettings.position) {
    case FontVariantPosition::Normal:
        break;
    case FontVariantPosition::Subscript:
        features.set(fontFeatureTag("subs"), 1);
        break;
    case FontVariantPosition::Superscript:
        features.set(fontFeatureTag("sups"), 1);
        break;
    }

    switch (variantSettings.caps) {
    case FontVariantCaps::Normal:
        break;
    case FontVariantCaps::AllSmall:
        features.set(fontFeatureTag("c2sc"), 1);
        FALLTHROUGH;
    case FontVariantCaps::Small:
        features.set(fontFeatureTag("smcp"), 1);
        break;
    case FontVariantCaps::AllPetite:
        features.set(fontFeatureTag("c2pc"), 1);
        FALLTHROUGH;
    case FontVariantCaps::Petite:
        features.set(fontFeatureTag("pcap"), 1);
        break;
    case FontVariantCaps::Unicase:
        features.set(fontFeatureTag("unic"), 1);
        break;
    case FontVariantCaps::Titling:
        features.set(fontFeatureTag("titl"), 1);
        break;
    }

    switch (variantSettings.numericFigure) {
    case FontVariantNumericFigure::Normal:
        break;
    case FontVariantNumericFigure::LiningNumbers:
        features.set(fontFeatureTag("lnum"), 1);
        break;
    case FontVariantNumericFigure::OldStyleNumbers:
        features.set(fontFeatureTag("onum"), 1);
        break;
    }

    switch (variantSettings.numericSpacing) {
    case FontVariantNumericSpacing::Normal:
        break;
    case FontVariantNumericSpacing::ProportionalNumbers:
        features.set(fontFeatureTag("pnum"), 1);
        break;
    case FontVariantNumericSpacing::TabularNumbers:
        features.set(fontFeatureTag("tnum"), 1);
        break;
    }

    switch (variantSettings.numericFraction) {
    case FontVariantNumericFraction::Normal:
        break;
    case FontVariantNumericFraction::DiagonalFractions:
        features.set(fontFeatureTag("frac"), 1);
        break;
    case FontVariantNumericFraction::StackedFractions:
        features.set(fontFeatureTag("afrc"), 1);
        break;
    }

    switch (variantSettings.numericOrdinal) {
    case FontVariantNumericOrdinal::Normal:
        break;
    case FontVariantNumericOrdinal::Yes:
        features.set(fontFeatureTag("ordn"), 1);
        break;
    }

    switch (variantSettings.numericSlashedZero) {
    case FontVariantNumericSlashedZero::Normal:
        break;
    case FontVariantNumericSlashedZero::Yes:
        features.set(fontFeatureTag("zero"), 1);
        break;
    }

    if (!variantSettings.alternates.isNormal()) {
        auto values = variantSettings.alternates.values();
        if (values.historicalForms)
            features.set(fontFeatureTag("hist"), 1);
        
        // TODO: handle other tags.
        // https://bugs.webkit.org/show_bug.cgi?id=246121
    }

    switch (variantSettings.eastAsianVariant) {
    case FontVariantEastAsianVariant::Normal:
        break;
    case FontVariantEastAsianVariant::Jis78:
        features.set(fontFeatureTag("jp78"), 1);
        break;
    case FontVariantEastAsianVariant::Jis83:
        features.set(fontFeatureTag("jp83"), 1);
        break;
    case FontVariantEastAsianVariant::Jis90:
        features.set(fontFeatureTag("jp90"), 1);
        break;
    case FontVariantEastAsianVariant::Jis04:
        features.set(fontFeatureTag("jp04"), 1);
        break;
    case FontVariantEastAsianVariant::Simplified:
        features.set(fontFeatureTag("smpl"), 1);
        break;
    case FontVariantEastAsianVariant::Traditional:
        features.set(fontFeatureTag("trad"), 1);
        break;
    }

    switch (variantSettings.eastAsianWidth) {
    case FontVariantEastAsianWidth::Normal:
        break;
    case FontVariantEastAsianWidth::Full:
        features.set(fontFeatureTag("fwid"), 1);
        break;
    case FontVariantEastAsianWidth::Proportional:
        features.set(fontFeatureTag("pwid"), 1);
        break;
    }

    switch (variantSettings.eastAsianRuby) {
    case FontVariantEastAsianRuby::Normal:
        break;
    case FontVariantEastAsianRuby::Yes:
        features.set(fontFeatureTag("ruby"), 1);
        break;
    }
    
    return features;
}

} // namespace WebCore
