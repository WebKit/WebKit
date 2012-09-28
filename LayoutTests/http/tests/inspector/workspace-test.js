var initialize_WorkspaceTest = function() {

InspectorTest.testWorkspace;
InspectorTest.createWorkspace = function()
{
    InspectorTest.testWorkspace = new WebInspector.Workspace();
    InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, InspectorTest._defaultUISourceCodeProviderEventHandler);
    InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeReplaced, InspectorTest._defaultUISourceCodeProviderEventHandler);
    InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeRemoved, InspectorTest._defaultUISourceCodeProviderEventHandler);
}

InspectorTest.waitForWorkspaceUISourceCodeReplacedEvent = function(callback)
{
    InspectorTest.testWorkspace.removeEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeReplaced, InspectorTest._defaultUISourceCodeProviderEventHandler);
    InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeReplaced, uiSourceCodeReplaced);

    function uiSourceCodeReplaced(event)
    {
        InspectorTest.testWorkspace.removeEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeReplaced, uiSourceCodeReplaced);
        InspectorTest.testWorkspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeReplaced, InspectorTest._defaultUISourceCodeProviderEventHandler);
        callback(event.data.uiSourceCode, event.data.oldUISourceCode);
    }
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
