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
        if (event.data !== messageId)
            return;
        if (!recurring)
            window.removeEventListener("message", onMessage, false);
        callback();
        if (event.ports && event.ports[0])
            event.ports[0].postMessage("");
    }
    window.addEventListener("message", onMessage, false);
}

InspectorTest.runExtensionTests = function()
{
    function addExtension(callback)
    {
        InjectedScriptAccess.getDefault().evaluate("location.href", "console", function(result) {
            var extensionURL = result.description.replace(/\/[^/]*$/, "/resources/extension-main.html");
            WebInspector.addExtensions([{ startPage: extensionURL }]);
            if (callback)
                callback();
        });
    } 
    InspectorTest.dispatchOnMessage("extension-tests-done", function() {
        InspectorTest.completeTest();
    });
    InspectorTest.reloadPageIfNeeded(addExtension);
}

InspectorTest.dumpSidebarContent = function()
{
    var sidebarPanes = WebInspector.panels.scripts.sidebarPanes;
    // the sidebar of interest is presumed to always be last.
    var sidebar = sidebarPanes[Object.keys(sidebarPanes).pop()];
    InspectorTest.addResult("Sidebar content: " + sidebar.bodyElement.textContent);
}

InspectorTest.dispatchOnMessage("dump-sidebar-content", InspectorTest.dumpSidebarContent, true);

}

var test = function()
{
    InspectorTest.runExtensionTests();
}
