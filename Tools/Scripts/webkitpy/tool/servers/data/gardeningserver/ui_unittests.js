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

test("BuilderResults.resultsByTest", 1, function() {
    var resultsByTest = ui.resultsByTest(kExampleResultsByTest);
    equal(resultsByTest.html(),
        '<div class="test">' +
            '<div class="testName">scrollbars/custom-scrollbar-with-incomplete-style.html</div>' +
                '<div class="builders">' +
                    '<div class="builder"><div class="builderName">Mock Builder</div><div class="actual">CRASH</div><div class="expected">IMAGE</div></div>' +
                    '<div class="builder"><div class="builderName">Mock Linux</div><div class="actual">CRASH</div><div class="expected">TEXT</div></div>' +
                '</div>' +
            '</div>' +
        '<div class="test">' +
            '<div class="testName">userscripts/another-test.html</div>' +
            '<div class="builders">' +
                '<div class="builder"><div class="builderName">Mock Builder</div><div class="actual">TEXT</div><div class="expected">PASS</div></div>' +
            '</div>' +
        '</div>');
});
