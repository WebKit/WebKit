/*
 * Copyright (C) 2013 University of Washington. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// A ProbeSet clusters Probes from the same Breakpoint and their samples.

WebInspector.ProbeSet = function(breakpoint)
{
    console.assert(breakpoint instanceof WebInspector.Breakpoint, "Unknown breakpoint argument: ", breakpoint);

    WebInspector.Object.call(this);
    this._breakpoint = breakpoint;
    this._probes = [];
    this._probesByIdentifier = new Map;

    this._createDataTable();

    WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceChanged, this);
    WebInspector.Probe.addEventListener(WebInspector.Probe.Event.SampleAdded, this._sampleCollected, this);
    WebInspector.Breakpoint.addEventListener(WebInspector.Breakpoint.Event.ResolvedStateDidChange, this._breakpointResolvedStateDidChange, this);
}

WebInspector.Object.addConstructorFunctions(WebInspector.ProbeSet);

WebInspector.ProbeSet.Event = {
    ProbeAdded: "probe-set-probe-added",
    ProbeRemoved: "probe-set-probe-removed",
    ResolvedStateDidChange: "probe-set-resolved-state-did-change",
    SamplesCleared: "probe-set-samples-cleared",
};

WebInspector.ProbeSet.SampleObjectTitle = "Object";

WebInspector.ProbeSet.prototype = {
    constructor: WebInspector.ProbeSet,
    __proto__: WebInspector.Object.prototype,

    // Public

   get breakpoint()
   {
        return this._breakpoint;
   },

    get probes()
    {
        return this._probes.slice();
    },

    get dataTable()
    {
        return this._dataTable;
    },

    clear: function()
    {
        this._breakpoint.clearActions(WebInspector.BreakpointAction.Type.Probe);
    },

    clearSamples: function()
    {
        for (var probe of this._probes)
            probe.clearSamples();

        this._createDataTable();
        this.dispatchEventToListeners(WebInspector.ProbeSet.Event.SamplesCleared, this);
    },

    createProbe: function(expression)
    {
        this.breakpoint.createAction(WebInspector.BreakpointAction.Type.Probe, null, expression);
    },

    addProbe: function(probe)
    {
        console.assert(probe instanceof WebInspector.Probe, "Tried to add non-probe ", probe, " to probe group", this);
        console.assert(probe.breakpoint === this.breakpoint, "Probe and ProbeSet must have same breakpoint.", probe, this);

        this._probes.push(probe);
        this._probesByIdentifier.set(probe.identifier, probe);

        this.dataTable.addProbe(probe);
        this.dispatchEventToListeners(WebInspector.ProbeSet.Event.ProbeAdded, probe);
    },

    removeProbe: function(probe)
    {
        console.assert(probe instanceof WebInspector.Probe, "Tried to remove non-probe ", probe, " to probe group", this);
        console.assert(this._probes.indexOf(probe) != -1, "Tried to remove probe", probe, " not in group ", this);
        console.assert(this._probesByIdentifier.has(probe.identifier), "Tried to remove probe", probe, " not in group ", this);

        this._probes.splice(this._probes.indexOf(probe), 1);
        this._probesByIdentifier.delete(probe.identifier);
        this.dataTable.removeProbe(probe);
        this.dispatchEventToListeners(WebInspector.ProbeSet.Event.ProbeRemoved, probe);
    },

    willRemove: function()
    {
        console.assert(!this._probes.length, "ProbeSet.willRemove called, but probes still associated with group: ", this._probes);

        WebInspector.Frame.removeEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceChanged, this);
        WebInspector.Probe.removeEventListener(WebInspector.Probe.Event.SampleAdded, this._sampleCollected, this);
        WebInspector.Breakpoint.removeEventListener(WebInspector.Breakpoint.Event.ResolvedStateDidChange, this._breakpointResolvedStateDidChange, this);
    },

    // Private

    _mainResourceChanged: function()
    {
        this.dataTable.mainResourceChanged();
    },

    _createDataTable: function()
    {
        if (this.dataTable)
            this.dataTable.willRemove();

        this._dataTable = new WebInspector.ProbeSetDataTable(this);
    },

    _sampleCollected: function(event)
    {
        var sample = event.data;
        console.assert(sample instanceof WebInspector.ProbeSample, "Tried to add non-sample to probe group: ", sample);

        var probe = event.target;
        if (!this._probesByIdentifier.has(probe.identifier))
            return;

        console.assert(this.dataTable);
        this.dataTable.addSampleForProbe(probe, sample);
    },

    _breakpointResolvedStateDidChange: function(event)
    {
        this.dispatchEventToListeners(WebInspector.ProbeSet.Event.ResolvedStateDidChange);
    }
};
