var initialize_ResourceTreeTest = function() {

InspectorTest.dumpResources = function(formatter)
{
    var results = [];

    function formatterWrapper(resource)
    {
        if (formatter)
            results.push({ resource: resource, text: formatter(resource) });
        else
            results.push({ resource: resource, text: resource.url });
    }

    WebInspector.resourceTreeModel.forAllResources(formatterWrapper);

    function comparator(result1, result2)
    {
        return result1.resource.url.localeCompare(result2.resource.url);
    }
    results.sort(comparator);

    for (var i = 0; i < results.length; ++i)
        InspectorTest.addResult(results[i].text);
}

InspectorTest.dumpResourcesURLMap = function()
{
    var results = [];
    for (var url in WebInspector.resourceTreeModel._resourcesByURL) {
        var resource = WebInspector.resourceTreeModel._resourcesByURL[url];
        results.push({ url: url, resource: resource });
    }

    function comparator(result1, result2)
    {
        if (result1.url > result2.url)
            return 1;
        if (result2.url > result1.url)
            return -1;
        return 0;
    }

    results.sort(comparator);

    for (var i = 0; i < results.length; ++i)
        InspectorTest.addResult(results[i].url + " == " + results[i].resource.url);
}

InspectorTest.dumpResourcesTree = function()
{
    function dump(treeItem, prefix)
    {
        // We don't need to print the bubbles content here.
        if (typeof(treeItem._resetBubble) === "function")
            treeItem._resetBubble();

        InspectorTest.addResult(prefix + treeItem.listItemElement.textContent);

        treeItem.expand();
        var children = treeItem.children;
        for (var i = 0; children && i < children.length; ++i)
            dump(children[i], prefix + "    ");
    }

    WebInspector.setCurrentPanel(WebInspector.panels.resources);
    dump(WebInspector.panels.resources.resourcesListTreeElement, "");
}

InspectorTest.dumpResourceTreeEverything = function()
{
    function format(resource)
    {
        return WebInspector.Resource.Type.toString(resource.type) + " " + resource.url;
    }

    InspectorTest.addResult("Resources:");
    InspectorTest.dumpResources(format);

    InspectorTest.addResult("");
    InspectorTest.addResult("Resources URL Map:");
    InspectorTest.dumpResourcesURLMap();

    InspectorTest.addResult("");
    InspectorTest.addResult("Resources Tree:");
    InspectorTest.dumpResourcesTree();
}

};
