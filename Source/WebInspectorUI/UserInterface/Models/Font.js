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

WI.Font = class Font
{
    constructor(name, variationAxes)
    {
        this._name = name;
        this._variationAxes = variationAxes;
    }

    // Static

    static fromPayload(payload)
    {
        let variationAxes = payload.variationAxes.map((axisPayload) => WI.FontVariationAxis.fromPayload(axisPayload));
        return new WI.Font(payload.displayName, variationAxes);
    }

    // Public

    get name() { return this._name; }
    get variationAxes() { return this._variationAxes; }

    variationAxis(tag)
    {
        return this._variationAxes.find((axis) => axis.tag === tag);
    }

    calculateFontProperties(domNodeStyle)
    {
        let featuresMap = this._calculateFontFeatureAxes(domNodeStyle);
        let variationsMap = this._calculateFontVariationAxes(domNodeStyle);
        let propertiesMap = this._calculateProperties({domNodeStyle, featuresMap, variationsMap});
        return {propertiesMap, featuresMap, variationsMap};
    }

    // Private

    _calculateProperties(style)
    {
        let resultProperties = new Map;

        this._populateProperty("font-size", style, resultProperties, {
            keywordComputedReplacements: ["larger", "smaller", "xx-small", "x-small", "small", "medium", "large", "x-large", "xx-large", "xxx-large"],
        });
        this._populateProperty("font-style", style, resultProperties, {
            variations: ["ital", "slnt"],
            keywordReplacements: new Map([
                ["oblique", "oblique 14deg"],
            ]),
        });
        this._populateProperty("font-weight", style, resultProperties, {
            variations: ["wght"],
            keywordComputedReplacements: ["bolder", "lighter"],
            keywordReplacements: new Map([
                ["normal", "400"],
                ["bold", "700"],
            ]),
        });
        this._populateProperty("font-stretch", style, resultProperties, {
            variations: ["wdth"],
            keywordReplacements: new Map([
                ["ultra-condensed", "50%"],
                ["extra-condensed", "62.5%"],
                ["condensed", "75%"],
                ["semi-condensed", "87.5%"],
                ["normal", "100%"],
                ["semi-expanded", "112.5%"],
                ["expanded", "125%"],
                ["extra-expanded", "150%"],
                ["ultra-expanded", "200%"],
            ]),
        });

        this._populateProperty("font-variant-ligatures", style, resultProperties, {features: ["liga", "clig", "dlig", "hlig", "calt"]});
        this._populateProperty("font-variant-position", style, resultProperties, {features: ["subs", "sups"]});
        this._populateProperty("font-variant-caps", style, resultProperties, {features: ["smcp", "c2sc", "pcap", "c2pc", "unic", "titl"]});
        this._populateProperty("font-variant-numeric", style, resultProperties, {features: ["lnum", "onum", "pnum", "tnum", "frac", "afrc", "ordn", "zero"]});
        this._populateProperty("font-variant-alternates", style, resultProperties, {features: ["hist"] });
        this._populateProperty("font-variant-east-asian", style, resultProperties, {features: ["jp78", "jp83", "jp90", "jp04", "smpl", "trad", "fwid", "pwid", "ruby"]});

        return resultProperties;
    }

    _calculateFontFeatureAxes(domNodeStyle)
    {
        return this._parseFontFeatureOrVariationSettings(domNodeStyle, "font-feature-settings");
    }

    _calculateFontVariationAxes(domNodeStyle)
    {
        let cssVariationSettings = this._parseFontFeatureOrVariationSettings(domNodeStyle, "font-variation-settings");
        let resultAxes = new Map;

        for (let axis of this._variationAxes) {
            // `value` can be undefined.
            resultAxes.set(axis.tag, {
                name: axis.name,
                minimumValue: axis.minimumValue,
                maximumValue: axis.maximumValue,
                defaultValue: axis.defaultValue,
                value: cssVariationSettings.get(axis.tag),
            });
        }

        return resultAxes;
    }

    _parseFontFeatureOrVariationSettings(domNodeStyle, property)
    {
        let cssSettings = new Map;
        let cssSettingsRawValue = this._computedPropertyValueForName(domNodeStyle, property);

        if (cssSettingsRawValue !== "normal") {
            for (let axis of cssSettingsRawValue.split(",")) {
                // Tags can contains upper and lowercase latin letters, numbers, and spaces (only ending with space(s)). Values will be numbers, `on`, or `off`.
                let [tag, value] = axis.match(WI.Font.SettingPattern);
                tag = tag.replaceAll(/["']/g, "");
                if (!value || value === "on")
                    value = 1;
                else if (value === "off")
                    value = 0;
                cssSettings.set(tag, parseFloat(value));
            }
        }

        return cssSettings;
    }

    _populateProperty(name, style, resultProperties, {variations, features, keywordComputedReplacements, keywordReplacements})
    {
        resultProperties.set(name, this._computeProperty(name, style, {variations, features, keywordComputedReplacements, keywordReplacements}));
    }

    _computeProperty(name, style, {variations, features, keywordComputedReplacements, keywordReplacements})
    {
        variations ??= [];
        features ??= [];
        keywordComputedReplacements ??= [];
        keywordReplacements ??= new Map;

        let resultProperties = {};

        let value = this._effectivePropertyValueForName(style.domNodeStyle, name);
        
        if (!value || value === "inherit" || keywordComputedReplacements.includes(value))
            value = this._computedPropertyValueForName(style.domNodeStyle, name);

        if (keywordReplacements.has(value))
            value = keywordReplacements.get(value);

        resultProperties.value = value;

        for (let fontVariationSetting of variations) {
            let variationSettingValue = style.variationsMap.get(fontVariationSetting);
            if (variationSettingValue) {
                resultProperties.variations ??= new Map;
                resultProperties.variations.set(fontVariationSetting, variationSettingValue);

                // Remove the tag so it is not presented twice.
                style.variationsMap.delete(fontVariationSetting);
            }
        }

        for (let fontFeatureSetting of features) {
            let featureSettingValue = style.featuresMap.get(fontFeatureSetting);
            if (featureSettingValue || featureSettingValue === 0) {
                resultProperties.features ??= new Map;
                resultProperties.features.set(fontFeatureSetting, featureSettingValue);

                // Remove the tag so it is not presented twice.
                style.featuresMap.delete(fontFeatureSetting);
            }
        }

        return resultProperties;
    }

    _effectivePropertyValueForName(domNodeStyle, name)
    {
        return domNodeStyle.effectivePropertyForName(name)?.value || "";
    }

    _computedPropertyValueForName(domNodeStyle, name)
    {
        return domNodeStyle.computedStyle?.propertyForName(name)?.value || "";
    }
};

WI.Font.SettingPattern = /[^\s"']+|["']([^"']*)["']/g;
