/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

WI.BreakpointTreeElement = class BreakpointTreeElement extends WI.GeneralTreeElement
{
    constructor(breakpoint, {className, title} = {})
    {
        console.assert(breakpoint instanceof WI.Breakpoint);

        if (!className)
            className = WI.BreakpointTreeElement.GenericLineIconStyleClassName;

        const subtitle = null;
        super(["breakpoint", className], title, subtitle, breakpoint);

        this._breakpoint = breakpoint;
        this._probeSet = null;

        this._listenerSet = new WI.EventListenerSet(this, "BreakpointTreeElement listeners");
        if (!title)
            this._listenerSet.register(breakpoint, WI.Breakpoint.Event.LocationDidChange, this._breakpointLocationDidChange);
        this._listenerSet.register(breakpoint, WI.Breakpoint.Event.DisabledStateDidChange, this._updateStatus);
        this._listenerSet.register(breakpoint, WI.Breakpoint.Event.AutoContinueDidChange, this._updateStatus);
        this._listenerSet.register(breakpoint, WI.Breakpoint.Event.ResolvedStateDidChange, this._updateStatus);
        this._listenerSet.register(WI.debuggerManager, WI.DebuggerManager.Event.BreakpointsEnabledDidChange, this._updateStatus);
        this._listenerSet.register(WI.debuggerManager, WI.DebuggerManager.Event.ProbeSetAdded, this._probeSetAdded);
        this._listenerSet.register(WI.debuggerManager, WI.DebuggerManager.Event.ProbeSetRemoved, this._probeSetRemoved);

        this.status = document.createElement("img");
        this.status.className = WI.BreakpointTreeElement.StatusImageElementStyleClassName;
        this._listenerSet.register(this.status, "mousedown", this._statusImageElementMouseDown);
        this._listenerSet.register(this.status, "click", this._statusImageElementClicked);

        if (!title)
            this._updateTitles();
        this._updateStatus();

        this._iconAnimationLayerElement = document.createElement("span");
        this.iconElement.appendChild(this._iconAnimationLayerElement);
        this.tooltipHandledSeparately = true;
    }

    // Public

    get breakpoint()
    {
        return this._breakpoint;
    }

    get filterableData()
    {
        return {text: [this.breakpoint.contentIdentifier]};
    }

    ondelete()
    {
        if (!WI.debuggerManager.isBreakpointRemovable(this._breakpoint))
            return true;

        // We set this flag so that TreeOutlines that will remove this
        // BreakpointTreeElement will know whether it was deleted from
        // within the TreeOutline or from outside it (e.g. TextEditor).
        this.__deletedViaDeleteKeyboardShortcut = true;

        WI.debuggerManager.removeBreakpoint(this._breakpoint);
        return true;
    }

    onenter()
    {
        this._breakpoint.cycleToNextMode();
        return true;
    }

    onspace()
    {
        this._breakpoint.cycleToNextMode();
        return true;
    }

    onattach()
    {
        super.onattach();

        this._listenerSet.install();

        for (var probeSet of WI.debuggerManager.probeSets)
            if (probeSet.breakpoint === this._breakpoint)
                this._addProbeSet(probeSet);
    }

    ondetach()
    {
        super.ondetach();

        this._listenerSet.uninstall();

        if (this._probeSet)
            this._removeProbeSet(this._probeSet);
    }

    populateContextMenu(contextMenu, event)
    {
        WI.breakpointPopoverController.appendContextMenuItems(contextMenu, this._breakpoint, this.status);

        super.populateContextMenu(contextMenu, event);
    }

    // Private

    _updateTitles()
    {
        var sourceCodeLocation = this._breakpoint.sourceCodeLocation;

        var displayLineNumber = sourceCodeLocation.displayLineNumber;
        var displayColumnNumber = sourceCodeLocation.displayColumnNumber;
        if (displayColumnNumber > 0)
            this.mainTitle = WI.UIString("Line %d:%d").format(displayLineNumber + 1, displayColumnNumber + 1); // The user visible line and column numbers are 1-based.
        else
            this.mainTitle = WI.UIString("Line %d").format(displayLineNumber + 1); // The user visible line number is 1-based.

        if (sourceCodeLocation.hasMappedLocation()) {
            this.subtitle = sourceCodeLocation.formattedLocationString();

            if (sourceCodeLocation.hasFormattedLocation())
                this.subtitleElement.classList.add(WI.BreakpointTreeElement.FormattedLocationStyleClassName);
            else
                this.subtitleElement.classList.remove(WI.BreakpointTreeElement.FormattedLocationStyleClassName);

            this.tooltip = this.mainTitle + " \u2014 " + WI.UIString("originally %s").format(sourceCodeLocation.originalLocationString());
        }
    }

    _updateStatus()
    {
        if (!this.status)
            return;

        if (this._breakpoint.disabled)
            this.status.classList.add(WI.BreakpointTreeElement.StatusImageDisabledStyleClassName);
        else
            this.status.classList.remove(WI.BreakpointTreeElement.StatusImageDisabledStyleClassName);

        if (this._breakpoint.autoContinue)
            this.status.classList.add(WI.BreakpointTreeElement.StatusImageAutoContinueStyleClassName);
        else
            this.status.classList.remove(WI.BreakpointTreeElement.StatusImageAutoContinueStyleClassName);

        if (this._breakpoint.resolved && WI.debuggerManager.breakpointsEnabled)
            this.status.classList.add(WI.BreakpointTreeElement.StatusImageResolvedStyleClassName);
        else
            this.status.classList.remove(WI.BreakpointTreeElement.StatusImageResolvedStyleClassName);
    }

    _addProbeSet(probeSet)
    {
        console.assert(probeSet instanceof WI.ProbeSet);
        console.assert(probeSet.breakpoint === this._breakpoint);
        console.assert(probeSet !== this._probeSet);

        this._probeSet = probeSet;
        probeSet.addEventListener(WI.ProbeSet.Event.SamplesCleared, this._samplesCleared, this);
        probeSet.dataTable.addEventListener(WI.ProbeSetDataTable.Event.FrameInserted, this._dataUpdated, this);
    }

    _removeProbeSet(probeSet)
    {
        console.assert(probeSet instanceof WI.ProbeSet);
        console.assert(probeSet === this._probeSet);

        probeSet.removeEventListener(WI.ProbeSet.Event.SamplesCleared, this._samplesCleared, this);
        probeSet.dataTable.removeEventListener(WI.ProbeSetDataTable.Event.FrameInserted, this._dataUpdated, this);
        this._probeSet = null;
    }

    _probeSetAdded(event)
    {
        var probeSet = event.data.probeSet;
        if (probeSet.breakpoint === this._breakpoint)
            this._addProbeSet(probeSet);
    }

    _probeSetRemoved(event)
    {
        var probeSet = event.data.probeSet;
        if (probeSet.breakpoint === this._breakpoint)
            this._removeProbeSet(probeSet);
    }

    _samplesCleared(event)
    {
        console.assert(this._probeSet);

        var oldTable = event.data.oldTable;
        oldTable.removeEventListener(WI.ProbeSetDataTable.Event.FrameInserted, this._dataUpdated, this);
        this._probeSet.dataTable.addEventListener(WI.ProbeSetDataTable.Event.FrameInserted, this._dataUpdated, this);
    }

    _dataUpdated()
    {
        if (this.element.classList.contains(WI.BreakpointTreeElement.ProbeDataUpdatedStyleClassName)) {
            clearTimeout(this._removeIconAnimationTimeoutIdentifier);
            this.element.classList.remove(WI.BreakpointTreeElement.ProbeDataUpdatedStyleClassName);
            // We want to restart the animation, which can only be done by removing the class,
            // performing layout, and re-adding the class. Try adding class back on next run loop.
            window.requestAnimationFrame(this._dataUpdated.bind(this));
            return;
        }

        this.element.classList.add(WI.BreakpointTreeElement.ProbeDataUpdatedStyleClassName);
        this._removeIconAnimationTimeoutIdentifier = setTimeout(() => {
            this.element.classList.remove(WI.BreakpointTreeElement.ProbeDataUpdatedStyleClassName);
        }, WI.BreakpointTreeElement.ProbeDataUpdatedAnimationDuration);
    }

    _breakpointLocationDidChange(event)
    {
        console.assert(event.target === this._breakpoint);

        // The Breakpoint has a new display SourceCode. The sidebar will remove us, and ondetach() will clear listeners.
        if (event.data.oldDisplaySourceCode === this._breakpoint.displaySourceCode)
            return;

        this._updateTitles();
    }

    _statusImageElementMouseDown(event)
    {
        // To prevent the tree element from selecting.
        event.stopPropagation();
    }

    _statusImageElementClicked(event)
    {
        this._breakpoint.cycleToNextMode();
    }
};

WI.BreakpointTreeElement.GenericLineIconStyleClassName = "breakpoint-generic-line-icon";
WI.BreakpointTreeElement.StatusImageElementStyleClassName = "status-image";
WI.BreakpointTreeElement.StatusImageResolvedStyleClassName = "resolved";
WI.BreakpointTreeElement.StatusImageAutoContinueStyleClassName = "auto-continue";
WI.BreakpointTreeElement.StatusImageDisabledStyleClassName = "disabled";
WI.BreakpointTreeElement.FormattedLocationStyleClassName = "formatted-location";
WI.BreakpointTreeElement.ProbeDataUpdatedStyleClassName = "data-updated";

WI.BreakpointTreeElement.ProbeDataUpdatedAnimationDuration = 400; // milliseconds
