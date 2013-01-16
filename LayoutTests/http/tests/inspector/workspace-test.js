var initialize_WorkspaceTest = function() {

InspectorTest.testWorkspace;
InspectorTest.createWorkspace = function()
{
    InspectorTest.testWorkspace = new WebInspector.Workspace();
    InspectorTest.testNetworkWorkspaceProvider = new WebInspector.SimpleWorkspaceProvider(InspectorTest.testWorkspace);
    InspectorTest.testWorkspace.addProject(WebInspector.projectNames.Network, InspectorTest.testNetworkWorkspaceProvider);
    InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, InspectorTest._defaultUISourceCodeProviderEventHandler);
    InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeRemoved, InspectorTest._defaultUISourceCodeProviderEventHandler);
}

InspectorTest.waitForWorkspaceUISourceCodeAddedEvent = function(callback)
{
    InspectorTest.testWorkspace.removeEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, InspectorTest._defaultUISourceCodeProviderEventHandler);
    InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, uiSourceCodeAdded);

    function uiSourceCodeAdded(event)
    {
        InspectorTest.testWorkspace.removeEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, uiSourceCodeAdded);
        InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, InspectorTest._defaultUISourceCodeProviderEventHandler);
        callback(event.data);
    }
}

InspectorTest.addMockUISourceCodeToWorkspace = function(url, type, content)
{
    var isDocument = type === WebInspector.resourceTypes.Document;
    var mockContentProvider = new WebInspector.StaticContentProvider(type, content);
    InspectorTest.testNetworkWorkspaceProvider.addFileForURL(url, mockContentProvider, !isDocument);
}

InspectorTest._defaultUISourceCodeProviderEventHandler = function(event)
{
    throw new Error("Unexpected UISourceCodeProvider event: " + event.type + ".");
}

InspectorTest.dumpUISourceCode = function(uiSourceCode, callback)
{
    var url = uiSourceCode.originURL().replace(/.*LayoutTests/, "LayoutTests");
    InspectorTest.addResult("UISourceCode: " + url);
    InspectorTest.addResult("UISourceCode is editable: " + uiSourceCode.isEditable());
    if (uiSourceCode.contentType() === WebInspector.resourceTypes.Script || uiSourceCode.contentType() === WebInspector.resourceTypes.Document)
        InspectorTest.addResult("UISourceCode is content script: " + !!uiSourceCode.isContentScript);
    uiSourceCode.requestContent(didRequestContent);

    function didRequestContent(content, contentEncoded, mimeType)
    {
        InspectorTest.addResult("Mime type: " + mimeType);
        InspectorTest.addResult("UISourceCode content: " + content);
        callback();
    }
}

};
