(function () {

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

test("summarizeTest", 3, function() {
    var testName = 'userscripts/another-test.html';
    var summary = ui.summarizeTest(testName, kExampleResultsByTest[testName]);
    var summaryHTML = summary.html();
    ok(summaryHTML.indexOf('scrollbars/custom-scrollbar-with-incomplete-style.html') == -1);
    ok(summaryHTML.indexOf('userscripts/another-test.html') != -1);
    ok(summaryHTML.indexOf('Mock Builder') != -1);
});

test("summarizeTest (data attributes)", 2, function() {
    var testName = 'userscripts/taco.html';
    var summary = ui.summarizeTest(testName, {
        "Mock Builder": {
            "expected": "PASS",
            "actual": "TEXT IMAGE+TEXT"
        }
    });
    equal(summary.attr(config.kTestNameAttr), "userscripts/taco.html");
    equal(summary.attr(config.kFailureTypesAttr), "TEXT IMAGE+TEXT");
});

test("summarizeRegressionRange", 3, function() {
    var summaryWithMultipleRevisions = ui.summarizeRegressionRange(0, 0);
    summaryWithMultipleRevisions.wrap('<wrapper></wrapper>');
    equal(summaryWithMultipleRevisions.parent().html(), '<div class="regression-range">Unknown</div>');

    var summaryWithMultipleRevisions = ui.summarizeRegressionRange(90424, 90426);
    summaryWithMultipleRevisions.wrap('<wrapper></wrapper>');
    equal(summaryWithMultipleRevisions.parent().html(), '<div class="regression-range"><a target="_blank" href="http://trac.webkit.org/log/trunk/?rev=90424&amp;stop_rev=90427&amp;limit=100&amp;verbose=on">r90427-r90424</a></div>');

    var summaryWithOneRevision = ui.summarizeRegressionRange(90425, 90426);
    summaryWithOneRevision.wrap('<wrapper></wrapper>');
    equal(summaryWithOneRevision.parent().html(), '<div class="regression-range"><a target="_blank" href="http://trac.webkit.org/log/trunk/?rev=90425&amp;stop_rev=90427&amp;limit=100&amp;verbose=on">r90427-r90425</a></div>');
});

test("failureCount", 4, function() {
    equal(ui.failureCount(0), '');
    equal(ui.failureCount(1), 'Seen once.');
    equal(ui.failureCount(2), 'Seen 2 times.');
    equal(ui.failureCount(3), 'Seen 3 times.');
});

test("alertMessageForCompileErrors", 1, function() {
    var message = ui.alertMessageForCompileErrors(['Mock Builder', 'Another Builder']);
    message.wrap('<wrapper></wrapper>');
    equal(message.parent().html(),
        '<div class="compile-errors">Build Failed:<ul>' +
            '<li><a target="_blank" href="http://build.chromium.org/p/chromium.webkit/waterfall?builder=Mock+Builder">Mock Builder</a></li>' +
            '<li><a target="_blank" href="http://build.chromium.org/p/chromium.webkit/waterfall?builder=Another+Builder">Another Builder</a></li>' +
        '</ul></div>');
});

test("failureDetailsStatus", 1, function() {
    var status = ui.failureDetailsStatus('userscripts/another-test.html', 'Mock Builder', 'TEXT', ['Mock Builder', 'Another Builder']);
    status.wrap('<wrapper></wrapper>');
    equal(status.parent().html(),
        '<span>' +
            '<span class="test-name TEXT">userscripts/another-test.html</span>' +
            '<span class="builder-list">' +
                '<span class="builder-name selected">Mock Builder</span>' +
                '<span class="builder-name">Another Builder</span>' +
            '</span>' +
        '</span>');
});

test("failureDetails", 1, function() {
    var testResults = ui.failureDetails([
        'http://example.com/layout-test-results/foo-bar-diff.txt',
        'http://example.com/layout-test-results/foo-bar-expected.png',
        'http://example.com/layout-test-results/foo-bar-actual.png',
        'http://example.com/layout-test-results/foo-bar-diff.png',
    ]);
    testResults.wrap('<wrapper></wrapper>');
    equal(testResults.parent().html(),
        '<table class="failure-details">' +
            '<tbody>' +
                '<tr>' +
                    '<td><iframe src="http://example.com/layout-test-results/foo-bar-diff.txt" class="diff"></iframe></td>' +
                    '<td><img src="http://example.com/layout-test-results/foo-bar-expected.png" class="expected"></td>' +
                    '<td><img src="http://example.com/layout-test-results/foo-bar-actual.png" class="actual"></td>' +
                    '<td><img src="http://example.com/layout-test-results/foo-bar-diff.png" class="diff"></td>' +
                '</tr>' +
            '</tbody>' +
        '</table>');
});

test("failureDetails (empty)", 1, function() {
    var testResults = ui.failureDetails([]);
    testResults.wrap('<wrapper></wrapper>');
    equal(testResults.parent().html(),
        '<table class="failure-details">' +
            '<tbody>' +
                '<tr>' +
                    '<td><div class="missing-data">No data</div></td>' +
                '</tr>' +
            '</tbody>' +
        '</table>');
});


})();
