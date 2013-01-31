var initialize_CanvasWebGLProfilerTest = function() {

InspectorTest.enableCanvasAgent = function(callback)
{
    var dispatcher = InspectorBackend._domainDispatchers["Canvas"];
    if (!dispatcher) {
        InspectorBackend.registerCanvasDispatcher({
            contextCreated: function() {},
            traceLogsRemoved: function() {}
        });
    }

    function canvasAgentEnabled(error)
    {
        if (!error)
            InspectorTest.safeWrap(callback)();
        else {
            InspectorTest.addResult("FAILED to enable CanvasAgent: " + error);
            InspectorTest.completeTest();
        }
    }
    try {
        CanvasAgent.enable(canvasAgentEnabled);
    } catch (e) {
        InspectorTest.addResult("Exception while enabling CanvasAgent: " + e);
        InspectorTest.completeTest();
    }
};

InspectorTest.disableCanvasAgent = function(callback)
{
    function canvasAgentDisabled(error)
    {
        if (!error)
            InspectorTest.safeWrap(callback)();
        else {
            InspectorTest.addResult("FAILED to disable CanvasAgent: " + error);
            InspectorTest.completeTest();
        }
    }
    try {
        CanvasAgent.disable(canvasAgentDisabled);
    } catch (e) {
        InspectorTest.addResult("Exception while disabling CanvasAgent: " + e);
        InspectorTest.completeTest();
    }
};

InspectorTest.dumpTraceLogCall = function(call, indent)
{
    indent = indent || "";
    function formatSourceURL(url)
    {
        return url ? "\"" + url.replace(/^.*\/([^\/]+)\/?$/, "$1") + "\"" : "null";
    }
    var args = (call.arguments || []).map(function(arg) {
        return arg.description;
    });
    var properties = [
        "{Call}",
        call.functionName ? "functionName:\"" + call.functionName + "\"" : "",
        call.arguments ? "arguments:[" + args.join(",") + "]" : "",
        call.result ? "result:" + call.result.description : "",
        call.property ? "property:\"" + call.property + "\"" : "",
        call.value ? "value:" + call.value.description : "",
        call.isDrawingCall ? "isDrawingCall:true" : "",
        "sourceURL:" + formatSourceURL(call.sourceURL),
        "lineNumber:" + call.lineNumber,
        "columnNumber:" + call.columnNumber
    ];
    InspectorTest.addResult(indent + properties.filter(Boolean).join("  "));
};

InspectorTest.dumpTraceLog = function(traceLog, indent)
{
    indent = indent || "";
    var calls = traceLog.calls;
    var properties = [
        "{TraceLog}",
        "alive:" + !!traceLog.alive,
        "startOffset:" + (traceLog.startOffset || 0),
        "#calls:" + traceLog.totalAvailableCalls
    ];
    InspectorTest.addResult(indent + properties.filter(Boolean).join("  "));
    for (var i = 0, n = calls.length; i < n; ++i)
        InspectorTest.dumpTraceLogCall(calls[i], indent + "  ");
};

};

function createWebGLContext(opt_canvas)
{
    if (window.testRunner)
        testRunner.overridePreference("WebKitWebGLEnabled", "1");

    var canvas = opt_canvas || document.createElement("canvas");
    var contextIds = ["experimental-webgl", "webkit-3d", "3d"];
    for (var i = 0, contextId; contextId = contextIds[i]; ++i) {
        var gl = canvas.getContext(contextId);
        if (gl)
            return gl;
    }
    return null;
}

function createCanvas2DContext(opt_canvas)
{
    var canvas = opt_canvas || document.createElement("canvas");
    return canvas.getContext("2d");
}
