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

module("iu.results");

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

test("View", 6, function() {
    var resultsView = new ui.results.View({
        fetchResultsURLs: $.noop
    });
    var controller = new controllers.ResultsDetails(resultsView, kExampleResultsByTest);
    controller.showTest("scrollbars/custom-scrollbar-with-incomplete-style.html");
    equal(resultsView.currentTestName(), "scrollbars/custom-scrollbar-with-incomplete-style.html");
    equal(resultsView.currentBuilderName(), "Mock Builder");
    resultsView.showResults({
        "testName": "userscripts/another-test.html",
        "builderName": "Mock Builder",
    });
    equal(resultsView.currentTestName(), "userscripts/another-test.html");
    equal(resultsView.currentBuilderName(), "Mock Builder");
    resultsView.showResults({
        "testName": "scrollbars/custom-scrollbar-with-incomplete-style.html",
        "builderName": "Mock Linux",
    });
    equal(resultsView.currentTestName(), "scrollbars/custom-scrollbar-with-incomplete-style.html");
    equal(resultsView.currentBuilderName(), "Mock Linux");
});

test("ResultsSelector", 1, function() {
    var resultsSelector = new ui.results.ResultsSelector();
    resultsSelector.setResultsByTest(kExampleResultsByTest);
    equal($(resultsSelector).wrap('<div>').parent().html(),
        '<table class="results-selector">' +
            '<thead>' +
                '<tr><td></td><td>Mock Builder</td><td>Mock Linux</td></tr>' +
            '</thead>' +
            '<tbody>' +
                '<tr>' +
                    '<td class="test-name">userscripts/another-test.html</td>' +
                    '<td class="result">TEXT</td>' +
                    '<td></td>' +
                '</tr>' +
                '<tr>' +
                    '<td class="test-name">scrollbars/custom-scrollbar-with-incomplete-style.html</td>' +
                    '<td class="result">CRASH</td>' +
                    '<td class="result">CRASH</td>' +
                '</tr>' +
            '</tbody>' +
        '</table>');
});


})();
