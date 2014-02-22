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

WebInspector.ProfileNodeTreeElement = function(profileNode, delegate)
{
    console.assert(profileNode);

    this._profileNode = profileNode;
    this._delegate = delegate || null;

    var title = profileNode.functionName;
    var subtitle = "";

    if (!title) {
        switch (profileNode.type) {
        case WebInspector.ProfileNode.Type.Function:
            title = WebInspector.UIString("(anonymous function)");
            break;
        case WebInspector.ProfileNode.Type.Program:
            title = WebInspector.UIString("(program)");
            break;
        default:
            title = WebInspector.UIString("(anonymous function)");
            console.error("Unknown ProfileNode type: " + profileNode.type);
        }
    }

    var sourceCodeLocation = this._profileNode.sourceCodeLocation;
    if (sourceCodeLocation) {
        subtitle = document.createElement("span");
        sourceCodeLocation.populateLiveDisplayLocationString(subtitle, "textContent");
    }

    var className;

    switch (this._profileNode.type) {
    case WebInspector.ProfileNode.Type.Function:
        className = WebInspector.CallFrameTreeElement.FunctionIconStyleClassName;
        if (!sourceCodeLocation)
            className = WebInspector.CallFrameTreeElement.NativeIconStyleClassName;
        break;
    case WebInspector.ProfileNode.Type.Program:
        className = WebInspector.TimelineRecordTreeElement.EvaluatedRecordIconStyleClass;
        break;
    }

    console.assert(className);

    // This is more than likely an event listener function with an "on" prefix and it is
    // as long or longer than the shortest event listener name -- "oncut".
    if (profileNode.functionName && profileNode.functionName.startsWith("on") && profileNode.functionName.length >= 5)
        className = WebInspector.CallFrameTreeElement.EventListenerIconStyleClassName;

    var hasChildren = !!profileNode.childNodes.length;

    WebInspector.GeneralTreeElement.call(this, [className], title, subtitle, profileNode, hasChildren);

    this.small = true;
    this.shouldRefreshChildren = true;

    if (sourceCodeLocation)
        this.tooltipHandledSeparately = true;
};

WebInspector.ProfileNodeTreeElement.prototype = {
    constructor: WebInspector.ProfileNodeTreeElement,
    __proto__: WebInspector.GeneralTreeElement.prototype,

    // Public

    get profileNode()
    {
        return this._profileNode;
    },

    get filterableData()
    {
        var url = this._profileNode.sourceCodeLocation ? this._profileNode.sourceCodeLocation.sourceCode.url : "";
        return {text: [this.mainTitle, url || ""]};
    },

    // Protected

    onattach: function()
    {
        WebInspector.GeneralTreeElement.prototype.onattach.call(this);

        console.assert(this.element);

        if (!this.tooltipHandledSeparately)
            return;

        var tooltipPrefix = this.mainTitle + "\n";
        this._profileNode.sourceCodeLocation.populateLiveDisplayLocationTooltip(this.element, tooltipPrefix);
    },

    onpopulate: function()
    {
        if (!this.hasChildren || !this.shouldRefreshChildren)
            return;

        this.shouldRefreshChildren = false;

        this.removeChildren();

        if (this._delegate && typeof this._delegate.populateProfileNodeTreeElement === "function") {
            this._delegate.populateProfileNodeTreeElement(this);
            return;
        }

        for (var childProfileNode of this._profileNode.childNodes) {
            var childTreeElement = new WebInspector.ProfileNodeTreeElement(childProfileNode);
            this.appendChild(childTreeElement);
        }
    }
};
