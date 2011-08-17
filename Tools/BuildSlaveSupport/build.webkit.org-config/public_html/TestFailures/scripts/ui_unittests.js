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

test("results.ResultsGrid", 1, function() {
    var grid = new ui.results.ResultsGrid()
    grid.addResults([
        'http://example.com/layout-test-results/foo-bar-diff.txt',
        'http://example.com/layout-test-results/foo-bar-expected.png',
        'http://example.com/layout-test-results/foo-bar-actual.png',
        'http://example.com/layout-test-results/foo-bar-diff.png',
    ]);
    equal(grid.innerHTML,
        '<table class="comparison">' +
            '<thead>' +
                '<tr>' +
                    '<th>Expected</th>' +
                    '<th>Actual</th>' +
                    '<th>Diff</th>' +
                '</tr>' +
            '</thead>' +
            '<tbody>' +
                '<tr>' +
                    '<td class="expected"><img class="image-result" src="http://example.com/layout-test-results/foo-bar-expected.png"></td>' +
                    '<td class="actual"><img class="image-result" src="http://example.com/layout-test-results/foo-bar-actual.png"></td>' +
                    '<td class="diff"><img class="image-result" src="http://example.com/layout-test-results/foo-bar-diff.png"></td>' +
                '</tr>' +
            '</tbody>' +
        '</table>' +
        '<table class="comparison">' +
            '<thead>' +
                '<tr>' +
                    '<th>Expected</th>' +
                    '<th>Actual</th>' +
                    '<th>Diff</th>' +
                '</tr>' +
            '</thead>' +
            '<tbody>' +
                '<tr>' +
                    '<td class="expected"></td>' +
                    '<td class="actual"></td>' +
                    '<td class="diff"><iframe class="text-result" src="http://example.com/layout-test-results/foo-bar-diff.txt"></iframe></td>' +
                '</tr>' +
            '</tbody>' +
        '</table>');
});

test("results.ResultsGrid (crashlog)", 1, function() {
    var grid = new ui.results.ResultsGrid()
    grid.addResults(['http://example.com/layout-test-results/foo-bar-crash-log.txt']);
    equal(grid.innerHTML, '<iframe class="text-result" src="http://example.com/layout-test-results/foo-bar-crash-log.txt"></iframe>');
});

test("results.ResultsGrid (empty)", 1, function() {
    var grid = new ui.results.ResultsGrid()
    grid.addResults([]);
    equal(grid.innerHTML, '');
});

test("summarizeFailure", 1, function() {
    var failureAnalysis = {
        "testName": "svg/dynamic-updates/SVGFETurbulenceElement-svgdom-baseFrequency-prop.html",
        "resultNodesByBuilder": {
            "Webkit Mac10.5 (CG)": {
                "expected": "IMAGE",
                "actual": "PASS"
            },
            "Webkit Mac10.5 (CG)(dbg)(2)": {
                "expected": "IMAGE",
                "actual":"PASS"
            }
        }
    }

    var failureSummary = ui.summarizeFailure(failureAnalysis);
    var failureInfoList = ui.failureInfoListForSummary(failureSummary);

    deepEqual(failureInfoList, [{
        "testName": "svg/dynamic-updates/SVGFETurbulenceElement-svgdom-baseFrequency-prop.html",
        "builderName": "Webkit Mac10.5 (CG)",
        "failureTypeList": [
            "PASS"
        ]
      }, {
        "testName": "svg/dynamic-updates/SVGFETurbulenceElement-svgdom-baseFrequency-prop.html",
        "builderName": "Webkit Mac10.5 (CG)(dbg)(2)",
        "failureTypeList": [
            "PASS"
        ]
      }
    ]);
});

})();
