var initialize_DebuggerTest = function() {

InspectorTest.startDebuggerTest = function(callback)
{
    WebInspector.showPanel("scripts");

    if (WebInspector.panels.scripts._debuggerEnabled)
        startTest();
    else {
        InspectorTest._addSniffer(WebInspector, "debuggerWasEnabled", startTest);
        WebInspector.panels.scripts._toggleDebugging(false);
    }

    function startTest()
    {
        InspectorTest.addResult("Debugger was enabled.");
        InspectorTest._addSniffer(WebInspector, "pausedScript", InspectorTest._pausedScript, true);
        InspectorTest._addSniffer(WebInspector, "resumedScript", InspectorTest._resumedScript, true);
        callback();
    }
};

InspectorTest.completeDebuggerTest = function()
{
    var scriptsPanel = WebInspector.panels.scripts;

    if (!scriptsPanel.breakpointsActivated)
        scriptsPanel.toggleBreakpointsButton.element.click();

    InspectorTest.resumeExecution(disableDebugger);

    function disableDebugger()
    {
        if (!scriptsPanel._debuggerEnabled)
            completeTest();
        else {
            InspectorTest._addSniffer(WebInspector, "debuggerWasDisabled", completeTest);
            scriptsPanel._toggleDebugging(false);
        }
    }

    function completeTest()
    {
        InspectorTest.addResult("Debugger was disabled.");
        InspectorTest.completeTest();
    }
};

InspectorTest.runTestFunctionAndWaitUntilPaused = function(callback)
{
    InspectorTest.evaluateInConsole("setTimeout(testFunction, 0)");
    InspectorTest.addResult("Set timer for test function.");
    InspectorTest.waitUntilPaused(callback);
};

InspectorTest.waitUntilPaused = function(callback)
{
    if (InspectorTest._callFrames)
        callback(InspectorTest._callFrames);
    else
        InspectorTest._waitUntilPausedCallback = callback;
};

InspectorTest.waitUntilResumed = function(callback)
{
    if (!InspectorTest._callFrames)
        callback();
    else
        InspectorTest._waitUntilResumedCallback = callback;
};

InspectorTest.resumeExecution = function(callback)
{
    if (WebInspector.panels.scripts.paused)
        WebInspector.panels.scripts._togglePause();
    if (callback)
        InspectorTest.waitUntilResumed(callback);
};

InspectorTest.captureStackTrace = function(callFrames)
{
    InspectorTest.addResult("Call stack:");
    for (var i = 0; i < callFrames.length; i++) {
        var frame = callFrames[i];
        var scriptOrResource = WebInspector.panels.scripts._sourceIDMap[frame.sourceID];
        var url = scriptOrResource && WebInspector.displayNameForURL(scriptOrResource.sourceURL || scriptOrResource.url);
        var s = "    " + i + ") " + frame.functionName + " (" + url + ":" + frame.line + ")";
        InspectorTest.addResult(s);
    }
};

InspectorTest._pausedScript = function(callFrames)
{
    InspectorTest.addResult("Script execution paused.");
    InspectorTest._callFrames = callFrames;
    if (InspectorTest._waitUntilPausedCallback) {
        var callback = InspectorTest._waitUntilPausedCallback;
        delete InspectorTest._waitUntilPausedCallback;
        callback(InspectorTest._callFrames);
    }
}

InspectorTest._resumedScript = function()
{
    InspectorTest.addResult("Script execution resumed.");
    delete InspectorTest._callFrames;
    if (InspectorTest._waitUntilResumedCallback) {
        var callback = InspectorTest._waitUntilResumedCallback;
        delete InspectorTest._waitUntilResumedCallback;
        callback();
    }
};

InspectorTest.showScriptSource = function(scriptName, callback)
{
    if (InspectorTest._scriptsAreParsed([scriptName]))
        InspectorTest._showScriptSource(scriptName, callback);
    else
        InspectorTest._addSniffer(WebInspector, "parsedScriptSource", InspectorTest.showScriptSource.bind(InspectorTest, scriptName, callback));
};

InspectorTest.waitUntilCurrentSourceFrameIsLoaded = function(callback)
{
    var sourceFrame = WebInspector.currentPanel.visibleView.sourceFrame;
    if (sourceFrame._loaded)
        callback(sourceFrame);
    else
        InspectorTest._addSniffer(sourceFrame, "setContent", callback.bind(null, sourceFrame));
};

InspectorTest._scriptsAreParsed = function(scripts)
{
    var scriptSelect = document.getElementById("scripts-files");
    var options = scriptSelect.options;

    // Check that all the expected scripts are present.
    for (var i = 0; i < scripts.length; i++) {
        var found = false;
        for (var j = 0; j < options.length; j++) {
            if (options[j].text === scripts[i]) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
};

InspectorTest._showScriptSource = function(scriptName, callback)
{
    var scriptSelect = document.getElementById("scripts-files");
    var options = scriptSelect.options;
    var scriptsPanel = WebInspector.panels.scripts;

    // Select page's script if it's not current option.
    var scriptResource;
    if (options[scriptSelect.selectedIndex].text === scriptName)
        scriptResource = options[scriptSelect.selectedIndex].representedObject;
    else {
        var pageScriptIndex = -1;
        for (var i = 0; i < options.length; i++) {
            if (options[i].text === scriptName) {
                pageScriptIndex = i;
                break;
            }
        }
        scriptResource = options[pageScriptIndex].representedObject;
        scriptsPanel._showScriptOrResource(scriptResource);
    }

    InspectorTest.waitUntilCurrentSourceFrameIsLoaded(callback);
};

InspectorTest.expandProperties = function(properties, callback)
{
    var index = 0;
    function expandNextPath()
    {
        if (index === properties.length) {
            callback();
            return;
        }
        var parentTreeElement = properties[index++];
        var path = properties[index++];
        InspectorTest._expandProperty(parentTreeElement, path, 0, expandNextPath);
    }
    InspectorTest.runAfterPendingDispatches(expandNextPath);
};

InspectorTest._expandProperty = function(parentTreeElement, path, pathIndex, callback)
{
    if (pathIndex === path.length) {
        InspectorTest.addResult("Expanded property: " + path.join("."));
        callback();
        return;
    }
    var name = path[pathIndex++];
    var propertyTreeElement = InspectorTest._findChildPropertyTreeElement(parentTreeElement, name);
    if (!propertyTreeElement) {
       InspectorTest.addResult("Failed to expand property: " + path.slice(0, pathIndex).join("."));
       InspectorTest.completeDebuggerTest();
       return;
    }
    propertyTreeElement.expand();
    InspectorTest.runAfterPendingDispatches(InspectorTest._expandProperty.bind(InspectorTest, propertyTreeElement, path, pathIndex, callback));
};

InspectorTest._findChildPropertyTreeElement = function(parent, childName)
{
    var children = parent.children;
    for (var i = 0; i < children.length; i++) {
        var treeElement = children[i];
        var property = treeElement.property;
        if (property.name === childName)
            return treeElement;
    }
};

};
