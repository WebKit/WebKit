var initialize_WorkspaceTest = function() {

InspectorTest.testWorkspace;
InspectorTest.createWorkspace = function()
{
    InspectorTest.testWorkspace = new WebInspector.Workspace();
    InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, InspectorTest._defaultUISourceCodeProviderEventHandler);
    InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeRemoved, InspectorTest._defaultUISourceCodeProviderEventHandler);
    InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.TemporaryUISourceCodeAdded, InspectorTest._defaultUISourceCodeProviderEventHandler);
    InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.TemporaryUISourceCodeRemoved, InspectorTest._defaultUISourceCodeProviderEventHandler);
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

InspectorTest.waitForWorkspaceTemporaryUISourceCodeAddedEvent = function(callback)
{
    InspectorTest.testWorkspace.removeEventListener(WebInspector.UISourceCodeProvider.Events.TemporaryUISourceCodeAdded, InspectorTest._defaultUISourceCodeProviderEventHandler);
    InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.TemporaryUISourceCodeAdded, temporaryUISourceCodeAdded);

    function temporaryUISourceCodeAdded(event)
    {
        InspectorTest.testWorkspace.removeEventListener(WebInspector.UISourceCodeProvider.Events.TemporaryUISourceCodeAdded, temporaryUISourceCodeAdded);
        InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.TemporaryUISourceCodeAdded, InspectorTest._defaultUISourceCodeProviderEventHandler);
        callback(event.data);
    }
}

InspectorTest.waitForWorkspaceTemporaryUISourceCodeRemovedEvent = function(callback)
{
    InspectorTest.testWorkspace.removeEventListener(WebInspector.UISourceCodeProvider.Events.TemporaryUISourceCodeRemoved, InspectorTest._defaultUISourceCodeProviderEventHandler);
    InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.TemporaryUISourceCodeRemoved, temporaryUISourceCodeRemoved);

    function temporaryUISourceCodeRemoved(event)
    {
        InspectorTest.testWorkspace.removeEventListener(WebInspector.UISourceCodeProvider.Events.TemporaryUISourceCodeRemoved, temporaryUISourceCodeRemoved);
        InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.TemporaryUISourceCodeRemoved, InspectorTest._defaultUISourceCodeProviderEventHandler);
        callback(event.data);
    }
}

InspectorTest.addMockUISourceCodeToWorkspace = function(url, type, content)
{
    var isDocument = type === WebInspector.resourceTypes.Document;
    var mockContentProvider = new WebInspector.StaticContentProvider(type, content);
    var uiSourceCode = new WebInspector.JavaScriptSource(url, mockContentProvider, !isDocument);
    InspectorTest.testWorkspace.project().addUISourceCode(uiSourceCode);
}

InspectorTest._defaultUISourceCodeProviderEventHandler = function(event)
{
    throw new Error("Unexpected UISourceCodeProvider event: " + event.type + ".");
}

InspectorTest.dumpUISourceCode = function(uiSourceCode, callback)
{
    var url = uiSourceCode.url.replace(/.*LayoutTests/, "LayoutTests");
    InspectorTest.addResult("UISourceCode: " + url);
    if (uiSourceCode instanceof WebInspector.JavaScriptSource) {
        InspectorTest.addResult("UISourceCode is editable: " + uiSourceCode.isEditable());
        InspectorTest.addResult("UISourceCode is content script: " + uiSourceCode.isContentScript);
    }
    uiSourceCode.requestContent(didRequestContent);

    function didRequestContent(content, contentEncoded, mimeType)
    {
        InspectorTest.addResult("Mime type: " + mimeType);
        InspectorTest.addResult("UISourceCode content: " + content);
        callback();
    }
}

};
