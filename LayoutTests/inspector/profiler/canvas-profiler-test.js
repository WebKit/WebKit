var initialize_CanvasWebGLProfilerTest = function() {

InspectorTest.enableCanvasAgent = function(callback)
{
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
