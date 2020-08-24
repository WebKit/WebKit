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
        if (!options.hideTarget)
            rows.push(new WI.DetailsSectionSimpleRow(WI.UIString("Target"), this._targetTextOrLink()));
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

        if (this.supportsStateModification) {
            if (WI.DOMManager.supportsDisablingEventListeners()) {
                this._eventListenerEnabledToggleElement = document.createElement("input");
                this._eventListenerEnabledToggleElement.type = "checkbox";
                this._updateDisabledToggle();
                this._eventListenerEnabledToggleElement.addEventListener("change", (event) => {
                    this.isEventListenerDisabled = !this._eventListenerEnabledToggleElement.checked;
                    this.hasEventListenerBreakpoint = false;
                });

                let toggleLabel = document.createElement("span");
                toggleLabel.textContent = WI.UIString("Enabled");
                toggleLabel.addEventListener("click", (event) => {
                    this._eventListenerEnabledToggleElement.click();
                });

                rows.push(new WI.DetailsSectionSimpleRow(toggleLabel, this._eventListenerEnabledToggleElement));
            }

            if (WI.DOMManager.supportsEventListenerBreakpoints()) {
                let toggleContainer = document.createElement("span");

                this._eventListenerBreakpointToggleElement = toggleContainer.appendChild(document.createElement("input"));
                this._eventListenerBreakpointToggleElement.type = "checkbox";
                this._updateBreakpointToggle();
                this._eventListenerBreakpointToggleElement.addEventListener("change", (event) => {
                    this.isEventListenerDisabled = false;
                    this.hasEventListenerBreakpoint = !!this._eventListenerBreakpointToggleElement.checked;
                });

                if (WI.DOMManager.supportsEventListenerBreakpointConfiguration()) {
                    let revealBreakpointGoToArrow = toggleContainer.appendChild(WI.createGoToArrowButton());
                    revealBreakpointGoToArrow.title = WI.UIString("Reveal in Sources Tab");
                    revealBreakpointGoToArrow.addEventListener("click", (event) => {
                        console.assert(this.hasEventListenerBreakpoint);

                        let breakpointToSelect = WI.domManager.breakpointForEventListenerId(this._eventListener.eventListenerId);
                        console.assert(breakpointToSelect);

                        WI.showSourcesTab({breakpointToSelect});
                    });
                }

                let toggleLabel = document.createElement("span");
                toggleLabel.textContent = WI.UIString("Breakpoint");
                toggleLabel.addEventListener("click", (event) => {
                    this._eventListenerBreakpointToggleElement.click();
                });

                rows.push(new WI.DetailsSectionSimpleRow(toggleLabel, toggleContainer));
            }
        }

        this.rows = rows;
    }

    // Public

    get supportsStateModification()
    {
        // COMPATIBILITY (iOS 11): DOM.EventListenerId did not exist.
        return !!this._eventListener.eventListenerId;
    }

    get isEventListenerDisabled()
    {
        console.assert(WI.DOMManager.supportsDisablingEventListeners());
        if (!this.supportsStateModification)
            return false;
        return this._eventListener.disabled;
    }

    set isEventListenerDisabled(disabled)
    {
        console.assert(WI.DOMManager.supportsDisablingEventListeners());
        if (!this.supportsStateModification)
            return;

        if (this._eventListener.disabled === disabled)
            return;

        this._eventListener.disabled = disabled;

        this._updateDisabledToggle();

        WI.domManager.setEventListenerDisabled(this._eventListener, this._eventListener.disabled);
    }

    get hasEventListenerBreakpoint()
    {
        console.assert(WI.DOMManager.supportsEventListenerBreakpoints());
        if (!this.supportsStateModification)
            return false;
        return this._eventListener.hasBreakpoint;
    }

    set hasEventListenerBreakpoint(hasBreakpoint)
    {
        console.assert(WI.DOMManager.supportsEventListenerBreakpoints());
        if (!this.supportsStateModification)
            return;

        if (this._eventListener.hasBreakpoint === hasBreakpoint)
            return;

        this._eventListener.hasBreakpoint = hasBreakpoint;

        this._updateBreakpointToggle();

        if (this._eventListener.hasBreakpoint)
            WI.domManager.setBreakpointForEventListener(this._eventListener);
        else
            WI.domManager.removeBreakpointForEventListener(this._eventListener);
    }

    // Private

    _targetTextOrLink()
    {
        if (this._eventListener.onWindow)
            return WI.unlocalizedString("window");

        let node = this._eventListener.node;
        if (node)
            return WI.linkifyNodeReference(node);

        console.assert();
        return "";
    }

    _functionTextOrLink()
    {
        let anonymous = false;
        let functionName = this._eventListener.handlerName;

        // COMPATIBILITY (iOS 12.2): DOM.EventListener.handlerBody was replaced by DOM.EventListener.handlerName.
        if (!functionName && this._eventListener.handlerBody) {
            let match = this._eventListener.handlerBody.match(/function ([^\(]+?)\(/);
            if (match)
                functionName = match[1];
        }

        if (!functionName) {
            anonymous = true;
            functionName = WI.UIString("(anonymous function)");
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

    _updateDisabledToggle()
    {
        console.assert(this._eventListenerEnabledToggleElement);
        this._eventListenerEnabledToggleElement.checked = !this._eventListener.disabled;
        this._eventListenerEnabledToggleElement.title = this._eventListener.disabled ? WI.UIString("Enable Event Listener") : WI.UIString("Disable Event Listener");
    }

    _updateBreakpointToggle()
    {
        console.assert(this._eventListenerBreakpointToggleElement);
        this._eventListenerBreakpointToggleElement.checked = this._eventListener.hasBreakpoint;
        this._eventListenerBreakpointToggleElement.title = this._eventListener.hasBreakpoint ? WI.UIString("Delete Breakpoint") : WI.UIString("Add Breakpoint");
    }
};
