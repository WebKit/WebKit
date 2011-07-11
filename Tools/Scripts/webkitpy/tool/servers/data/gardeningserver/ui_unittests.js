module("iu");

var kExampleResultsByTest = {
    "scrollbars/custom-scrollbar-with-incomplete-style.html": {
        "Mock Builder": {
            "expected": "IMAGE",
            "actual": "CRASH"
        },
        "Mock Linux": {
            "expected": "TEXT",
            "actual": "CRASH"
        }
    },
    "userscripts/another-test.html": {
        "Mock Builder": {
            "expected": "PASS",
            "actual": "TEXT"
        }
    }
}

test("summarizeResultsByTest", 1, function() {
    var resultsSummary = ui.summarizeResultsByTest(kExampleResultsByTest);
    equal(resultsSummary.html(),
        '<div class="test">' +
            '<div class="testName">scrollbars/custom-scrollbar-with-incomplete-style.html</div>' +
             '<div class="builders">' +
                 '<div class="builder"><div class="builderName">Mock Builder</div><div class="actual">CRASH</div><div class="expected">IMAGE</div>' +
                     '<button class="show-results">Show Results</button><button class="regression-range">Regression Range</button></div>' +
                 '<div class="builder"><div class="builderName">Mock Linux</div><div class="actual">CRASH</div><div class="expected">TEXT</div>' +
                     '<button class="show-results">Show Results</button><button class="regression-range">Regression Range</button></div>' +
             '</div>' +
        '</div>' +
        '<div class="test">' +
            '<div class="testName">userscripts/another-test.html</div>' +
            '<div class="builders">' +
                '<div class="builder"><div class="builderName">Mock Builder</div><div class="actual">TEXT</div><div class="expected">PASS</div>' +
                    '<button class="show-results">Show Results</button><button class="regression-range">Regression Range</button></div>' +
            '</div>' +
        '</div>');
});

test("results", 1, function() {
    var testResults = ui.results([
        'http://example.com/layout-test-results/foo-bar-expected.txt',
        'http://example.com/layout-test-results/foo-bar-actual.txt',
        'http://example.com/layout-test-results/foo-bar-diff.txt',
        'http://example.com/layout-test-results/foo-bar-expected.png',
        'http://example.com/layout-test-results/foo-bar-actual.png',
        'http://example.com/layout-test-results/foo-bar-diff.png',
    ]);
    equal(testResults.html(),
        '<iframe src="http://example.com/layout-test-results/foo-bar-expected.txt" class="expected"></iframe>' +
        '<iframe src="http://example.com/layout-test-results/foo-bar-actual.txt" class="actual"></iframe>' +
        '<iframe src="http://example.com/layout-test-results/foo-bar-diff.txt" class="diff"></iframe>' +
        '<img src="http://example.com/layout-test-results/foo-bar-expected.png" class="expected">' +
        '<img src="http://example.com/layout-test-results/foo-bar-actual.png" class="actual">' +
        '<img src="http://example.com/layout-test-results/foo-bar-diff.png" class="diff">');
});
