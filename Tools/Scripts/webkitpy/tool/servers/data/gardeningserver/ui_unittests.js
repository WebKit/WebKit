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

test("summarizeResultsByTest", 3, function() {
    var resultsSummary = ui.summarizeResultsByTest(kExampleResultsByTest);
    var resultsSummaryHTML = resultsSummary.html();
    ok(resultsSummaryHTML.indexOf('scrollbars/custom-scrollbar-with-incomplete-style.html') != -1);
    ok(resultsSummaryHTML.indexOf('userscripts/another-test.html') != -1);
    ok(resultsSummaryHTML.indexOf('Mock Builder') != -1);
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
