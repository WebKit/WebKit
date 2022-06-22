/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

WI.FontDetailsPanel = class FontDetailsPanel extends WI.StyleDetailsPanel
{
    constructor(delegate)
    {
        const className = "font";
        const identifier = "font";
        const label = WI.UIString("Font", "Font @ Font Details Sidebar Title", "Title for the Font details sidebar.");
        super(delegate, className, identifier, label);

        this._fontPropertiesMap = new Map;
        this._fontVariationsMap = new Map;
        this._fontFeaturesMap = new Map;
    }

    // Public

    refresh(significantChange)
    {
        super.refresh(significantChange);

        let fontMap = this.nodeStyles.computedPrimaryFont?.calculateFontProperties(this.nodeStyles) ?? {};
        this._fontPropertiesMap = fontMap.propertiesMap ?? new Map;
        this._fontVariationsMap = fontMap.variationsMap ?? new Map;
        this._fontFeaturesMap = fontMap.featuresMap ?? new Map;

        this._fontNameRow.value = this.nodeStyles.computedPrimaryFont.name;

        this._fontSizeRow.value = this._formatSizeValue(this._fontPropertiesMap.get("font-size"));
        this._fontStyleRow.value = this._formatStyleValue(this._fontPropertiesMap.get("font-style"));
        this._fontWeightRow.value = this._formatSimpleSingleValue(this._fontPropertiesMap.get("font-weight"), "wght", "%s");
        this._fontStretchRow.value = this._formatSimpleSingleValue(this._fontPropertiesMap.get("font-stretch"), "wdth", WI.UIString("%s%%", "%s%% @ Font Details Sidebar", "A single value expressed as a percentage where the value has already been converted from a number to a string."));

        this._fontVariantLigaturesRow.value = this._formatLigatureValue(this._fontPropertiesMap.get("font-variant-ligatures"));
        this._fontVariantPositionRow.value = this._formatPositionValue(this._fontPropertiesMap.get("font-variant-position"));
        this._fontVariantCapsRow.value = this._formatCapitalsValue(this._fontPropertiesMap.get("font-variant-caps"));
        this._fontVariantNumericRow.value = this._formatNumericValue(this._fontPropertiesMap.get("font-variant-numeric"));
        this._fontVariantAlternatesRow.value = this._formatAlternatesValue(this._fontPropertiesMap.get("font-variant-alternates"));
        this._fontVariantEastAsianRow.value = this._formatEastAsianValue(this._fontPropertiesMap.get("font-variant-east-asian"));

        let featureRows = [];
        for (let [key, value] of this._fontFeaturesMap)
            featureRows.push(new WI.DetailsSectionSimpleRow(key, value));
        this._fontAdditionalFeaturesGroup.rows = featureRows;
        this._fontAdditionalFeaturesGroup.hidden = !featureRows.length;

        let variationRows = [];
        for (let [key, value] of this._fontVariationsMap) {
            let name = value.name ? WI.UIString("%s (%s)").format(value.name, key) : key;
            variationRows.push(new WI.DetailsSectionSimpleRow(name, this._formatVariationValue(value)));
        }
        if (!variationRows.length) {
            let emptyRow = new WI.DetailsSectionRow(WI.UIString("No additional variation axes.", "No additional variation axes. @ Font Details Sidebar", "Message shown when there are no additional variation axes to show."));
            emptyRow.showEmptyMessage();
            variationRows.push(emptyRow);
        }
        this._fontVariationsGroup.rows = variationRows;
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        // Identity
        this._fontNameRow = new WI.DetailsSectionSimpleRow(WI.UIString("Name", "Name @ Font Details Sidebar Property", "Property title for the family name of the font."));
        let previewGroup = new WI.DetailsSectionGroup([this._fontNameRow]);

        let fontNameSection = new WI.DetailsSection("font-identity", WI.UIString("Identity", "Identity @ Font Details Sidebar Section", "Section title for font identity information."), [previewGroup]);
        this.element.appendChild(fontNameSection.element);

        // Basic Properties
        this._fontSizeRow = new WI.DetailsSectionSimpleRow(WI.UIString("Size", "Size @ Font Details Sidebar Property", "Property title for `font-size`."));
        this._fontStyleRow = new WI.DetailsSectionSimpleRow(WI.UIString("Style", "Style @ Font Details Sidebar Property", "Property title for `font-style`."));
        this._fontWeightRow = new WI.DetailsSectionSimpleRow(WI.UIString("Weight", "Weight @ Font Details Sidebar Property", "Property title for `font-weight`."));
        this._fontStretchRow = new WI.DetailsSectionSimpleRow(WI.UIString("Stretch", "Stretch @ Font Details Sidebar Property", "Property title for `font-stretch`."));
        let basicPropertiesGroup = new WI.DetailsSectionGroup([this._fontSizeRow, this._fontStyleRow, this._fontWeightRow, this._fontStretchRow]);

        let fontBasicPropertiesSection = new WI.DetailsSection("font-basic-properties", WI.UIString("Basic Properties", "Basic Properties @ Font Details Sidebar Section", "Section title for basic font properties."), [basicPropertiesGroup]);
        this.element.appendChild(fontBasicPropertiesSection.element);

        // Feature Properties
        this._fontVariantLigaturesRow = new WI.DetailsSectionSimpleRow(WI.UIString("Ligatures", "Ligatures @ Font Details Sidebar Property", "Property title for `font-variant-ligatures`."));
        this._fontVariantPositionRow = new WI.DetailsSectionSimpleRow(WI.UIString("Position", "Position @ Font Details Sidebar Property", "Property title for `font-variant-position`."));
        this._fontVariantCapsRow = new WI.DetailsSectionSimpleRow(WI.UIString("Capitals", "Capitals @ Font Details Sidebar Property", "Property title for `font-variant-caps`."));
        this._fontVariantNumericRow = new WI.DetailsSectionSimpleRow(WI.UIString("Numeric", "Numeric @ Font Details Sidebar Property", "Property title for `font-variant-numeric`."));
        this._fontVariantAlternatesRow = new WI.DetailsSectionSimpleRow(WI.UIString("Alternate Glyphs", "Alternate Glyphs @ Font Details Sidebar Property", "Property title for `font-variant-alternates`."));
        this._fontVariantEastAsianRow = new WI.DetailsSectionSimpleRow(WI.UIString("East Asian", "East Asian @ Font Details Sidebar Property", "Property title for `font-variant-east-asian`."));
        let featurePropertiesGroup = new WI.DetailsSectionGroup([this._fontVariantLigaturesRow, this._fontVariantPositionRow, this._fontVariantCapsRow, this._fontVariantNumericRow, this._fontVariantAlternatesRow, this._fontVariantEastAsianRow]);

        this._fontAdditionalFeaturesGroup = new WI.DetailsSectionGroup;

        let fontFeaturePropertiesSection = new WI.DetailsSection("font-feature-properties", WI.UIString("Feature Properties", "Feature Properties @ Font Details Sidebar Section", "Section title for font feature properties."), [featurePropertiesGroup, this._fontAdditionalFeaturesGroup]);
        this.element.appendChild(fontFeaturePropertiesSection.element);

        // Variation Properties
        this._fontVariationsGroup = new WI.DetailsSectionGroup;

        let fontVariationPropertiesSection = new WI.DetailsSection("font-variation-properties", WI.UIString("Variation Properties", "Variation Properties @ Font Details Sidebar Section", "Section title for font variation properties."), [this._fontVariationsGroup]);
        this.element.appendChild(fontVariationPropertiesSection.element);
    }

    _formatSizeValue(property)
    {
        return property.value;
    }

    _formatStyleValue(property)
    {
        // `slnt` is expressed in negated values relative to `oblique`.
        if (this._hasVariationValue(property, "slnt"))
            return WI.UIString("Oblique %ddeg", "Oblique %ddeg @ Font Details Sidebar Property Value", "Property value for oblique text.").format(-1 * parseFloat(property.variations.get("slnt").value));

        if (this._hasVariationValue(property, "ital"))
            return WI.UIString("Italic", "Italic @ Font Details Sidebar Property Value", "Property value for oblique text.");

        if (property.value === "normal")
            return WI.UIString("Normal", "Normal @ Font Details Sidebar Property Value", "Property value for any `normal` CSS value.");

        return property.value;
    }

    _formatSimpleSingleValue(property, variationTag, variationFormat)
    {
        let result;
        if (this._hasVariationValue(property, variationTag))
            result = variationFormat.format(this._formatAxisValueAsString(property.variations.get(variationTag).value));
        else
            result = this._formatAxisValueAsString(property.value);

        if (this._hasVariationValue(property, variationTag, {optional: true})) {
            let axis = property.variations.get(variationTag);
            return this._createVariationValueElement(result, axis.minimumValue, axis.maximumValue, axis.defaultValue);
        }

        return result;
    }

    _formatVariationValue(variation)
    {
        let value = variation.value || variation.value === 0 ? variation.value : variation.defaultValue;
        return this._createVariationValueElement(this._formatAxisValueAsString(value), variation.minimumValue, variation.maximumValue, variation.defaultValue);
    }

    _createVariationValueElement(value, minimumValue, maximumValue, defaultValue)
    {
        let valueElement = document.createElement("div");
        valueElement.textContent = value;

        let secondaryElement = valueElement.appendChild(document.createElement("span"));
        secondaryElement.className = "secondary";
        secondaryElement.textContent = WI.UIString(" (Range: %s-%s, Default: %s)", " (Range: %s-%s, Default: %s) @ Font Details Sidebar", "A range and default value for a single variation axis of a font.").format(this._formatAxisValueAsString(minimumValue), this._formatAxisValueAsString(maximumValue), this._formatAxisValueAsString(defaultValue));
        
        return valueElement;
    }

    _formatAxisValueAsString(value)
    {
        const options = {
            minimumFractionDigits: 0,
            maximumFractionDigits: 2,
            useGrouping: false,
        }
        return value.toLocaleString(undefined, options);
    }

    _formatSimpleFeatureValues(property, features, noMatchesResult)
    {
        let valueParts = property.value.match(WI.Font.SettingPattern);
        let results = [];
        // Only one result should be pushed per group.
        let resultGroups = new Set;

        for (let feature of features) {
            if ((!feature.group || !resultGroups.has(feature.group)) && this._featureIsEnabled(property, feature.tag, valueParts.includes(feature.name))) {
                results.push(feature.result);
                resultGroups.add(feature.group);
            }
        }

        if (results.length)
            return results.join(WI.UIString(", "));
        return noMatchesResult;
    }

    _formatLigatureValue(property)
    {
        let valueParts = property.value.match(WI.Font.SettingPattern);

        let results = [];

        // Common ligatures is a special case where it is on by default, so we have to check if they are disabled.
        if (!this._featureIsEnabled(property, WI.unlocalizedString("clig"), !valueParts.includes(WI.unlocalizedString("no-common-ligatures"))))
            results.push(WI.UIString("Disabled Common", "Disabled Common @ Font Details Sidebar Property Value", "Property value for `font-variant-ligatures: no-common-ligatures`."));
        else
            results.push(WI.UIString("Common", "Common @ Font Details Sidebar Property Value", "Property value for `font-variant-ligatures: common-ligatures`."));

        // Add the other ligature features only if there was at least one to avoid concatinating with an empty string.
        let otherResults = this._formatSimpleFeatureValues(property, [
            {
                tag: WI.unlocalizedString("dlig"),
                name: WI.unlocalizedString("discretionary-ligatures"),
                result: WI.UIString("Discretionary", "Discretionary @ Font Details Sidebar Property Value", "Property value for `font-variant-ligatures: discretionary-ligatures`."),
            },
            {
                tag: WI.unlocalizedString("hlig"),
                name: WI.unlocalizedString("historical-ligatures"),
                result: WI.UIString("Historical", "Historical @ Font Details Sidebar Property Value", "Property value for `font-variant-ligatures: historical-ligatures`."),
            },
            {
                tag: WI.unlocalizedString("calt"),
                name: WI.unlocalizedString("contextual"),
                result: WI.UIString("Contextual Alternates", "Contextual Alternates @ Font Details Sidebar Property Value", "Property value for `font-variant-ligatures: contextual`."),
            },
        ]);
        if (otherResults)
            results.push(otherResults);

        return results.join(WI.UIString(", "));
    }

    _formatPositionValue(property)
    {
        return this._formatSimpleFeatureValues(property, [
            {
                tag: WI.unlocalizedString("subs"),
                name: WI.unlocalizedString("sub"),
                result: WI.UIString("Subscript", "Subscript @ Font Details Sidebar Property Value", "Property value for `font-variant-position: sub`."),
                group: "position",
            },
            {
                tag: WI.unlocalizedString("sups"),
                name: WI.unlocalizedString("super"),
                result: WI.UIString("Superscript", "Superscript @ Font Details Sidebar Property Value", "Property value for `font-variant-position: super`."),
                group: "position",
            },
        ], WI.UIString("Normal", "Normal @ Font Details Sidebar Property Value", "Property value for any `normal` CSS value."));
    }

    _formatCapitalsValue(property)
    {
        return this._formatSimpleFeatureValues(property, [
            {
                tag: WI.unlocalizedString("c2sc"),
                name: WI.unlocalizedString("all-small-caps"),
                result: WI.UIString("All Small Capitals", "All Small Capitals @ Font Details Sidebar Property Value", "Property value for `font-variant-capitals: all-small-caps`."),
                group: "capitals",
            },
            {
                tag: WI.unlocalizedString("smcp"),
                name: WI.unlocalizedString("small-caps"),
                result: WI.UIString("Small Capitals", "Small Capitals @ Font Details Sidebar Property Value", "Property value for `font-variant-capitals: small-caps`."),
                group: "capitals",
            },
            {
                tag: WI.unlocalizedString("c2pc"),
                name: WI.unlocalizedString("all-petite-caps"),
                result: WI.UIString("All Petite Capitals", "All Petite Capitals @ Font Details Sidebar Property Value", "Property value for `font-variant-capitals: all-petite-caps`."),
                group: "capitals",
            },
            {
                tag: WI.unlocalizedString("pcap"),
                name: WI.unlocalizedString("petite-caps"),
                result: WI.UIString("Petite Capitals", "Petite Capitals @ Font Details Sidebar Property Value", "Property value for `font-variant-capitals: petite-caps`."),
                group: "capitals",
            },
            {
                tag: WI.unlocalizedString("unic"),
                name: WI.unlocalizedString("unicase"),
                result: WI.UIString("Unicase", "Unicase @ Font Details Sidebar Property Value", "Property value for `font-variant-capitals: unicase`."),
                group: "capitals",
            },
            {
                tag: WI.unlocalizedString("titl"),
                name: WI.unlocalizedString("titling-caps"),
                result: WI.UIString("Titling Capitals", "Titling Capitals @ Font Details Sidebar Property Value", "Property value for `font-variant-capitals: titling-caps`."),
                group: "capitals",
            },
        ], WI.UIString("Normal", "Normal @ Font Details Sidebar Property Value", "Property value for any `normal` CSS value."));
    }

    _formatNumericValue(property)
    {
        return this._formatSimpleFeatureValues(property, [
            {
                tag: WI.unlocalizedString("lnum"),
                name: WI.unlocalizedString("lining-nums"),
                result: WI.UIString("Lining Numerals", "Lining Numerals @ Font Details Sidebar Property Value", "Property value for `font-variant-numeric: lining-nums`."),
                group: "figures",
            },
            {
                tag: WI.unlocalizedString("onum"),
                name: WI.unlocalizedString("oldstyle-nums"),
                result: WI.UIString("Old-Style Numerals", "Old-Style Numerals @ Font Details Sidebar Property Value", "Property value for `font-variant-numeric: oldstyle-nums`."),
                group: "figures",
            },
            {
                tag: WI.unlocalizedString("pnum"),
                name: WI.unlocalizedString("proportional-nums"),
                result: WI.UIString("Proportional Numerals", "Proportional Numerals @ Font Details Sidebar Property Value", "Property value for `font-variant-numeric: proportional-nums`."),
                group: "spacing",
            },
            {
                tag: WI.unlocalizedString("tnum"),
                name: WI.unlocalizedString("tabular-nums"),
                result: WI.UIString("Tabular Numerals", "Tabular Numerals @ Font Details Sidebar Property Value", "Property value for `font-variant-numeric: tabular-nums`."),
                group: "spacing",
            },
            {
                tag: WI.unlocalizedString("frac"),
                name: WI.unlocalizedString("diagonal-fractions"),
                result: WI.UIString("Diagonal Fractions", "Diagonal Fractions @ Font Details Sidebar Property Value", "Property value for `font-variant-numeric: diagonal-fractions`."),
                group: "fractions",
            },
            {
                tag: WI.unlocalizedString("afrc"),
                name: WI.unlocalizedString("stacked-fractions"),
                result: WI.UIString("Stacked Fractions", "Stacked Fractions @ Font Details Sidebar Property Value", "Property value for `font-variant-numeric: stacked-fractions`."),
                group: "fractions",
            },
            {
                tag: WI.unlocalizedString("ordn"),
                name: WI.unlocalizedString("ordinal"),
                result: WI.UIString("Ordinal Letter Forms", "Ordinal Letter Forms @ Font Details Sidebar Property Value", "Property value for `font-variant-numeric: ordinal`."),
            },
            {
                tag: WI.unlocalizedString("zero"),
                name: WI.unlocalizedString("slashed-zero"),
                result: WI.UIString("Slashed Zeros", "Slashed Zeros @ Font Details Sidebar Property Value", "Property value for `font-variant-numeric: slashed-zero`."),
            },
        ], WI.UIString("Normal", "Normal @ Font Details Sidebar Property Value", "Property value for any `normal` CSS value."));
    }

    _formatAlternatesValue(property)
    {
        return this._formatSimpleFeatureValues(property, [
            {
                tag: WI.unlocalizedString("hist"),
                name: WI.unlocalizedString("historical-forms"),
                result: WI.UIString("Historical Forms", "Historical Forms @ Font Details Sidebar Property Value", "Property value for `font-variant-alternates: historical-forms`."),
            },
        ], WI.UIString("Normal", "Normal @ Font Details Sidebar Property Value", "Property value for any `normal` CSS value."));
    }

    _formatEastAsianValue(property)
    {
        return this._formatSimpleFeatureValues(property, [
            {
                tag: WI.unlocalizedString("jp78"),
                name: WI.unlocalizedString("jis78"),
                result: WI.UIString("JIS78 Forms", "JIS78 Forms @ Font Details Sidebar Property Value", "Property value for `font-variant-alternates: jis78`."),
                group: "forms",
            },
            {
                tag: WI.unlocalizedString("jp83"),
                name: WI.unlocalizedString("jis83"),
                result: WI.UIString("JIS83 Forms", "JIS83 Forms @ Font Details Sidebar Property Value", "Property value for `font-variant-alternates: jis83`."),
                group: "forms",
            },
            {
                tag: WI.unlocalizedString("jp90"),
                name: WI.unlocalizedString("jis90"),
                result: WI.UIString("JIS90 Forms", "JIS90 Forms @ Font Details Sidebar Property Value", "Property value for `font-variant-alternates: jis90`."),
                group: "forms",
            },
            {
                tag: WI.unlocalizedString("jp04"),
                name: WI.unlocalizedString("jis04"),
                result: WI.UIString("JIS2004 Forms", "JIS2004 Forms @ Font Details Sidebar Property Value", "Property value for `font-variant-alternates: jis04`."),
                group: "forms",
            },
            {
                tag: WI.unlocalizedString("smpl"),
                name: WI.unlocalizedString("simplified"),
                result: WI.UIString("Simplified Forms", "Simplified Forms @ Font Details Sidebar Property Value", "Property value for `font-variant-alternates: simplified`."),
                group: "forms",
            },
            {
                tag: WI.unlocalizedString("trad"),
                name: WI.unlocalizedString("traditional"),
                result: WI.UIString("Traditional Forms", "Traditional Forms @ Font Details Sidebar Property Value", "Property value for `font-variant-alternates: traditional`."),
                group: "forms",
            },
            {
                tag: WI.unlocalizedString("fwid"),
                name: WI.unlocalizedString("full-width"),
                result: WI.UIString("Full-Width Variants", "Full-Width Variants @ Font Details Sidebar Property Value", "Property value for `font-variant-alternates: full-width`."),
                group: "width",
            },
            {
                tag: WI.unlocalizedString("pwid"),
                name: WI.unlocalizedString("proportional-width"),
                result: WI.UIString("Proportional-Width Variants", "Proportional-Width Variants @ Font Details Sidebar Property Value", "Property value for `font-variant-alternates: proportional-width`."),
                group: "width",
            },
            {
                tag: WI.unlocalizedString("ruby"),
                name: WI.unlocalizedString("ruby"),
                result: WI.UIString("Ruby Glyphs", "Ruby Glyphs @ Font Details Sidebar Property Value", "Property value for `font-variant-alternates: ruby`."),
            },
        ], WI.UIString("Normal", "Normal @ Font Details Sidebar Property Value", "Property value for any `normal` CSS value."));
    }

    _featureIsEnabled(property, featureTag, tagNotPresentCondition)
    {
        let featureValue = property.features?.get(featureTag);
        if (featureValue || featureValue === 0)
            return !!featureValue;
        return tagNotPresentCondition;
    }

    _hasVariationValue(property, variationTag, {optional} = {})
    {
        if (property.variations?.has(variationTag))
            return property.variations.get(variationTag).value || optional;
        return false;
    }
};
