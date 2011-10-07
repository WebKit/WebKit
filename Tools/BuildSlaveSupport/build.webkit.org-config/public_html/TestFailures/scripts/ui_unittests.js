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
                    '<td class="expected result-container"><img class="image-result" src="http://example.com/layout-test-results/foo-bar-expected.png"></td>' +
                    '<td class="actual result-container"><img class="image-result" src="http://example.com/layout-test-results/foo-bar-actual.png"></td>' +
                    '<td class="diff result-container"><img class="image-result" src="http://example.com/layout-test-results/foo-bar-diff.png"></td>' +
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
                    '<td class="expected result-container"></td>' +
                    '<td class="actual result-container"></td>' +
                    '<td class="diff result-container"><iframe class="text-result" src="http://example.com/layout-test-results/foo-bar-diff.txt"></iframe></td>' +
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

test("time", 6, function() {
    var time = new ui.RelativeTime();
    equal(time.tagName, 'TIME');
    equal(time.className, 'relative');
    deepEqual(Object.getOwnPropertyNames(time.__proto__).sort(), [
        'date',
        'init',
        'setDate',
        'update',
    ]);
    equal(time.outerHTML, '<time class="relative"></time>');
    var tenMinutesAgo = new Date();
    tenMinutesAgo.setMinutes(tenMinutesAgo.getMinutes() - 10);
    time.setDate(tenMinutesAgo);
    equal(time.outerHTML, '<time class="relative">10 minutes ago</time>');
    equal(time.date().getTime(), tenMinutesAgo.getTime());
});

test("MessageBox", 1, function() {
    var messageBox = new ui.MessageBox('The Title', 'First message');
    messageBox.addMessage('Second Message');
    equal(messageBox.outerHTML,
        '<div class="ui-dialog-content ui-widget-content" style="width: auto; min-height: 132px; height: auto; " scrolltop="0" scrollleft="0">' +
            '<div>' +
                '<div class="message">First message</div>' +
                '<div class="message">Second Message</div>' +
            '</div>' +
        '</div>');
    messageBox.close();
});

})();
