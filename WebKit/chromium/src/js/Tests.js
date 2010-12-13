/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/**
 * @fileoverview This file contains small testing framework along with the
 * test suite for the frontend. These tests are a part of the continues build
 * and are executed by the devtools_sanity_unittest.cc as a part of the
 * Interactive UI Test suite.
 * FIXME: change field naming style to use trailing underscore.
 */

if (window.domAutomationController) {

var ___interactiveUiTestsMode = true;

/**
 * Test suite for interactive UI tests.
 * @constructor
 */
TestSuite = function()
{
    this.controlTaken_ = false;
    this.timerId_ = -1;
};


/**
 * Reports test failure.
 * @param {string} message Failure description.
 */
TestSuite.prototype.fail = function(message)
{
    if (this.controlTaken_)
        this.reportFailure_(message);
    else
        throw message;
};


/**
 * Equals assertion tests that expected === actual.
 * @param {Object} expected Expected object.
 * @param {Object} actual Actual object.
 * @param {string} opt_message User message to print if the test fails.
 */
TestSuite.prototype.assertEquals = function(expected, actual, opt_message)
{
    if (expected !== actual) {
        var message = "Expected: '" + expected + "', but was '" + actual + "'";
        if (opt_message)
            message = opt_message + "(" + message + ")";
        this.fail(message);
    }
};


/**
 * True assertion tests that value == true.
 * @param {Object} value Actual object.
 * @param {string} opt_message User message to print if the test fails.
 */
TestSuite.prototype.assertTrue = function(value, opt_message)
{
    this.assertEquals(true, !!value, opt_message);
};


/**
 * Contains assertion tests that string contains substring.
 * @param {string} string Outer.
 * @param {string} substring Inner.
 */
TestSuite.prototype.assertContains = function(string, substring)
{
    if (string.indexOf(substring) === -1)
        this.fail("Expected to: '" + string + "' to contain '" + substring + "'");
};


/**
 * Takes control over execution.
 */
TestSuite.prototype.takeControl = function()
{
    this.controlTaken_ = true;
    // Set up guard timer.
    var self = this;
    this.timerId_ = setTimeout(function() {
        self.reportFailure_("Timeout exceeded: 20 sec");
    }, 20000);
};


/**
 * Releases control over execution.
 */
TestSuite.prototype.releaseControl = function()
{
    if (this.timerId_ !== -1) {
        clearTimeout(this.timerId_);
        this.timerId_ = -1;
    }
    this.reportOk_();
};


/**
 * Async tests use this one to report that they are completed.
 */
TestSuite.prototype.reportOk_ = function()
{
    window.domAutomationController.send("[OK]");
};


/**
 * Async tests use this one to report failures.
 */
TestSuite.prototype.reportFailure_ = function(error)
{
    if (this.timerId_ !== -1) {
        clearTimeout(this.timerId_);
        this.timerId_ = -1;
    }
    window.domAutomationController.send("[FAILED] " + error);
};


/**
 * Runs all global functions starting with "test" as unit tests.
 */
TestSuite.prototype.runTest = function(testName)
{
    try {
        this[testName]();
        if (!this.controlTaken_)
            this.reportOk_();
    } catch (e) {
        this.reportFailure_(e);
    }
};


/**
 * @param {string} panelName Name of the panel to show.
 */
TestSuite.prototype.showPanel = function(panelName)
{
    // Open Scripts panel.
    var toolbar = document.getElementById("toolbar");
    var button = toolbar.getElementsByClassName(panelName)[0];
    button.click();
    this.assertEquals(WebInspector.panels[panelName], WebInspector.currentPanel);
};


/**
 * Overrides the method with specified name until it's called first time.
 * @param {Object} receiver An object whose method to override.
 * @param {string} methodName Name of the method to override.
 * @param {Function} override A function that should be called right after the
 *     overriden method returns.
 * @param {boolean} opt_sticky Whether restore original method after first run
 *     or not.
 */
TestSuite.prototype.addSniffer = function(receiver, methodName, override, opt_sticky)
{
    var orig = receiver[methodName];
    if (typeof orig !== "function")
        this.fail("Cannot find method to override: " + methodName);
    var test = this;
    receiver[methodName] = function(var_args) {
        try {
            var result = orig.apply(this, arguments);
        } finally {
            if (!opt_sticky)
                receiver[methodName] = orig;
        }
        // In case of exception the override won't be called.
        try {
            override.apply(this, arguments);
        } catch (e) {
            test.fail("Exception in overriden method '" + methodName + "': " + e);
        }
        return result;
    };
};


// UI Tests


/**
 * Tests that resources tab is enabled when corresponding item is selected.
 */
TestSuite.prototype.testEnableResourcesTab = function()
{
    this.showPanel("resources");

    var test = this;
    this.addSniffer(WebInspector, "updateResource",
        function(payload) {
            test.assertEquals("simple_page.html", payload.lastPathComponent);
            WebInspector.panels.resources.refresh();
            WebInspector.panels.resources.revealAndSelectItem(WebInspector.resources[payload.id]);

            test.releaseControl();
        });

    // Following call should lead to reload that we capture in the
    // addResource override.
    WebInspector.panels.resources._enableResourceTracking();

    // We now have some time to report results to controller.
    this.takeControl();
};


/**
 * Tests that profiler works.
 */
TestSuite.prototype.testProfilerTab = function()
{
    this.showPanel("profiles");

    var panel = WebInspector.panels.profiles;
    var test = this;

    function findDisplayedNode() {
        var node = panel.visibleView.profileDataGridTree.children[0];
        if (!node) {
            // Profile hadn't been queried yet, re-schedule.
            window.setTimeout(findDisplayedNode, 100);
            return;
        }

        // Iterate over displayed functions and search for a function
        // that is called "fib" or "eternal_fib". If found, this will mean
        // that we actually have profiled page's code.
        while (node) {
            if (node.functionName.indexOf("fib") !== -1)
                test.releaseControl();
            node = node.traverseNextNode(true, null, true);
        }

        test.fail();
    }

    function findVisibleView() {
        if (!panel.visibleView) {
            setTimeout(findVisibleView, 0);
            return;
        }
        setTimeout(findDisplayedNode, 0);
    }

    findVisibleView();
    this.takeControl();
};


/**
 * Tests that heap profiler works.
 */
TestSuite.prototype.testHeapProfiler = function()
{
    this.showPanel("profiles");

    var panel = WebInspector.panels.profiles;
    var test = this;

    function findDisplayedNode() {
        var node = panel.visibleView.dataGrid.children[0];
        if (!node) {
            // Profile hadn't been queried yet, re-schedule.
            window.setTimeout(findDisplayedNode, 100);
            return;
        }

        // Iterate over displayed functions and find node called "A"
        // If found, this will mean that we actually have taken heap snapshot.
        while (node) {
            if (node.constructorName.indexOf("A") !== -1) {
                test.releaseControl();
                return;
            }
            node = node.traverseNextNode(false, null, true);
        }

        test.fail();
    }

    function findVisibleView() {
        if (!panel.visibleView) {
            setTimeout(findVisibleView, 0);
            return;
        }
        setTimeout(findDisplayedNode, 0);
    }

    WebInspector.HeapSnapshotProfileType.prototype.buttonClicked();
    findVisibleView();
    this.takeControl();
};


/**
 * Tests that scripts tab can be open and populated with inspected scripts.
 */
TestSuite.prototype.testShowScriptsTab = function()
{
    this.showPanel("scripts");
    var test = this;
    // There should be at least main page script.
    this._waitUntilScriptsAreParsed(["debugger_test_page.html"],
        function() {
            test.releaseControl();
        });
    // Wait until all scripts are added to the debugger.
    this.takeControl();
};


/**
 * Tests that scripts tab is populated with inspected scripts even if it
 * hadn't been shown by the moment inspected paged refreshed.
 * @see http://crbug.com/26312
 */
TestSuite.prototype.testScriptsTabIsPopulatedOnInspectedPageRefresh = function()
{
    var test = this;
    this.assertEquals(WebInspector.panels.elements, WebInspector.currentPanel, "Elements panel should be current one.");

    this.addSniffer(WebInspector.panels.scripts, "reset", waitUntilScriptIsParsed);

    // Reload inspected page. It will reset the debugger agent.
    test.evaluateInConsole_(
        "window.location.reload(true);",
        function(resultText) {});

    function waitUntilScriptIsParsed() {
        test.showPanel("scripts");
        test._waitUntilScriptsAreParsed(["debugger_test_page.html"],
            function() {
                test.releaseControl();
            });
    }

    // Wait until all scripts are added to the debugger.
    this.takeControl();
};


/**
 * Tests that scripts list contains content scripts.
 */
TestSuite.prototype.testContentScriptIsPresent = function()
{
    this.showPanel("scripts");
    var test = this;

    test._waitUntilScriptsAreParsed(
        ["page_with_content_script.html", "simple_content_script.js"],
        function() {
          test.releaseControl();
        });

    // Wait until all scripts are added to the debugger.
    this.takeControl();
};


/**
 * Tests that scripts are not duplicaed on Scripts tab switch.
 */
TestSuite.prototype.testNoScriptDuplicatesOnPanelSwitch = function()
{
    var test = this;

    // There should be two scripts: one for the main page and another
    // one which is source of console API(see
    // InjectedScript._ensureCommandLineAPIInstalled).
    var expectedScriptsCount = 2;
    var parsedScripts = [];

    this.showPanel("scripts");


    function switchToElementsTab() {
        test.showPanel("elements");
        setTimeout(switchToScriptsTab, 0);
    }

    function switchToScriptsTab() {
        test.showPanel("scripts");
        setTimeout(checkScriptsPanel, 0);
    }

    function checkScriptsPanel() {
        test.assertTrue(!!WebInspector.panels.scripts.visibleView, "No visible script view.");
        test.assertTrue(test._scriptsAreParsed(["debugger_test_page.html"]), "Some scripts are missing.");
        checkNoDuplicates();
        test.releaseControl();
    }

    function checkNoDuplicates() {
        var scriptSelect = document.getElementById("scripts-files");
        var options = scriptSelect.options;
        for (var i = 0; i < options.length; i++) {
            var scriptName = options[i].text;
            for (var j = i + 1; j < options.length; j++)
                test.assertTrue(scriptName !== options[j].text, "Found script duplicates: " + test.optionsToString_(options));
        }
    }

    test._waitUntilScriptsAreParsed(
        ["debugger_test_page.html"],
        function() {
            checkNoDuplicates();
            setTimeout(switchToElementsTab, 0);
        });


    // Wait until all scripts are added to the debugger.
    this.takeControl();
};


// Tests that debugger works correctly if pause event occurs when DevTools
// frontend is being loaded.
TestSuite.prototype.testPauseWhenLoadingDevTools = function()
{
    this.showPanel("scripts");
    var test = this;

    var expectations = {
            functionsOnStack: ["callDebugger"],
            lineNumber: 8,
            lineText: "  debugger;"
        };


    // Script execution can already be paused.
    if (WebInspector.currentPanel.paused) {
        var callFrame = WebInspector.currentPanel.sidebarPanes.callstack.selectedCallFrame;
        this.assertEquals(expectations.functionsOnStack[0], callFrame.functionName);
        var callbackInvoked = false;
        this._checkSourceFrameWhenLoaded(expectations, function() {
                callbackInvoked = true;
                if (test.controlTaken_)
                    test.releaseControl();
            });
        if (!callbackInvoked) {
            test.takeControl();
        }
        return;
    }

    this._waitForScriptPause(
        {
            functionsOnStack: ["callDebugger"],
            lineNumber: 8,
            lineText: "  debugger;"
        },
        function() {
            test.releaseControl();
        });
    this.takeControl();
};


// Tests that pressing "Pause" will pause script execution if the script
// is already running.
TestSuite.prototype.testPauseWhenScriptIsRunning = function()
{
    this.showPanel("scripts");
    var test = this;

    test.evaluateInConsole_(
        'setTimeout("handleClick()" , 0)',
        function(resultText) {
          test.assertTrue(!isNaN(resultText), "Failed to get timer id: " + resultText);
          testScriptPauseAfterDelay();
        });

    // Wait for some time to make sure that inspected page is running the
    // infinite loop.
    function testScriptPauseAfterDelay() {
        setTimeout(testScriptPause, 300);
    }

    function testScriptPause() {
        // The script should be in infinite loop. Click "Pause" button to
        // pause it and wait for the result.
        WebInspector.panels.scripts.pauseButton.click();

        test._waitForScriptPause(
            {
                functionsOnStack: ["handleClick", ""],
                lineNumber: 5,
                lineText: "  while(true) {"
            },
            function() {
                test.releaseControl();
            });
    }

    this.takeControl();
};


/**
 * Serializes options collection to string.
 * @param {HTMLOptionsCollection} options
 * @return {string}
 */
TestSuite.prototype.optionsToString_ = function(options)
{
    var names = [];
    for (var i = 0; i < options.length; i++)
        names.push('"' + options[i].text + '"');
    return names.join(",");
};


/**
 * Ensures that main HTML resource is selected in Scripts panel and that its
 * source frame is setup. Invokes the callback when the condition is satisfied.
 * @param {HTMLOptionsCollection} options
 * @param {function(WebInspector.SourceView,string)} callback
 */
TestSuite.prototype.showMainPageScriptSource_ = function(scriptName, callback)
{
    var test = this;

    var scriptSelect = document.getElementById("scripts-files");
    var options = scriptSelect.options;

    test.assertTrue(options.length, "Scripts list is empty");

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
        test.assertTrue(-1 !== pageScriptIndex, "Script with url " + scriptName + " not found among " + test.optionsToString_(options));
        scriptResource = options[pageScriptIndex].representedObject;

        // Current panel is "Scripts".
        WebInspector.currentPanel._showScriptOrResource(scriptResource);
        test.assertEquals(pageScriptIndex, scriptSelect.selectedIndex, "Unexpected selected option index.");
    }

    test.assertTrue(scriptResource instanceof WebInspector.Resource,
                    "Unexpected resource class.");
    test.assertTrue(!!scriptResource.url, "Resource URL is null.");
    test.assertTrue(scriptResource.url.search(scriptName + "$") !== -1, "Main HTML resource should be selected.");

    var scriptsPanel = WebInspector.panels.scripts;

    var view = scriptsPanel.visibleView;
    test.assertTrue(view instanceof WebInspector.SourceView);

    if (!view.sourceFrame._loaded) {
        test.addSniffer(view, "_sourceFrameSetupFinished", function(event) {
            callback(view, scriptResource.url);
        });
    } else
        callback(view, scriptResource.url);
};


/*
 * Evaluates the code in the console as if user typed it manually and invokes
 * the callback when the result message is received and added to the console.
 * @param {string} code
 * @param {function(string)} callback
 */
TestSuite.prototype.evaluateInConsole_ = function(code, callback)
{
    WebInspector.showConsole();
    WebInspector.console.prompt.text = code;
    WebInspector.console.promptElement.dispatchEvent( TestSuite.createKeyEvent("Enter"));

    this.addSniffer(WebInspector.ConsoleView.prototype, "addMessage",
        function(commandResult) {
            callback(commandResult.toMessageElement().textContent);
        });
};


/**
 * Tests that console auto completion works when script execution is paused.
 */
TestSuite.prototype.testCompletionOnPause = function()
{
    this.showPanel("scripts");
    var test = this;
    this._executeCodeWhenScriptsAreParsed("handleClick()", ["completion_on_pause.html"]);

    this._waitForScriptPause(
        {
            functionsOnStack: ["innerFunction", "handleClick", ""],
            lineNumber: 9,
            lineText: "    debugger;"
        },
        showConsole);

    function showConsole() {
        if (WebInspector.currentFocusElement === WebInspector.console.promptElement)
            testLocalsCompletion();
        else {
            test.addSniffer(WebInspector.console, "afterShow", testLocalsCompletion);
            WebInspector.showConsole();
        }
    }

    function testLocalsCompletion() {
        checkCompletions("th", ["parameter1", "closureLocal", "p", "createClosureLocal"], testThisCompletion);
    }

    function testThisCompletion() {
        checkCompletions("this.", ["field1", "field2", "m"], testFieldCompletion);
    }

    function testFieldCompletion() {
        checkCompletions("this.field1.", ["id", "name"], function() { test.releaseControl(); });
    }

    function checkCompletions(expression, expectedProperties, callback) {
        test.addSniffer(WebInspector.console, "_reportCompletions",
            function(bestMatchOnly, completionsReadyCallback, dotNotation, bracketNotation, prefix, result, isException) {
                test.assertTrue(!isException, "Exception while collecting completions");
                for (var i = 0; i < expectedProperties.length; i++) {
                    var name = expectedProperties[i];
                    test.assertTrue(result[name], "Name " + name + " not found among the completions: " + JSON.stringify(result));
                }
                setTimeout(callback, 0);
            });
      WebInspector.console.prompt.text = expression;
      WebInspector.console.prompt.autoCompleteSoon();
    }

    this.takeControl();
};


/**
 * Checks current execution line against expectations.
 * @param {WebInspector.SourceFrame} sourceFrame
 * @param {number} lineNumber Expected line number
 * @param {string} lineContent Expected line text
 */
TestSuite.prototype._checkExecutionLine = function(sourceFrame, lineNumber, lineContent)
{
    this.assertEquals(lineNumber, sourceFrame.executionLine, "Unexpected execution line number.");
    this.assertEquals(lineContent, sourceFrame._textModel.line(lineNumber - 1), "Unexpected execution line text.");
}


/**
 * Checks that all expected scripts are present in the scripts list
 * in the Scripts panel.
 * @param {Array.<string>} expected Regular expressions describing
 *     expected script names.
 * @return {boolean} Whether all the scripts are in "scripts-files" select
 *     box
 */
TestSuite.prototype._scriptsAreParsed = function(expected)
{
    var scriptSelect = document.getElementById("scripts-files");
    var options = scriptSelect.options;

    // Check that at least all the expected scripts are present.
    var missing = expected.slice(0);
    for (var i = 0 ; i < options.length; i++) {
        for (var j = 0; j < missing.length; j++) {
            if (options[i].text.search(missing[j]) !== -1) {
                missing.splice(j, 1);
                break;
            }
        }
    }
    return missing.length === 0;
};


/**
 * Waits for script pause, checks expectations, and invokes the callback.
 * @param {Object} expectations  Dictionary of expectations
 * @param {function():void} callback
 */
TestSuite.prototype._waitForScriptPause = function(expectations, callback)
{
    var test = this;
    // Wait until script is paused.
    test.addSniffer(
        WebInspector,
        "pausedScript",
        function(details) {
            var callFrames = details.callFrames;
            var functionsOnStack = [];
            for (var i = 0; i < callFrames.length; i++)
                functionsOnStack.push(callFrames[i].functionName);

            test.assertEquals(expectations.functionsOnStack.join(","), functionsOnStack.join(","), "Unexpected stack.");

            // Check that execution line where the script is paused is
            // expected one.
            test._checkSourceFrameWhenLoaded(expectations, callback);
        });
};


/**
 * Waits for current source frame to load, checks expectations, and invokes
 * the callback.
 * @param {Object} expectations  Dictionary of expectations
 * @param {function():void} callback
 */
TestSuite.prototype._checkSourceFrameWhenLoaded = function(expectations, callback)
{
    var test = this;

    var frame = WebInspector.currentPanel.visibleView.sourceFrame;
    if (frame._loaded)
        checkExecLine();
    else {
        setTimeout(function() {
            test._checkSourceFrameWhenLoaded(expectations, callback);
        }, 100);
    }
    function checkExecLine() {
        test._checkExecutionLine(frame, expectations.lineNumber, expectations.lineText);
        callback();
    }
};


/**
 * Waits until all the scripts are parsed and asynchronously executes the code
 * in the inspected page.
 */
TestSuite.prototype._executeCodeWhenScriptsAreParsed = function(code, expectedScripts)
{
    var test = this;

    function executeFunctionInInspectedPage() {
        // Since breakpoints are ignored in evals' calculate() function is
        // execute after zero-timeout so that the breakpoint is hit.
        test.evaluateInConsole_(
            'setTimeout("' + code + '" , 0)',
            function(resultText) {
                test.assertTrue(!isNaN(resultText), "Failed to get timer id: " + resultText + ". Code: " + code);
            });
    }

    test._waitUntilScriptsAreParsed(expectedScripts, executeFunctionInInspectedPage);
};


/**
 * Waits until all the scripts are parsed and invokes the callback.
 */
TestSuite.prototype._waitUntilScriptsAreParsed = function(expectedScripts, callback)
{
    var test = this;

    function waitForAllScripts() {
        if (test._scriptsAreParsed(expectedScripts))
            callback();
        else
            test.addSniffer(WebInspector, "parsedScriptSource", waitForAllScripts);
    }

    waitForAllScripts();
};


/**
 * Executes the 'code' with InjectedScriptAccess.getProperties overriden
 * so that all callbacks passed to InjectedScriptAccess.getProperties are
 * extended with the "hook".
 * @param {Function} hook The hook function.
 * @param {Function} code A code snippet to be executed.
 */
TestSuite.prototype._hookGetPropertiesCallback = function(hook, code)
{
    var accessor = InjectedScriptAccess.prototype;
    var orig = accessor.getProperties;
    accessor.getProperties = function(objectProxy, ignoreHasOwnProperty, abbreviate, callback) {
        orig.call(this, objectProxy, ignoreHasOwnProperty, abbreviate,
            function() {
              callback.apply(this, arguments);
              hook();
            });
    };
    try {
        code();
    } finally {
        accessor.getProperties = orig;
    }
};


/**
 * Tests "Pause" button will pause debugger when a snippet is evaluated.
 */
TestSuite.prototype.testPauseInEval = function()
{
    this.showPanel("scripts");

    var test = this;

    var pauseButton = document.getElementById("scripts-pause");
    pauseButton.click();

    devtools.tools.evaluateJavaScript("fib(10)");

    this.addSniffer(WebInspector, "pausedScript",
        function() {
            test.releaseControl();
        });

    test.takeControl();
};


/**
 * Key event with given key identifier.
 */
TestSuite.createKeyEvent = function(keyIdentifier)
{
    var evt = document.createEvent("KeyboardEvent");
    evt.initKeyboardEvent("keydown", true /* can bubble */, true /* can cancel */, null /* view */, keyIdentifier, "");
    return evt;
};


/**
 * Test runner for the test suite.
 */
var uiTests = {};


/**
 * Run each test from the test suit on a fresh instance of the suite.
 */
uiTests.runAllTests = function()
{
    // For debugging purposes.
    for (var name in TestSuite.prototype) {
        if (name.substring(0, 4) === "test" && typeof TestSuite.prototype[name] === "function")
            uiTests.runTest(name);
    }
};


/**
 * Run specified test on a fresh instance of the test suite.
 * @param {string} name Name of a test method from TestSuite class.
 */
uiTests.runTest = function(name)
{
    if (uiTests._populatedInterface)
        new TestSuite().runTest(name);
    else
        uiTests._pendingTestName = name;
};

(function() {

function runTests()
{
    uiTests._populatedInterface = true;
    var name = uiTests._pendingTestName;
    delete uiTests._pendingTestName;
    if (name)
        new TestSuite().runTest(name);
}

var oldShowElementsPanel = WebInspector.showElementsPanel;
WebInspector.showElementsPanel = function()
{
    oldShowElementsPanel.call(this);
    runTests();
}

var oldShowPanel = WebInspector.showPanel;
WebInspector.showPanel = function(name)
{
    oldShowPanel.call(this, name);
    runTests();
}

})();

}
