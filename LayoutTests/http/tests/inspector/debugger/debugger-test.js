TestPage.registerInitializer(function() {

InspectorTest.startTracingBreakpoints = function()
{
    if (!WI.debuggerManager)
        return;

    WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.BreakpointAdded, function(event) {
        InspectorTest.log("Breakpoint was added.");
    });

    WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.BreakpointRemoved, function(event) {
        InspectorTest.log("Breakpoint was removed.");
    });

    WI.Breakpoint.addEventListener(WI.Breakpoint.Event.DisabledStateDidChange, function(event) {
        var breakpoint = event.target;
        console.assert(breakpoint instanceof WI.Breakpoint, "Unexpected object type!");

        InspectorTest.log("Breakpoint disabled state changed: " + breakpoint.disabled);
    });

    WI.Breakpoint.addEventListener(WI.Breakpoint.Event.ResolvedStateDidChange, function(event) {
        var breakpoint = event.target;
        console.assert(breakpoint instanceof WI.Breakpoint, "Unexpected object type!");

        InspectorTest.log("Breakpoint resolved state changed: " + breakpoint.resolved);
    });

    WI.Breakpoint.addEventListener(WI.Breakpoint.Event.AutoContinueDidChange, function(event) {
        var breakpoint = event.target;
        console.assert(breakpoint instanceof WI.Breakpoint, "Unexpected object type!");

        InspectorTest.log("Breakpoint autocontinue state changed: " + breakpoint.autoContinue);
    });

    WI.Breakpoint.addEventListener(WI.Breakpoint.Event.ConditionDidChange, function(event) {
        var breakpoint = event.target;
        console.assert(breakpoint instanceof WI.Breakpoint, "Unexpected object type!");

        InspectorTest.log("Breakpoint condition changed: " + breakpoint.condition);
    });

    WI.Breakpoint.addEventListener(WI.Breakpoint.Event.ActionsDidChange, function(event) {
        var breakpoint = event.target;
        console.assert(breakpoint instanceof WI.Breakpoint, "Unexpected object type!");

        InspectorTest.log("Breakpoint actions changed. New count: " + breakpoint.actions.length);
    });
}

InspectorTest.startTracingProbes = function()
{
    if (!WI.debuggerManager)
        return;

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
