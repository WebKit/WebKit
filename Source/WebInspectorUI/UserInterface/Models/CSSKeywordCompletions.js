/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WI.CSSKeywordCompletions = {};

WI.CSSKeywordCompletions.forPartialPropertyName = function(text, {caretPosition, allowEmptyPrefix} = {})
{
    allowEmptyPrefix ??= false;

    // FIXME: <webkit.org/b/227157> Styles: Support completions mid-token.
    if (caretPosition !== text.length)
        return {prefix: "", completions: []};

    if (!text.length && allowEmptyPrefix)
        return {prefix: text, completions: WI.cssManager.propertyNameCompletions.values};

    return {prefix: text, completions: WI.cssManager.propertyNameCompletions.executeQuery(text)};
};

WI.CSSKeywordCompletions.forPartialPropertyValue = function(text, propertyName, {caretPosition, additionalFunctionValueCompletionsProvider} = {})
{
    caretPosition ??= text.length;

    console.assert(caretPosition >= 0 && caretPosition <= text.length, text, caretPosition);
    if (caretPosition < 0 || caretPosition > text.length)
        return {prefix: "", completions: []};

    if (!text.length)
        return {prefix: "", completions: WI.CSSKeywordCompletions.forProperty(propertyName).values};

    let tokens = WI.tokenizeCSSValue(text);

    // Find the token that the cursor is either in or at the end of.
    let indexOfTokenAtCaret = -1;
    let passedCharacters = 0;
    for (let i = 0; i < tokens.length; ++i) {
        passedCharacters += tokens[i].value.length;
        if (passedCharacters >= caretPosition) {
            indexOfTokenAtCaret = i;
            break;
        }
    }

    let tokenAtCaret = tokens[indexOfTokenAtCaret];
    console.assert(tokenAtCaret, text, caretPosition);
    if (!tokenAtCaret)
        return {prefix: "", completions: []};

    if (tokenAtCaret.type && /\b(comment|string)\b/.test(tokenAtCaret.type))
        return {prefix: "", completions: []};

    let currentTokenValue = tokenAtCaret.value.trim();
    let caretIsInMiddleOfToken = caretPosition !== passedCharacters;

    // FIXME: <webkit.org/b/227157 Styles: Support completions mid-token.
    // If the cursor was in middle of a token or the next token starts with a valid character for a value, we are effectively mid-token.
    let tokenAfterCaret = tokens[indexOfTokenAtCaret + 1];
    if ((caretIsInMiddleOfToken && currentTokenValue.length) || (!caretIsInMiddleOfToken && tokenAfterCaret && /[a-zA-Z0-9-]/.test(tokenAfterCaret.value[0])))
        return {prefix: "", completions: []};

    // If the current token value is a comma or open parenthesis, treat it as if we are at the start of a new token.
    if (currentTokenValue === "(" || currentTokenValue === ",")
        currentTokenValue = "";

    // It's not valid CSS to append completions immediately after a closing parenthesis.
    let tokenBeforeCaret = tokens[indexOfTokenAtCaret - 1];
    if (currentTokenValue === ")" || tokenBeforeCaret?.value === ")")
        return {prefix: "", completions: []};

    // The CodeMirror CSS-mode tokenizer splits a hyphen-prefixed identifier into two tokens: a leading `-`
    // for a value like `-name`, or a `-vendor-` meta token followed by the remainder for a value like
    // `-apple-system`. Rejoin the preceding hyphen-terminated token so the whole identifier is the prefix.
    if (currentTokenValue.length && tokenBeforeCaret?.value.endsWith("-")) {
        currentTokenValue = tokenBeforeCaret.value + currentTokenValue;
    }

    let functionName = null;
    let preceedingFunctionDepth = 0;
    for (let i = indexOfTokenAtCaret; i >= 0; --i) {
        let value = tokens[i].value;

        // There may be one or more complete functions between the cursor and the current scope's functions name.
        if (value === ")")
            ++preceedingFunctionDepth;
        else if (value === "(") {
            if (preceedingFunctionDepth)
                --preceedingFunctionDepth;
            else {
                functionName = tokens[i - 1]?.value;
                break;
            }
        }
    }

    let valueCompletions;
    if (functionName)
        valueCompletions = WI.CSSKeywordCompletions.forFunction(functionName, {additionalFunctionValueCompletionsProvider});
    else
        valueCompletions = WI.CSSKeywordCompletions.forProperty(propertyName);

    return {prefix: currentTokenValue, completions: valueCompletions.executeQuery(currentTokenValue)};
};

WI.CSSKeywordCompletions.forProperty = function(propertyName)
{
    let acceptedKeywords = ["initial", "unset", "revert", "revert-layer", "var()", "env()"];

    function addKeywordsForName(name) {
        let isNotPrefixed = name.charAt(0) !== "-";

        let keywords = WI.CSSKeywordCompletions.KeywordsForPropertyName.get(name);
        if (!keywords && isNotPrefixed)
            keywords = WI.CSSKeywordCompletions.KeywordsForPropertyName.get("-webkit-" + name);
        if (keywords)
            acceptedKeywords.pushAll(keywords);

        if (WI.CSSKeywordCompletions.isColorAwareProperty(name))
            acceptedKeywords.pushAll(WI.CSSKeywordCompletions.ColorValues);

        // Only suggest "inherit" on inheritable properties even though it is valid on all properties.
        if (WI.CSSKeywordCompletions.InheritedPropertyNames.has(name))
            acceptedKeywords.push("inherit");
        else if (isNotPrefixed && WI.CSSKeywordCompletions.InheritedPropertyNames.has("-webkit-" + name))
            acceptedKeywords.push("inherit");
    }

    addKeywordsForName(propertyName);

    let unaliasedName = WI.CSSKeywordCompletions.PropertyNameForAlias.get(propertyName);
    if (unaliasedName)
        addKeywordsForName(unaliasedName);

    let longhandNames = WI.CSSKeywordCompletions.LonghandPropertyNamesForShorthandPropertyName.get(propertyName);
    if (longhandNames) {
        for (let longhandName of longhandNames)
            addKeywordsForName(longhandName);
    }

    if (acceptedKeywords.includes(WI.CSSKeywordCompletions.AllPropertyNamesPlaceholder) && WI.cssManager.propertyNameCompletions) {
        acceptedKeywords.remove(WI.CSSKeywordCompletions.AllPropertyNamesPlaceholder);
        acceptedKeywords.pushAll(WI.cssManager.propertyNameCompletions.values);
    }

    return new WI.CSSCompletions(Array.from(new Set(acceptedKeywords)), {acceptEmptyPrefix: true});
};

WI.CSSKeywordCompletions.isColorAwareProperty = function(name)
{
    if (WI.CSSKeywordCompletions.ColorAwareProperties.has(name))
        return true;

    let isNotPrefixed = name.charAt(0) !== "-";
    if (isNotPrefixed && WI.CSSKeywordCompletions.ColorAwareProperties.has("-webkit-" + name))
        return true;

    if (name.endsWith("color"))
        return true;

    return false;
};

WI.CSSKeywordCompletions.isEasingAwareProperty = function(name)
{
    if (WI.CSSKeywordCompletions.EasingAwareProperties.has(name))
        return true;

    let isNotPrefixed = name.charAt(0) !== "-";
    if (isNotPrefixed && WI.CSSKeywordCompletions.EasingAwareProperties.has("-webkit-" + name))
        return true;

    return false;
};

WI.CSSKeywordCompletions.forFunction = function(functionName, {additionalFunctionValueCompletionsProvider} = {})
{
    let suggestions = ["var()"];

    if (functionName === "var")
        suggestions = [];
    else if (functionName === "calc" || functionName === "min" || functionName === "max")
        suggestions.push("calc()", "min()", "max()");
    else if (functionName === "env")
        suggestions.push("safe-area-inset-top", "safe-area-inset-right", "safe-area-inset-bottom", "safe-area-inset-left");
    else if (functionName === "image-set")
        suggestions.push("url()");
    else if (functionName === "repeat")
        suggestions.push("auto", "auto-fill", "auto-fit", "min-content", "max-content");
    else if (functionName === "steps")
        suggestions.push("jump-none", "jump-start", "jump-end", "jump-both", "start", "end");
    else if (functionName.endsWith("gradient")) {
        suggestions.push("to", "left", "right", "top", "bottom");
        suggestions.pushAll(WI.CSSKeywordCompletions.ColorValues);
    }

    if (additionalFunctionValueCompletionsProvider)
        suggestions.pushAll(additionalFunctionValueCompletionsProvider(functionName));

    return new WI.CSSCompletions(suggestions, {acceptEmptyPrefix: true});
};

WI.CSSKeywordCompletions.AllPropertyNamesPlaceholder = "__all-properties__";

// Populated by CSS.getSupportedCSSProperties.
WI.CSSKeywordCompletions.PropertyNameForAlias = new Map;
WI.CSSKeywordCompletions.LonghandPropertyNamesForShorthandPropertyName = new Multimap;
WI.CSSKeywordCompletions.ShorthandPropertyNamesForLonghandPropertyName = new Multimap;
WI.CSSKeywordCompletions.InheritedPropertyNames = new Set;
WI.CSSKeywordCompletions.KeywordsForPropertyName = new Multimap;

// COMPATIBILITY (macOS X.Y, iOS X.Y): the `colors` parameter of `CSS.getSupportedCSSProperties` did not exist yet.
WI.CSSKeywordCompletions.ColorValues = [
    "aqua", "black", "blue", "fuchsia", "gray", "green", "lime", "maroon", "navy", "olive", "orange", "purple", "red",
    "silver", "teal", "white", "yellow", "transparent", "currentcolor", "grey", "aliceblue", "antiquewhite",
    "aquamarine", "azure", "beige", "bisque", "blanchedalmond", "blueviolet", "brown", "burlywood", "cadetblue",
    "chartreuse", "chocolate", "coral", "cornflowerblue", "cornsilk", "crimson", "cyan", "darkblue", "darkcyan",
    "darkgoldenrod", "darkgray", "darkgreen", "darkgrey", "darkkhaki", "darkmagenta", "darkolivegreen", "darkorange",
    "darkorchid", "darkred", "darksalmon", "darkseagreen", "darkslateblue", "darkslategray", "darkslategrey",
    "darkturquoise", "darkviolet", "deeppink", "deepskyblue", "dimgray", "dimgrey", "dodgerblue", "firebrick",
    "floralwhite", "forestgreen", "gainsboro", "ghostwhite", "gold", "goldenrod", "greenyellow", "honeydew", "hotpink",
    "indianred", "indigo", "ivory", "khaki", "lavender", "lavenderblush", "lawngreen", "lemonchiffon", "lightblue",
    "lightcoral", "lightcyan", "lightgoldenrodyellow", "lightgray", "lightgreen", "lightgrey", "lightpink",
    "lightsalmon", "lightseagreen", "lightskyblue", "lightslategray", "lightslategrey", "lightsteelblue", "lightyellow",
    "limegreen", "linen", "magenta", "mediumaquamarine", "mediumblue", "mediumorchid", "mediumpurple", "mediumseagreen",
    "mediumslateblue", "mediumspringgreen", "mediumturquoise", "mediumvioletred", "midnightblue", "mintcream",
    "mistyrose", "moccasin", "navajowhite", "oldlace", "olivedrab", "orangered", "orchid", "palegoldenrod", "palegreen",
    "paleturquoise", "palevioletred", "papayawhip", "peachpuff", "peru", "pink", "plum", "powderblue", "rebeccapurple", "rosybrown",
    "royalblue", "saddlebrown", "salmon", "sandybrown", "seagreen", "seashell", "sienna", "skyblue", "slateblue",
    "slategray", "slategrey", "snow", "springgreen", "steelblue", "tan", "thistle", "tomato", "turquoise", "violet",
    "wheat", "whitesmoke", "yellowgreen", "rgb()", "rgba()", "hsl()", "hsla()", "color()", "hwb()", "lch()", "lab()",
    "color-mix()", "color-contrast()", "light-dark()",
];

// COMPATIBILITY (macOS X.Y, iOS X.Y): the `colorAware` property of `CSS.CSSPropertyInfo` did not exist yet.
WI.CSSKeywordCompletions.ColorAwareProperties = new Set([
    "background",
    "background-color",
    "background-image",
    "border",
    "border-color",
    "border-bottom",
    "border-bottom-color",
    "border-left",
    "border-left-color",
    "border-right",
    "border-right-color",
    "border-top",
    "border-top-color",
    "box-shadow", "-webkit-box-shadow",
    "color",
    "column-rule", "-webkit-column-rule",
    "column-rule-color", "-webkit-column-rule-color",
    "fill",
    "outline",
    "outline-color",
    "stroke",
    "text-decoration-color", "-webkit-text-decoration-color",
    "text-emphasis", "-webkit-text-emphasis",
    "text-emphasis-color", "-webkit-text-emphasis-color",
    "text-line-through",
    "text-line-through-color",
    "text-overline",
    "text-overline-color",
    "text-shadow",
    "text-underline",
    "text-underline-color",
    "-webkit-text-fill-color",
    "-webkit-text-stroke",
    "-webkit-text-stroke-color",

    // iOS Properties
    "-webkit-tap-highlight-color",
]);

// COMPATIBILITY (macOS X.Y, iOS X.Y): the `timingFunctionAware` property of `CSS.CSSPropertyInfo` did not exist yet.
WI.CSSKeywordCompletions.EasingAwareProperties = new Set([
    "animation", "-webkit-animation",
    "animation-timing-function", "-webkit-animation-timing-function",
    "transition", "-webkit-transition",
    "transition-timing-function", "-webkit-transition-timing-function",
]);
