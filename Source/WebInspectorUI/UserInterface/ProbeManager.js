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

WebInspector.ProbeManager = function()
{
    WebInspector.Object.call(this);

    // Used to detect deleted probe actions.
    this._knownProbeIdentifiersForBreakpoint = new Map;

    // Main lookup tables for probes and probe sets.
    this._probesByIdentifier = new Map;
    this._probeSetsByBreakpoint = new Map;

    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointAdded, this._breakpointAdded, this);
    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointRemoved, this._breakpointRemoved, this);
    WebInspector.Breakpoint.addEventListener(WebInspector.Breakpoint.Event.ActionsDidChange, this._breakpointActionsChanged, this);

    // Saved breakpoints should not be restored on the first event loop turn, because it
    // makes manager initialization order very fragile. No breakpoints should be available.
    console.assert(!WebInspector.debuggerManager.breakpoints.length, "No breakpoints should exist before all the managers are constructed.");
}

WebInspector.ProbeManager.Event = {
    ProbeSetAdded: "probe-manager-probe-set-added",
    ProbeSetRemoved: "probe-manager-probe-set-removed",
};

WebInspector.ProbeManager.prototype = {
    constructor: WebInspector.ProbeManager,
    __proto__: WebInspector.Object.prototype,

    // Public

    get probeSets()
    {
        var sets = [];
        for (var set of this._probeSetsByBreakpoint.values())
            sets.push(set);

        return sets;
    },

    // Protected (called by WebInspector.DebuggerObserver)

    didSampleProbe: function(sample)
    {
        console.assert(this._probesByIdentifier.has(sample.probeId), "Unknown probe identifier specified for sample: ", sample);
        var probe = this._probesByIdentifier.get(sample.probeId);
        probe.addSample(new WebInspector.ProbeSample(sample.sampleId, sample.batchId, sample.timestamp, sample.payload));
    },

    // Private

    _breakpointAdded: function(breakpointOrEvent)
    {
        var breakpoint;
        if (breakpointOrEvent instanceof WebInspector.Breakpoint)
            breakpoint = breakpointOrEvent;
        else
            breakpoint = breakpointOrEvent.data.breakpoint;

        console.assert(breakpoint instanceof WebInspector.Breakpoint, "Unknown object passed as breakpoint: ", breakpoint);

        if (this._knownProbeIdentifiersForBreakpoint.has(breakpoint))
            return;

        this._knownProbeIdentifiersForBreakpoint.set(breakpoint, new Set);

        this._breakpointActionsChanged(breakpoint);
    },

    _breakpointRemoved: function(event)
    {
        var breakpoint = event.data.breakpoint;
        console.assert(this._knownProbeIdentifiersForBreakpoint.has(breakpoint));

        this._breakpointActionsChanged(breakpoint);
        this._knownProbeIdentifiersForBreakpoint.delete(breakpoint);
    },

    _breakpointActionsChanged: function(breakpointOrEvent)
    {
        var breakpoint;
        if (breakpointOrEvent instanceof WebInspector.Breakpoint)
            breakpoint = breakpointOrEvent;
        else
            breakpoint = breakpointOrEvent.target;

        console.assert(breakpoint instanceof WebInspector.Breakpoint, "Unknown object passed as breakpoint: ", breakpoint);

        // Sometimes actions change before the added breakpoint is fully dispatched.
        if (!this._knownProbeIdentifiersForBreakpoint.has(breakpoint)) {
            this._breakpointAdded(breakpoint);
            return;
        }

        var knownProbeIdentifiers = this._knownProbeIdentifiersForBreakpoint.get(breakpoint);
        var seenProbeIdentifiers = new Set;

        breakpoint.probeActions.forEach(function(probeAction) {
            var probeIdentifier = probeAction.id;
            console.assert(probeIdentifier, "Probe added without breakpoint action identifier: ", breakpoint);

            seenProbeIdentifiers.add(probeIdentifier);
            if (!knownProbeIdentifiers.has(probeIdentifier)) {
                // New probe; find or create relevant probe set.
                knownProbeIdentifiers.add(probeIdentifier);
                var probeSet = this._probeSetForBreakpoint(breakpoint);
                var newProbe = new WebInspector.Probe(probeIdentifier, breakpoint, probeAction.data);
                this._probesByIdentifier.set(probeIdentifier, newProbe);
                probeSet.addProbe(newProbe);
                return;
            }

            var probe = this._probesByIdentifier.get(probeIdentifier);
            console.assert(probe, "Probe known but couldn't be found by identifier: ", probeIdentifier);
            // Update probe expression; if it differed, change events will fire.
            probe.expression = probeAction.data;
        }, this);

        // Look for missing probes based on what we saw last.
        knownProbeIdentifiers.forEach(function(probeIdentifier) {
            if (seenProbeIdentifiers.has(probeIdentifier))
                return;

            // The probe has gone missing, remove it.
            var probeSet = this._probeSetForBreakpoint(breakpoint);
            var probe = this._probesByIdentifier.get(probeIdentifier);
            this._probesByIdentifier.delete(probeIdentifier);
            knownProbeIdentifiers.delete(probeIdentifier);
            probeSet.removeProbe(probe);

            // Remove the probe set if it has become empty.
            if (!probeSet.probes.length) {
                this._probeSetsByBreakpoint.delete(probeSet.breakpoint);
                probeSet.willRemove();
                this.dispatchEventToListeners(WebInspector.ProbeManager.Event.ProbeSetRemoved, probeSet);
            }
        }, this);
    },

    _probeSetForBreakpoint: function(breakpoint)
    {
        if (this._probeSetsByBreakpoint.has(breakpoint))
            return this._probeSetsByBreakpoint.get(breakpoint);

        var newProbeSet = new WebInspector.ProbeSet(breakpoint);
        this._probeSetsByBreakpoint.set(breakpoint, newProbeSet);
        this.dispatchEventToListeners(WebInspector.ProbeManager.Event.ProbeSetAdded, newProbeSet);
        return newProbeSet;
    }
};
