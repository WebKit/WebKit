
function frontend_ensureDebuggerEnabled(callback)
{
    if (WebInspector.panels.scripts._debuggerEnabled) {
        callback();
        return;
    }

    frontend_addSniffer(WebInspector, "debuggerWasEnabled", callback);
    WebInspector.panels.scripts._toggleDebugging(false);
}

function frontend_ensureDebuggerDisabled(callback)
{
    if (!WebInspector.panels.scripts._debuggerEnabled) {
        callback();
        return;
    }

    frontend_addSniffer(WebInspector, "debuggerWasDisabled", callback);
    WebInspector.panels.scripts._toggleDebugging(false);
}

function frontend_completeDebuggerTest(testController)
{
    if (WebInspector.panels.scripts.paused) {
        WebInspector.panels.scripts._togglePause();
        testController.results.push("Resumed script execution.");
    }
    frontend_ensureDebuggerDisabled(function()
    {
        testController.results.push("Disabled debugger.");
        testController.notifyDone();
    });
}

function frontend_scriptsAreParsed(scripts)
{
    var scriptSelect = document.getElementById("scripts-files");
    var options = scriptSelect.options;

    // Check that all the expected scripts are present.
    for (var i = 0; i < scripts.length; i++) {
        var found = false;
        for (var j = 0; j < options.length; j++) {
            if (options[j].text.search(scripts[i]) !== -1) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
};

function frontend_waitUntilScriptsAreParsed(scripts, callback)
{
    function waitForAllScripts()
    {
        if (frontend_scriptsAreParsed(scripts))
            callback();
        else
            frontend_addSniffer(WebInspector, "parsedScriptSource", waitForAllScripts);
    }
    waitForAllScripts();
};

function frontend_ensureSourceFrameLoaded(sourceFrame, callback)
{
    if (!sourceFrame._loaded)
        frontend_addSniffer(sourceFrame, "setContent", callback);
    else
        callback();
}

function frontend_showScriptSource(scriptName, callback)
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
    frontend_ensureSourceFrameLoaded(view.sourceFrame, callback.bind(null, view));
};

function frontend_captureStackTrace(callFrames, testController)
{
    testController.results.push("Call stack:");
    for (var i = 0; i < callFrames.length; i++) {
        var frame = callFrames[i];
        var scriptOrResource = WebInspector.panels.scripts._sourceIDMap[frame.sourceID];
        var url = scriptOrResource && WebInspector.displayNameForURL(scriptOrResource.sourceURL || scriptOrResource.url);
        var s = "    " + i + ") " + frame.functionName + " (" + url + ":" + frame.line + ")";
        testController.results.push(s);
    }
}
