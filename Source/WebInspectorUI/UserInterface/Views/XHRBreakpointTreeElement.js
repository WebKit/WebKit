/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.XHRBreakpointTreeElement = class XHRBreakpointTreeElement extends WI.GeneralTreeElement
{
    constructor(breakpoint, className, title)
    {
        console.assert(breakpoint instanceof WI.XHRBreakpoint);

        if (!className)
            className = WI.BreakpointTreeElement.GenericLineIconStyleClassName;

        let subtitle;
        if (!title) {
            title = WI.UIString("URL");
            if (breakpoint.type === WI.XHRBreakpoint.Type.Text)
                subtitle = doubleQuotedString(breakpoint.url);
            else
                subtitle = "/" + breakpoint.url + "/";
        }

        super(["breakpoint", className], title, subtitle, breakpoint);

        this._statusImageElement = document.createElement("img");
        this._statusImageElement.classList.add("status-image", "resolved");
        this.status = this._statusImageElement;
        this.tooltipHandledSeparately = true;

        breakpoint.addEventListener(WI.XHRBreakpoint.Event.DisabledStateDidChange, this._updateStatus, this);

        this._updateStatus();
    }

    // Protected

    onattach()
    {
        super.onattach();

        this._boundStatusImageElementClicked = this._statusImageElementClicked.bind(this);
        this._boundStatusImageElementFocused = this._statusImageElementFocused.bind(this);
        this._boundStatusImageElementMouseDown = this._statusImageElementMouseDown.bind(this);

        this._statusImageElement.addEventListener("click", this._boundStatusImageElementClicked);
        this._statusImageElement.addEventListener("focus", this._boundStatusImageElementFocused);
        this._statusImageElement.addEventListener("mousedown", this._boundStatusImageElementMouseDown);
    }

    ondetach()
    {
        super.ondetach();

        this._statusImageElement.removeEventListener("click", this._boundStatusImageElementClicked);
        this._statusImageElement.removeEventListener("focus", this._boundStatusImageElementFocused);
        this._statusImageElement.removeEventListener("mousedown", this._boundStatusImageElementMouseDown);

        this._boundStatusImageElementClicked = null;
        this._boundStatusImageElementFocused = null;
        this._boundStatusImageElementMouseDown = null;
    }

    ondelete()
    {
        WI.domDebuggerManager.removeXHRBreakpoint(this.representedObject);
        return true;
    }

    onenter()
    {
        this._toggleBreakpoint();
        return true;
    }

    onspace()
    {
        this._toggleBreakpoint();
        return true;
    }

    populateContextMenu(contextMenu, event)
    {
        let breakpoint = this.representedObject;
        let label = breakpoint.disabled ? WI.UIString("Enable Breakpoint") : WI.UIString("Disable Breakpoint");
        contextMenu.appendItem(label, this._toggleBreakpoint.bind(this));

        if (WI.domDebuggerManager.isBreakpointRemovable(breakpoint)) {
            contextMenu.appendSeparator();
            contextMenu.appendItem(WI.UIString("Delete Breakpoint"), function() {
                WI.domDebuggerManager.removeXHRBreakpoint(breakpoint);
            });
        }
    }

    // Private

    _statusImageElementClicked(event)
    {
        this._toggleBreakpoint();
    }

    _statusImageElementFocused(event)
    {
        // Prevent tree outline focus.
        event.stopPropagation();
    }

    _statusImageElementMouseDown(event)
    {
        // Prevent tree element selection.
        event.stopPropagation();
    }

    _toggleBreakpoint()
    {
        this.representedObject.disabled = !this.representedObject.disabled;
    }

    _updateStatus()
    {
        this._statusImageElement.classList.toggle("disabled", this.representedObject.disabled);
    }
};
