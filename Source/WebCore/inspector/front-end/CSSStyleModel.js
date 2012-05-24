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

/**
 * @constructor
 * @extends {WebInspector.Object}
 */
WebInspector.CSSStyleModel = function()
{
    this._pendingCommandsMajorState = [];
    WebInspector.domAgent.addEventListener(WebInspector.DOMAgent.Events.UndoRedoRequested, this._undoRedoRequested, this);
    WebInspector.domAgent.addEventListener(WebInspector.DOMAgent.Events.UndoRedoCompleted, this._undoRedoCompleted, this);
    this._resourceBinding = new WebInspector.CSSStyleModelResourceBinding(this);
    InspectorBackend.registerCSSDispatcher(new WebInspector.CSSDispatcher(this));
    CSSAgent.enable();
}

/**
 * @param {Array.<CSSAgent.CSSRule>} ruleArray
 */
WebInspector.CSSStyleModel.parseRuleArrayPayload = function(ruleArray)
{
    var result = [];
    for (var i = 0; i < ruleArray.length; ++i)
        result.push(WebInspector.CSSRule.parsePayload(ruleArray[i]));
    return result;
}

WebInspector.CSSStyleModel.Events = {
    StyleSheetChanged: "StyleSheetChanged",
    MediaQueryResultChanged: "MediaQueryResultChanged"
}

WebInspector.CSSStyleModel.prototype = {
    /**
     * @param {DOMAgent.NodeId} nodeId
     * @param {?Array.<string>|undefined} forcedPseudoClasses
     * @param {boolean} needPseudo
     * @param {boolean} needInherited
     * @param {function(?*)} userCallback
     */
    getMatchedStylesAsync: function(nodeId, forcedPseudoClasses, needPseudo, needInherited, userCallback)
    {
        /**
         * @param {function(?*)} userCallback
         * @param {?Protocol.Error} error
         * @param {Array.<CSSAgent.CSSRule>=} matchedPayload
         * @param {Array.<CSSAgent.PseudoIdRules>=} pseudoPayload
         * @param {Array.<CSSAgent.InheritedStyleEntry>=} inheritedPayload
         */
        function callback(userCallback, error, matchedPayload, pseudoPayload, inheritedPayload)
        {
            if (error) {
                if (userCallback)
                    userCallback(null);
                return;
            }

            var result = {};
            if (matchedPayload)
                result.matchedCSSRules = WebInspector.CSSStyleModel.parseRuleArrayPayload(matchedPayload);

            if (pseudoPayload) {
                result.pseudoElements = [];
                for (var i = 0; i < pseudoPayload.length; ++i) {
                    var entryPayload = pseudoPayload[i];
                    result.pseudoElements.push({ pseudoId: entryPayload.pseudoId, rules: WebInspector.CSSStyleModel.parseRuleArrayPayload(entryPayload.rules) });
                }
            }

            if (inheritedPayload) {
                result.inherited = [];
                for (var i = 0; i < inheritedPayload.length; ++i) {
                    var entryPayload = inheritedPayload[i];
                    var entry = {};
                    if (entryPayload.inlineStyle)
                        entry.inlineStyle = WebInspector.CSSStyleDeclaration.parsePayload(entryPayload.inlineStyle);
                    if (entryPayload.matchedCSSRules)
                        entry.matchedCSSRules = WebInspector.CSSStyleModel.parseRuleArrayPayload(entryPayload.matchedCSSRules);
                    result.inherited.push(entry);
                }
            }

            if (userCallback)
                userCallback(result);
        }

        CSSAgent.getMatchedStylesForNode(nodeId, forcedPseudoClasses || [], needPseudo, needInherited, callback.bind(null, userCallback));
    },

    /**
     * @param {DOMAgent.NodeId} nodeId
     * @param {?Array.<string>|undefined} forcedPseudoClasses
     * @param {function(?WebInspector.CSSStyleDeclaration)} userCallback
     */
    getComputedStyleAsync: function(nodeId, forcedPseudoClasses, userCallback)
    {
        /**
         * @param {function(?WebInspector.CSSStyleDeclaration)} userCallback
         */
        function callback(userCallback, error, computedPayload)
        {
            if (error || !computedPayload)
                userCallback(null);
            else
                userCallback(WebInspector.CSSStyleDeclaration.parseComputedStylePayload(computedPayload));
        }

        CSSAgent.getComputedStyleForNode(nodeId, forcedPseudoClasses || [], callback.bind(null, userCallback));
    },

    /**
     * @param {DOMAgent.NodeId} nodeId
     * @param {function(?WebInspector.CSSStyleDeclaration, ?WebInspector.CSSStyleDeclaration)} userCallback
     */
    getInlineStylesAsync: function(nodeId, userCallback)
    {
        /**
         * @param {function(?WebInspector.CSSStyleDeclaration, ?WebInspector.CSSStyleDeclaration)} userCallback
         * @param {?Protocol.Error} error
         * @param {?CSSAgent.CSSStyle=} inlinePayload
         * @param {?CSSAgent.CSSStyle=} attributesStylePayload
         */
        function callback(userCallback, error, inlinePayload, attributesStylePayload)
        {
            if (error || !inlinePayload)
                userCallback(null, null);
            else
                userCallback(WebInspector.CSSStyleDeclaration.parsePayload(inlinePayload), attributesStylePayload ? WebInspector.CSSStyleDeclaration.parsePayload(attributesStylePayload) : null);
        }

        CSSAgent.getInlineStylesForNode(nodeId, callback.bind(null, userCallback));
    },

    /**
     * @param {CSSAgent.CSSRuleId} ruleId
     * @param {DOMAgent.NodeId} nodeId
     * @param {string} newSelector
     * @param {function(WebInspector.CSSRule, boolean)} successCallback
     * @param {function()} failureCallback
     */
    setRuleSelector: function(ruleId, nodeId, newSelector, successCallback, failureCallback)
    {
        /**
         * @param {DOMAgent.NodeId} nodeId
         * @param {function(WebInspector.CSSRule, boolean)} successCallback
         * @param {CSSAgent.CSSRule} rulePayload
         * @param {?Array.<DOMAgent.NodeId>} selectedNodeIds
         */
        function checkAffectsCallback(nodeId, successCallback, rulePayload, selectedNodeIds)
        {
            if (!selectedNodeIds)
                return;
            var doesAffectSelectedNode = (selectedNodeIds.indexOf(nodeId) >= 0);
            var rule = WebInspector.CSSRule.parsePayload(rulePayload);
            successCallback(rule, doesAffectSelectedNode);
        }

        /**
         * @param {DOMAgent.NodeId} nodeId
         * @param {function(WebInspector.CSSRule, boolean)} successCallback
         * @param {function()} failureCallback
         * @param {?Protocol.Error} error
         * @param {string} newSelector
         * @param {?CSSAgent.CSSRule} rulePayload
         */
        function callback(nodeId, successCallback, failureCallback, newSelector, error, rulePayload)
        {
            this._pendingCommandsMajorState.pop();
            if (error)
                failureCallback();
            else {
                WebInspector.domAgent.markUndoableState();
                var ownerDocumentId = this._ownerDocumentId(nodeId);
                if (ownerDocumentId)
                    WebInspector.domAgent.querySelectorAll(ownerDocumentId, newSelector, checkAffectsCallback.bind(this, nodeId, successCallback, rulePayload));
                else
                    failureCallback();
            }
        }

        this._pendingCommandsMajorState.push(true);
        CSSAgent.setRuleSelector(ruleId, newSelector, callback.bind(this, nodeId, successCallback, failureCallback, newSelector));
    },

    /**
     * @param {DOMAgent.NodeId} nodeId
     * @param {string} selector
     * @param {function(WebInspector.CSSRule, boolean)} successCallback
     * @param {function()} failureCallback
     */
    addRule: function(nodeId, selector, successCallback, failureCallback)
    {
        /**
         * @param {DOMAgent.NodeId} nodeId
         * @param {function(WebInspector.CSSRule, boolean)} successCallback
         * @param {CSSAgent.CSSRule} rulePayload
         * @param {?Array.<DOMAgent.NodeId>} selectedNodeIds
         */
        function checkAffectsCallback(nodeId, successCallback, rulePayload, selectedNodeIds)
        {
            if (!selectedNodeIds)
                return;

            var doesAffectSelectedNode = (selectedNodeIds.indexOf(nodeId) >= 0);
            var rule = WebInspector.CSSRule.parsePayload(rulePayload);
            successCallback(rule, doesAffectSelectedNode);
        }

        /**
         * @param {function(WebInspector.CSSRule, boolean)} successCallback
         * @param {function()} failureCallback
         * @param {string} selector
         * @param {?Protocol.Error} error
         * @param {?CSSAgent.CSSRule} rulePayload
         */
        function callback(successCallback, failureCallback, selector, error, rulePayload)
        {
            this._pendingCommandsMajorState.pop();
            if (error) {
                // Invalid syntax for a selector
                failureCallback();
            } else {
                WebInspector.domAgent.markUndoableState();
                var ownerDocumentId = this._ownerDocumentId(nodeId);
                if (ownerDocumentId)
                    WebInspector.domAgent.querySelectorAll(ownerDocumentId, selector, checkAffectsCallback.bind(this, nodeId, successCallback, rulePayload));
                else
                    failureCallback();
            }
        }

        this._pendingCommandsMajorState.push(true);
        CSSAgent.addRule(nodeId, selector, callback.bind(this, successCallback, failureCallback, selector));
    },

    mediaQueryResultChanged: function()
    {
        this.dispatchEventToListeners(WebInspector.CSSStyleModel.Events.MediaQueryResultChanged);
    },

    /**
     * @param {DOMAgent.NodeId} nodeId
     */
    _ownerDocumentId: function(nodeId)
    {
        var node = WebInspector.domAgent.nodeForId(nodeId);
        if (!node)
            return null;
        return node.ownerDocument ? node.ownerDocument.id : null;
    },

    /**
     * @param {CSSAgent.StyleSheetId} styleSheetId
     */
    _fireStyleSheetChanged: function(styleSheetId)
    {
        if (!this._pendingCommandsMajorState.length)
            return;

        var majorChange = this._pendingCommandsMajorState[this._pendingCommandsMajorState.length - 1];

        if (!majorChange || !styleSheetId || !this.hasEventListeners(WebInspector.CSSStyleModel.Events.StyleSheetChanged))
            return;

        this.dispatchEventToListeners(WebInspector.CSSStyleModel.Events.StyleSheetChanged, { styleSheetId: styleSheetId, majorChange: majorChange });
    },

    /**
     * @param {CSSAgent.StyleSheetId} styleSheetId
     * @param {string} newText
     * @param {boolean} majorChange
     * @param {function(?string)} userCallback
     */
    setStyleSheetText: function(styleSheetId, newText, majorChange, userCallback)
    {
        function callback(error)
        {
            this._pendingCommandsMajorState.pop();
            if (!error && majorChange)
                WebInspector.domAgent.markUndoableState();
            
            if (!error && userCallback)
                userCallback(error);
        }
        this._pendingCommandsMajorState.push(majorChange);
        CSSAgent.setStyleSheetText(styleSheetId, newText, callback.bind(this));
    },

    _undoRedoRequested: function()
    {
        this._pendingCommandsMajorState.push(true);
    },

    _undoRedoCompleted: function()
    {
        this._pendingCommandsMajorState.pop();
    },

    /**
     * @param {WebInspector.CSSRule} rule
     * @param {function(?WebInspector.Resource)} callback
     */
    getViaInspectorResourceForRule: function(rule, callback)
    {
        if (!rule.id) {
            callback(null);
            return;
        }
        this._resourceBinding._requestViaInspectorResource(rule.id.styleSheetId, callback);
    }
}

WebInspector.CSSStyleModel.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @param {CSSAgent.CSSStyle} payload
 */
WebInspector.CSSStyleDeclaration = function(payload)
{
    this.id = payload.styleId;
    this.width = payload.width;
    this.height = payload.height;
    this.range = payload.range;
    this._shorthandValues = WebInspector.CSSStyleDeclaration.buildShorthandValueMap(payload.shorthandEntries);
    this._livePropertyMap = {}; // LIVE properties (source-based or style-based) : { name -> CSSProperty }
    this._allProperties = []; // ALL properties: [ CSSProperty ]
    this._longhandProperties = {}; // shorthandName -> [ CSSProperty ]
    this.__disabledProperties = {}; // DISABLED properties: { index -> CSSProperty }
    var payloadPropertyCount = payload.cssProperties.length;

    var propertyIndex = 0;
    for (var i = 0; i < payloadPropertyCount; ++i) {
        var property = WebInspector.CSSProperty.parsePayload(this, i, payload.cssProperties[i]);
        this._allProperties.push(property);
        if (property.disabled)
            this.__disabledProperties[i] = property;
        if (!property.active && !property.styleBased)
            continue;
        var name = property.name;
        this[propertyIndex] = name;
        this._livePropertyMap[name] = property;

        // Index longhand properties.
        if (property.shorthand) { // only for parsed
            var longhands = this._longhandProperties[property.shorthand];
            if (!longhands) {
                longhands = [];
                this._longhandProperties[property.shorthand] = longhands;
            }
            longhands.push(property);
        }
        ++propertyIndex;
    }
    this.length = propertyIndex;
    if ("cssText" in payload)
        this.cssText = payload.cssText;
}

/**
 * @param {Array.<CSSAgent.ShorthandEntry>} shorthandEntries
 * @return {Object}
 */
WebInspector.CSSStyleDeclaration.buildShorthandValueMap = function(shorthandEntries)
{
    var result = {};
    for (var i = 0; i < shorthandEntries.length; ++i)
        result[shorthandEntries[i].name] = shorthandEntries[i].value;
    return result;
}

/**
 * @param {CSSAgent.CSSStyle} payload
 * @return {WebInspector.CSSStyleDeclaration}
 */
WebInspector.CSSStyleDeclaration.parsePayload = function(payload)
{
    return new WebInspector.CSSStyleDeclaration(payload);
}

/**
 * @param {Array.<CSSAgent.CSSComputedStyleProperty>} payload
 * @return {WebInspector.CSSStyleDeclaration}
 */
WebInspector.CSSStyleDeclaration.parseComputedStylePayload = function(payload)
{
    var newPayload = /** @type {CSSAgent.CSSStyle} */ { cssProperties: [], shorthandEntries: [], width: "", height: "" };
    if (payload)
        newPayload.cssProperties = payload;

    return new WebInspector.CSSStyleDeclaration(newPayload);
}

WebInspector.CSSStyleDeclaration.prototype = {
    get allProperties()
    {
        return this._allProperties;
    },

    /**
     * @param {string} name
     * @return {WebInspector.CSSProperty|undefined}
     */
    getLiveProperty: function(name)
    {
        return this._livePropertyMap[name];
    },

    /**
     * @param {string} name
     * @return {string}
     */
    getPropertyValue: function(name)
    {
        var property = this._livePropertyMap[name];
        return property ? property.value : "";
    },

    /**
     * @param {string} name
     * @return {string}
     */
    getPropertyPriority: function(name)
    {
        var property = this._livePropertyMap[name];
        return property ? property.priority : "";
    },

    /**
     * @param {string} name
     * @return {boolean}
     */
    isPropertyImplicit: function(name)
    {
        var property = this._livePropertyMap[name];
        return property ? property.implicit : "";
    },

    /**
     * @param {string} name
     * @return {Array.<WebInspector.CSSProperty>}
     */
    getLonghandProperties: function(name)
    {
        return this._longhandProperties[name] || [];
    },

    /**
     * @param {string} shorthandProperty
     * @return {string}
     */
    getShorthandValue: function(shorthandProperty)
    {
        var property = this.getLiveProperty(shorthandProperty);
        return property ? property.value : this._shorthandValues[shorthandProperty];
    },

    /**
     * @param {string} shorthandProperty
     * @return {?string}
     */
    getShorthandPriority: function(shorthandProperty)
    {
        var priority = this.getPropertyPriority(shorthandProperty);
        if (priority)
            return priority;

        var longhands = this._longhandProperties[shorthandProperty];
        return longhands ? this.getPropertyPriority(longhands[0]) : null;
    },

    /**
     * @param {number} index
     * @return {?WebInspector.CSSProperty}
     */
    propertyAt: function(index)
    {
        return (index < this.allProperties.length) ? this.allProperties[index] : null;
    },

    /**
     * @return {number}
     */
    pastLastSourcePropertyIndex: function()
    {
        for (var i = this.allProperties.length - 1; i >= 0; --i) {
            var property = this.allProperties[i];
            if (property.active || property.disabled)
                return i + 1;
        }
        return 0;
    },

    /**
     * @param {number=} index
     */
    newBlankProperty: function(index)
    {
        index = (typeof index === "undefined") ? this.pastLastSourcePropertyIndex() : index;
        return new WebInspector.CSSProperty(this, index, "", "", "", "active", true, false, "", "");
    },

    /**
     * @param {number} index
     * @param {string} name
     * @param {string} value
     * @param {function(?WebInspector.CSSStyleDeclaration)} userCallback
     */
    insertPropertyAt: function(index, name, value, userCallback)
    {
        /**
         * @param {function(?WebInspector.CSSStyleDeclaration)} userCallback
         * @param {?string} error
         * @param {CSSAgent.CSSStyle} payload
         */
        function callback(userCallback, error, payload)
        {
            WebInspector.cssModel._pendingCommandsMajorState.pop();
            if (!userCallback)
                return;

            if (error) {
                console.error(error);
                userCallback(null);
            } else {
                userCallback(WebInspector.CSSStyleDeclaration.parsePayload(payload));
            }
        }

        if (!this.id)
            throw "No style id";

        WebInspector.cssModel._pendingCommandsMajorState.push(true);
        CSSAgent.setPropertyText(this.id, index, name + ": " + value + ";", false, callback.bind(this, userCallback));
    },

    /**
     * @param {string} name
     * @param {string} value
     * @param {function(?WebInspector.CSSStyleDeclaration)} userCallback
     */
    appendProperty: function(name, value, userCallback)
    {
        this.insertPropertyAt(this.allProperties.length, name, value, userCallback);
    }
}

/**
 * @constructor
 * @param {CSSAgent.CSSRule} payload
 */
WebInspector.CSSRule = function(payload)
{
    this.id = payload.ruleId;
    this.selectorText = payload.selectorText;
    this.sourceLine = payload.sourceLine;
    this.sourceURL = payload.sourceURL;
    this.origin = payload.origin;
    this.style = WebInspector.CSSStyleDeclaration.parsePayload(payload.style);
    this.style.parentRule = this;
    this.selectorRange = payload.selectorRange;
    if (payload.media)
        this.media = WebInspector.CSSMedia.parseMediaArrayPayload(payload.media);
}

/**
 * @param {CSSAgent.CSSRule} payload
 * @return {WebInspector.CSSRule}
 */
WebInspector.CSSRule.parsePayload = function(payload)
{
    return new WebInspector.CSSRule(payload);
}

WebInspector.CSSRule.prototype = {
    get isUserAgent()
    {
        return this.origin === "user-agent";
    },

    get isUser()
    {
        return this.origin === "user";
    },

    get isViaInspector()
    {
        return this.origin === "inspector";
    },

    get isRegular()
    {
        return this.origin === "regular";
    }
}

/**
 * @constructor
 * @param {?WebInspector.CSSStyleDeclaration} ownerStyle
 * @param {number} index
 * @param {string} name
 * @param {string} value
 * @param {?string} priority
 * @param {string} status
 * @param {boolean} parsedOk
 * @param {boolean} implicit
 * @param {string} shorthand
 * @param {?string=} text
 */
WebInspector.CSSProperty = function(ownerStyle, index, name, value, priority, status, parsedOk, implicit, shorthand, text)
{
    this.ownerStyle = ownerStyle;
    this.index = index;
    this.name = name;
    this.value = value;
    this.priority = priority;
    this.status = status;
    this.parsedOk = parsedOk;
    this.implicit = implicit;
    this.shorthand = shorthand;
    this.text = text;
}

/**
 * @param {?WebInspector.CSSStyleDeclaration} ownerStyle
 * @param {number} index
 * @param {CSSAgent.CSSProperty} payload
 * @return {WebInspector.CSSProperty}
 */
WebInspector.CSSProperty.parsePayload = function(ownerStyle, index, payload)
{
    // The following default field values are used in the payload:
    // priority: ""
    // parsedOk: true
    // implicit: false
    // status: "style"
    // shorthandName: ""
    var result = new WebInspector.CSSProperty(
        ownerStyle, index, payload.name, payload.value, payload.priority || "", payload.status || "style", ("parsedOk" in payload) ? !!payload.parsedOk : true, !!payload.implicit, payload.shorthandName || "", payload.text);
    return result;
}

WebInspector.CSSProperty.prototype = {
    get propertyText()
    {
        if (this.text !== undefined)
            return this.text;

        if (this.name === "")
            return "";
        return this.name + ": " + this.value + (this.priority ? " !" + this.priority : "") + ";";
    },

    get isLive()
    {
        return this.active || this.styleBased;
    },

    get active()
    {
        return this.status === "active";
    },

    get styleBased()
    {
        return this.status === "style";
    },

    get inactive()
    {
        return this.status === "inactive";
    },

    get disabled()
    {
        return this.status === "disabled";
    },

    /**
     * Replaces "propertyName: propertyValue [!important];" in the stylesheet by an arbitrary propertyText.
     *
     * @param {string} propertyText
     * @param {boolean} majorChange
     * @param {boolean} overwrite
     * @param {function(?WebInspector.CSSStyleDeclaration)=} userCallback
     */
    setText: function(propertyText, majorChange, overwrite, userCallback)
    {
        /**
         * @param {?WebInspector.CSSStyleDeclaration} style
         */
        function enabledCallback(style)
        {
            if (userCallback)
                userCallback(style);
        }

        /**
         * @param {?string} error
         * @param {?CSSAgent.CSSStyle} stylePayload
         */
        function callback(error, stylePayload)
        {
            WebInspector.cssModel._pendingCommandsMajorState.pop();
            if (!error) {
                if (majorChange)
                    WebInspector.domAgent.markUndoableState();
                this.text = propertyText;
                var style = WebInspector.CSSStyleDeclaration.parsePayload(stylePayload);
                var newProperty = style.allProperties[this.index];

                if (newProperty && this.disabled && !propertyText.match(/^\s*$/)) {
                    newProperty.setDisabled(false, enabledCallback);
                    return;
                }

                if (userCallback)
                    userCallback(style);
            } else {
                if (userCallback)
                    userCallback(null);
            }
        }

        if (!this.ownerStyle)
            throw "No ownerStyle for property";

        if (!this.ownerStyle.id)
            throw "No owner style id";

        // An index past all the properties adds a new property to the style.
        WebInspector.cssModel._pendingCommandsMajorState.push(majorChange);
        CSSAgent.setPropertyText(this.ownerStyle.id, this.index, propertyText, overwrite, callback.bind(this));
    },

    /**
     * @param {string} newValue
     * @param {boolean} majorChange
     * @param {boolean} overwrite
     * @param {function(?WebInspector.CSSStyleDeclaration)=} userCallback
     */
    setValue: function(newValue, majorChange, overwrite, userCallback)
    {
        var text = this.name + ": " + newValue + (this.priority ? " !" + this.priority : "") + ";"
        this.setText(text, majorChange, overwrite, userCallback);
    },

    /**
     * @param {boolean} disabled
     * @param {function(?WebInspector.CSSStyleDeclaration)=} userCallback
     */
    setDisabled: function(disabled, userCallback)
    {
        if (!this.ownerStyle && userCallback)
            userCallback(null);
        if (disabled === this.disabled && userCallback)
            userCallback(this.ownerStyle);

        /**
         * @param {?string} error
         * @param {CSSAgent.CSSStyle} stylePayload
         */
        function callback(error, stylePayload)
        {
            WebInspector.cssModel._pendingCommandsMajorState.pop();
            if (error) {
                if (userCallback)
                    userCallback(null);
                return;
            }
            WebInspector.domAgent.markUndoableState();
            if (userCallback) {
                var style = WebInspector.CSSStyleDeclaration.parsePayload(stylePayload);
                userCallback(style);
            }
        }

        if (!this.ownerStyle.id)
            throw "No owner style id";

        WebInspector.cssModel._pendingCommandsMajorState.push(false);
        CSSAgent.toggleProperty(this.ownerStyle.id, this.index, disabled, callback.bind(this));
    }
}

/**
 * @constructor
 * @param {CSSAgent.CSSMedia} payload
 */
WebInspector.CSSMedia = function(payload)
{
    this.text = payload.text;
    this.source = payload.source;
    this.sourceURL = payload.sourceURL || "";
    this.sourceLine = typeof payload.sourceLine === "undefined" || this.source === "linkedSheet" ? -1 : payload.sourceLine;
}

WebInspector.CSSMedia.Source = {
    LINKED_SHEET: "linkedSheet",
    INLINE_SHEET: "inlineSheet",
    MEDIA_RULE: "mediaRule",
    IMPORT_RULE: "importRule"
};

/**
 * @param {CSSAgent.CSSMedia} payload
 * @return {WebInspector.CSSMedia}
 */
WebInspector.CSSMedia.parsePayload = function(payload)
{
    return new WebInspector.CSSMedia(payload);
}

/**
 * @param {Array.<CSSAgent.CSSMedia>} payload
 * @return {Array.<WebInspector.CSSMedia>}
 */
WebInspector.CSSMedia.parseMediaArrayPayload = function(payload)
{
    var result = [];
    for (var i = 0; i < payload.length; ++i)
        result.push(WebInspector.CSSMedia.parsePayload(payload[i]));
    return result;
}

/**
 * @constructor
 * @param {CSSAgent.CSSStyleSheetBody} payload
 */
WebInspector.CSSStyleSheet = function(payload)
{
    this.id = payload.styleSheetId;
    this.rules = [];
    this.styles = {};
    for (var i = 0; i < payload.rules.length; ++i) {
        var rule = WebInspector.CSSRule.parsePayload(payload.rules[i]);
        this.rules.push(rule);
        if (rule.style)
            this.styles[rule.style.id] = rule.style;
    }
    if ("text" in payload)
        this._text = payload.text;
}

/**
 * @param {CSSAgent.StyleSheetId} styleSheetId
 * @param {function(?WebInspector.CSSStyleSheet)} userCallback
 */
WebInspector.CSSStyleSheet.createForId = function(styleSheetId, userCallback)
{
    /**
     * @param {?string} error
     * @param {CSSAgent.CSSStyleSheetBody} styleSheetPayload
     */
    function callback(error, styleSheetPayload)
    {
        if (error)
            userCallback(null);
        else
            userCallback(new WebInspector.CSSStyleSheet(styleSheetPayload));
    }
    CSSAgent.getStyleSheet(styleSheetId, callback.bind(this));
}

WebInspector.CSSStyleSheet.prototype = {
    /**
     * @return {string|undefined}
     */
    getText: function()
    {
        return this._text;
    },

    /**
     * @param {string} newText
     * @param {boolean} majorChange
     * @param {function(?string)=} userCallback
     */
    setText: function(newText, majorChange, userCallback)
    {
        /**
         * @param {?string} error
         */
        function callback(error)
        {
            if (!error)
                WebInspector.domAgent.markUndoableState();

            WebInspector.cssModel._pendingCommandsMajorState.pop();
            if (userCallback)
                userCallback(error);
        }

        WebInspector.cssModel._pendingCommandsMajorState.push(majorChange);
        CSSAgent.setStyleSheetText(this.id, newText, callback.bind(this));
    }
}

/**
 * @constructor
 * @implements {WebInspector.ResourceDomainModelBinding}
 */
WebInspector.CSSStyleModelResourceBinding = function(cssModel)
{
    this._cssModel = cssModel;
    this._frameAndURLToStyleSheetId = {};
    this._styleSheetIdToHeader = {};
    this._cssModel.addEventListener(WebInspector.CSSStyleModel.Events.StyleSheetChanged, this._styleSheetChanged, this);
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.InspectedURLChanged, this._inspectedURLChanged, this);
    WebInspector.Resource.registerDomainModelBinding(WebInspector.resourceTypes.Stylesheet, this);
}

WebInspector.CSSStyleModelResourceBinding.prototype = {
    /**
     * @param {WebInspector.Resource} resource
     * @param {string} content
     * @param {boolean} majorChange
     * @param {function(?string)} userCallback
     */
    setContent: function(resource, content, majorChange, userCallback)
    {
        if (majorChange && resource.type === WebInspector.resourceTypes.Stylesheet)
            resource.addRevision(content);

        if (this._styleSheetIdForResource(resource)) {
            this._innerSetContent(resource, content, majorChange, userCallback, null);
            return;
        }
        this._loadStyleSheetHeaders(this._innerSetContent.bind(this, resource, content, majorChange, userCallback));
    },

    /**
     * @return {boolean}
     */
    canSetContent: function()
    {
        return true;
    },

    /**
     * @param {WebInspector.Resource} resource
     * @return {CSSAgent.StyleSheetId}
     */
    _styleSheetIdForResource: function(resource)
    {
        return this._frameAndURLToStyleSheetId[resource.frameId + ":" + resource.url];
    },

    /**
     * @param {WebInspector.Event} event
     */
    _inspectedURLChanged: function(event)
    {
        // Main frame navigation - clear history.
        this._frameAndURLToStyleSheetId = {};
        this._styleSheetIdToHeader = {};
    },

    /**
     * @param {WebInspector.Resource} resource
     * @param {string} content
     * @param {boolean} majorChange
     * @param {function(?string)} userCallback
     * @param {?string} error
     */
    _innerSetContent: function(resource, content, majorChange, userCallback, error)
    {
        if (error) {
            userCallback(error);
            return;
        }

        var styleSheetId = this._styleSheetIdForResource(resource);
        if (!styleSheetId) {
            userCallback("No stylesheet found: " + resource.frameId + ":" + resource.url);
            return;
        }

        this._isSettingContent = true;
        function callbackWrapper(error)
        {
            userCallback(error);
            delete this._isSettingContent;
        }
        this._cssModel.setStyleSheetText(styleSheetId, content, majorChange, callbackWrapper.bind(this));
    },

    /**
     * @param {function(?string)} callback
     */
    _loadStyleSheetHeaders: function(callback)
    {
        /**
         * @param {?string} error
         * @param {Array.<CSSAgent.CSSStyleSheetHeader>} infos
         */
        function didGetAllStyleSheets(error, infos)
        {
            if (error) {
                callback(error);
                return;
            }

            for (var i = 0; i < infos.length; ++i) {
                var info = infos[i];
                if (info.origin === "inspector") {
                    this._getOrCreateInspectorResource(info);
                    continue;
                }
                this._frameAndURLToStyleSheetId[info.frameId + ":" + info.sourceURL] = info.styleSheetId;
                this._styleSheetIdToHeader[info.styleSheetId] = info;
            }
            callback(null);
        }
        CSSAgent.getAllStyleSheets(didGetAllStyleSheets.bind(this));
    },

    /**
     * @param {WebInspector.Event} event
     */
    _styleSheetChanged: function(event)
    {
        if (this._isSettingContent)
            return;

        if (!event.data.majorChange)
            return;

        /**
         * @param {?string} error
         * @param {string} content
         */
        function callback(error, content)
        {
            if (!error)
                this._innerStyleSheetChanged(event.data.styleSheetId, content);
        }
        CSSAgent.getStyleSheetText(event.data.styleSheetId, callback.bind(this));
    },

    /**
     * @param {CSSAgent.StyleSheetId} styleSheetId
     * @param {string} content
     */
    _innerStyleSheetChanged: function(styleSheetId, content)
    {
        function setContent()
        {
            var header = this._styleSheetIdToHeader[styleSheetId];
            if (!header)
                return;

            var frame = WebInspector.resourceTreeModel.frameForId(header.frameId);
            if (!frame)
                return;

            var styleSheetURL = header.origin === "inspector" ? this._viaInspectorResourceURL(header.sourceURL) : header.sourceURL;
            var resource = frame.resourceForURL(styleSheetURL);
            if (!resource)
                return;

            if (resource.type === WebInspector.resourceTypes.Stylesheet)
                resource.addRevision(content);
        }

        if (!this._styleSheetIdToHeader[styleSheetId]) {
            this._loadStyleSheetHeaders(setContent.bind(this));
            return;
        }
        setContent.call(this);
    },

    /**
     * @param {CSSAgent.StyleSheetId} styleSheetId
     * @param {function(?WebInspector.Resource)} callback
     */
    _requestViaInspectorResource: function(styleSheetId, callback)
    {
        var header = this._styleSheetIdToHeader[styleSheetId];
        if (header) {
            callback(this._getOrCreateInspectorResource(header));
            return;
        }

        function headersLoaded()
        {
            var header = this._styleSheetIdToHeader[styleSheetId];
            if (header)
                callback(this._getOrCreateInspectorResource(header));
            else
                callback(null);
        }
        this._loadStyleSheetHeaders(headersLoaded.bind(this));
    },

    /**
     * @param {CSSAgent.CSSStyleSheetHeader} header
     * @return {?WebInspector.Resource}
     */
    _getOrCreateInspectorResource: function(header)
    {
        var frame = WebInspector.resourceTreeModel.frameForId(header.frameId);
        if (!frame)
            return null;

        var viaInspectorURL = this._viaInspectorResourceURL(header.sourceURL);    
        var inspectorResource = frame.resourceForURL(viaInspectorURL);
        if (inspectorResource)
            return inspectorResource;

        var resource = frame.resourceForURL(header.sourceURL);
        if (!resource)
            return null;

        this._frameAndURLToStyleSheetId[header.frameId + ":" + viaInspectorURL] = header.styleSheetId;
        this._styleSheetIdToHeader[header.styleSheetId] = header;
        inspectorResource = new WebInspector.Resource(null, viaInspectorURL, resource.documentURL, resource.frameId, resource.loaderId, WebInspector.resourceTypes.Stylesheet, "text/css", true);
        /**
         * @param {function(?string, boolean, string)} callback
         */
        function overrideRequestContent(callback)
        {
            function callbackWrapper(error, content)
            {
                callback(error ? "" : content, false, "text/css");
            }
            CSSAgent.getStyleSheetText(header.styleSheetId, callbackWrapper);
        }
        inspectorResource.requestContent = overrideRequestContent;
        frame.addResource(inspectorResource);
        return inspectorResource;
    },

    /**
     * @param {string} documentURL
     * @return {string}
     */
    _viaInspectorResourceURL: function(documentURL)
    {
        var parsedURL = new WebInspector.ParsedURL(documentURL);
        var fakeURL = "inspector://" + parsedURL.host + parsedURL.folderPathComponents;
        if (!fakeURL.endsWith("/"))
            fakeURL += "/";
        fakeURL += "inspector-stylesheet";
        return fakeURL;
    }
}

WebInspector.CSSStyleModelResourceBinding.prototype.__proto__ = WebInspector.ResourceDomainModelBinding.prototype;

/**
 * @constructor
 * @implements {CSSAgent.Dispatcher}
 * @param {WebInspector.CSSStyleModel} cssModel
 */
WebInspector.CSSDispatcher = function(cssModel)
{
    this._cssModel = cssModel;
}

WebInspector.CSSDispatcher.prototype = {
    mediaQueryResultChanged: function()
    {
        this._cssModel.mediaQueryResultChanged();
    },

    /**
     * @param {CSSAgent.StyleSheetId} styleSheetId
     */
    styleSheetChanged: function(styleSheetId)
    {
        this._cssModel._fireStyleSheetChanged(styleSheetId);
    }
}

/**
 * @type {WebInspector.CSSStyleModel}
 */
WebInspector.cssModel = null;
