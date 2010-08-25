var initialize_DebuggerTest = function() {

InspectorTest.startDebuggerTest = function(callback)
{
    if (WebInspector.panels.scripts._debuggerEnabled)
        startTest();
    else {
        InspectorTest._addSniffer(WebInspector, "debuggerWasEnabled", startTest);
        WebInspector.panels.scripts._toggleDebugging(false);
    }

    function startTest()
    {
        InspectorTest.addResult("Debugger was enabled.");
        callback();
    }
}

InspectorTest.completeDebuggerTest = function()
{
    var scriptsPanel = WebInspector.panels.scripts;

    if (!scriptsPanel.breakpointsActivated)
        scriptsPanel.toggleBreakpointsButton.element.click();

    InspectorTest.resumeExecution();

    if (!scriptsPanel._debuggerEnabled)
        completeTest();
    else {
        InspectorTest._addSniffer(WebInspector, "debuggerWasDisabled", completeTest);
        scriptsPanel._toggleDebugging(false);
    }

    function completeTest()
    {
        InspectorTest.addResult("Debugger was disabled.");
        InspectorTest.completeTest();
    }
};

InspectorTest.waitUntilPaused = function(callback)
{
    InspectorTest._addSniffer(WebInspector, "pausedScript", pausedScript);

    function pausedScript(callFrames)
    {
        InspectorTest.addResult("Paused at line " + callFrames[0].line + " in " + callFrames[0].functionName);
        callback(callFrames);
    }
};

InspectorTest.resumeExecution = function()
{
    if (WebInspector.panels.scripts.paused)
        WebInspector.panels.scripts._togglePause();
};

InspectorTest.showScriptSource = function(scriptName, callback)
{
    if (InspectorTest._scriptsAreParsed([scriptName]))
        InspectorTest._showScriptSource(scriptName, callback);
    else
        InspectorTest._addSniffer(WebInspector, "parsedScriptSource", InspectorTest.showScriptSource.bind(InspectorTest, scriptName, callback));
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

    var view = scriptsPanel.visibleView;
    callback = callback.bind(null, view);
    if (!view.sourceFrame._loaded)
        InspectorTest._addSniffer(view.sourceFrame, "setContent", callback);
    else
        callback();
};

}
