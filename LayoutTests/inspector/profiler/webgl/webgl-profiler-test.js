var initialize_WebGLProfilerTest = function() {

InspectorTest.enableWebGLAgent = function(callback)
{
    function webGLAgentEnabled(error)
    {
        if (!error)
            InspectorTest.safeWrap(callback)();
        else {
            InspectorTest.addResult("FAILED to enable WebGLAgent: " + error);
            InspectorTest.completeTest();
        }
    }
    try {
        WebGLAgent.enable(webGLAgentEnabled);
    } catch (e) {
        InspectorTest.addResult("Exception while enabling WebGLAgent", e);
        InspectorTest.completeTest();
    }
};

};

function createWebGLContext(opt_canvas)
{
    var canvas = opt_canvas || document.createElement("canvas");
    var contextIds = ["experimental-webgl", "webkit-3d", "3d"];
    for (var i = 0, contextId; contextId = contextIds[i]; ++i) {
        var gl = canvas.getContext(contextId);
        if (gl)
            return gl;
    }
    return null;
}

if (window.testRunner)
    testRunner.overridePreference("WebKitWebGLEnabled", "1");
