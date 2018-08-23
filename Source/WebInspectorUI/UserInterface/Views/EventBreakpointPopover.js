/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

WI.EventBreakpointPopover = class EventBreakpointPopover extends WI.Popover
{
    constructor(delegate)
    {
        super(delegate);

        this._breakpoint = null;

        this._targetElement = null;
        this._preferredEdges = null;

        this.windowResizeHandler = this._presentOverTargetElement.bind(this);
    }

    // Public

    get breakpoint() { return this._breakpoint; }

    show(targetElement, preferredEdges)
    {
        this._targetElement = targetElement;
        this._preferredEdges = preferredEdges;

        let contentElement = document.createElement("div");
        contentElement.classList.add("event-breakpoint-content");

        let label = contentElement.appendChild(document.createElement("div"));
        label.classList.add("label");
        label.textContent = WI.UIString("Break on events with name:");

        let typeContainer = contentElement.appendChild(document.createElement("div"));
        typeContainer.classList.add("event-type");

        this._typeSelectElement = typeContainer.appendChild(document.createElement("select"));
        this._typeSelectElement.addEventListener("change", this._handleTypeSelectChange.bind(this));
        this._typeSelectElement.addEventListener("keydown", (event) => {
            if (isEnterKey(event))
                this.dismiss();
        });

        let createOption = (text, value) => {
            let optionElement = this._typeSelectElement.appendChild(document.createElement("option"));
            optionElement.value = value;
            optionElement.textContent = text;
        };

        createOption(WI.UIString("DOM Event"), WI.EventBreakpoint.Type.Listener);

        if (WI.DOMDebuggerManager.supportsEventBreakpoints()) {
            createOption(WI.unlocalizedString("requestAnimationFrame"), "requestAnimationFrame");
            createOption(WI.unlocalizedString("setTimeout"), "setTimeout");
            createOption(WI.unlocalizedString("setInterval"), "setInterval");
        } else
            this._typeSelectElement.hidden = true;

        this._domEventNameInputElement = typeContainer.appendChild(document.createElement("input"));
        this._domEventNameInputElement.placeholder = WI.UIString("Example: “%s”").format("click");
        this._domEventNameInputElement.spellcheck = false;
        this._domEventNameInputElement.addEventListener("keydown", (event) => {
            if (!isEnterKey(event))
                return;

            this.dismiss();
        });

        this.content = contentElement;

        this._presentOverTargetElement();

        this._typeSelectElement.value = WI.EventBreakpoint.Type.Listener;
        this._domEventNameInputElement.select();
    }

    dismiss()
    {
        let type = this._typeSelectElement.value;
        let value = null;

        if (type === WI.EventBreakpoint.Type.Listener)
            value = this._domEventNameInputElement.value;
        else {
            value = type;

            if (value === "requestAnimationFrame")
                type = WI.EventBreakpoint.Type.AnimationFrame;
            else if (value === "setTimeout" || value === "setInterval")
                type = WI.EventBreakpoint.Type.Timer;
        }

        if (type && value)
            this._breakpoint = new WI.EventBreakpoint(type, value);

        super.dismiss();
    }

    // Private

    _presentOverTargetElement()
    {
        if (!this._targetElement)
            return;

        let targetFrame = WI.Rect.rectFromClientRect(this._targetElement.getBoundingClientRect());
        this.present(targetFrame, this._preferredEdges);
    }

    _handleTypeSelectChange(event)
    {
        this._domEventNameInputElement.hidden = this._typeSelectElement.value !== WI.EventBreakpoint.Type.Listener;

        this.update();
    }
};
