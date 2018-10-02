/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WI.EventListenerSectionGroup = class EventListenerSectionGroup extends WI.DetailsSectionGroup
{
    constructor(eventListener, options = {})
    {
        super();

        this._eventListener = eventListener;

        var rows = [];
        if (!options.hideType)
            rows.push(new WI.DetailsSectionSimpleRow(WI.UIString("Event"), this._eventListener.type));
        if (!options.hideNode)
            rows.push(new WI.DetailsSectionSimpleRow(WI.UIString("Node"), this._nodeTextOrLink()));
        rows.push(new WI.DetailsSectionSimpleRow(WI.UIString("Function"), this._functionTextOrLink()));

        if (this._eventListener.useCapture)
            rows.push(new WI.DetailsSectionSimpleRow(WI.UIString("Capturing"), WI.UIString("Yes")));
        else
            rows.push(new WI.DetailsSectionSimpleRow(WI.UIString("Bubbling"), WI.UIString("Yes")));

        if (this._eventListener.isAttribute)
            rows.push(new WI.DetailsSectionSimpleRow(WI.UIString("Attribute"), WI.UIString("Yes")));

        if (this._eventListener.passive)
            rows.push(new WI.DetailsSectionSimpleRow(WI.UIString("Passive"), WI.UIString("Yes")));

        if (this._eventListener.once)
            rows.push(new WI.DetailsSectionSimpleRow(WI.UIString("Once"), WI.UIString("Yes")));

        if (this._eventListener.eventListenerId) {
            if (DOMAgent.setEventListenerDisabled)
                rows.push(this._createDisabledToggleRow());

            if (DOMAgent.setBreakpointForEventListener && DOMAgent.removeBreakpointForEventListener)
                rows.push(this._createBreakpointToggleRow());
        }

        this.rows = rows;
    }

    // Private

    _nodeTextOrLink()
    {
        var node = this._eventListener.node;
        console.assert(node);
        if (!node)
            return "";

        if (node.nodeType() === Node.DOCUMENT_NODE)
            return "document";

        return WI.linkifyNodeReference(node);
    }

    _functionTextOrLink()
    {
        var match = this._eventListener.handlerBody.match(/function ([^\(]+?)\(/);
        if (match) {
            var anonymous = false;
            var functionName = match[1];
        } else {
            var anonymous = true;
            var functionName = WI.UIString("(anonymous function)");
        }

        if (!this._eventListener.location)
            return functionName;

        var sourceCode = WI.debuggerManager.scriptForIdentifier(this._eventListener.location.scriptId, WI.mainTarget);
        if (!sourceCode)
            return functionName;

        var sourceCodeLocation = sourceCode.createSourceCodeLocation(this._eventListener.location.lineNumber, this._eventListener.location.columnNumber || 0);

        const options = {
            dontFloat: anonymous,
            ignoreNetworkTab: true,
            ignoreSearchTab: true,
        };
        let linkElement = WI.createSourceCodeLocationLink(sourceCodeLocation, options);

        if (anonymous)
            return linkElement;

        var fragment = document.createDocumentFragment();
        fragment.append(linkElement, functionName);
        return fragment;
    }

    _createDisabledToggleRow()
    {
        let toggleElement = document.createElement("input");
        toggleElement.type = "checkbox";
        toggleElement.checked = !this._eventListener.disabled;

        let updateTitle = () => {
            if (this._eventListener.disabled)
                toggleElement.title = WI.UIString("Enable Event Listener");
            else
                toggleElement.title = WI.UIString("Disable Event Listener");
        };

        updateTitle();

        toggleElement.addEventListener("change", (event) => {
            this._eventListener.disabled = !toggleElement.checked;
            WI.domManager.setEventListenerDisabled(this._eventListener, this._eventListener.disabled);
            updateTitle();
        });

        let toggleLabel = document.createElement("span");
        toggleLabel.textContent = WI.UIString("Enabled");
        toggleLabel.addEventListener("click", (event) => {
            toggleElement.click();
        });

        return new WI.DetailsSectionSimpleRow(toggleLabel, toggleElement);
    }

    _createBreakpointToggleRow()
    {
        let checkboxElement = document.createElement("input");
        checkboxElement.type = "checkbox";
        checkboxElement.checked = !!this._eventListener.hasBreakpoint;

        let updateTitle = () => {
            if (this._eventListener.hasBreakpoint)
                checkboxElement.title = WI.UIString("Delete Breakpoint");
            else
                checkboxElement.title = WI.UIString("Add Breakpoint");
        };

        updateTitle();

        checkboxElement.addEventListener("change", (event) => {
            this._eventListener.hasBreakpoint = !!checkboxElement.checked;
            if (this._eventListener.hasBreakpoint)
                WI.domManager.setBreakpointForEventListener(this._eventListener);
            else
                WI.domManager.removeBreakpointForEventListener(this._eventListener);

            updateTitle();
        });

        let labelElement = document.createElement("span");
        labelElement.textContent = WI.UIString("Breakpoint");
        labelElement.addEventListener("click", (event) => {
            checkboxElement.click();
        });

        return new WI.DetailsSectionSimpleRow(labelElement, checkboxElement);
    }
};
