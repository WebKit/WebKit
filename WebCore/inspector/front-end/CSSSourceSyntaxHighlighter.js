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
    this.rules = [{
        pattern: /^\/\*[^\*]*\*+([^\/*][^*]*\*+)*\//i,
        action: commentAction
    }, {
        pattern: /^(?:\/\*(?:[^\*]|\*[^\/])*)/i,
        action: commentStartAction
    }, {
        pattern: /^(?:(?:[^\*]|\*[^\/])*\*+\/)/i,
        action: commentEndAction,
        continueStateCondition: this.ContinueState.Comment
    }, {
        pattern: /^.*/i,
        action: commentMiddleAction,
        continueStateCondition: this.ContinueState.Comment
    }, {
        pattern: /^(?:(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\*)(?:#-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\.-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\[\s*-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*(?:(?:=|~=|\|=)\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|(?:"(?:[^\\\"]|(?:\\[\da-f]{1,6}\s?|\.))*"|'(?:[^\\\']|(?:\\[\da-f]{1,6}\s?|\.))*'))\s*)?\]|:(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\(\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*)?\)))*|(?:#-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\.-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\[\s*-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*(?:(?:=|~=|\|=)\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|(?:"(?:[^\\\"]|(?:\\[\da-f]{1,6}\s?|\.))*"|'(?:[^\\\']|(?:\\[\da-f]{1,6}\s?|\.))*'))\s*)?\]|:(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\(\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*)?\)))+)/i,
        action: selectorAction,
        lexStateCondition: this.LexState.Initial
    }, {
        pattern: startBlockPattern,
        action: startRulesetBlockAction,
        lexStateCondition: this.LexState.Initial,
        dontAppendNonToken: true
    }, {
        pattern: identPattern,
        action: propertyAction,
        lexStateCondition: this.LexState.DeclarationProperty,
        dontAppendNonToken: true
    }, {
        pattern: /^:/i,
        action: declarationColonAction,
        lexStateCondition: this.LexState.DeclarationProperty,
        dontAppendNonToken: true
    }, {
        pattern: /^(?:#(?:[\da-f]{6}|[\da-f]{3})|rgba\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\)|hsla\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\)|rgb\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\)|hsl\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\))/i,
        action: colorAction,
        lexStateCondition: this.LexState.DeclarationValue
    }, {
        pattern: /^(?:-?(?:\d+|\d*\.\d+)(?:em|rem|__qem|ex|px|cm|mm|in|pt|pc|deg|rad|grad|turn|ms|s|Hz|kHz|%)?)/i,
        action: numvalueAction,
        lexStateCondition: this.LexState.DeclarationValue
    }, {
        pattern: urlPattern,
        action: urlAction,
        lexStateCondition: this.LexState.DeclarationValue
    }, {
        pattern: stringPattern,
        action: stringAction,
        lexStateCondition: this.LexState.DeclarationValue
    }, {
        pattern: /^!\s*important/i,
        action: importantAction,
        lexStateCondition: this.LexState.DeclarationValue
    }, {
        pattern: identPattern,
        action: valueIdentAction,
        lexStateCondition: this.LexState.DeclarationValue,
        dontAppendNonToken: true
    }, {
        pattern: /^;/i,
        action: declarationSemicolonAction,
        lexStateCondition: this.LexState.DeclarationValue,
        dontAppendNonToken: true
    }, {
        pattern: endBlockPattern,
        action: endRulesetBlockAction,
        lexStateCondition: this.LexState.DeclarationProperty,
        dontAppendNonToken: true
    }, {
        pattern: endBlockPattern,
        action: endRulesetBlockAction,
        lexStateCondition: this.LexState.DeclarationValue,
        dontAppendNonToken: true
    }, {
        pattern: /^@media/i,
        action: atMediaAction,
        lexStateCondition: this.LexState.Initial
    }, {
        pattern: startBlockPattern,
        action: startAtMediaBlockAction,
        lexStateCondition: this.LexState.AtMedia,
        dontAppendNonToken: true
    }, {
        pattern: /^@-webkit-keyframes/i,
        action: atKeyframesAction,
        lexStateCondition: this.LexState.Initial
    }, {
        pattern: startBlockPattern,
        action: startAtMediaBlockAction,
        lexStateCondition: this.LexState.AtKeyframes,
        dontAppendNonToken: true
    }, {
        pattern: /^@-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*/i,
        action: atRuleAction,
        lexStateCondition: this.LexState.Initial
    }, {
        pattern: /^;/i,
        action: endAtRuleAction,
        lexStateCondition: this.LexState.AtRule
    }, {
        pattern: urlPattern,
        action: urlAction,
        lexStateCondition: this.LexState.AtRule
    }, {
        pattern: stringPattern,
        action: stringAction,
        lexStateCondition: this.LexState.AtRule
    }, {
        pattern: stringPattern,
        action: stringAction,
        lexStateCondition: this.LexState.AtKeyframes
    }, {
        pattern: identPattern,
        action: atRuleIdentAction,
        lexStateCondition: this.LexState.AtRule,
        dontAppendNonToken: true
    }, {
        pattern: identPattern,
        action: atRuleIdentAction,
        lexStateCondition: this.LexState.AtMedia,
        dontAppendNonToken: true
    }, {
        pattern: startBlockPattern,
        action: startAtRuleBlockAction,
        lexStateCondition: this.LexState.AtRule,
        dontAppendNonToken: true
    }];
    
    function commentAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-comment"));
    }
    
    function commentStartAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-comment"));
        this.continueState = this.ContinueState.Comment;
    }
    
    function commentEndAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-comment"));
        this.continueState = this.ContinueState.None;
    }

    function commentMiddleAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-comment"));
    }
    
    function selectorAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-selector"));
    }
    
    function startRulesetBlockAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.DeclarationProperty;
    }
    
    function endRulesetBlockAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.Initial;
    }
    
    const propertyKeywords = {
        "background": true,
        "background-attachment": true,
        "background-clip": true,
        "background-color": true,
        "background-image": true,
        "background-origin": true,
        "background-position": true,
        "background-position-x": true,
        "background-position-y": true,
        "background-repeat": true,
        "background-repeat-x": true,
        "background-repeat-y": true,
        "background-size": true,
        "border": true,
        "border-bottom": true,
        "border-bottom-color": true,
        "border-bottom-left-radius": true,
        "border-bottom-right-radius": true,
        "border-bottom-style": true,
        "border-bottom-width": true,
        "border-collapse": true,
        "border-color": true,
        "border-left": true,
        "border-left-color": true,
        "border-left-style": true,
        "border-left-width": true,
        "border-radius": true,
        "border-right": true,
        "border-right-color": true,
        "border-right-style": true,
        "border-right-width": true,
        "border-spacing": true,
        "border-style": true,
        "border-top": true,
        "border-top-color": true,
        "border-top-left-radius": true,
        "border-top-right-radius": true,
        "border-top-style": true,
        "border-top-width": true,
        "border-width": true,
        "bottom": true,
        "caption-side": true,
        "clear": true,
        "clip": true,
        "color": true,
        "content": true,
        "counter-increment": true,
        "counter-reset": true,
        "cursor": true,
        "direction": true,
        "display": true,
        "empty-cells": true,
        "float": true,
        "font": true,
        "font-family": true,
        "font-size": true,
        "font-stretch": true,
        "font-style": true,
        "font-variant": true,
        "font-weight": true,
        "height": true,
        "left": true,
        "letter-spacing": true,
        "line-height": true,
        "list-style": true,
        "list-style-image": true,
        "list-style-position": true,
        "list-style-type": true,
        "margin": true,
        "margin-bottom": true,
        "margin-left": true,
        "margin-right": true,
        "margin-top": true,
        "max-height": true,
        "max-width": true,
        "min-height": true,
        "min-width": true,
        "opacity": true,
        "orphans": true,
        "outline": true,
        "outline-color": true,
        "outline-offset": true,
        "outline-style": true,
        "outline-width": true,
        "overflow": true,
        "overflow-x": true,
        "overflow-y": true,
        "padding": true,
        "padding-bottom": true,
        "padding-left": true,
        "padding-right": true,
        "padding-top": true,
        "page": true,
        "page-break-after": true,
        "page-break-before": true,
        "page-break-inside": true,
        "pointer-events": true,
        "position": true,
        "quotes": true,
        "resize": true,
        "right": true,
        "size": true,
        "src": true,
        "table-layout": true,
        "text-align": true,
        "text-decoration": true,
        "text-indent": true,
        "text-line-through": true,
        "text-line-through-color": true,
        "text-line-through-mode": true,
        "text-line-through-style": true,
        "text-line-through-width": true,
        "text-overflow": true,
        "text-overline": true,
        "text-overline-color": true,
        "text-overline-mode": true,
        "text-overline-style": true,
        "text-overline-width": true,
        "text-rendering": true,
        "text-shadow": true,
        "text-transform": true,
        "text-underline": true,
        "text-underline-color": true,
        "text-underline-mode": true,
        "text-underline-style": true,
        "text-underline-width": true,
        "top": true,
        "unicode-bidi": true,
        "unicode-range": true,
        "vertical-align": true,
        "visibility": true,
        "white-space": true,
        "widows": true,
        "width": true,
        "word-break": true,
        "word-spacing": true,
        "word-wrap": true,
        "z-index": true,
        "zoom": true,
        "-webkit-animation": true,
        "-webkit-animation-delay": true,
        "-webkit-animation-direction": true,
        "-webkit-animation-duration": true,
        "-webkit-animation-iteration-count": true,
        "-webkit-animation-name": true,
        "-webkit-animation-play-state": true,
        "-webkit-animation-timing-function": true,
        "-webkit-appearance": true,
        "-webkit-backface-visibility": true,
        "-webkit-background-clip": true,
        "-webkit-background-composite": true,
        "-webkit-background-origin": true,
        "-webkit-background-size": true,
        "-webkit-binding": true,
        "-webkit-border-fit": true,
        "-webkit-border-horizontal-spacing": true,
        "-webkit-border-image": true,
        "-webkit-border-radius": true,
        "-webkit-border-vertical-spacing": true,
        "-webkit-box-align": true,
        "-webkit-box-direction": true,
        "-webkit-box-flex": true,
        "-webkit-box-flex-group": true,
        "-webkit-box-lines": true,
        "-webkit-box-ordinal-group": true,
        "-webkit-box-orient": true,
        "-webkit-box-pack": true,
        "-webkit-box-reflect": true,
        "-webkit-box-shadow": true,
        "-webkit-box-sizing": true,
        "-webkit-column-break-after": true,
        "-webkit-column-break-before": true,
        "-webkit-column-break-inside": true,
        "-webkit-column-count": true,
        "-webkit-column-gap": true,
        "-webkit-column-rule": true,
        "-webkit-column-rule-color": true,
        "-webkit-column-rule-style": true,
        "-webkit-column-rule-width": true,
        "-webkit-column-width": true,
        "-webkit-columns": true,
        "-webkit-font-size-delta": true,
        "-webkit-font-smoothing": true,
        "-webkit-highlight": true,
        "-webkit-line-break": true,
        "-webkit-line-clamp": true,
        "-webkit-margin-bottom-collapse": true,
        "-webkit-margin-collapse": true,
        "-webkit-margin-start": true,
        "-webkit-margin-top-collapse": true,
        "-webkit-marquee": true,
        "-webkit-marquee-direction": true,
        "-webkit-marquee-increment": true,
        "-webkit-marquee-repetition": true,
        "-webkit-marquee-speed": true,
        "-webkit-marquee-style": true,
        "-webkit-mask": true,
        "-webkit-mask-attachment": true,
        "-webkit-mask-box-image": true,
        "-webkit-mask-clip": true,
        "-webkit-mask-composite": true,
        "-webkit-mask-image": true,
        "-webkit-mask-origin": true,
        "-webkit-mask-position": true,
        "-webkit-mask-position-x": true,
        "-webkit-mask-position-y": true,
        "-webkit-mask-repeat": true,
        "-webkit-mask-repeat-x": true,
        "-webkit-mask-repeat-y": true,
        "-webkit-mask-size": true,
        "-webkit-match-nearest-mail-blockquote-color": true,
        "-webkit-nbsp-mode": true,
        "-webkit-padding-start": true,
        "-webkit-perspective": true,
        "-webkit-perspective-origin": true,
        "-webkit-perspective-origin-x": true,
        "-webkit-perspective-origin-y": true,
        "-webkit-rtl-ordering": true,
        "-webkit-text-decorations-in-effect": true,
        "-webkit-text-fill-color": true,
        "-webkit-text-security": true,
        "-webkit-text-size-adjust": true,
        "-webkit-text-stroke": true,
        "-webkit-text-stroke-color": true,
        "-webkit-text-stroke-width": true,
        "-webkit-transform": true,
        "-webkit-transform-origin": true,
        "-webkit-transform-origin-x": true,
        "-webkit-transform-origin-y": true,
        "-webkit-transform-origin-z": true,
        "-webkit-transform-style": true,
        "-webkit-transition": true,
        "-webkit-transition-delay": true,
        "-webkit-transition-duration": true,
        "-webkit-transition-property": true,
        "-webkit-transition-timing-function": true,
        "-webkit-user-drag": true,
        "-webkit-user-modify": true,
        "-webkit-user-select": true,
        "-webkit-variable-declaration-block": true
    };
    function propertyAction(token)
    {
        this.cursor += token.length;
        if (token in propertyKeywords) {
            this.appendNonToken.call(this);
            this.newLine.appendChild(this.createSpan(token, "webkit-css-property"));
        } else
            this.nonToken += token;
    }
    
    function declarationColonAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.DeclarationValue;
    }

    const valueKeywords = {
        "inherit": true,
        "initial": true,
        "none": true,
        "hidden": true,
        "inset": true,
        "groove": true,
        "ridge": true,
        "outset": true,
        "dotted": true,
        "dashed": true,
        "solid": true,
        "double": true,
        "caption": true,
        "icon": true,
        "menu": true,
        "message-box": true,
        "small-caption": true,
        "-webkit-mini-control": true,
        "-webkit-small-control": true,
        "-webkit-control": true,
        "status-bar": true,
        "italic": true,
        "oblique": true,
        "all": true,
        "small-caps": true,
        "normal": true,
        "bold": true,
        "bolder": true,
        "lighter": true,
        "xx-small": true,
        "x-small": true,
        "small": true,
        "medium": true,
        "large": true,
        "x-large": true,
        "xx-large": true,
        "-webkit-xxx-large": true,
        "smaller": true,
        "larger": true,
        "wider": true,
        "narrower": true,
        "ultra-condensed": true,
        "extra-condensed": true,
        "condensed": true,
        "semi-condensed": true,
        "semi-expanded": true,
        "expanded": true,
        "extra-expanded": true,
        "ultra-expanded": true,
        "serif": true,
        "sans-serif": true,
        "cursive": true,
        "fantasy": true,
        "monospace": true,
        "-webkit-body": true,
        "aqua": true,
        "black": true,
        "blue": true,
        "fuchsia": true,
        "gray": true,
        "green": true,
        "lime": true,
        "maroon": true,
        "navy": true,
        "olive": true,
        "orange": true,
        "purple": true,
        "red": true,
        "silver": true,
        "teal": true,
        "white": true,
        "yellow": true,
        "transparent": true,
        "-webkit-link": true,
        "-webkit-activelink": true,
        "activeborder": true,
        "activecaption": true,
        "appworkspace": true,
        "background": true,
        "buttonface": true,
        "buttonhighlight": true,
        "buttonshadow": true,
        "buttontext": true,
        "captiontext": true,
        "graytext": true,
        "highlight": true,
        "highlighttext": true,
        "inactiveborder": true,
        "inactivecaption": true,
        "inactivecaptiontext": true,
        "infobackground": true,
        "infotext": true,
        "match": true,
        "menutext": true,
        "scrollbar": true,
        "threeddarkshadow": true,
        "threedface": true,
        "threedhighlight": true,
        "threedlightshadow": true,
        "threedshadow": true,
        "window": true,
        "windowframe": true,
        "windowtext": true,
        "-webkit-focus-ring-color": true,
        "currentcolor": true,
        "grey": true,
        "-webkit-text": true,
        "repeat": true,
        "repeat-x": true,
        "repeat-y": true,
        "no-repeat": true,
        "clear": true,
        "copy": true,
        "source-over": true,
        "source-in": true,
        "source-out": true,
        "source-atop": true,
        "destination-over": true,
        "destination-in": true,
        "destination-out": true,
        "destination-atop": true,
        "xor": true,
        "plus-darker": true,
        "plus-lighter": true,
        "baseline": true,
        "middle": true,
        "sub": true,
        "super": true,
        "text-top": true,
        "text-bottom": true,
        "top": true,
        "bottom": true,
        "-webkit-baseline-middle": true,
        "-webkit-auto": true,
        "left": true,
        "right": true,
        "center": true,
        "justify": true,
        "-webkit-left": true,
        "-webkit-right": true,
        "-webkit-center": true,
        "outside": true,
        "inside": true,
        "disc": true,
        "circle": true,
        "square": true,
        "decimal": true,
        "decimal-leading-zero": true,
        "lower-roman": true,
        "upper-roman": true,
        "lower-greek": true,
        "lower-alpha": true,
        "lower-latin": true,
        "upper-alpha": true,
        "upper-latin": true,
        "hebrew": true,
        "armenian": true,
        "georgian": true,
        "cjk-ideographic": true,
        "hiragana": true,
        "katakana": true,
        "hiragana-iroha": true,
        "katakana-iroha": true,
        "inline": true,
        "block": true,
        "list-item": true,
        "run-in": true,
        "compact": true,
        "inline-block": true,
        "table": true,
        "inline-table": true,
        "table-row-group": true,
        "table-header-group": true,
        "table-footer-group": true,
        "table-row": true,
        "table-column-group": true,
        "table-column": true,
        "table-cell": true,
        "table-caption": true,
        "-webkit-box": true,
        "-webkit-inline-box": true,
        "-wap-marquee": true,
        "auto": true,
        "crosshair": true,
        "default": true,
        "pointer": true,
        "move": true,
        "vertical-text": true,
        "cell": true,
        "context-menu": true,
        "alias": true,
        "progress": true,
        "no-drop": true,
        "not-allowed": true,
        "-webkit-zoom-in": true,
        "-webkit-zoom-out": true,
        "e-resize": true,
        "ne-resize": true,
        "nw-resize": true,
        "n-resize": true,
        "se-resize": true,
        "sw-resize": true,
        "s-resize": true,
        "w-resize": true,
        "ew-resize": true,
        "ns-resize": true,
        "nesw-resize": true,
        "nwse-resize": true,
        "col-resize": true,
        "row-resize": true,
        "text": true,
        "wait": true,
        "help": true,
        "all-scroll": true,
        "-webkit-grab": true,
        "-webkit-grabbing": true,
        "ltr": true,
        "rtl": true,
        "capitalize": true,
        "uppercase": true,
        "lowercase": true,
        "visible": true,
        "collapse": true,
        "above": true,
        "absolute": true,
        "always": true,
        "avoid": true,
        "below": true,
        "bidi-override": true,
        "blink": true,
        "both": true,
        "close-quote": true,
        "crop": true,
        "cross": true,
        "embed": true,
        "fixed": true,
        "hand": true,
        "hide": true,
        "higher": true,
        "invert": true,
        "landscape": true,
        "level": true,
        "line-through": true,
        "local": true,
        "loud": true,
        "lower": true,
        "-webkit-marquee": true,
        "mix": true,
        "no-close-quote": true,
        "no-open-quote": true,
        "nowrap": true,
        "open-quote": true,
        "overlay": true,
        "overline": true,
        "portrait": true,
        "pre": true,
        "pre-line": true,
        "pre-wrap": true,
        "relative": true,
        "scroll": true,
        "separate": true,
        "show": true,
        "static": true,
        "thick": true,
        "thin": true,
        "underline": true,
        "-webkit-nowrap": true,
        "stretch": true,
        "start": true,
        "end": true,
        "reverse": true,
        "horizontal": true,
        "vertical": true,
        "inline-axis": true,
        "block-axis": true,
        "single": true,
        "multiple": true,
        "forwards": true,
        "backwards": true,
        "ahead": true,
        "up": true,
        "down": true,
        "slow": true,
        "fast": true,
        "infinite": true,
        "slide": true,
        "alternate": true,
        "read-only": true,
        "read-write": true,
        "read-write-plaintext-only": true,
        "element": true,
        "ignore": true,
        "intrinsic": true,
        "min-intrinsic": true,
        "clip": true,
        "ellipsis": true,
        "discard": true,
        "dot-dash": true,
        "dot-dot-dash": true,
        "wave": true,
        "continuous": true,
        "skip-white-space": true,
        "break-all": true,
        "break-word": true,
        "space": true,
        "after-white-space": true,
        "checkbox": true,
        "radio": true,
        "push-button": true,
        "square-button": true,
        "button": true,
        "button-bevel": true,
        "default-button": true,
        "list-button": true,
        "listbox": true,
        "listitem": true,
        "media-fullscreen-button": true,
        "media-mute-button": true,
        "media-play-button": true,
        "media-seek-back-button": true,
        "media-seek-forward-button": true,
        "media-rewind-button": true,
        "media-return-to-realtime-button": true,
        "media-slider": true,
        "media-sliderthumb": true,
        "media-volume-slider-container": true,
        "media-volume-slider": true,
        "media-volume-sliderthumb": true,
        "media-controls-background": true,
        "media-current-time-display": true,
        "media-time-remaining-display": true,
        "menulist": true,
        "menulist-button": true,
        "menulist-text": true,
        "menulist-textfield": true,
        "slider-horizontal": true,
        "slider-vertical": true,
        "sliderthumb-horizontal": true,
        "sliderthumb-vertical": true,
        "caret": true,
        "searchfield": true,
        "searchfield-decoration": true,
        "searchfield-results-decoration": true,
        "searchfield-results-button": true,
        "searchfield-cancel-button": true,
        "textfield": true,
        "textarea": true,
        "caps-lock-indicator": true,
        "round": true,
        "border": true,
        "border-box": true,
        "content": true,
        "content-box": true,
        "padding": true,
        "padding-box": true,
        "contain": true,
        "cover": true,
        "logical": true,
        "visual": true,
        "lines": true,
        "running": true,
        "paused": true,
        "flat": true,
        "preserve-3d": true,
        "ease": true,
        "linear": true,
        "ease-in": true,
        "ease-out": true,
        "ease-in-out": true,
        "document": true,
        "reset": true,
        "visiblePainted": true,
        "visibleFill": true,
        "visibleStroke": true,
        "painted": true,
        "fill": true,
        "stroke": true,
        "antialiased": true,
        "subpixel-antialiased": true,
        "optimizeSpeed": true,
        "optimizeLegibility": true,
        "geometricPrecision": true
    };
    function valueIdentAction(token) {
        this.cursor += token.length;
        if (token in valueKeywords) {
            this.appendNonToken.call(this);
            this.newLine.appendChild(this.createSpan(token, "webkit-css-keyword"));
        } else
            this.nonToken += token;
    }

    function numvalueAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-number"));
    }
    
    function declarationSemicolonAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.DeclarationProperty;
    }
    
    function urlAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-url"));
    }
    
    function stringAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-string"));
    }
    
    function colorAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-color"));
    }
    
    function importantAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-important"));
    }
    
    function atMediaAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-at-rule"));
        this.lexState = this.LexState.AtMedia;
    }
    
    function startAtMediaBlockAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.Initial;
    }
    
    function atKeyframesAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-at-rule"));
        this.lexState = this.LexState.AtKeyframes;
    }
    
    function startAtKeyframesBlockAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.Initial;
    }
    
    function atRuleAction(token) {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-at-rule"));
        this.lexState = this.LexState.AtRule;
    }
    
    function endAtRuleAction(token) {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.Initial;
    }
    
    function startAtRuleBlockAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.DeclarationProperty;
    }
    
    const mediaTypes = ["all", "aural", "braille", "embossed", "handheld", "print", "projection", "screen", "tty", "tv"];
    function atRuleIdentAction(token) {
        this.cursor += token.length;
        if (mediaTypes.indexOf(token) === -1)
            this.nonToken += token;
        else {
            this.appendNonToken.call(this);
            this.newLine.appendChild(this.createSpan(token, "webkit-css-keyword"));
        }
    }
}

WebInspector.CSSSourceSyntaxHighlighter.prototype.__proto__ = WebInspector.SourceSyntaxHighlighter.prototype;
