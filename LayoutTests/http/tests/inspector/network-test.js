var initialize_NetworkTest = function() {

InspectorTest.enableBackgroundEventCollection = function()
{
    if (!WebInspector.panels.network._backgroundCollectionEnabled)
        WebInspector.panels.network._toggleBackgroundEventsCollection();
    else
        throw "BackgroundEventCollection already enabled.";
}

InspectorTest.disableBackgroundEventCollection = function ()
{
    if (WebInspector.panels.network._backgroundCollectionEnabled)
        WebInspector.panels.network._toggleBackgroundEventsCollection();
}

InspectorTest.dumpNetworkResources = function()
{
    var resources = WebInspector.panels.network.resources.slice();
    resources.sort(function(a, b) {return a.url.localeCompare(b.url);});
    InspectorTest.addResult("resources count = " + resources.length);
    for (i = 0; i < resources.length; i++)
        InspectorTest.addResult(resources[i].url);
}

};
