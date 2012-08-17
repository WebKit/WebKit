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

InspectorTest._replyToExtension = function(port, data)
{
    port.postMessage({ response: data });
}

function onMessage(event)
{
    if (typeof event.data !== "object" || !event.data.expression)
        return;
    if (event.ports && event.ports[0])
        var reply = InspectorTest._replyToExtension.bind(null, event.ports[0]); // reply() is intended to be used by the code being evaluated.
    try {
        var result = eval(event.data.expression);
    } catch (e) {
        InspectorTest.addResult("Exception while running: " + event.data.expression + "\n" + (e.stack || e));
        InspectorTest.completeTest();
    }
}

window.addEventListener("message", InspectorTest.safeWrap(onMessage), false);

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
