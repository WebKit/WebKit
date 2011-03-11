function log(message)
{
    output(message);
}

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

InspectorTest.dispatchOnMessage = function(messageId, callback, recurring)
{
    function onMessage(event)
    {
        if (typeof(event.data) !== "object" || event.data.command !== messageId)
            return;
        if (!recurring)
            window.removeEventListener("message", onMessage, false);
        var port = event.ports && event.ports[0];
        if (callback(event.data, port) && port)
            port.postMessage("");
    }
    window.addEventListener("message", onMessage, false);
}

InspectorTest.runExtensionTests = function()
{
    RuntimeAgent.evaluate("location.href", "console", false, function(error, result) {
        if (error)
            return;
        var pageURL = result.description;
        var extensionURL = (/^https?:/.test(pageURL) ?
            pageURL.replace(/^(https?:\/\/[^/]*\/).*$/,"$1") :
            pageURL.replace(/\/inspector\/extensions\/[^/]*$/, "/http/tests")) +
            "/inspector/resources/extension-main.html";
        WebInspector.addExtensions([{ startPage: extensionURL }]);
    });
}

InspectorTest.dispatchOnMessage("extension-tests-done", InspectorTest.completeTest, true);

function extensionOutput(message)
{
    InspectorTest.addResult(message.text);
}
InspectorTest.dispatchOnMessage("output", extensionOutput, true);

function dumpSidebarContent(message, port)
{
    var sidebarPanes = WebInspector.panels.elements.sidebarPanes;
    // the sidebar of interest is presumed to always be last.
    var sidebar = sidebarPanes[Object.keys(sidebarPanes).pop()];
    InspectorTest.runAfterPendingDispatches(function() {
        InspectorTest.addResult("Sidebar content: " + sidebar.bodyElement.textContent);
        port.postMessage("");
    });
}
InspectorTest.dispatchOnMessage("dump-sidebar-content", dumpSidebarContent, true);

function reloadPage(data, port)
{
    InspectorTest.reloadPage(function() {
        port.postMessage("");
    });
}
InspectorTest.dispatchOnMessage("reload", reloadPage, true);

}

var test = function()
{
    InspectorTest.runExtensionTests();
}
