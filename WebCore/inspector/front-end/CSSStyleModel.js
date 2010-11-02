/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

WebInspector.CSSStyleModel = function()
{
}

WebInspector.CSSStyleModel.parseRuleArrayPayload = function(ruleArray)
{
    var result = [];
    for (var i = 0; i < ruleArray.length; ++i)
        result.push(WebInspector.CSSRule.parsePayload(ruleArray[i]));
    return result;
}

WebInspector.CSSStyleModel.prototype = {
    getStylesAsync: function(nodeId, userCallback)
    {
        function callback(userCallback, payload)
        {
            if (!payload) {
                if (userCallback)
                    userCallback(null);
                return;
            }

            var result = {};
            if ("inlineStyle" in payload)
                result.inlineStyle = WebInspector.CSSStyleDeclaration.parsePayload(payload.inlineStyle);

            result.computedStyle = WebInspector.CSSStyleDeclaration.parsePayload(payload.computedStyle);
            result.matchedCSSRules = WebInspector.CSSStyleModel.parseRuleArrayPayload(payload.matchedCSSRules);

            result.styleAttributes = {};
            for (var name in payload.styleAttributes)
                result.styleAttributes[name] = WebInspector.CSSStyleDeclaration.parsePayload(payload.styleAttributes[name]);

            result.pseudoElements = [];
            for (var i = 0; i < payload.pseudoElements.length; ++i) {
                var entryPayload = payload.pseudoElements[i];
                result.pseudoElements.push({ pseudoId: entryPayload.pseudoId, rules: WebInspector.CSSStyleModel.parseRuleArrayPayload(entryPayload.rules) });
            }

            result.inherited = [];
            for (var i = 0; i < payload.inherited.length; ++i) {
                var entryPayload = payload.inherited[i];
                var entry = {};
                if ("inlineStyle" in entryPayload)
                    entry.inlineStyle = WebInspector.CSSStyleDeclaration.parsePayload(entryPayload.inlineStyle);
                if ("matchedCSSRules" in entryPayload)
                    entry.matchedCSSRules = WebInspector.CSSStyleModel.parseRuleArrayPayload(entryPayload.matchedCSSRules);
                result.inherited.push(entry);
            }

            if (userCallback)
                userCallback(result);
        }

        InspectorBackend.getStyles(nodeId, false, callback.bind(null, userCallback));
    },

    getComputedStyleAsync: function(nodeId, userCallback)
    {
        function callback(userCallback, stylePayload)
        {
            if (!stylePayload)
                userCallback(null);
            else
                userCallback(WebInspector.CSSStyleDeclaration.parsePayload(stylePayload));
        }

        InspectorBackend.getComputedStyle(nodeId, callback.bind(null, userCallback));
    },

    getInlineStyleAsync: function(nodeId, userCallback)
    {
        function callback(userCallback, stylePayload)
        {
            if (!stylePayload)
                userCallback(null);
            else
                userCallback(WebInspector.CSSStyleDeclaration.parsePayload(stylePayload));
        }

        InspectorBackend.getInlineStyle(nodeId, callback.bind(null, userCallback));
    },

    setRuleSelector: function(ruleId, newContent, nodeId, successCallback, failureCallback)
    {
        function callback(newRulePayload, doesAffectSelectedNode)
        {
            if (!newRulePayload)
                failureCallback();
            else
                successCallback(WebInspector.CSSRule.parsePayload(newRulePayload), doesAffectSelectedNode);
        }

        InspectorBackend.setRuleSelector(ruleId, newContent, nodeId, callback);
    },

    addRule: function(nodeId, newContent, successCallback, failureCallback)
    {
        function callback(rule, doesAffectSelectedNode)
        {
            if (!rule) {
                // Invalid syntax for a selector
                failureCallback();
            } else {
                var styleRule = WebInspector.CSSRule.parsePayload(rule);
                styleRule.rule = rule;
                successCallback(styleRule, doesAffectSelectedNode);
            }
        }

        InspectorBackend.addRule(newContent, nodeId, callback);
    },

    setCSSText: function(styleId, cssText)
    {
        InspectorBackend.setStyleText(styleId, cssText);
    }
}
