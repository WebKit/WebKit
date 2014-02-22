window.ProbeHelper = {};

ProbeHelper.simplifiedProbeSample = function(messageObject)
{
    var data = messageObject.params.sample;
    return {
        probeId: data.probeId,
        batchId: data.batchId,
        sampleId: data.sampleId,
        payload: data.payload
    };
}

ProbeHelper.stringifyProbeSample = function(ProbeSample)
{
    console.assert(ProbeSample instanceof WebInspector.ProbeSample, "Unexpected object type!");
    return JSON.stringify({
        "sampleId": ProbeSample.sampleId,
        "batchId": ProbeSample.batchId,
    });
}

ProbeHelper.installTracingListeners = function()
{
    if (!WebInspector.debuggerManager || !WebInspector.probeManager)
        return;

    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointAdded, function(event) {
        InspectorTest.log("Breakpoint was added.");
    });

    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointRemoved, function(event) {
        InspectorTest.log("Breakpoint was removed.");
    });

    WebInspector.Breakpoint.addEventListener(WebInspector.Breakpoint.Event.DisabledStateDidChange, function(event) {
        var breakpoint = event.target;
        console.assert(breakpoint instanceof WebInspector.Breakpoint, "Unexpected object type!");

        InspectorTest.log("Breakpoint disabled state changed: " + breakpoint.disabled);
    });

    WebInspector.Breakpoint.addEventListener(WebInspector.Breakpoint.Event.ResolvedStateDidChange, function(event) {
        var breakpoint = event.target;
        console.assert(breakpoint instanceof WebInspector.Breakpoint, "Unexpected object type!");

        InspectorTest.log("Breakpoint resolved state changed: " + breakpoint.resolved);
    });

    WebInspector.Breakpoint.addEventListener(WebInspector.Breakpoint.Event.AutoContinueDidChange, function(event) {
        var breakpoint = event.target;
        console.assert(breakpoint instanceof WebInspector.Breakpoint, "Unexpected object type!");

        InspectorTest.log("Breakpoint autocontinue state changed: " + breakpoint.autoContinue);
    });

    WebInspector.Breakpoint.addEventListener(WebInspector.Breakpoint.Event.ConditionDidChange, function(event) {
        var breakpoint = event.target;
        console.assert(breakpoint instanceof WebInspector.Breakpoint, "Unexpected object type!");

        InspectorTest.log("Breakpoint condition changed: " + breakpoint.condition);
    });

    WebInspector.Breakpoint.addEventListener(WebInspector.Breakpoint.Event.ActionsDidChange, function(event) {
        var breakpoint = event.target;
        console.assert(breakpoint instanceof WebInspector.Breakpoint, "Unexpected object type!");

        InspectorTest.log("Breakpoint actions changed. New count: " + breakpoint.actions.length);
    });

    WebInspector.probeManager.addEventListener(WebInspector.ProbeManager.Event.ProbeSetAdded, function(event) {
        var probeSet = event.data.probeSet;
        console.assert(probeSet instanceof WebInspector.ProbeSet, "Unexpected object type!");

        InspectorTest.log("Probe set was added. New count: " + WebInspector.probeManager.probeSets.length);
    });

    WebInspector.probeManager.addEventListener(WebInspector.ProbeManager.Event.ProbeSetRemoved, function(event) {
        var probeSet = event.data.probeSet;
        console.assert(probeSet instanceof WebInspector.ProbeSet, "Unexpected object type!");

        InspectorTest.log("Probe set was removed. New count: " + WebInspector.probeManager.probeSets.length);
    });

    WebInspector.Probe.addEventListener(WebInspector.Probe.Event.ExpressionChanged, function(event) {
        var probe = event.target;
        console.assert(probe instanceof WebInspector.Probe, "Unexpected object type!");

        InspectorTest.log("Probe with identifier " + probe.id + " changed expression: " + probe.expression);
    });

    WebInspector.Probe.addEventListener(WebInspector.Probe.Event.SampleAdded, function(event) {
        var probe = event.target;
        var sample = event.data;
        console.assert(probe instanceof WebInspector.Probe, "Unexpected object type!");
        console.assert(sample instanceof WebInspector.ProbeSample, "Unexpected object type!");

        InspectorTest.log("Probe with identifier " + probe.id + " added sample: " + sample);
    });

    WebInspector.Probe.addEventListener(WebInspector.Probe.Event.SamplesCleared, function(event) {
        var probe = event.target;
        console.assert(probe instanceof WebInspector.Probe, "Unexpected object type!");

        InspectorTest.log("Probe with identifier " + probe.id + " cleared all samples.");
    });

    WebInspector.ProbeSet.addEventListener(WebInspector.ProbeSet.Event.ProbeAdded, function(event) {
        var probe = event.data;
        var probeSet = event.target;
        console.assert(probe instanceof WebInspector.Probe, "Unexpected object type!");
        console.assert(probeSet instanceof WebInspector.ProbeSet, "Unexpected object type!");

        InspectorTest.log("Probe with identifier " + probe.id + " was added to probe set.");
        InspectorTest.log("Probe set's probe count: " + probeSet.probes.length);
    });

    WebInspector.ProbeSet.addEventListener(WebInspector.ProbeSet.Event.ProbeRemoved, function(event) {
        var probe = event.data;
        var probeSet = event.target;
        console.assert(probe instanceof WebInspector.Probe, "Unexpected object type!");
        console.assert(probeSet instanceof WebInspector.ProbeSet, "Unexpected object type!");

        InspectorTest.log("Probe with identifier " + probe.id + " was removed from probe set.");
        InspectorTest.log("Probe set's probe count: " + probeSet.probes.length);
    });
}
