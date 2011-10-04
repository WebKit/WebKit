var initialize_SearchTest = function() {

InspectorTest.dumpSearchResults = function(searchResults)
{
    InspectorTest.addResult("Search results: ");
    for (var i = 0; i < searchResults.length; i++)
        InspectorTest.addResult("url: " + searchResults[i].url + ", matchesCount: " + searchResults[i].matchesCount);
    InspectorTest.addResult("");
};

InspectorTest.dumpSearchMatches = function(searchMatches)
{
    InspectorTest.addResult("Search matches: ");
    for (var i = 0; i < searchMatches.length; i++)
        InspectorTest.addResult("lineNumber: " + searchMatches[i].lineNumber + ", line: '" + searchMatches[i].lineContent + "'");
    InspectorTest.addResult("");
};

InspectorTest.runAfterResourcesAreCreated = function(resourceURLs, callback)
{
    InspectorTest._runAfterResourcesAreCreated(resourceURLs.keySet(), callback);
}

InspectorTest._runAfterResourcesAreCreated = function(resourceURLs, callback)
{
    function checkResource(resource)
    {
        for (var url in resourceURLs) {
            if (resource.url.indexOf(url) !== -1)
                delete resourceURLs[url];
        }
    }

    function maybeCallback()
    {
        if (!Object.keys(resourceURLs).length) {
            callback();
            return true;
        }
    }

    function addSniffer(resource)
    {
        InspectorTest.addSniffer(WebInspector.ResourceTreeModel.prototype, "_bindResourceURL", onResourceBind.bind(this));
    }

    function onResourceBind(resource)
    {
        checkResource(resource);
        if (!maybeCallback())
            addSniffer();
    }

    function visit(resource)
    {
        checkResource(resource);
        return maybeCallback();
    }

    var succeeded = WebInspector.resourceTreeModel.forAllResources(visit);
    if (!succeeded)
        addSniffer();
}

};
