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

test("infobarMessageForCompileErrors", 1, function() {
    var message = ui.infobarMessageForCompileErrors(['Mock Builder', 'Another Builder']);
    message.wrap('<wrapper></wrapper>');
    equal(message.parent().html(),
        '<div class="compile-errors">Build Failed:<ul>' +
            '<li><a target="_blank" href="http://build.chromium.org/p/chromium.webkit/waterfall?builder=Mock+Builder">Mock Builder</a></li>' +
            '<li><a target="_blank" href="http://build.chromium.org/p/chromium.webkit/waterfall?builder=Another+Builder">Another Builder</a></li>' +
        '</ul></div>');
});

test("failureDetailsStatus", 1, function() {
    var status = ui.failureDetailsStatus({
        'builderName': 'Mock Builder',
        'testName': 'userscripts/another-test.html',
        'failureTypeList': ['TEXT'],
    }, ['Mock Builder', 'Another Builder']);
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
