var initialize_DebuggerTest = function() {

InspectorTest.startDebuggerTest = function(callback, quiet)
{
    if (quiet !== undefined)
        InspectorTest._quiet = quiet;
    WebInspector.showPanel("scripts");

    if (WebInspector.panels.scripts._debuggerEnabled)
        startTest();
    else {
        InspectorTest.addSniffer(WebInspector.debuggerModel, "_debuggerWasEnabled", startTest);
        WebInspector.panels.scripts.toggleDebugging(false);
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

    WebInspector.debuggerModel.setBreakpointsActive(true);
    InspectorTest.resumeExecution(disableDebugger);

    function disableDebugger()
    {
        if (!scriptsPanel._debuggerEnabled)
            completeTest();
        else {
            InspectorTest.addSniffer(WebInspector.debuggerModel, "_debuggerWasDisabled", completeTest);
            scriptsPanel.toggleDebugging(false);
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

InspectorTest.captureStackTrace = function(callFrames, dropLineNumbers)
{
    InspectorTest.addResult("Call stack:");
    for (var i = 0; i < callFrames.length; i++) {
        var frame = callFrames[i];
        var script = WebInspector.debuggerModel.scriptForId(frame.location.scriptId);
        var url;
        var lineNumber;
        if (script) {
            url = WebInspector.displayNameForURL(script.sourceURL);
            lineNumber = frame.location.lineNumber + 1;
        } else {
            url = "(internal script)";
            lineNumber = "(line number)";
        }
        var s = "    " + i + ") " + frame.functionName + " (" + url + (dropLineNumbers ? "" : ":" + lineNumber) + ")";
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

InspectorTest._pausedScript = function(callFrames, reason, auxData)
{
    if (!InspectorTest._quiet)
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
    if (!InspectorTest._quiet)
        InspectorTest.addResult("Script execution resumed.");
    delete InspectorTest._callFrames;
    if (InspectorTest._waitUntilResumedCallback) {
        var callback = InspectorTest._waitUntilResumedCallback;
        delete InspectorTest._waitUntilResumedCallback;
        callback();
    }
};

InspectorTest.showScriptSourceOnScriptsPanel = function(panel, scriptName, callback)
{
    var uiSourceCodes = panel._uiSourceCodeProvider.uiSourceCodes();
    for (var i = 0; i < uiSourceCodes.length; ++i) {
        if (uiSourceCodes[i].parsedURL.lastPathComponent === scriptName) {
            panel.showUISourceCode(uiSourceCodes[i]);
            var sourceFrame = panel.visibleView;
            if (sourceFrame.loaded)
                callback(sourceFrame);
            else
                InspectorTest.addSniffer(sourceFrame, "onTextViewerContentLoaded", callback.bind(null, sourceFrame));
            return;
        }
    }
    InspectorTest.addSniffer(panel, "_addUISourceCode", InspectorTest.showScriptSource.bind(InspectorTest, scriptName, callback));
};

InspectorTest.showScriptSource = function(scriptName, callback)
{
    InspectorTest.showScriptSourceOnScriptsPanel(WebInspector.panels.scripts, scriptName, callback)
};

InspectorTest.dumpScriptsNavigator = function(navigator)
{
    InspectorTest.addResult("Dumping ScriptsNavigator 'Scripts' tab:");
    dumpNavigatorTreeOutline(navigator._scriptsView._scriptsTree);
    InspectorTest.addResult("Dumping ScriptsNavigator 'Content scripts' tab:");
    dumpNavigatorTreeOutline(navigator._contentScriptsView._scriptsTree);

    function dumpNavigatorTreeElement(prefix, treeElement)
    {
        InspectorTest.addResult(prefix + treeElement.titleText);
        for (var i = 0; i < treeElement.children.length; ++i)
            dumpNavigatorTreeElement(prefix + "  ", treeElement.children[i]);
    }

    function dumpNavigatorTreeOutline(treeOutline)
    {
        for (var i = 0; i < treeOutline.children.length; ++i)
            dumpNavigatorTreeElement("  ", treeOutline.children[i]);
    }
};

InspectorTest.dumpComboBoxFileSelector = function(comboBoxFileSelector)
{
    var rootURL = "http://localhost:8080/LayoutTests/inspector/debugger/";
    InspectorTest.addResult("Dumping ComboBoxFileSelector:");
    var select = comboBoxFileSelector._filesSelectElement;
    for (var i = 0; i < select.length; ++i) {
        var option = select[i];
        var text = option.text.replace(/.*LayoutTests/, "LayoutTests");
        text = text.replace(/\u00a0/g, " ").replace(/\u2026/g, "...");
        var tooltip = option.title.replace(rootURL, "<root>/");
        InspectorTest.addResult(text + (tooltip ? "(" + tooltip + ")" : ""));
    }
};

InspectorTest.setBreakpoint = function(sourceFrame, lineNumber, condition, enabled)
{
    sourceFrame._setBreakpoint(lineNumber, condition, enabled);
};

InspectorTest.removeBreakpoint = function(sourceFrame, lineNumber)
{
    sourceFrame._breakpointManager.findBreakpoint(sourceFrame._uiSourceCode, lineNumber).remove();
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

InspectorTest.setQuiet = function(quiet)
{
    InspectorTest._quiet = quiet;
};

InspectorTest.queryScripts = function(filter)
{
    var scripts = [];
    for (var scriptId in WebInspector.debuggerModel._scripts) {
        var script = WebInspector.debuggerModel._scripts[scriptId];
        if (filter(script))
            scripts.push(script);
    }
    return scripts;
};

InspectorTest.createScriptMock = function(url, startLine, startColumn, isContentScript, source)
{
    var scriptId = ++InspectorTest._lastScriptId;
    var lineCount = source.lineEndings().length;
    var endLine = startLine + lineCount - 1;
    var endColumn = lineCount === 1 ? startColumn + source.length : source.length - source.lineEndings()[lineCount - 2];
    var script = new WebInspector.Script(scriptId, url, startLine, startColumn, endLine, endColumn, isContentScript);
    script.requestContent = function(callback) { callback(source); };
    WebInspector.debuggerModel._registerScript(script);
    return script;
}

InspectorTest._lastScriptId = 0;

InspectorTest.checkRawLocation = function(script, lineNumber, columnNumber, location)
{
    InspectorTest.assertEquals(script.scriptId, location.scriptId);
    InspectorTest.assertEquals(lineNumber, location.lineNumber);
    InspectorTest.assertEquals(columnNumber, location.columnNumber);
};

InspectorTest.checkUILocation = function(uiSourceCode, lineNumber, columnNumber, location)
{
    InspectorTest.assertEquals(uiSourceCode, location.uiSourceCode);
    InspectorTest.assertEquals(lineNumber, location.lineNumber);
    InspectorTest.assertEquals(columnNumber, location.columnNumber);
};

};
