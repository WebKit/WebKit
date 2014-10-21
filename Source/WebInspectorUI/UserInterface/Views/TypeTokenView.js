/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

WebInspector.TypeTokenView = function(tokenAnnotator, shouldHaveRightMargin, shouldHaveLeftMargin, titleType, functionOrVariableName)
{
    console.assert(titleType === WebInspector.TypeTokenView.TitleType.Variable  || titleType === WebInspector.TypeTokenView.TitleType.ReturnStatement);

    WebInspector.Object.call(this);

    var span = document.createElement("span");
    span.classList.add("type-token");
    if (shouldHaveRightMargin)
        span.classList.add("type-token-right-spacing");
    if (shouldHaveLeftMargin)
        span.classList.add("type-token-left-spacing");

    this.element = span;
    this._tokenAnnotator = tokenAnnotator;
    this._types = null;
    this._colorClass = null;

    this._popoverTitle = WebInspector.TypeTokenView.titleForPopover(titleType, functionOrVariableName);

    this._setUpMouseoverHandlers();
};

WebInspector.TypeTokenView.titleForPopover = function(titleType, functionOrVariableName)
{
    var titleString = null;
    if (titleType === WebInspector.TypeTokenView.TitleType.Variable)
        titleString = WebInspector.UIString("Type information for variable: %s").format(functionOrVariableName);
    else {
        if (functionOrVariableName)
            titleString = WebInspector.UIString("Return type for function: %s").format(functionOrVariableName);
        else
            titleString = WebInspector.UIString("Return type for anonymous function");
    }

    return titleString;
};

WebInspector.TypeTokenView.TitleType = {
    Variable: "title-type-variable",
    ReturnStatement: "title-type-return-statement"
};

WebInspector.TypeTokenView.ColorClassForType = {
    "String": "type-token-string",
    "Function": "type-token-function",
    "Number": "type-token-number",
    "Integer": "type-token-number",
    "Undefined": "type-token-empty",
    "Null": "type-token-empty",
    "(?)": "type-token-empty",
    "Boolean": "type-token-boolean",
    "(many)": "type-token-many"
};

WebInspector.TypeTokenView.DelayHoverTime = 350;

WebInspector.TypeTokenView.prototype = {
    constructor: WebInspector.TypeTokenView,
    __proto__: WebInspector.Object.prototype,

    // Public

    update: function(types)
    {
        this._types = types;

        var title = this._displayTypeName();
        this.element.textContent = title;
        var hashString = title[title.length - 1] === "?" ? title.slice(0, title.length - 1) : title;

        if (this._colorClass)
            this.element.classList.remove(this._colorClass);

        this._colorClass = WebInspector.TypeTokenView.ColorClassForType[hashString] || "type-token-default";
        this.element.classList.add(this._colorClass);
    },

    // Private

    _setUpMouseoverHandlers: function()
    {
        var timeoutID = null;

        this.element.addEventListener("mouseover", function() {
            function showPopoverAfterDelay()
            {
                timeoutID = null;

                var domRect = this.element.getBoundingClientRect();
                var bounds = new WebInspector.Rect(domRect.left, domRect.top, domRect.width, domRect.height);
                this._tokenAnnotator.sourceCodeTextEditor.showPopoverForTypes(this._types, bounds, this._popoverTitle);
            }

            if (this._shouldShowPopover())
                timeoutID = setTimeout(showPopoverAfterDelay.bind(this), WebInspector.TypeTokenView.DelayHoverTime);
        }.bind(this));

        this.element.addEventListener("mouseout", function() {
            if (timeoutID)
                clearTimeout(timeoutID);
        }.bind(this));
    },

    _shouldShowPopover: function()
    {
        if (this._types.primitiveTypeNames && this._types.primitiveTypeNames.length > 1)
            return true;

        if (this._types.structures && this._types.structures.length)
            return true;

        return false;
    },

    _displayTypeName: function()
    {
        var typeSet = WebInspector.TypeSet.fromPayload(this._types);

        if (this._types.leastCommonAncestor && !(this._types.primitiveTypeNames && this._types.primitiveTypeNames.length)) {
            if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.Object))
                return this._types.leastCommonAncestor;
            if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.Object | WebInspector.TypeSet.NullOrUndefinedTypeBits))
                return this._types.leastCommonAncestor + "?";
        }

        // The order of these checks are important. 
        // For example, if a value is only a function, it is contained in TypeFunction, but it is also contained in (TypeFunction | TypeNull).
        // Therefore, more specific types must be checked first.

        // The strings returned here should match those in TypeTokenView.ColorClassForType
        if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.Function))
            return "Function";
        if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.Undefined))
            return "Undefined";
        if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.Null))
            return "Null";
        if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.Boolean))
            return "Boolean";
        if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.Integer))
            return "Integer";
        if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.Number | WebInspector.TypeSet.TypeBit.Integer))
            return "Number";
        if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.String))
            return "String";

        if (typeSet.isContainedIn(WebInspector.TypeSet.NullOrUndefinedTypeBits))
            return "(?)";

        if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.Function | WebInspector.TypeSet.NullOrUndefinedTypeBits))
            return "Function?";
        if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.Boolean | WebInspector.TypeSet.NullOrUndefinedTypeBits))
            return "Boolean?";
        if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.Integer | WebInspector.TypeSet.NullOrUndefinedTypeBits))
            return "Integer?";
        if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.Number | WebInspector.TypeSet.TypeBit.Integer | WebInspector.TypeSet.NullOrUndefinedTypeBits))
            return "Number?";
        if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.String | WebInspector.TypeSet.NullOrUndefinedTypeBits))
            return "String?";
       
        if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.Object | WebInspector.TypeSet.TypeBit.Function | WebInspector.TypeSet.TypeBit.String))
            return "Object";
        if (typeSet.isContainedIn(WebInspector.TypeSet.TypeBit.Object | WebInspector.TypeSet.TypeBit.Function | WebInspector.TypeSet.TypeBit.String | WebInspector.TypeSet.NullOrUndefinedTypeBits))
            return "Object?";

        return "(" + WebInspector.UIString("many") + ")";
    }
};
