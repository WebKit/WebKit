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

module('ui.notifications');

test('Notification', 5, function() {
    var notification = new ui.notifications.Notification();
    equal(notification.tagName, 'LI');
    equal(notification.innerHTML, '<div class="how"></div><div class="what"></div>');
    equal(notification.index(), 0);
    notification.setIndex(1);
    equal(notification.index(), 1);
    // FIXME: Really need to figure out how to mock/test animated removal.
    ok(notification.dismiss);
});

test('Stream', 11, function() {
    var stream = new ui.notifications.Stream();
    equal(stream.tagName, 'OL');
    equal(stream.className, 'notifications');
    equal(stream.childElementCount, 0);

    var notification;

    notification = new ui.notifications.Info('-o-matic');
    notification.setIndex(2);
    stream.add(notification);
    equal(stream.childElementCount, 1);
    equal(stream.textContent, '-o-matic');

    notification = new ui.notifications.Info('garden');
    notification.setIndex(3);
    stream.add(notification);
    equal(stream.childElementCount, 2);
    equal(stream.textContent, 'garden-o-matic');

    notification = new ui.notifications.Info(' is ');
    notification.setIndex(1);
    stream.add(notification);
    equal(stream.childElementCount, 3);
    equal(stream.textContent, 'garden-o-matic is ');

    notification = new ui.notifications.Info('awesome!');
    stream.add(notification);
    equal(stream.childElementCount, 4);
    equal(stream.textContent, 'garden-o-matic is awesome!');
});

test('Info', 2, function() {
    var info = new ui.notifications.Info('info');
    equal(info.tagName, 'LI');
    equal(info.innerHTML, '<div class="how"></div><div class="what">info</div>');
});

test('FailingTestGroup', 2, function() {
    var failingTest = new ui.notifications.FailingTestGroup('test');
    equal(failingTest.tagName, 'LI');
    equal(failingTest.innerHTML, 'test');
});

test('SuspiciousCommit', 2, function() {
    var suspiciousCommit = new ui.notifications.SuspiciousCommit({revision: 1, summary: "summary", author: "author", reviewer: "reviewer"});
    equal(suspiciousCommit.tagName, 'LI');
    equal(suspiciousCommit.innerHTML,
        '<div class="description">' +
            '<a href="http://trac.webkit.org/changeset/1" target="_blank">1</a>' +
            '<span class="summary">summary</span>' +
            '<span class="author">author</span>' +
            '<span class="reviewer">reviewer</span>' +
        '</div>' +
        '<ul class="actions">' +
            '<li><button class="action" title="Blames this failure on this revision.">Blame</button></li>' +
            '<li><button class="action" title="Rolls out this revision.">Roll out</button></li>' +
        '</ul>');
});

test('FailingTestsSummary', 12, function() {
    var testFailures = new ui.notifications.FailingTestsSummary();
    equal(testFailures.tagName, 'LI');
    equal(testFailures.innerHTML,
        '<div class="how">' +
            '<time class="relative">Just now</time>' +
            '<table class="failures">' +
                '<thead><tr><td>type</td><td>release</td><td>debug</td></tr></thead>' +
                '<tbody><tr class="BUILDING" style="display: none; "><td>BUILDING</td><td></td><td></td></tr></tbody>' +
            '</table>' +
        '</div>' +
        '<div class="what">' +
            '<div class="problem">' +
                '<ul class="effects"></ul>' +
                '<ul class="actions">' +
                    '<li><button class="action default" title="Examine these failures in detail.">Examine</button></li>' +
                    '<li><button class="action">Rebaseline</button></li>' +
                    '<li><button class="action">Mark as Expected</button></li>' +
                '</ul>' +
            '</div>' +
            '<ul class="causes"></ul>' +
        '</div>');
    testFailures.addFailureAnalysis({testName: 'test', resultNodesByBuilder: {}});
    equal(testFailures.index(), 0);
    equal(testFailures.innerHTML,
        '<div class="how">' +
            '<time class="relative">Just now</time>' +
            '<table class="failures">' +
                '<thead><tr><td>type</td><td>release</td><td>debug</td></tr></thead>' +
                '<tbody><tr class="BUILDING" style="display: none; "><td>BUILDING</td><td></td><td></td></tr></tbody>' +
            '</table>' +
        '</div>' +
        '<div class="what">' +
            '<div class="problem">' +
                '<ul class="effects">' +
                    '<li>test</li>' +
                '</ul>' +
                '<ul class="actions">' +
                    '<li><button class="action default" title="Examine these failures in detail.">Examine</button></li>' +
                    '<li><button class="action">Rebaseline</button></li>' +
                    '<li><button class="action">Mark as Expected</button></li>' +
                '</ul>' +
            '</div>' +
            '<ul class="causes"></ul>' +
        '</div>');
    ok(testFailures.containsFailureAnalysis({testName: 'test'}));
    ok(!testFailures.containsFailureAnalysis({testName: 'foo'}));
    testFailures.addFailureAnalysis({testName: 'test'});
    equal(testFailures.innerHTML,
        '<div class="how">' +
            '<time class="relative">Just now</time>' +
            '<table class="failures">' +
                '<thead><tr><td>type</td><td>release</td><td>debug</td></tr></thead>' +
                '<tbody><tr class="BUILDING" style="display: none; "><td>BUILDING</td><td></td><td></td></tr></tbody>' +
            '</table>' +
        '</div>' +
        '<div class="what">' +
            '<div class="problem">' +
                '<ul class="effects">' +
                    '<li>test</li>' +
                '</ul>' +
                '<ul class="actions">' +
                    '<li><button class="action default" title="Examine these failures in detail.">Examine</button></li>' +
                    '<li><button class="action">Rebaseline</button></li>' +
                    '<li><button class="action">Mark as Expected</button></li>' +
                '</ul>' +
            '</div>' +
            '<ul class="causes"></ul>' +
        '</div>');
    deepEqual(testFailures.testNameList(), ['test']);
    var time = new Date();
    time.setMinutes(time.getMinutes() - 10);
    testFailures.addCommitData({revision: 1, time: time, summary: "summary", author: "author", reviewer: "reviewer"});
    equal(testFailures.index(), time.getTime());
    equal(testFailures.innerHTML,
        '<div class="how">' +
            '<time class="relative">10 minutes ago</time>' +
            '<table class="failures">' +
                '<thead><tr><td>type</td><td>release</td><td>debug</td></tr></thead>' +
                '<tbody><tr class="BUILDING" style="display: none; "><td>BUILDING</td><td></td><td></td></tr></tbody>' +
            '</table>' +
        '</div>' +
        '<div class="what">' +
            '<div class="problem">' +
                '<ul class="effects">' +
                    '<li>test</li>' +
                '</ul>' +
                '<ul class="actions">' +
                    '<li><button class="action default" title="Examine these failures in detail.">Examine</button></li>' +
                    '<li><button class="action">Rebaseline</button></li>' +
                    '<li><button class="action">Mark as Expected</button></li>' +
                '</ul>' +
            '</div>' +
            '<ul class="causes">' +
                '<li>' +
                    '<div class="description">' +
                        '<a href="http://trac.webkit.org/changeset/1" target="_blank">1</a>' +
                        '<span class="summary">summary</span>' +
                        '<span class="author">author</span>' +
                        '<span class="reviewer">reviewer</span>' +
                    '</div>' +
                    '<ul class="actions">' +
                        '<li><button class="action" title="Blames this failure on this revision.">Blame</button></li>' +
                        '<li><button class="action" title="Rolls out this revision.">Roll out</button></li>' +
                    '</ul>' +
                '</li>' +
            '</ul>' +
        '</div>');

    testFailures.addFailureAnalysis({testName: 'foo', resultNodesByBuilder: {'Webkit Linux (dbg)(1)': { actual: 'TEXT'}}});
    equal(testFailures.innerHTML,
        '<div class="how">' +
            '<time class="relative">10 minutes ago</time>' +
            '<table class="failures">' +
                '<thead><tr><td>type</td><td>release</td><td>debug</td></tr></thead>' +
                '<tbody>' +
                    '<tr class="TEXT">' +
                        '<td>TEXT</td>' +
                        '<td></td>' +
                        '<td><a target="_blank" href="http://build.chromium.org/p/chromium.webkit/waterfall?builder=Webkit+Linux+(dbg)(1)"><span class="architecture">64-bit</span><span class="version">lucid</span></a></td>' +
                    '</tr>' +
                    '<tr class="BUILDING" style="display: none; "><td>BUILDING</td><td></td><td></td></tr>' +
                '</tbody>' +
            '</table>' +
        '</div>' +
        '<div class="what">' +
            '<div class="problem">' +
                '<ul class="effects">' +
                    '<li>test</li>' +
                    '<li>foo</li>' +
                '</ul>' +
                '<ul class="actions">' +
                    '<li><button class="action default" title="Examine these failures in detail.">Examine</button></li>' +
                    '<li><button class="action">Rebaseline</button></li>' +
                    '<li><button class="action">Mark as Expected</button></li>' +
                '</ul>' +
            '</div>' +
            '<ul class="causes">' +
                '<li>' +
                    '<div class="description">' +
                        '<a href="http://trac.webkit.org/changeset/1" target="_blank">1</a>' +
                        '<span class="summary">summary</span>' +
                        '<span class="author">author</span>' +
                        '<span class="reviewer">reviewer</span>' +
                    '</div>' +
                    '<ul class="actions">' +
                        '<li><button class="action" title="Blames this failure on this revision.">Blame</button></li>' +
                        '<li><button class="action" title="Rolls out this revision.">Roll out</button></li>' +
                    '</ul>' +
                '</li>' +
            '</ul>' +
        '</div>');

    testFailures.updateBuilderResults({'Webkit Mac10.5 (CG)': { actual: 'BUILDING'}});
    equal(testFailures.innerHTML,
        '<div class="how">' +
            '<time class="relative">10 minutes ago</time>' +
            '<table class="failures">' +
                '<thead><tr><td>type</td><td>release</td><td>debug</td></tr></thead>' +
                '<tbody>' +
                    '<tr class="TEXT">' +
                        '<td>TEXT</td>' +
                        '<td></td>' +
                        '<td><a target="_blank" href="http://build.chromium.org/p/chromium.webkit/waterfall?builder=Webkit+Linux+(dbg)(1)"><span class="architecture">64-bit</span><span class="version">lucid</span></a></td>' +
                    '</tr>' +
                    '<tr class="BUILDING" style="">' +
                        '<td>BUILDING</td>' +
                        '<td><a target="_blank" href="http://build.chromium.org/p/chromium.webkit/waterfall?builder=Webkit+Mac10.5+(CG)"><span class="version">leopard</span></a></td>' +
                        '<td></td>' +
                    '</tr>' +
                '</tbody>' +
            '</table>' +
        '</div>' +
        '<div class="what">' +
            '<div class="problem">' +
                '<ul class="effects">' +
                    '<li>test</li>' +
                    '<li>foo</li>' +
                '</ul>' +
                '<ul class="actions">' +
                    '<li><button class="action default" title="Examine these failures in detail.">Examine</button></li>' +
                    '<li><button class="action">Rebaseline</button></li>' +
                    '<li><button class="action">Mark as Expected</button></li>' +
                '</ul>' +
            '</div>' +
            '<ul class="causes">' +
                '<li>' +
                    '<div class="description">' +
                        '<a href="http://trac.webkit.org/changeset/1" target="_blank">1</a>' +
                        '<span class="summary">summary</span>' +
                        '<span class="author">author</span>' +
                        '<span class="reviewer">reviewer</span>' +
                    '</div>' +
                    '<ul class="actions">' +
                        '<li><button class="action" title="Blames this failure on this revision.">Blame</button></li>' +
                        '<li><button class="action" title="Rolls out this revision.">Roll out</button></li>' +
                    '</ul>' +
                '</li>' +
            '</ul>' +
        '</div>');
});

test('FailingTestsSummary (grouping)', 1, function() {
    var testFailures = new ui.notifications.FailingTestsSummary();
    testFailures.addFailureAnalysis({testName: 'path/to/test1.html', resultNodesByBuilder: {}});
    testFailures.addFailureAnalysis({testName: 'path/to/test2.html', resultNodesByBuilder: {}});
    testFailures.addFailureAnalysis({testName: 'path/to/test3.html', resultNodesByBuilder: {}});
    testFailures.addFailureAnalysis({testName: 'path/to/test4.html', resultNodesByBuilder: {}});
    testFailures.addFailureAnalysis({testName: 'path/another/test.html', resultNodesByBuilder: {}});
    equal(testFailures.innerHTML,
        '<div class="how">' +
            '<time class="relative">Just now</time>' +
            '<table class="failures">' +
                '<thead><tr><td>type</td><td>release</td><td>debug</td></tr></thead>' +
                '<tbody><tr class="BUILDING" style="display: none; "><td>BUILDING</td><td></td><td></td></tr></tbody>' +
            '</table>' +
        '</div>' +
        '<div class="what">' +
            '<div class="problem">' +
                '<ul class="effects">' +
                    '<li>path/to (4 tests)</li>' +
                    '<li>path/another/test.html</li>' +
                '</ul>' +
                '<ul class="actions">' +
                    '<li><button class="action default" title="Examine these failures in detail.">Examine</button></li>' +
                    '<li><button class="action">Rebaseline</button></li>' +
                    '<li><button class="action">Mark as Expected</button></li>' +
                '</ul>' +
            '</div>' +
            '<ul class="causes"></ul>' +
        '</div>');

});

test('BuildersFailing', 1, function() {
    var builderFailing = new ui.notifications.BuildersFailing();
    builderFailing.setFailingBuilders(['WebKit Linux', 'Webkit Vista']);
    equal(builderFailing.innerHTML,
        '<div class="how">' +
            '<time class="relative">Just now</time>' +
        '</div>' +
        '<div class="what">' +
            '<div class="problem">Build Failed:' +
                '<ul class="effects">' +
                    '<li class="builder-name"><a target="_blank" href="http://build.chromium.org/p/chromium.webkit/waterfall?builder=WebKit+Linux">WebKit Linux</a></li>' +
                    '<li class="builder-name"><a target="_blank" href="http://build.chromium.org/p/chromium.webkit/waterfall?builder=Webkit+Vista">Vista</a></li>' +
                '</ul>' +
            '</div>' +
            '<ul class="causes"></ul>' +
        '</div>');
});

}());
