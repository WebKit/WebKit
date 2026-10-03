/*
 * Copyright (C) 2026 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

WI.DOMBreakpointPopover = class DOMBreakpointPopover extends WI.BreakpointPopover
{
    constructor(delegate, breakpoint)
    {
        console.assert(breakpoint instanceof WI.DOMBreakpoint, breakpoint);

        super(delegate, breakpoint);
    }

    // Protected

    populateContent()
    {
        let typeLabelElement = document.createElement("label");
        typeLabelElement.textContent = WI.UIString("Type");

        this._typeSelectElement = document.createElement("select");
        this._typeSelectElement.id = "edit-breakpoint-popover-content-type";

        for (let type of Object.values(WI.DOMBreakpoint.Type)) {
            let optionElement = this._typeSelectElement.appendChild(document.createElement("option"));
            optionElement.textContent = WI.DOMBreakpoint.displayNameForType(type);
            optionElement.value = type;
        }

        this._typeSelectElement.value = this.breakpoint.type;
        typeLabelElement.setAttribute("for", this._typeSelectElement.id);

        this.addRow("type", typeLabelElement, this._typeSelectElement);

        setTimeout(() => {
            this._typeSelectElement.focus();
            this.update();
        });
    }

    createBreakpoint(options = {})
    {
        let domNodeOrInfo = {
            domNode: this.breakpoint.domNode,
            url: this.breakpoint.url,
            path: this.breakpoint.path,
        };
        return new WI.DOMBreakpoint(domNodeOrInfo, this._typeSelectElement.value, options);
    }
};
