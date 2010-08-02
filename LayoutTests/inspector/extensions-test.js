function log(message)
{
    output(message);
}

function frontend_initializeExtension(url)
{
    if (WebInspector.panels.resources.resourceTrackingEnabled)
        WebInspector.addExtensions([{ startPage: url }]);
    else
        InspectorBackend.enableResourceTracking(false);
}

function doit()
{
    var extensionURL = location.href.replace(/\/[^/]*$/, "/resources/extension-main.html");
    evaluateInWebInspector("frontend_initializeExtension('" + extensionURL + "')");
}

function done()
{
    function callback()
    {
        notifyDone();
    }
    evaluateInWebInspector("InspectorBackend.disableResourceTracking(false);", callback);
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
