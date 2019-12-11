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

WI.EventBreakpointTreeElement = class EventBreakpointTreeElement extends WI.GeneralTreeElement
{
    constructor(breakpoint, {className, title} = {})
    {
        console.assert(breakpoint instanceof WI.EventBreakpoint);

        let classNames = ["breakpoint", "event", `breakpoint-for-${breakpoint.type}`];
        if (className)
            classNames.push(className);

        if (!title)
            title = breakpoint.eventName;

        const subtitle = null;
        super(classNames, title, subtitle, breakpoint);

        this.status = WI.ImageUtilities.useSVGSymbol("Images/Breakpoint.svg");
        this.status.className = WI.BreakpointTreeElement.StatusImageElementStyleClassName;

        this.tooltipHandledSeparately = true;
    }

    // Protected

    onattach()
    {
        super.onattach();

        this.representedObject.addEventListener(WI.EventBreakpoint.Event.DisabledStateChanged, this._updateStatus, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.BreakpointsEnabledDidChange, this._updateStatus, this);

        this._boundStatusImageElementClicked = this._statusImageElementClicked.bind(this);
        this._boundStatusImageElementFocused = this._statusImageElementFocused.bind(this);
        this._boundStatusImageElementMouseDown = this._statusImageElementMouseDown.bind(this);

        this.status.addEventListener("click", this._boundStatusImageElementClicked);
        this.status.addEventListener("focus", this._boundStatusImageElementFocused);
        this.status.addEventListener("mousedown", this._boundStatusImageElementMouseDown);

        this._updateStatus();
    }

    ondetach()
    {
        super.ondetach();

        this.representedObject.removeEventListener(null, null, this);
        WI.debuggerManager.removeEventListener(null, null, this);

        this.status.removeEventListener("click", this._boundStatusImageElementClicked);
        this.status.removeEventListener("focus", this._boundStatusImageElementFocused);
        this.status.removeEventListener("mousedown", this._boundStatusImageElementMouseDown);

        this._boundStatusImageElementClicked = null;
        this._boundStatusImageElementFocused = null;
        this._boundStatusImageElementMouseDown = null;
    }

    ondelete()
    {
        // We set this flag so that TreeOutlines that will remove this
        // BreakpointTreeElement will know whether it was deleted from
        // within the TreeOutline or from outside it (e.g. TextEditor).
        this.__deletedViaDeleteKeyboardShortcut = true;

        if (this.representedObject.eventListener)
            WI.domManager.removeBreakpointForEventListener(this.representedObject.eventListener);
        else
            WI.domDebuggerManager.removeEventBreakpoint(this.representedObject);

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

        contextMenu.appendItem(WI.UIString("Delete Breakpoint"), () => {
            if (breakpoint.eventListener)
                WI.domManager.removeBreakpointForEventListener(breakpoint.eventListener);
            else
                WI.domDebuggerManager.removeEventBreakpoint(breakpoint);
        });
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
        if (!this.status)
            return;

        this.status.classList.toggle(WI.BreakpointTreeElement.StatusImageDisabledStyleClassName, this.representedObject.disabled);
        this.status.classList.toggle(WI.BreakpointTreeElement.StatusImageResolvedStyleClassName, WI.debuggerManager.breakpointsEnabled);
    }
};
