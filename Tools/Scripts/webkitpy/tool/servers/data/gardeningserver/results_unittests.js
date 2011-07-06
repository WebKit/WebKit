module("results");

var kExampleResultsJSON = {
    "tests": {
        "scrollbars": {
            "custom-scrollbar-with-incomplete-style.html": {
                "expected": "IMAGE",
                "actual": "IMAGE"
            },
            "flaky-scrollbarhtml": {
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
}

test("BuilderResults.unexpectedFailures", 1, function() {
    var builderResults = new results.BuilderResults(kExampleResultsJSON);
    var unexpectedFailures = builderResults.unexpectedFailures();
    deepEqual(unexpectedFailures, {
        "userscripts/another-test.html": {
            "expected": "PASS",
            "actual": "TEXT"
        }
    });
});

test("unexpectedFailuresByTest", 1, function() {
    var builderResults = new results.BuilderResults(kExampleResultsJSON);
    var unexpectedFailuresByTest = results.unexpectedFailuresByTest({
        "Mock Builder": builderResults
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
