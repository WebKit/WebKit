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

InspectorTest.dispatchOnMessage = function(messageId, callback)
{
    function onMessage(event)
    {
        if (event.data === messageId) {
            window.removeEventListener("message", onMessage, false);
            callback();
        }
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
        InspectorTest.disableResourceTracking();
        InspectorTest.completeTest();
    });
    InspectorTest.enableResourceTracking(addExtension);
}

}

var test = function()
{
    InspectorTest.runExtensionTests();
}
