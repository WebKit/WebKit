/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

TestPage.registerInitializer(function() {

ProtocolTest.Probe = {};

ProtocolTest.Probe.sanitizeProbeSample = function(messageObject)
{
    var data = messageObject.params.sample;
    return {
        probeId: data.probeId,
        batchId: data.batchId,
        sampleId: data.sampleId,
        payload: data.payload
    };
}

ProtocolTest.Probe.stringifyProbeSample = function(ProbeSample)
{
    console.assert(ProbeSample instanceof WI.ProbeSample, "Unexpected object type!");
    return JSON.stringify({
        "sampleId": ProbeSample.sampleId,
        "batchId": ProbeSample.batchId,
    });
}

ProtocolTest.Probe.installTracingListeners = function()
{
    if (!WI.debuggerManager)
        return;

    WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.BreakpointAdded, function(event) {
        InspectorTest.log("Breakpoint was added.");
    });

    WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.BreakpointRemoved, function(event) {
        InspectorTest.log("Breakpoint was removed.");
    });

    WI.JavaScriptBreakpoint.addEventListener(WI.Breakpoint.Event.DisabledStateDidChange, function(event) {
        var breakpoint = event.target;
        console.assert(breakpoint instanceof WI.JavaScriptBreakpoint, "Unexpected object type!");

        InspectorTest.log("Breakpoint disabled state changed: " + breakpoint.disabled);
    });

    WI.JavaScriptBreakpoint.addEventListener(WI.JavaScriptBreakpoint.Event.ResolvedStateDidChange, function(event) {
        var breakpoint = event.target;
        console.assert(breakpoint instanceof WI.JavaScriptBreakpoint, "Unexpected object type!");

        InspectorTest.log("Breakpoint resolved state changed: " + breakpoint.resolved);
    });

    WI.JavaScriptBreakpoint.addEventListener(WI.Breakpoint.Event.AutoContinueDidChange, function(event) {
        var breakpoint = event.target;
        console.assert(breakpoint instanceof WI.JavaScriptBreakpoint, "Unexpected object type!");

        InspectorTest.log("Breakpoint autocontinue state changed: " + breakpoint.autoContinue);
    });

    WI.JavaScriptBreakpoint.addEventListener(WI.Breakpoint.Event.ConditionDidChange, function(event) {
        var breakpoint = event.target;
        console.assert(breakpoint instanceof WI.JavaScriptBreakpoint, "Unexpected object type!");

        InspectorTest.log("Breakpoint condition changed: " + breakpoint.condition);
    });

    WI.JavaScriptBreakpoint.addEventListener(WI.Breakpoint.Event.ActionsDidChange, function(event) {
        var breakpoint = event.target;
        console.assert(breakpoint instanceof WI.JavaScriptBreakpoint, "Unexpected object type!");

        InspectorTest.log("Breakpoint actions changed. New count: " + breakpoint.actions.length);
    });

    WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ProbeSetAdded, function(event) {
        var probeSet = event.data.probeSet;
        console.assert(probeSet instanceof WI.ProbeSet, "Unexpected object type!");

        InspectorTest.log("Probe set was added. New count: " + WI.debuggerManager.probeSets.length);
    });

    WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ProbeSetRemoved, function(event) {
        var probeSet = event.data.probeSet;
        console.assert(probeSet instanceof WI.ProbeSet, "Unexpected object type!");

        InspectorTest.log("Probe set was removed. New count: " + WI.debuggerManager.probeSets.length);
    });

    WI.Probe.addEventListener(WI.Probe.Event.ExpressionChanged, function(event) {
        var probe = event.target;
        console.assert(probe instanceof WI.Probe, "Unexpected object type!");

        InspectorTest.log("Probe with identifier " + probe.id + " changed expression: " + probe.expression);
    });

    WI.Probe.addEventListener(WI.Probe.Event.SampleAdded, function(event) {
        var probe = event.target;
        var sample = event.data;
        console.assert(probe instanceof WI.Probe, "Unexpected object type!");
        console.assert(sample instanceof WI.ProbeSample, "Unexpected object type!");

        InspectorTest.log("Probe with identifier " + probe.id + " added sample: " + sample);
    });

    WI.Probe.addEventListener(WI.Probe.Event.SamplesCleared, function(event) {
        var probe = event.target;
        console.assert(probe instanceof WI.Probe, "Unexpected object type!");

        InspectorTest.log("Probe with identifier " + probe.id + " cleared all samples.");
    });

    WI.ProbeSet.addEventListener(WI.ProbeSet.Event.ProbeAdded, function(event) {
        var probe = event.data;
        var probeSet = event.target;
        console.assert(probe instanceof WI.Probe, "Unexpected object type!");
        console.assert(probeSet instanceof WI.ProbeSet, "Unexpected object type!");

        InspectorTest.log("Probe with identifier " + probe.id + " was added to probe set.");
        InspectorTest.log("Probe set's probe count: " + probeSet.probes.length);
    });

    WI.ProbeSet.addEventListener(WI.ProbeSet.Event.ProbeRemoved, function(event) {
        var probe = event.data;
        var probeSet = event.target;
        console.assert(probe instanceof WI.Probe, "Unexpected object type!");
        console.assert(probeSet instanceof WI.ProbeSet, "Unexpected object type!");

        InspectorTest.log("Probe with identifier " + probe.id + " was removed from probe set.");
        InspectorTest.log("Probe set's probe count: " + probeSet.probes.length);
    });
}

});
