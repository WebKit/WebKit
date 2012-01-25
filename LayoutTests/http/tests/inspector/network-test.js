var initialize_NetworkTest = function() {

InspectorTest.dumpNetworkResources = function()
{
    var resources = WebInspector.panels.network.resources.slice();
    resources.sort(function(a, b) {return a.url.localeCompare(b.url);});
    InspectorTest.addResult("resources count = " + resources.length);
    for (i = 0; i < resources.length; i++)
        InspectorTest.addResult(resources[i].url);
}

InspectorTest.resetInspectorResourcesData = function(callback)
{
    InspectorTest.evaluateInPage("resetInspectorResourcesData()", nextStep);

    function nextStep(result)
    {
        if (!result) {
            InspectorTest.addResult("This test can not be run as window.internals is not available.");
            Inspector.completeTest();
        } else
            callback();
    }
}

};

function doXHR(method, url, async, callback)
{
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function()
    {
        if (xhr.readyState === XMLHttpRequest.DONE) {
            if (typeof(callback) === "function")
                callback();
        }
    };
    xhr.open(method, url, async);
    xhr.send(null);
}

function resetInspectorResourcesData()
{
    if (!window.internals)
        return false;

    internals.settings.setInspectorResourcesDataSizeLimits(10 * 1000 * 1000, 1000 * 1000);
    return true;
}

