var initialize_ProfilerTest = function() {

InspectorTest.startProfilerTest = function(callback)
{
    WebInspector.showPanel("profiles");

    function startTest()
    {
        InspectorTest.addResult("Profiler was enabled.");
        InspectorTest.addSniffer(WebInspector.panels.profiles, "addProfileHeader", InspectorTest._profileHeaderAdded, true);
        InspectorTest.addSniffer(WebInspector.CPUProfileView.prototype, "refresh", InspectorTest._profileViewRefresh, true);
        InspectorTest.safeWrap(callback)();
    }

    if (WebInspector.panels.profiles._profilerEnabled)
        startTest();
    else {
        InspectorTest.addSniffer(WebInspector.panels.profiles, "_profilerWasEnabled", startTest);
        WebInspector.panels.profiles._toggleProfiling(false);
    }
};

InspectorTest.completeProfilerTest = function()
{
    function completeTest()
    {
        InspectorTest.addResult("");
        InspectorTest.addResult("Profiler was disabled.");
        InspectorTest.completeTest();
    }

    var profilesPanel = WebInspector.panels.profiles;
    if (!profilesPanel._profilerEnabled)
        completeTest();
    else {
        InspectorTest.addSniffer(WebInspector.panels.profiles, "_profilerWasDisabled", completeTest);
        profilesPanel._toggleProfiling(false);
    }
};

InspectorTest.runProfilerTestSuite = function(testSuite)
{
    var testSuiteTests = testSuite.slice();

    function runner()
    {
        if (!testSuiteTests.length) {
            InspectorTest.completeProfilerTest();
            return;
        }

        var nextTest = testSuiteTests.shift();
        InspectorTest.addResult("");
        InspectorTest.addResult("Running: " + /function\s([^(]*)/.exec(nextTest)[1]);
        InspectorTest.safeWrap(nextTest)(runner, runner);
    }

    InspectorTest.startProfilerTest(runner);
};

InspectorTest.showProfileWhenAdded = function(title)
{
    InspectorTest._showProfileWhenAdded = title;
};

InspectorTest._profileHeaderAdded = function(profile)
{
    if (InspectorTest._showProfileWhenAdded === profile.title) {
        WebInspector.panels.profiles.showProfile(profile);
    }
};

InspectorTest.waitUntilProfileViewIsShown = function(title, callback)
{
    callback = InspectorTest.safeWrap(callback);

    var profilesPanel = WebInspector.panels.profiles;
    if (profilesPanel.visibleView && profilesPanel.visibleView.profile && profilesPanel.visibleView.profile.title === title)
        callback(profilesPanel.visibleView);
    else
        InspectorTest._waitUntilProfileViewIsShownCallback = { title: title, callback: callback };
}

InspectorTest._profileViewRefresh = function()
{
    // Called in the context of ProfileView.
    if (InspectorTest._waitUntilProfileViewIsShownCallback && InspectorTest._waitUntilProfileViewIsShownCallback.title === this.profile.title) {
        var callback = InspectorTest._waitUntilProfileViewIsShownCallback;
        delete InspectorTest._waitUntilProfileViewIsShownCallback;
        callback.callback(this);
    }
};

};
