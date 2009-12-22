/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Joseph Pecoraro
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

WebInspector.CSSSourceSyntaxHighlighter = function(table, sourceFrame) {
    WebInspector.SourceSyntaxHighlighter.call(this, table, sourceFrame);

    this.LexState = {
        Initial: 1,
        DeclarationProperty: 2,
        DeclarationValue: 3,
        AtMedia: 4,
        AtRule: 5,
        AtKeyframes: 6
    };
    this.ContinueState = {
        None: 0,
        Comment: 1
    };
    
    this.nonToken = "";
    this.cursor = 0;
    this.lineIndex = -1;
    this.lineCode = "";
    this.newLine = null;
    this.lexState = this.LexState.Initial;
    this.continueState = this.ContinueState.None;
    
    const urlPattern = /^url\(\s*(?:(?:"(?:[^\\\"]|(?:\\[\da-f]{1,6}\s?|\.))*"|'(?:[^\\\']|(?:\\[\da-f]{1,6}\s?|\.))*')|(?:[!#$%&*-~\w]|(?:\\[\da-f]{1,6}\s?|\.))*)\s*\)/i;
    const stringPattern = /^(?:"(?:[^\\\"]|(?:\\[\da-f]{1,6}\s?|\.))*"|'(?:[^\\\']|(?:\\[\da-f]{1,6}\s?|\.))*')/i;
    const identPattern = /^-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*/i;
    const startBlockPattern = /^{/i;
    const endBlockPattern = /^}/i;

    const propertyKeywords = [
        "background", "background-attachment", "background-clip", "background-color", "background-image",
        "background-origin", "background-position", "background-position-x", "background-position-y",
        "background-repeat", "background-repeat-x", "background-repeat-y", "background-size", "border",
        "border-bottom", "border-bottom-color", "border-bottom-left-radius", "border-bottom-right-radius",
        "border-bottom-style", "border-bottom-width", "border-collapse", "border-color", "border-left",
        "border-left-color", "border-left-style", "border-left-width", "border-radius", "border-right",
        "border-right-color", "border-right-style", "border-right-width", "border-spacing", "border-style",
        "border-top", "border-top-color", "border-top-left-radius", "border-top-right-radius", "border-top-style",
        "border-top-width", "border-width", "bottom", "caption-side", "clear", "clip", "color", "content",
        "counter-increment", "counter-reset", "cursor", "direction", "display", "empty-cells", "float",
        "font", "font-family", "font-size", "font-stretch", "font-style", "font-variant", "font-weight",
        "height", "left", "letter-spacing", "line-height", "list-style", "list-style-image", "list-style-position",
        "list-style-type", "margin", "margin-bottom", "margin-left", "margin-right", "margin-top", "max-height",
        "max-width", "min-height", "min-width", "opacity", "orphans", "outline", "outline-color", "outline-offset",
        "outline-style", "outline-width", "overflow", "overflow-x", "overflow-y", "padding", "padding-bottom",
        "padding-left", "padding-right", "padding-top", "page", "page-break-after", "page-break-before",
        "page-break-inside", "pointer-events", "position", "quotes", "resize", "right", "size", "src",
        "table-layout", "text-align", "text-decoration", "text-indent", "text-line-through", "text-line-through-color",
        "text-line-through-mode", "text-line-through-style", "text-line-through-width", "text-overflow", "text-overline",
        "text-overline-color", "text-overline-mode", "text-overline-style", "text-overline-width", "text-rendering",
        "text-shadow", "text-transform", "text-underline", "text-underline-color", "text-underline-mode",
        "text-underline-style", "text-underline-width", "top", "unicode-bidi", "unicode-range", "vertical-align",
        "visibility", "white-space", "widows", "width", "word-break", "word-spacing", "word-wrap", "z-index", "zoom",
        "-webkit-animation", "-webkit-animation-delay", "-webkit-animation-direction", "-webkit-animation-duration",
        "-webkit-animation-iteration-count", "-webkit-animation-name", "-webkit-animation-play-state",
        "-webkit-animation-timing-function", "-webkit-appearance", "-webkit-backface-visibility",
        "-webkit-background-clip", "-webkit-background-composite", "-webkit-background-origin", "-webkit-background-size",
        "-webkit-binding", "-webkit-border-fit", "-webkit-border-horizontal-spacing", "-webkit-border-image",
        "-webkit-border-radius", "-webkit-border-vertical-spacing", "-webkit-box-align", "-webkit-box-direction",
        "-webkit-box-flex", "-webkit-box-flex-group", "-webkit-box-lines", "-webkit-box-ordinal-group",
        "-webkit-box-orient", "-webkit-box-pack", "-webkit-box-reflect", "-webkit-box-shadow", "-webkit-box-sizing",
        "-webkit-column-break-after", "-webkit-column-break-before", "-webkit-column-break-inside", "-webkit-column-count",
        "-webkit-column-gap", "-webkit-column-rule", "-webkit-column-rule-color", "-webkit-column-rule-style",
        "-webkit-column-rule-width", "-webkit-column-width", "-webkit-columns", "-webkit-font-size-delta",
        "-webkit-font-smoothing", "-webkit-highlight", "-webkit-line-break", "-webkit-line-clamp",
        "-webkit-margin-bottom-collapse", "-webkit-margin-collapse", "-webkit-margin-start", "-webkit-margin-top-collapse",
        "-webkit-marquee", "-webkit-marquee-direction", "-webkit-marquee-increment", "-webkit-marquee-repetition",
        "-webkit-marquee-speed", "-webkit-marquee-style", "-webkit-mask", "-webkit-mask-attachment",
        "-webkit-mask-box-image", "-webkit-mask-clip", "-webkit-mask-composite", "-webkit-mask-image",
        "-webkit-mask-origin", "-webkit-mask-position", "-webkit-mask-position-x", "-webkit-mask-position-y",
        "-webkit-mask-repeat", "-webkit-mask-repeat-x", "-webkit-mask-repeat-y", "-webkit-mask-size",
        "-webkit-match-nearest-mail-blockquote-color", "-webkit-nbsp-mode", "-webkit-padding-start",
        "-webkit-perspective", "-webkit-perspective-origin", "-webkit-perspective-origin-x", "-webkit-perspective-origin-y",
        "-webkit-rtl-ordering", "-webkit-text-decorations-in-effect", "-webkit-text-fill-color", "-webkit-text-security",
        "-webkit-text-size-adjust", "-webkit-text-stroke", "-webkit-text-stroke-color", "-webkit-text-stroke-width",
        "-webkit-transform", "-webkit-transform-origin", "-webkit-transform-origin-x", "-webkit-transform-origin-y",
        "-webkit-transform-origin-z", "-webkit-transform-style", "-webkit-transition", "-webkit-transition-delay",
        "-webkit-transition-duration", "-webkit-transition-property", "-webkit-transition-timing-function",
        "-webkit-user-drag", "-webkit-user-modify", "-webkit-user-select", "-webkit-variable-declaration-block"
    ].keySet();
    
    const valueKeywords = [
        "inherit", "initial", "none", "hidden", "inset", "groove", "ridge", "outset", "dotted", "dashed",
        "solid", "double", "caption", "icon", "menu", "message-box", "small-caption", "-webkit-mini-control",
        "-webkit-small-control", "-webkit-control", "status-bar", "italic", "oblique", "all", "small-caps",
        "normal", "bold", "bolder", "lighter", "xx-small", "x-small", "small", "medium", "large", "x-large",
        "xx-large", "-webkit-xxx-large", "smaller", "larger", "wider", "narrower", "ultra-condensed",
        "extra-condensed", "condensed", "semi-condensed", "semi-expanded", "expanded", "extra-expanded",
        "ultra-expanded", "serif", "sans-serif", "cursive", "fantasy", "monospace", "-webkit-body", "aqua",
        "black", "blue", "fuchsia", "gray", "green", "lime", "maroon", "navy", "olive", "orange", "purple",
        "red", "silver", "teal", "white", "yellow", "transparent", "-webkit-link", "-webkit-activelink",
        "activeborder", "activecaption", "appworkspace", "background", "buttonface", "buttonhighlight",
        "buttonshadow", "buttontext", "captiontext", "graytext", "highlight", "highlighttext", "inactiveborder",
        "inactivecaption", "inactivecaptiontext", "infobackground", "infotext", "match", "menutext", "scrollbar",
        "threeddarkshadow", "threedface", "threedhighlight", "threedlightshadow", "threedshadow", "window",
        "windowframe", "windowtext", "-webkit-focus-ring-color", "currentcolor", "grey", "-webkit-text", "repeat",
        "repeat-x", "repeat-y", "no-repeat", "clear", "copy", "source-over", "source-in", "source-out",
        "source-atop", "destination-over", "destination-in", "destination-out", "destination-atop", "xor",
        "plus-darker", "plus-lighter", "baseline", "middle", "sub", "super", "text-top", "text-bottom", "top",
        "bottom", "-webkit-baseline-middle", "-webkit-auto", "left", "right", "center", "justify", "-webkit-left",
        "-webkit-right", "-webkit-center", "outside", "inside", "disc", "circle", "square", "decimal",
        "decimal-leading-zero", "lower-roman", "upper-roman", "lower-greek", "lower-alpha", "lower-latin",
        "upper-alpha", "upper-latin", "hebrew", "armenian", "georgian", "cjk-ideographic", "hiragana", "katakana",
        "hiragana-iroha", "katakana-iroha", "inline", "block", "list-item", "run-in", "compact", "inline-block",
        "table", "inline-table", "table-row-group", "table-header-group", "table-footer-group", "table-row",
        "table-column-group", "table-column", "table-cell", "table-caption", "-webkit-box", "-webkit-inline-box",
        "-wap-marquee", "auto", "crosshair", "default", "pointer", "move", "vertical-text", "cell", "context-menu",
        "alias", "progress", "no-drop", "not-allowed", "-webkit-zoom-in", "-webkit-zoom-out", "e-resize", "ne-resize",
        "nw-resize", "n-resize", "se-resize", "sw-resize", "s-resize", "w-resize", "ew-resize", "ns-resize", "nesw-resize",
        "nwse-resize", "col-resize", "row-resize", "text", "wait", "help", "all-scroll", "-webkit-grab", "-webkit-grabbing",
        "ltr", "rtl", "capitalize", "uppercase", "lowercase", "visible", "collapse", "above", "absolute", "always",
        "avoid", "below", "bidi-override", "blink", "both", "close-quote", "crop", "cross", "embed", "fixed", "hand",
        "hide", "higher", "invert", "landscape", "level", "line-through", "local", "loud", "lower", "-webkit-marquee",
        "mix", "no-close-quote", "no-open-quote", "nowrap", "open-quote", "overlay", "overline", "portrait", "pre",
        "pre-line", "pre-wrap", "relative", "scroll", "separate", "show", "static", "thick", "thin", "underline",
        "-webkit-nowrap", "stretch", "start", "end", "reverse", "horizontal", "vertical", "inline-axis", "block-axis",
        "single", "multiple", "forwards", "backwards", "ahead", "up", "down", "slow", "fast", "infinite", "slide",
        "alternate", "read-only", "read-write", "read-write-plaintext-only", "element", "ignore", "intrinsic",
        "min-intrinsic", "clip", "ellipsis", "discard", "dot-dash", "dot-dot-dash", "wave", "continuous",
        "skip-white-space", "break-all", "break-word", "space", "after-white-space", "checkbox", "radio", "push-button",
        "square-button", "button", "button-bevel", "default-button", "list-button", "listbox", "listitem", 
        "media-fullscreen-button", "media-mute-button", "media-play-button", "media-seek-back-button",
        "media-seek-forward-button", "media-rewind-button", "media-return-to-realtime-button", "media-slider",
        "media-sliderthumb", "media-volume-slider-container", "media-volume-slider", "media-volume-sliderthumb",
        "media-controls-background", "media-current-time-display", "media-time-remaining-display", "menulist",
        "menulist-button", "menulist-text", "menulist-textfield", "slider-horizontal", "slider-vertical",
        "sliderthumb-horizontal", "sliderthumb-vertical", "caret", "searchfield", "searchfield-decoration", 
        "searchfield-results-decoration", "searchfield-results-button", "searchfield-cancel-button", "textfield",
        "textarea", "caps-lock-indicator", "round", "border", "border-box", "content", "content-box", "padding",
        "padding-box", "contain", "cover", "logical", "visual", "lines", "running", "paused", "flat", "preserve-3d",
        "ease", "linear", "ease-in", "ease-out", "ease-in-out", "document", "reset", "visiblePainted", "visibleFill",
        "visibleStroke", "painted", "fill", "stroke", "antialiased", "subpixel-antialiased", "optimizeSpeed",
        "optimizeLegibility", "geometricPrecision"
    ].keySet();

    const mediaTypes = ["all", "aural", "braille", "embossed", "handheld", "print", "projection", "screen", "tty", "tv"].keySet();

    this.rules = [{
        name: "commentAction",
        pattern: /^\/\*[^\*]*\*+([^\/*][^*]*\*+)*\//i,
        style: "webkit-css-comment"
    }, {
        name: "commentStartAction",
        pattern: /^(?:\/\*(?:[^\*]|\*[^\/])*)/i,
        style: "webkit-css-comment",
        postContinueState: this.ContinueState.Comment
    }, {
        name: "commentEndAction",
        pattern: /^(?:(?:[^\*]|\*[^\/])*\*+\/)/i,
        style: "webkit-css-comment",
        preContinueState: this.ContinueState.Comment,
        postContinueState: this.ContinueState.None
    }, {
        name: "commentMiddleAction",
        pattern: /^.*/i,
        style: "webkit-css-comment",
        preContinueState: this.ContinueState.Comment
    }, {
        name: "selectorAction",
        pattern: /^(?:(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\*)(?:#-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\.-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\[\s*-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*(?:(?:=|~=|\|=)\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|(?:"(?:[^\\\"]|(?:\\[\da-f]{1,6}\s?|\.))*"|'(?:[^\\\']|(?:\\[\da-f]{1,6}\s?|\.))*'))\s*)?\]|:(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\(\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*)?\)))*|(?:#-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\.-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\[\s*-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*(?:(?:=|~=|\|=)\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|(?:"(?:[^\\\"]|(?:\\[\da-f]{1,6}\s?|\.))*"|'(?:[^\\\']|(?:\\[\da-f]{1,6}\s?|\.))*'))\s*)?\]|:(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\(\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*)?\)))+)/i,
        style: "webkit-css-selector",
        preLexState: this.LexState.Initial
    }, {
        name: "startRulesetBlockAction",
        pattern: startBlockPattern,
        preLexState: this.LexState.Initial,
        postLexState: this.LexState.DeclarationProperty
    }, {
        name: "propertyAction",
        pattern: identPattern,
        style: "webkit-css-property",
        keywords: propertyKeywords,
        preLexState: this.LexState.DeclarationProperty,
    }, {
        name: "declarationColonAction",
        pattern: /^:/i,
        preLexState: this.LexState.DeclarationProperty,
        postLexState: this.LexState.DeclarationValue
    }, {
        name: "colorAction",
        pattern: /^(?:#(?:[\da-f]{6}|[\da-f]{3})|rgba\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\)|hsla\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\)|rgb\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\)|hsl\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\))/i,
        style: "webkit-css-color",
        preLexState: this.LexState.DeclarationValue
    }, {
        name: "numvalueAction",
        pattern: /^(?:-?(?:\d+|\d*\.\d+)(?:em|rem|__qem|ex|px|cm|mm|in|pt|pc|deg|rad|grad|turn|ms|s|Hz|kHz|%)?)/i,
        style: "webkit-css-number",
        preLexState: this.LexState.DeclarationValue
    }, {
        name: "urlAction",
        pattern: urlPattern,
        style: "webkit-css-url",
        preLexState: this.LexState.DeclarationValue
    }, {
        name: "stringAction",
        pattern: stringPattern,
        style: "webkit-css-string",
        preLexState: this.LexState.DeclarationValue
    }, {
        name: "importantAction",
        pattern: /^!\s*important/i,
        style: "webkit-css-important",
        preLexState: this.LexState.DeclarationValue
    }, {
        name: "valueIdentAction",
        pattern: identPattern,
        keywords: valueKeywords,
        style: "webkit-css-keyword",
        preLexState: this.LexState.DeclarationValue
    }, {
        name: "declarationSemicolonAction",
        pattern: /^;/i,
        preLexState: this.LexState.DeclarationValue,
        postLexState: this.LexState.DeclarationProperty
    }, {
        name: "endRulesetBlockAction",
        pattern: endBlockPattern,
        preLexState: this.LexState.DeclarationProperty,
        postLexState: this.LexState.Initial
    }, {
        name: "atMediaAction",
        pattern: /^@media/i,
        style: "webkit-css-at-rule",
        preLexState: this.LexState.Initial,
        postLexState: this.LexState.AtMedia
    }, {
        name: "startAtMediaBlockAction",
        pattern: startBlockPattern,
        preLexState: this.LexState.AtMedia,
        postLexState: this.LexState.Initial
    }, {
        name: "atKeyframesAction",
        pattern: /^@-webkit-keyframes/i,
        style: "webkit-css-at-rule",
        preLexState: this.LexState.Initial,
        postLexState: this.LexState.AtKeyframes
    }, {
        name: "startAtKeyframesBlockAction",
        pattern: startBlockPattern,
        preLexState: this.LexState.AtKeyframes,
        postLexState: this.LexState.Initial
    }, {
        name: "atRuleAction",
        style: "webkit-css-at-rule",
        pattern: /^@-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*/i,
        preLexState: this.LexState.Initial,
        postLexState: this.LexState.AtRule
    }, {
        name: "endAtRuleAction",
        pattern: /^;/i,
        preLexState: this.LexState.AtRule,
        postLexState: this.LexState.Initial
    }, {
        name: "urlAction",
        pattern: urlPattern,
        style: "webkit-css-url",
        preLexState: this.LexState.AtRule
    }, {
        name: "stringAction",
        pattern: stringPattern,
        style: "webkit-css-string",
        preLexState: this.LexState.AtRule
    }, {
        name: "stringAction",
        pattern: stringPattern,
        style: "webkit-css-string",
        preLexState: this.LexState.AtKeyframes
    }, {
        name: "atRuleIdentAction",
        pattern: identPattern,
        keywords: mediaTypes,
        style: "webkit-css-keyword",
        preLexState: this.LexState.AtRule
    }, {
        name: "atRuleIdentAction",
        pattern: identPattern,
        keywords: mediaTypes,
        style: "webkit-css-keyword",
        preLexState: this.LexState.AtMedia,
    }, {
        name: "startAtRuleBlockAction",
        pattern: startBlockPattern,
        preLexState: this.LexState.AtRule,
        postLexState: this.LexState.DeclarationProperty
    }];
}

WebInspector.CSSSourceSyntaxHighlighter.prototype.__proto__ = WebInspector.SourceSyntaxHighlighter.prototype;
