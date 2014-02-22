/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

WebInspector.BreakpointTreeElement = function(breakpoint, className, title)
{
    console.assert(breakpoint instanceof WebInspector.Breakpoint);

    if (!className)
        className = WebInspector.BreakpointTreeElement.GenericLineIconStyleClassName;

    WebInspector.GeneralTreeElement.call(this, [WebInspector.BreakpointTreeElement.StyleClassName, className], title, null, breakpoint, false);

    this._breakpoint = breakpoint;

    this._listeners = new WebInspector.EventListenerSet(this, "BreakpointTreeElement listeners");
    if (!title)
        this._listeners.register(breakpoint, WebInspector.Breakpoint.Event.LocationDidChange, this._breakpointLocationDidChange);
    this._listeners.register(breakpoint, WebInspector.Breakpoint.Event.DisabledStateDidChange, this._updateStatus);
    this._listeners.register(breakpoint, WebInspector.Breakpoint.Event.AutoContinueDidChange, this._updateStatus);
    this._listeners.register(breakpoint, WebInspector.Breakpoint.Event.ResolvedStateDidChange, this._updateStatus);

    this._listeners.register(WebInspector.probeManager, WebInspector.ProbeManager.Event.ProbeSetAdded, this._probeSetAdded);
    this._listeners.register(WebInspector.probeManager, WebInspector.ProbeManager.Event.ProbeSetRemoved, this._probeSetRemoved);

    this._statusImageElement = document.createElement("img");
    this._statusImageElement.className = WebInspector.BreakpointTreeElement.StatusImageElementStyleClassName;
    this._listeners.register(this._statusImageElement, "mousedown", this._statusImageElementMouseDown);
    this._listeners.register(this._statusImageElement, "click", this._statusImageElementClicked);

    if (!title)
        this._updateTitles();
    this._updateStatus();

    this.status = this._statusImageElement;
    this.small = true;

    this._iconAnimationLayerElement = document.createElement("span");
    this.iconElement.appendChild(this._iconAnimationLayerElement);
};

WebInspector.BreakpointTreeElement.GenericLineIconStyleClassName = "breakpoint-generic-line-icon";
WebInspector.BreakpointTreeElement.StyleClassName = "breakpoint";
WebInspector.BreakpointTreeElement.StatusImageElementStyleClassName = "status-image";
WebInspector.BreakpointTreeElement.StatusImageResolvedStyleClassName = "resolved";
WebInspector.BreakpointTreeElement.StatusImageAutoContinueStyleClassName = "auto-continue";
WebInspector.BreakpointTreeElement.StatusImageDisabledStyleClassName = "disabled";
WebInspector.BreakpointTreeElement.FormattedLocationStyleClassName = "formatted-location";
WebInspector.BreakpointTreeElement.ProbeDataUpdatedStyleClassName = "data-updated";

WebInspector.BreakpointTreeElement.ProbeDataUpdatedAnimationDuration = 400; // milliseconds


WebInspector.BreakpointTreeElement.prototype = {
    constructor: WebInspector.BreakpointTreeElement,

    // Public

    get breakpoint()
    {
        return this._breakpoint;
    },

    ondelete: function()
    {
        if (!WebInspector.debuggerManager.isBreakpointRemovable(this._breakpoint))
            return false;

        WebInspector.debuggerManager.removeBreakpoint(this._breakpoint);
        return true;
    },

    onenter: function()
    {
        this._breakpoint.cycleToNextMode();
        return true;
    },

    onspace: function()
    {
        this._breakpoint.cycleToNextMode();
        return true;
    },

    oncontextmenu: function(event)
    {
        var contextMenu = new WebInspector.ContextMenu(event);
        this._breakpoint.appendContextMenuItems(contextMenu, this._statusImageElement);
        contextMenu.show();
    },

    onattach: function()
    {
        WebInspector.GeneralTreeElement.prototype.onattach.call(this);

        this._listeners.install();

        for (var probeSet of WebInspector.probeManager.probeSets)
            if (probeSet.breakpoint === this._breakpoint)
                this._addProbeSet(probeSet);
    },

    ondetach: function()
    {
        WebInspector.GeneralTreeElement.prototype.ondetach.call(this);

        this._listeners.uninstall();

        if (this._probeSet)
            this._removeProbeSet(this._probeSet);
    },

    // Private

    _updateTitles: function()
    {
        var sourceCodeLocation = this._breakpoint.sourceCodeLocation;

        var displayLineNumber = sourceCodeLocation.displayLineNumber;
        var displayColumnNumber = sourceCodeLocation.displayColumnNumber;
        if (displayColumnNumber > 0)
            this.mainTitle = WebInspector.UIString("Line %d:%d").format(displayLineNumber + 1, displayColumnNumber + 1); // The user visible line and column numbers are 1-based.
        else
            this.mainTitle = WebInspector.UIString("Line %d").format(displayLineNumber + 1); // The user visible line number is 1-based.

        if (sourceCodeLocation.hasMappedLocation()) {
            this.subtitle = sourceCodeLocation.formattedLocationString();

            if (sourceCodeLocation.hasFormattedLocation())
                this.subtitleElement.classList.add(WebInspector.BreakpointTreeElement.FormattedLocationStyleClassName);
            else
                this.subtitleElement.classList.remove(WebInspector.BreakpointTreeElement.FormattedLocationStyleClassName);

            this.tooltip = this.mainTitle + " \u2014 " + WebInspector.UIString("originally %s").format(sourceCodeLocation.originalLocationString());
        }
    },

    _updateStatus: function()
    {
        if (this._breakpoint.disabled)
            this._statusImageElement.classList.add(WebInspector.BreakpointTreeElement.StatusImageDisabledStyleClassName);
        else
            this._statusImageElement.classList.remove(WebInspector.BreakpointTreeElement.StatusImageDisabledStyleClassName);

        if (this._breakpoint.autoContinue)
            this._statusImageElement.classList.add(WebInspector.BreakpointTreeElement.StatusImageAutoContinueStyleClassName);
        else
            this._statusImageElement.classList.remove(WebInspector.BreakpointTreeElement.StatusImageAutoContinueStyleClassName);

        if (this._breakpoint.resolved)
            this._statusImageElement.classList.add(WebInspector.BreakpointTreeElement.StatusImageResolvedStyleClassName);
        else
            this._statusImageElement.classList.remove(WebInspector.BreakpointTreeElement.StatusImageResolvedStyleClassName);
    },

    _addProbeSet: function(probeSet)
    {
        console.assert(probeSet instanceof WebInspector.ProbeSet);
        console.assert(probeSet.breakpoint === this._breakpoint);
        console.assert(probeSet !== this._probeSet);

        this._probeSet = probeSet;
        probeSet.addEventListener(WebInspector.ProbeSet.Event.SamplesCleared, this._samplesCleared, this);
        probeSet.dataTable.addEventListener(WebInspector.ProbeSetDataTable.Event.FrameInserted, this._dataUpdated, this);
    },

    _removeProbeSet: function(probeSet)
    {
        console.assert(probeSet instanceof WebInspector.ProbeSet);
        console.assert(probeSet === this._probeSet);

        probeSet.removeEventListener(WebInspector.ProbeSet.Event.SamplesCleared, this._samplesCleared, this);
        probeSet.dataTable.removeEventListener(WebInspector.ProbeSetDataTable.Event.FrameInserted, this._dataUpdated, this);
        delete this._probeSet;
    },

    _probeSetAdded: function(event)
    {
        var probeSet = event.data.probeSet;
        if (probeSet.breakpoint === this._breakpoint)
            this._addProbeSet(probeSet);
    },

    _probeSetRemoved: function(event)
    {
        var probeSet = event.data.probeSet;
        if (probeSet.breakpoint === this._breakpoint)
            this._removeProbeSet(probeSet);
    },

    _samplesCleared: function(event)
    {
        console.assert(this._probeSet);

        var oldTable = event.data.oldTable;
        oldTable.removeEventListener(WebInspector.ProbeSetDataTable.Event.FrameInserted, this._dataUpdated, this);
        this._probeSet.dataTable.addEventListener(WebInspector.ProbeSetDataTable.Event.FrameInserted, this._dataUpdated, this);
    },

    _dataUpdated: function()
    {
        if (this.element.classList.contains(WebInspector.BreakpointTreeElement.ProbeDataUpdatedStyleClassName)) {
            clearTimeout(this._removeIconAnimationTimeoutIdentifier);
            this.element.classList.remove(WebInspector.BreakpointTreeElement.ProbeDataUpdatedStyleClassName);
            // We want to restart the animation, which can only be done by removing the class,
            // performing layout, and re-adding the class. Try adding class back on next run loop.
            window.requestAnimationFrame(this._dataUpdated.bind(this));
            return;
        }

        this.element.classList.add(WebInspector.BreakpointTreeElement.ProbeDataUpdatedStyleClassName);
        this._removeIconAnimationTimeoutIdentifier = setTimeout(function() {
            this.element.classList.remove(WebInspector.BreakpointTreeElement.ProbeDataUpdatedStyleClassName);
        }.bind(this), WebInspector.BreakpointTreeElement.ProbeDataUpdatedAnimationDuration);
    },


    _breakpointLocationDidChange: function(event)
    {
        console.assert(event.target === this._breakpoint);

        // The Breakpoint has a new display SourceCode. The sidebar will remove us, and ondetach() will clear listeners.
        if (event.data.oldDisplaySourceCode === this._breakpoint.displaySourceCode)
            return;

        this._updateTitles();
    },

    _statusImageElementMouseDown: function(event)
    {
        // To prevent the tree element from selecting.
        event.stopPropagation();
    },

    _statusImageElementClicked: function(event)
    {
        this._breakpoint.cycleToNextMode();
    }
};

WebInspector.BreakpointTreeElement.prototype.__proto__ = WebInspector.GeneralTreeElement.prototype;
