function extensionFunctions()
{
    var functions = "";

    for (symbol in window) {
        if (/^extension_/.exec(symbol) && typeof window[symbol] === "function")
            functions += window[symbol].toString();
    }
    return functions;
}

var initialize_ExtensionsTest = function()
{

window.buildPlatformExtensionAPI = function(extensionInfo)
{
    function platformExtensionAPI(coreAPI)
    {
        window.webInspector = coreAPI;
        window._extensionServerForTests = extensionServer;
    }
    return platformExtensionAPI.toString();
}

InspectorTest._replyToExtension = function(requestId, port)
{
    WebInspector.extensionServer._dispatchCallback(requestId, port);
}

function onEvaluate(message, port)
{
    var reply = WebInspector.extensionServer._dispatchCallback.bind(WebInspector.extensionServer, message.requestId, port);
    try {
        eval(message.expression);
    } catch (e) {
        InspectorTest.addResult("Exception while running: " + message.expression + "\n" + (e.stack || e));
        InspectorTest.completeTest();
    }
}

WebInspector.extensionServer._registerHandler("evaluateForTestInFrontEnd", onEvaluate);

InspectorTest.showPanel = function(panelId)
{
    if (panelId === "extension")
        panelId = WebInspector.inspectorView._panelOrder[WebInspector.inspectorView._panelOrder.length - 1];
    WebInspector.showPanel(panelId);
}

InspectorTest.runExtensionTests = function()
{
    RuntimeAgent.evaluate("location.href", "console", false, function(error, result) {
        if (error)
            return;
        var pageURL = result.value;
        var extensionURL = (/^https?:/.test(pageURL) ?
            pageURL.replace(/^(https?:\/\/[^/]*\/).*$/,"$1") :
            pageURL.replace(/\/inspector\/extensions\/[^/]*$/, "/http/tests")) +
            "/inspector/resources/extension-main.html";
        WebInspector.addExtensions([{ startPage: extensionURL, name: "test extension", exposeWebInspectorNamespace: true }]);
    });
}

}

function extension_showPanel(panelId, callback)
{
    evaluateOnFrontend("InspectorTest.showPanel(unescape('" + escape(panelId) + "')); reply();", callback);
}

var test = function()
{
    InspectorTest.runExtensionTests();
}
