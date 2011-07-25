(function () {

module("results");

var kExampleResultsJSON = {
    "tests": {
        "scrollbars": {
            "custom-scrollbar-with-incomplete-style.html": {
                "expected": "IMAGE",
                "actual": "IMAGE"
            },
            "flaky-scrollbar.html": {
                "expected": "PASS",
                "actual": "PASS TEXT"
            }
        },
        "userscripts": {
            "user-script-video-document.html": {
                "expected": "FAIL",
                "actual": "TEXT"
            },
            "another-test.html": {
                "expected": "PASS",
                "actual": "TEXT"
            }
        },
    },
    "skipped": 339,
    "num_regressions": 14,
    "interrupted": false,
    "layout_tests_dir": "\/mnt\/data\/b\/build\/slave\/Webkit_Linux\/build\/src\/third_party\/WebKit\/LayoutTests",
    "version": 3,
    "num_passes": 15566,
    "has_pretty_patch": false,
    "fixable": 1233,
    "num_flaky":1,
    "uses_expectations_file": true,
    "has_wdiff": true,
    "revision": "90430"
};

test("unexpectedFailures", 1, function() {
    var unexpectedFailures = results.unexpectedFailures(kExampleResultsJSON);
    deepEqual(unexpectedFailures, {
        "userscripts/another-test.html": {
            "expected": "PASS",
            "actual": "TEXT"
        }
    });
});

test("unexpectedFailuresByTest", 1, function() {
    var unexpectedFailuresByTest = results.unexpectedFailuresByTest({
        "Mock Builder": kExampleResultsJSON
    });
    deepEqual(unexpectedFailuresByTest, {
        "userscripts/another-test.html": {
            "Mock Builder": {
                "expected": "PASS",
                "actual": "TEXT"
            }
        }
    });
});

test("resultKind", 12, function() {
    equals(results.resultKind("http://example.com/foo-actual.txt"), "actual");
    equals(results.resultKind("http://example.com/foo-expected.txt"), "expected");
    equals(results.resultKind("http://example.com/foo-diff.txt"), "diff");
    equals(results.resultKind("http://example.com/foo.bar-actual.txt"), "actual");
    equals(results.resultKind("http://example.com/foo.bar-expected.txt"), "expected");
    equals(results.resultKind("http://example.com/foo.bar-diff.txt"), "diff");
    equals(results.resultKind("http://example.com/foo-actual.png"), "actual");
    equals(results.resultKind("http://example.com/foo-expected.png"), "expected");
    equals(results.resultKind("http://example.com/foo-diff.png"), "diff");
    equals(results.resultKind("http://example.com/foo-pretty-diff.html"), "diff");
    equals(results.resultKind("http://example.com/foo-wdiff.html"), "diff");
    equals(results.resultKind("http://example.com/foo-xyz.html"), "unknown");
});

test("resultType", 12, function() {
    equals(results.resultType("http://example.com/foo-actual.txt"), "text");
    equals(results.resultType("http://example.com/foo-expected.txt"), "text");
    equals(results.resultType("http://example.com/foo-diff.txt"), "text");
    equals(results.resultType("http://example.com/foo.bar-actual.txt"), "text");
    equals(results.resultType("http://example.com/foo.bar-expected.txt"), "text");
    equals(results.resultType("http://example.com/foo.bar-diff.txt"), "text");
    equals(results.resultType("http://example.com/foo-actual.png"), "image");
    equals(results.resultType("http://example.com/foo-expected.png"), "image");
    equals(results.resultType("http://example.com/foo-diff.png"), "image");
    equals(results.resultType("http://example.com/foo-pretty-diff.html"), "text");
    equals(results.resultType("http://example.com/foo-wdiff.html"), "text");
    equals(results.resultType("http://example.com/foo.xyz"), "text");
});

test("resultNodeForTest", 4, function() {
    deepEqual(results.resultNodeForTest(kExampleResultsJSON, "userscripts/another-test.html"), {
        "expected": "PASS",
        "actual": "TEXT"
    });
    equals(results.resultNodeForTest(kExampleResultsJSON, "foo.html"), null);
    equals(results.resultNodeForTest(kExampleResultsJSON, "userscripts/foo.html"), null);
    equals(results.resultNodeForTest(kExampleResultsJSON, "userscripts/foo/bar.html"), null);
});

function NetworkSimulator()
{
    this._pendingCallbacks = [];
};

NetworkSimulator.prototype.scheduleCallback = function(callback)
{
    this._pendingCallbacks.push(callback);
}

NetworkSimulator.prototype.runTest = function(testCase)
{
    var self = this;
    var realBase = window.base;

    window.base = {};
    base.endsWith = realBase.endsWith;
    base.trimExtension = realBase.trimExtension;
    base.uniquifyArray = realBase.uniquifyArray;
    if (self.probeHook)
        base.probe = self.probeHook;
    if (self.jsonpHook)
        base.jsonp = self.jsonpHook;

    testCase();

    while (this._pendingCallbacks.length) {
        var callback = this._pendingCallbacks.shift();
        callback();
    }

    window.base = realBase;
    equal(window.base, realBase, "Failed to restore real base!");
}

test("walkHistory", 6, function() {
    var simulator = new NetworkSimulator();

    var keyMap = {
        "agx0ZXN0LXJlc3VsdHNyEAsSCFRlc3RGaWxlGLncUAw": {
            "tests": {
                "userscripts": {
                    "another-test.html": {
                        "expected": "PASS",
                        "actual": "TEXT"
                    }
                },
            },
            "revision": "90430"
        },
        "agx0ZXN0LXJlc3VsdHNyEAsSCFRlc3RGaWxlGNfTUAw":{
            "tests": {
                "userscripts": {
                    "user-script-video-document.html": {
                        "expected": "FAIL",
                        "actual": "TEXT"
                    },
                    "another-test.html": {
                        "expected": "PASS",
                        "actual": "TEXT"
                    }
                },
            },
            "revision": "90429"
        },
        "agx0ZXN0LXJlc3VsdHNyEAsSCFRlc3RGaWxlGJWCUQw":{
            "tests": {
                "userscripts": {
                    "another-test.html": {
                        "expected": "PASS",
                        "actual": "TEXT"
                    }
                },
            },
            "revision": "90426"
        },
        "agx0ZXN0LXJlc3VsdHNyEAsSCFRlc3RGaWxlGKbLUAw":{
            "tests": {
                "userscripts": {
                    "user-script-video-document.html": {
                        "expected": "FAIL",
                        "actual": "TEXT"
                    },
                },
            },
            "revision": "90424"
        },
        "abc":{
            "tests": {
                "userscripts": {
                    "another-test.html": {
                        "expected": "PASS",
                        "actual": "TEXT"
                    }
                },
            },
            "revision": "90426"
        },
        "xyz":{
            "tests": {
            },
            "revision": "90425"
        }
    };

    simulator.jsonpHook = function(url, callback) {
        simulator.scheduleCallback(function() {
            if (/dir=1/.test(url)) {
                if (/builder=Mock/.test(url)) {
                    callback([
                        { "key": "agx0ZXN0LXJlc3VsdHNyEAsSCFRlc3RGaWxlGLncUAw" },
                        { "key": "agx0ZXN0LXJlc3VsdHNyEAsSCFRlc3RGaWxlGNfTUAw" },
                        { "key": "agx0ZXN0LXJlc3VsdHNyEAsSCFRlc3RGaWxlGJWCUQw" },
                        { "key": "agx0ZXN0LXJlc3VsdHNyEAsSCFRlc3RGaWxlGKbLUAw" },
                        { "key": "agx0ZXN0LXJlc3VsdHNyEAsSCFRlc3RGaWxlGOj5UAw" },
                        { "key": "agx0ZXN0LXJlc3VsdHNyEAsSCFRlc3RGaWxlGP-AUQw" },
                        { "key": "agx0ZXN0LXJlc3VsdHNyEAsSCFRlc3RGaWxlGPL3UAw" },
                        { "key": "agx0ZXN0LXJlc3VsdHNyEAsSCFRlc3RGaWxlGNHJQAw" },
                    ]);
                } else if (/builder=Another/.test(url)) {
                    callback([
                        { "key": "abc" },
                        { "key": "xyz" },
                    ]);
                } else {
                    ok(false, 'Unexpected URL: ' + url);
                }
            } else {
                var key = url.match(/key=([^&]+)/)[1];
                callback(keyMap[key]);
            }
        });
    };
    simulator.runTest(function() {
        results.regressionRangeForFailure("Mock Builder", "userscripts/another-test.html", function(oldestFailingRevision, newestPassingRevision) {
            equals(oldestFailingRevision, 90426);
            equals(newestPassingRevision, 90424);
        });

        results.unifyRegressionRanges(["Mock Builder", "Another Builder"], "userscripts/another-test.html", function(oldestFailingRevision, newestPassingRevision) {
            equals(oldestFailingRevision, 90426);
            equals(newestPassingRevision, 90425);
        });

        results.countFailureOccurances(["Mock Builder", "Another Builder"], "userscripts/another-test.html", function(failureCount) {
            equals(failureCount, 4);
        });
    });
});

test("walkHistory (no revision)", 3, function() {
    var simulator = new NetworkSimulator();

    var keyMap = {
        "vsfdsfdsafsdafasd": {
            "tests": {
                "userscripts": {
                    "another-test.html": {
                        "expected": "PASS",
                        "actual": "TEXT"
                    }
                },
            },
            "revision": ""
        },
        "gavsavsrfgwaevwefawvae":{
            "tests": {
            },
            "revision": ""
        },
    };

    simulator.jsonpHook = function(url, callback) {
        simulator.scheduleCallback(function() {
            if (/dir=1/.test(url)) {
                callback([
                    { "key": "vsfdsfdsafsdafasd" },
                    { "key": "gavsavsrfgwaevwefawvae" },
                ]);
            } else {
                var key = url.match(/key=([^&]+)/)[1];
                callback(keyMap[key]);
            }
        });
    };

    simulator.runTest(function() {
        results.regressionRangeForFailure("Mock Builder", "userscripts/another-test.html", function(oldestFailingRevision, newestPassingRevision) {
            equals(oldestFailingRevision, 0);
            equals(newestPassingRevision, 0);
        });
    });
});

test("collectUnexpectedResults", 1, function() {
    var dictionaryOfResultNodes = {
        "foo": {
            "expected": "IMAGE",
            "actual": "IMAGE"
        },
        "bar": {
            "expected": "PASS",
            "actual": "PASS TEXT"
        },
        "baz": {
            "expected": "TEXT",
            "actual": "IMAGE"
        },
        "qux": {
            "expected": "PASS",
            "actual": "TEXT"
        },
        "taco": {
            "expected": "PASS",
            "actual": "TEXT"
        },
    };

    var collectedResults = results.collectUnexpectedResults(dictionaryOfResultNodes);
    deepEqual(collectedResults, ["TEXT", "IMAGE"]);
});

test("failureTypeToExtensionList", 5, function() {
    deepEqual(results.failureTypeToExtensionList('TEXT'), ['txt']);
    deepEqual(results.failureTypeToExtensionList('IMAGE+TEXT'), ['txt', 'png']);
    deepEqual(results.failureTypeToExtensionList('IMAGE'), ['png']);
    deepEqual(results.failureTypeToExtensionList('CRASH'), []);
    deepEqual(results.failureTypeToExtensionList('TIMEOUT'), []);
});

test("canRebaseline", 6, function() {
    ok(results.canRebaseline(['TEXT']));
    ok(results.canRebaseline(['IMAGE+TEXT', 'CRASH']));
    ok(results.canRebaseline(['IMAGE']));
    ok(!results.canRebaseline(['CRASH']));
    ok(!results.canRebaseline(['TIMEOUT']));
    ok(!results.canRebaseline([]));
});

test("fetchResultsURLs", 5, function() {
    var simulator = new NetworkSimulator();

    var probedURLs = [];
    simulator.probeHook = function(url, options)
    {
        simulator.scheduleCallback(function() {
            probedURLs.push(url);
            if (base.endsWith(url, '.txt'))
                options.success.call();
            else if (/taco.+png$/.test(url))
                options.success.call();
            else
                options.error.call();
        });
    };

    simulator.runTest(function() {
        results.fetchResultsURLs("Mock Builder", "userscripts/another-test.html", ['IMAGE', 'CRASH'], function(resultURLs) {
            deepEqual(resultURLs, [
                "http://build.chromium.org/f/chromium/layout_test_results/Mock_Builder/results/layout-test-results/userscripts/another-test-crash-log.txt"
            ]);
        });
        results.fetchResultsURLs("Mock Builder", "userscripts/another-test.html", ['TIMEOUT'], function(resultURLs) {
            deepEqual(resultURLs, []);
        });
        results.fetchResultsURLs("Mock Builder", "userscripts/taco.html", ['IMAGE', 'IMAGE+TEXT'], function(resultURLs) {
            deepEqual(resultURLs, [
                "http://build.chromium.org/f/chromium/layout_test_results/Mock_Builder/results/layout-test-results/userscripts/taco-expected.png",
                "http://build.chromium.org/f/chromium/layout_test_results/Mock_Builder/results/layout-test-results/userscripts/taco-actual.png",
                "http://build.chromium.org/f/chromium/layout_test_results/Mock_Builder/results/layout-test-results/userscripts/taco-diff.png",
                "http://build.chromium.org/f/chromium/layout_test_results/Mock_Builder/results/layout-test-results/userscripts/taco-diff.txt"
            ]);
        });
    });

    deepEqual(probedURLs, [
        "http://build.chromium.org/f/chromium/layout_test_results/Mock_Builder/results/layout-test-results/userscripts/another-test-expected.png",
        "http://build.chromium.org/f/chromium/layout_test_results/Mock_Builder/results/layout-test-results/userscripts/another-test-actual.png",
        "http://build.chromium.org/f/chromium/layout_test_results/Mock_Builder/results/layout-test-results/userscripts/another-test-diff.png",
        "http://build.chromium.org/f/chromium/layout_test_results/Mock_Builder/results/layout-test-results/userscripts/another-test-crash-log.txt",
        "http://build.chromium.org/f/chromium/layout_test_results/Mock_Builder/results/layout-test-results/userscripts/taco-expected.png",
        "http://build.chromium.org/f/chromium/layout_test_results/Mock_Builder/results/layout-test-results/userscripts/taco-actual.png",
        "http://build.chromium.org/f/chromium/layout_test_results/Mock_Builder/results/layout-test-results/userscripts/taco-diff.png",
        "http://build.chromium.org/f/chromium/layout_test_results/Mock_Builder/results/layout-test-results/userscripts/taco-diff.txt"
    ]);
});

})();
