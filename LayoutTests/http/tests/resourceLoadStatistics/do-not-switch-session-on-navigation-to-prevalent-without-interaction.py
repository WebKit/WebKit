#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <script src="/js-test-resources/js-test.js"></script>
    <script src="resources/util.js"></script>
</head>
<body onload="runTest()">
<script>
    description("Tests that the session is not switched upon top frame navigation to a prevalent resource without user interaction.");
    jsTestIsAsync = true;

    const prevalentOrigin = "http://127.0.0.1:8000";
    const nonPrevalentOrigin = "http://localhost:8000";
    const sessionCookieName = "sessionCookie";
    const persistentCookieName = "persistentCookie";
    const twoMinutesInSeconds = 120;

    function setSessionCookie() {
        document.cookie = sessionCookieName + "=1; path=/";
    }

    function setPersistentCookie() {
        document.cookie = persistentCookieName + "=1; path=/; Max-Age=" + twoMinutesInSeconds + ";";
    }

    function checkCookies(shouldHaveSessionCookie, shouldHavePersistentCookie) {
        let hasSessionCookie = (document.cookie + "").includes(sessionCookieName),
            hasPersistentCookie = (document.cookie + "").includes(persistentCookieName);

        if (shouldHaveSessionCookie && hasSessionCookie)
            testPassed("Should have and has the session cookie.");
        else if (shouldHaveSessionCookie && !hasSessionCookie) {
            testFailed("Should have but doesn\'t have the session cookie.");
            setEnableFeature(false, finishJSTest);
        } else if (!shouldHaveSessionCookie && hasSessionCookie) {
            testFailed("Shouldn\'t have but has the session cookie.");
            setEnableFeature(false, finishJSTest);
        } else
            testPassed("Shouldn\'t have and doesn\'t have the session cookie.");


        if (shouldHavePersistentCookie && hasPersistentCookie)
            testPassed("Should have and has the persistent cookie.");
        else if (shouldHavePersistentCookie && !hasPersistentCookie) {
            testFailed("Should have but doesn\'t have the persistent cookie.");
            setEnableFeature(false, finishJSTest);
        } else if (!shouldHavePersistentCookie && hasPersistentCookie) {
            testFailed("Shouldn\'t have but has the persistent cookie.");
            setEnableFeature(false, finishJSTest);
        } else
            testPassed("Shouldn\'t have and doesn\'t have the persistent cookie.");
    }

    function runTest() {
        switch (document.location.hash) {
            case "":
                if (document.location.origin !== prevalentOrigin)
                    testFailed("Test is not starting out on " + prevalentOrigin + ".");

                setEnableFeature(true, function () {
                    if (testRunner.isStatisticsPrevalentResource(prevalentOrigin))
                        testFailed(prevalentOrigin + " was classified as prevalent resource before the test starts.");
                    document.location.hash = "step1";
                    runTest();
                });
            case "#step1":
                setSessionCookie();
                setPersistentCookie();
                checkCookies(true, true);
                if (testRunner.hasStatisticsIsolatedSession(prevalentOrigin)) {
                    testFailed("Origin has isolated session.");
                    setEnableFeature(false, finishJSTest);
                } else
                    testPassed("Origin has no isolated session.");
                document.location.href = nonPrevalentOrigin + "/resourceLoadStatistics/do-not-switch-session-on-navigation-to-prevalent-without-interaction.py#step2";
                break;
            case "#step2":
                document.location.hash = "step3";
                if (document.location.origin !== nonPrevalentOrigin)
                    testFailed("Step 2 is not on " + nonPrevalentOrigin + ".");
                testRunner.setStatisticsPrevalentResource(prevalentOrigin, true, function() {
                    if (!testRunner.isStatisticsPrevalentResource(prevalentOrigin)) {
                        testFailed(prevalentOrigin + " did not get set as prevalent resource.");
                        setEnableFeature(false, finishJSTest);
                    }
                    testRunner.statisticsUpdateCookieBlocking(runTest);
                });
                break;
            case "#step3":
                document.location.href = prevalentOrigin + "/resourceLoadStatistics/do-not-switch-session-on-navigation-to-prevalent-without-interaction.py#step4";
                break;
            case "#step4":
                checkCookies(true, true);
                if (testRunner.hasStatisticsIsolatedSession(prevalentOrigin))
                    testFailed("Origin has isolated session.");
                else
                    testPassed("Origin has no isolated session.");
                setEnableFeature(false, finishJSTest);
                break;
            default:
                testFailed("Unknown hash.");
                setEnableFeature(false, finishJSTest);
        }
    }
</script>
</body>
</html>''')