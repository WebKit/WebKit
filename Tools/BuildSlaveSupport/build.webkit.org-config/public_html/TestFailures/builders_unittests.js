/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

(function () {

module("builders");

var kExampleBuilderStatusJSON =  {
    "Webkit Linux": {
        "basedir": "Webkit_Linux",
        "cachedBuilds": [11459, 11460, 11461, 11462],
        "category": "6webkit linux latest",
        "currentBuilds": [11462],
        "pendingBuilds": 0,
        "slaves": ["vm124-m1"],
        "state": "building"
    },
    "Webkit Mac10.6 (CG)": {
        "basedir": "Webkit_Linux",
        "cachedBuilds": [11459, 11460, 11461, 11462],
        "category": "6webkit linux latest",
        "currentBuilds": [11461, 11462],
        "pendingBuilds": 0,
        "slaves": ["vm124-m1"],
        "state": "building"
    },
};

var kExampleBuildInfoJSON = {
    "blame": ["abarth@webkit.org"],
    "builderName": "Webkit Linux",
    "changes": ["Files:\n Tools/BuildSlaveSupport/build.webkit.org-config/public_html/TestFailures/main.js\n Tools/ChangeLog\nAt: Thu 04 Aug 2011 00:50:38\nChanged By: abarth@webkit.org\nComments: Fix types.  Sadly, main.js has no test coverage.  (I need to think\nabout how to test this part of the code.)\n\n* BuildSlaveSupport/build.webkit.org-config/public_html/TestFailures/main.js:Properties: \n\n\n", "Files:\n LayoutTests/ChangeLog\n LayoutTests/platform/chromium-mac/fast/box-shadow/inset-box-shadows-expected.png\n LayoutTests/platform/chromium-mac/fast/repaint/shadow-multiple-horizontal-expected.png\n LayoutTests/platform/chromium-mac/fast/repaint/shadow-multiple-strict-horizontal-expected.png\n LayoutTests/platform/chromium-mac/fast/repaint/shadow-multiple-strict-vertical-expected.png\n LayoutTests/platform/chromium-mac/fast/repaint/shadow-multiple-vertical-expected.png\nAt: Thu 04 Aug 2011 00:50:38\nChanged By: abarth@webkit.org\nComments: Update baselines after <http://trac.webkit.org/changeset/92340>.\n\n* platform/chromium-mac/fast/box-shadow/inset-box-shadows-expected.png:\n* platform/chromium-mac/fast/repaint/shadow-multiple-horizontal-expected.png:\n* platform/chromium-mac/fast/repaint/shadow-multiple-strict-horizontal-expected.png:\n* platform/chromium-mac/fast/repaint/shadow-multiple-strict-vertical-expected.png:\n* platform/chromium-mac/fast/repaint/shadow-multiple-vertical-expected.png:Properties: \n\n\n"],
    "currentStep": null,
    "eta": null,
    "logs": [
        ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/update_scripts/logs/stdio"],
        ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/update/logs/stdio"],
        ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/compile/logs/stdio"],
        ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/test_shell_tests/logs/stdio"],
        ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/webkit_unit_tests/logs/stdio"],
        ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/webkit_tests/logs/stdio"],
        ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/archive_webkit_tests_results/logs/stdio"],
        ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/webkit_gpu_tests/logs/stdio"],
        ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/archive_webkit_tests_gpu_results/logs/stdio"]
    ],
    "number": 11461,
    "properties": [
        ["blamelist", ["abarth@webkit.org"], "Build"],
        ["branch", "trunk", "Build"],
        ["buildername", "Webkit Linux", "Build"],
        ["buildnumber", 11461, "Build"],
        ["got_revision", "95395", "Source"],
        ["got_webkit_revision", "92358", "Source"],
        ["gtest_filter", null, "Factory"],
        ["mastername", "chromium.webkit", "master.cfg"],
        ["revision", "92358", "Build"],
        ["scheduler", "s6_webkit_rel", "Scheduler"],
        ["slavename", "vm124-m1", "BuildSlave"]
    ],
    "reason": "",
    "requests": [{
        "builderName": "Webkit Linux",
        "builds": [11461],
        "source": {
            "branch": "trunk",
            "changes": [{
                "branch": "trunk",
                "category": null,
                "comments": "Fix types.  Sadly, main.js has no test coverage.  (I need to think\nabout how to test this part of the code.)\n\n* BuildSlaveSupport/build.webkit.org-config/public_html/TestFailures/main.js:",
                "files": ["Tools/BuildSlaveSupport/build.webkit.org-config/public_html/TestFailures/main.js", "Tools/ChangeLog"],
                "number": 43707,
                "properties": [],
                "repository": "",
                "revision": "92357",
                "revlink": "http://trac.webkit.org/changeset/92357",
                "when": 1312444238.855081,
                "who": "abarth@webkit.org"
            }, {
                "branch": "trunk",
                "category": null,
                "comments": "Update baselines after <http://trac.webkit.org/changeset/92340>.\n\n* platform/chromium-mac/fast/box-shadow/inset-box-shadows-expected.png:\n* platform/chromium-mac/fast/repaint/shadow-multiple-horizontal-expected.png:\n* platform/chromium-mac/fast/repaint/shadow-multiple-strict-horizontal-expected.png:\n* platform/chromium-mac/fast/repaint/shadow-multiple-strict-vertical-expected.png:\n* platform/chromium-mac/fast/repaint/shadow-multiple-vertical-expected.png:",
                "files": ["LayoutTests/ChangeLog", "LayoutTests/platform/chromium-mac/fast/box-shadow/inset-box-shadows-expected.png", "LayoutTests/platform/chromium-mac/fast/repaint/shadow-multiple-horizontal-expected.png", "LayoutTests/platform/chromium-mac/fast/repaint/shadow-multiple-strict-horizontal-expected.png", "LayoutTests/platform/chromium-mac/fast/repaint/shadow-multiple-strict-vertical-expected.png", "LayoutTests/platform/chromium-mac/fast/repaint/shadow-multiple-vertical-expected.png"],
                "number": 43708,
                "properties": [],
                "repository": "",
                "revision": "92358",
                "revlink": "http://trac.webkit.org/changeset/92358",
                "when": 1312444238.855538,
                "who": "abarth@webkit.org"
            }],
            "hasPatch": false,
            "revision": "92358"
        },
        "submittedAt": 1312444298.989818
    }],
    "results": 2,
    "slave": "vm124-m1",
    "sourceStamp": {
        "branch": "trunk",
        "changes": [{
            "branch": "trunk",
            "category": null,
            "comments": "Fix types.  Sadly, main.js has no test coverage.  (I need to think\nabout how to test this part of the code.)\n\n* BuildSlaveSupport/build.webkit.org-config/public_html/TestFailures/main.js:",
            "files": ["Tools/BuildSlaveSupport/build.webkit.org-config/public_html/TestFailures/main.js", "Tools/ChangeLog"],
            "number": 43707,
            "properties": [],
            "repository": "",
            "revision": "92357",
            "revlink": "http://trac.webkit.org/changeset/92357",
            "when": 1312444238.855081,
            "who": "abarth@webkit.org"
        }, {
            "branch": "trunk",
            "category": null,
            "comments": "Update baselines after <http://trac.webkit.org/changeset/92340>.\n\n* platform/chromium-mac/fast/box-shadow/inset-box-shadows-expected.png:\n* platform/chromium-mac/fast/repaint/shadow-multiple-horizontal-expected.png:\n* platform/chromium-mac/fast/repaint/shadow-multiple-strict-horizontal-expected.png:\n* platform/chromium-mac/fast/repaint/shadow-multiple-strict-vertical-expected.png:\n* platform/chromium-mac/fast/repaint/shadow-multiple-vertical-expected.png:",
            "files": ["LayoutTests/ChangeLog", "LayoutTests/platform/chromium-mac/fast/box-shadow/inset-box-shadows-expected.png", "LayoutTests/platform/chromium-mac/fast/repaint/shadow-multiple-horizontal-expected.png", "LayoutTests/platform/chromium-mac/fast/repaint/shadow-multiple-strict-horizontal-expected.png", "LayoutTests/platform/chromium-mac/fast/repaint/shadow-multiple-strict-vertical-expected.png", "LayoutTests/platform/chromium-mac/fast/repaint/shadow-multiple-vertical-expected.png"],
            "number": 43708,
            "properties": [],
            "repository": "",
            "revision": "92358",
            "revlink": "http://trac.webkit.org/changeset/92358",
            "when": 1312444238.855538,
            "who": "abarth@webkit.org"
        }],
        "hasPatch": false,
        "revision": "92358"
    },
    "steps": [{
        "eta": null,
        "expectations": [
            ["output", 2297, 2300.6571014083784]
        ],
        "isFinished": true,
        "isStarted": true,
        "logs": [
            ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/update_scripts/logs/stdio"]
        ],
        "name": "update_scripts",
        "results": [0, []],
        "statistics": {},
        "step_number": 0,
        "text": ["update_scripts"],
        "times": [1312444299.102834, 1312444309.270324],
        "urls": {}
    }, {
        "eta": null,
        "expectations": [
            ["output", 20453, 17580.5002389645]
        ],
        "isFinished": true,
        "isStarted": true,
        "logs": [
            ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/update/logs/stdio"]
        ],
        "name": "update",
        "results": [0, []],
        "statistics": {},
        "step_number": 1,
        "text": ["update", "r95395", "webkit r92358"],
        "times": [1312444309.270763, 1312444398.468139],
        "urls": {}
    }, {
        "eta": null,
        "expectations": [
            ["output", 17316, 2652.4850499589456]
        ],
        "isFinished": true,
        "isStarted": true,
        "logs": [
            ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/compile/logs/stdio"]
        ],
        "name": "compile",
        "results": [0, []],
        "statistics": {},
        "step_number": 2,
        "text": ["compile"],
        "times": [1312444398.46855, 1312444441.68838],
        "urls": {}
    }, {
        "eta": null,
        "expectations": [
            ["output", 91980, 92002.12628325251]
        ],
        "isFinished": true,
        "isStarted": true,
        "logs": [
            ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/test_shell_tests/logs/stdio"]
        ],
        "name": "test_shell_tests",
        "results": [0, []],
        "statistics": {},
        "step_number": 3,
        "text": ["test_shell_tests", "1 disabled", "2 flaky"],
        "times": [1312444441.688756, 1312444451.74908],
        "urls": {}
    }, {
        "eta": null,
        "expectations": [
            ["output", 20772, 20772.638503443086]
        ],
        "isFinished": true,
        "isStarted": true,
        "logs": [
            ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/webkit_unit_tests/logs/stdio"]
        ],
        "name": "webkit_unit_tests",
        "results": [0, []],
        "statistics": {},
        "step_number": 4,
        "text": ["webkit_unit_tests", "1 disabled"],
        "times": [1312444451.749574, 1312444452.306143],
        "urls": {}
    }, {
        "eta": null,
        "expectations": [
            ["output", 2477406, 2498915.6146275494]
        ],
        "isFinished": true,
        "isStarted": true,
        "logs": [
            ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/webkit_tests/logs/stdio"]
        ],
        "name": "webkit_tests",
        "results": [2, ["webkit_tests"]],
        "statistics": {},
        "step_number": 5,
        "text": ["webkit_tests", "2014 fixable", "(370 skipped)", "failed 1", "<div class=\"BuildResultInfo\">", "<a href=\"http://test-results.appspot.com/dashboards/flakiness_dashboard.html#master=ChromiumWebkit&tests=fast/box-shadow/box-shadow-clipped-slices.html\">", "<abbr title=\"fast/box-shadow/box-shadow-clipped-slices.html\">box-shadow-clipped-slices.html</abbr>", "</a>", "</div>"],
        "times": [1312444452.306695, 1312444768.888266],
        "urls": {}
    }, {
        "eta": null,
        "expectations": [
            ["output", 2668845, 2672746.372363254]
        ],
        "isFinished": true,
        "isStarted": true,
        "logs": [
            ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/archive_webkit_tests_results/logs/stdio"]
        ],
        "name": "archive_webkit_tests_results",
        "results": [0, []],
        "statistics": {},
        "step_number": 6,
        "text": ["archived webkit_tests results"],
        "times": [1312444768.888746, 1312444781.444399],
        "urls": {
            "layout test results": "http://build.chromium.org/buildbot/layout_test_results/Webkit_Linux\r/95395\rNone"
        }
    }, {
        "eta": null,
        "expectations": [
            ["output", 210958, 208138.4965182993]
        ],
        "isFinished": true,
        "isStarted": true,
        "logs": [
            ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/webkit_gpu_tests/logs/stdio"]
        ],
        "name": "webkit_gpu_tests",
        "results": [2, ["webkit_gpu_tests"]],
        "statistics": {},
        "step_number": 7,
        "text": ["webkit_gpu_tests", "148 fixable", "(24 skipped)", "failed 1", "<div class=\"BuildResultInfo\">", "<a href=\"http://test-results.appspot.com/dashboards/flakiness_dashboard.html#master=ChromiumWebkit&tests=compositing/scaling/tiled-layer-recursion.html\">", "<abbr title=\"compositing/scaling/tiled-layer-recursion.html\">tiled-layer-recursion.html</abbr>", "</a>", "</div>"],
        "times": [1312444781.444903, 1312444966.856074],
        "urls": {}
    }, {
        "eta": null,
        "expectations": [
            ["output", 148825, 147822.1074277072]
        ],
        "isFinished": true,
        "isStarted": true,
        "logs": [
            ["stdio", "http://build.chromium.org/p/chromium.webkitbuilders/Webkit%20Linux/builds/11461/steps/archive_webkit_tests_gpu_results/logs/stdio"]
        ],
        "name": "archive_webkit_tests_gpu_results",
        "results": [0, []],
        "statistics": {},
        "step_number": 8,
        "text": ["archived webkit_tests gpu results"],
        "times": [1312444966.856575, 1312444970.458655],
        "urls": {
            "layout test gpu results": "http://build.chromium.org/buildbot/layout_test_results/Webkit_Linux_-_GPU\r/95395\rNone"
        }
    }],
    "text": ["failed", "webkit_tests", "webkit_gpu_tests"],
    "times": [1312444299.10216, 1312444970.459138]
};

test("buildersFailingStepRequredForTestCoverage", 3, function() {
    var simulator = new NetworkSimulator();

    var failingBuildInfoJSON = JSON.parse(JSON.stringify(kExampleBuildInfoJSON));
    failingBuildInfoJSON.number = 11460;
    failingBuildInfoJSON.steps[2].results[0] = 1;

    var requestedURLs = [];
    simulator.get = function(url, callback)
    {
        requestedURLs.push(url);
        simulator.scheduleCallback(function() {
            if (/\/json\/builders$/.exec(url))
                callback(kExampleBuilderStatusJSON);
            else if (/Webkit%20Linux/.exec(url))
                callback(kExampleBuildInfoJSON);
            else if (/Webkit%20Mac10\.6%20\(CG\)/.exec(url))
                callback(failingBuildInfoJSON);
            else {
                ok(false, "Unexpected URL: " + url);
                callback();
            }
        });
    };

    simulator.runTest(function() {
        builders.buildersFailingStepRequredForTestCoverage(function(builderNameList) {
            deepEqual(builderNameList, ["Webkit Mac10.6 (CG)"]);
        });
    });

    deepEqual(requestedURLs, [
      "http://build.chromium.org/p/chromium.webkit/json/builders",
      "http://build.chromium.org/p/chromium.webkit/json/builders/Webkit%20Linux/builds/11461",
      "http://build.chromium.org/p/chromium.webkit/json/builders/Webkit%20Mac10.6%20(CG)/builds/11460",
    ]);
});

})();
