var initialize_ResourceTest = function() {

InspectorTest.HARPropertyFormatters = {
    bodySize: "formatAsTypeName",
    compression: "formatAsTypeName",
    headers: "formatAsTypeName",
    headersSize: "formatAsTypeName",
    id: "formatAsTypeName",
    onContentLoad: "formatAsTypeName",
    onLoad: "formatAsTypeName",
    receive: "formatAsTypeName",
    startedDateTime: "formatAsRecentTime",
    time: "formatAsTypeName",
    timings: "formatAsTypeName",
    version: "formatAsTypeName",
    wait: "formatAsTypeName",
};

// addObject checks own properties only, so make a deep copy rather than use prototype.

InspectorTest.HARPropertyFormattersWithSize = JSON.parse(JSON.stringify(InspectorTest.HARPropertyFormatters));
InspectorTest.HARPropertyFormattersWithSize.size = "formatAsTypeName";


InspectorTest.resourceURLComparer = function(r1, r2)
{
    return r1.request.url.localeCompare(r2.request.url);
}

InspectorTest.runAfterCachedResourcesProcessed = function(callback)
{
    if (!WebInspector.resourceTreeModel._cachedResourcesProcessed)
        WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.CachedResourcesLoaded, callback);
    else
        callback();
}

InspectorTest.runAfterResourcesAreFinished = function(resourceURLs, callback)
{
    var resourceURLsMap = resourceURLs.keySet();

    function checkResources()
    {
        WebInspector.resourceTreeModel.forAllResources(visit);
        function visit(resource)
        {
            for (url in resourceURLsMap) {
                if (resource.url.indexOf(url) !== -1)
                    delete resourceURLsMap[url];
            }
        }
        if (!Object.keys(resourceURLsMap).length) {
            WebInspector.resourceTreeModel.removeEventListener(WebInspector.ResourceTreeModel.EventTypes.ResourceAdded, checkResources);
            callback();
        }
    }
    checkResources();
    if (Object.keys(resourceURLsMap).length)
        WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.ResourceAdded, checkResources);
}

InspectorTest.showResource = function(resourceURL, callback)
{
    function showResourceCallback()
    {
        WebInspector.resourceTreeModel.forAllResources(visit);
        function visit(resource)
        {
            if (resource.url.indexOf(resourceURL) !== -1) {
                WebInspector.panels.resources.showResource(resource, 1);
                var sourceFrame = WebInspector.panels.resources._resourceViewForResource(resource);
                if (sourceFrame.loaded)
                    callback(sourceFrame);
                else
                    sourceFrame.addEventListener(WebInspector.SourceFrame.Events.Loaded, callback.bind(null, sourceFrame));
                return true;
            }
        }
    }
    InspectorTest.runAfterResourcesAreFinished([resourceURL], showResourceCallback);
}

}
