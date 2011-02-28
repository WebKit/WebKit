var initialize_DebuggerTest = function() {

InspectorTest.startDebuggerTest = function(callback, quiet)
{
    InspectorTest._quiet = quiet;
    WebInspector.showPanel("scripts");

    if (WebInspector.panels.scripts._debuggerEnabled)
        startTest();
    else {
        InspectorTest.addSniffer(WebInspector.panels.scripts, "debuggerWasEnabled", startTest);
        WebInspector.panels.scripts._toggleDebugging(false);
    }

    function startTest()
    {
        InspectorTest.addResult("Debugger was enabled.");
        InspectorTest.addSniffer(WebInspector.debuggerModel, "_pausedScript", InspectorTest._pausedScript, true);
        InspectorTest.addSniffer(WebInspector.debuggerModel, "_resumedScript", InspectorTest._resumedScript, true);
        InspectorTest.safeWrap(callback)();
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
            InspectorTest.addSniffer(WebInspector.panels.scripts, "debuggerWasDisabled", completeTest);
            scriptsPanel._toggleDebugging(false);
        }
    }

    function completeTest()
    {
        InspectorTest.addResult("Debugger was disabled.");
        InspectorTest.completeTest();
    }
};

InspectorTest.runDebuggerTestSuite = function(testSuite)
{
    var testSuiteTests = testSuite.slice();

    function runner()
    {
        if (!testSuiteTests.length) {
            InspectorTest.completeDebuggerTest();
            return;
        }

        var nextTest = testSuiteTests.shift();
        InspectorTest.addResult("");
        InspectorTest.addResult("Running: " + /function\s([^(]*)/.exec(nextTest)[1]);
        InspectorTest.safeWrap(nextTest)(runner, runner);
    }

    InspectorTest.startDebuggerTest(runner);
}

InspectorTest.runTestFunctionAndWaitUntilPaused = function(callback)
{
    InspectorTest.evaluateInConsole("setTimeout(testFunction, 0)");
    InspectorTest.addResult("Set timer for test function.");
    InspectorTest.waitUntilPaused(callback);
};

InspectorTest.waitUntilPaused = function(callback)
{
    callback = InspectorTest.safeWrap(callback);

    if (InspectorTest._callFrames)
        callback(InspectorTest._callFrames);
    else
        InspectorTest._waitUntilPausedCallback = callback;
};

InspectorTest.waitUntilPausedAndDumpStack = function(callback)
{
    InspectorTest.waitUntilPaused(step1);

    function step1(callFrames)
    {
        InspectorTest.captureStackTrace(callFrames);
        InspectorTest.addResult(WebInspector.panels.scripts.sidebarPanes.callstack.bodyElement.lastChild.innerText);
        InspectorTest.resumeExecution(InspectorTest.safeWrap(callback));
    }
};

InspectorTest.waitUntilResumed = function(callback)
{
    callback = InspectorTest.safeWrap(callback);

    if (!InspectorTest._callFrames)
        callback();
    else
        InspectorTest._waitUntilResumedCallback = callback;
};

InspectorTest.resumeExecution = function(callback)
{
    if (WebInspector.panels.scripts.paused)
        WebInspector.panels.scripts._togglePause();
    InspectorTest.waitUntilResumed(callback);
};

InspectorTest.captureStackTrace = function(callFrames)
{
    InspectorTest.addResult("Call stack:");
    for (var i = 0; i < callFrames.length; i++) {
        var frame = callFrames[i];
        var script = WebInspector.debuggerModel.scriptForSourceID(frame.sourceID);
        var scriptOrResource = script.resource || script;
        var url = scriptOrResource && WebInspector.displayNameForURL(scriptOrResource.sourceURL || scriptOrResource.url);
        var s = "    " + i + ") " + frame.functionName + " (" + url + ":" + (frame.line + 1) + ")";
        InspectorTest.addResult(s);
    }
};

InspectorTest.dumpSourceFrameContents = function(sourceFrame)
{
    InspectorTest.addResult("==Source frame contents start==");
    var textModel = sourceFrame._textModel;
    for (var i = 0; i < textModel.linesCount; ++i)
        InspectorTest.addResult(textModel.line(i));
    InspectorTest.addResult("==Source frame contents end==");
};

InspectorTest._pausedScript = function(details)
{
    if (!InspectorTest._quiet)
        InspectorTest.addResult("Script execution paused.");
    InspectorTest._callFrames = details.callFrames;
    if (InspectorTest._waitUntilPausedCallback) {
        var callback = InspectorTest._waitUntilPausedCallback;
        delete InspectorTest._waitUntilPausedCallback;
        callback(InspectorTest._callFrames);
    }
}

InspectorTest._resumedScript = function()
{
    if (!InspectorTest._quiet)
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
    var filesSelect = document.getElementById("scripts-files");
    for (var i = 0; i < filesSelect.length; ++i) {
        if (filesSelect[i].text === scriptName) {
            filesSelect.selectedIndex = i;
            WebInspector.panels.scripts._filesSelectChanged();
            var sourceFrame = WebInspector.panels.scripts.visibleView;
            if (sourceFrame.loaded)
                callback(sourceFrame);
            else
                sourceFrame.addEventListener(WebInspector.SourceFrame.Events.Loaded, callback.bind(null, sourceFrame));
            return;
        }
    }
    InspectorTest.addSniffer(WebInspector.panels.scripts, "_addOptionToFilesSelect", InspectorTest.showScriptSource.bind(InspectorTest, scriptName, callback));
};

InspectorTest.expandProperties = function(properties, callback)
{
    var index = 0;
    function expandNextPath()
    {
        if (index === properties.length) {
            InspectorTest.safeWrap(callback)();
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
