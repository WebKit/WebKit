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

module('ui.failures');

test('Configuration', 8, function() {
    raises(function() {
        new ui.failures.Configuration();
    });

    var configuration;
    configuration = new ui.failures.Configuration({});
    deepEqual(Object.getOwnPropertyNames(configuration.__proto__).sort(), [
        '_addSpan',
        'equals',
        'init',
    ]);
    equal(configuration.outerHTML, '<a target="_blank"></a>');
    configuration = new ui.failures.Configuration({is64bit: true, version: 'lucid'});
    equal(configuration.outerHTML, '<a target="_blank"><span class="architecture">64-bit</span><span class="version">lucid</span></a>');
    configuration = new ui.failures.Configuration({version: 'xp'});
    equal(configuration.outerHTML, '<a target="_blank"><span class="version">xp</span></a>');
    configuration._addSpan('foo', 'bar');
    equal(configuration.outerHTML, '<a target="_blank"><span class="version">xp</span><span class="foo">bar</span></a>');
    ok(configuration.equals({version: 'xp'}));
    ok(!configuration.equals({version: 'lucid',is64bit: true}));
});

test('FailureGrid', 9, function() {
    var grid = new ui.failures.FailureGrid();
    deepEqual(Object.getOwnPropertyNames(grid.__proto__).sort(), [
        "_rowByResult",
        "add",
        "init",
        "removeResultRows",
    ]);
    equal(grid.outerHTML, '<table class="failures">' +
        '<thead><tr><td>type</td><td>release</td><td>debug</td></tr></thead>' +
        '<tbody><tr class="BUILDING" style="display: none; "><td>BUILDING</td><td></td><td></td></tr></tbody>' +
    '</table>');
    var row = grid._rowByResult('TEXT');
    equal(grid.outerHTML, '<table class="failures">' +
        '<thead><tr><td>type</td><td>release</td><td>debug</td></tr></thead>' +
        '<tbody>' +
            '<tr class="TEXT">' +
                '<td>TEXT</td><td></td><td></td>' +
            '</tr>' +
            '<tr class="BUILDING" style="display: none; "><td>BUILDING</td><td></td><td></td></tr>' +
        '</tbody>' +
    '</table>');
    equal(row.outerHTML, '<tr class="TEXT"><td>TEXT</td><td></td><td></td></tr>');
    grid.add({});
    equal(grid.outerHTML, '<table class="failures">' +
        '<thead><tr><td>type</td><td>release</td><td>debug</td></tr></thead>' +
        '<tbody>' +
            '<tr class="TEXT">' +
                '<td>TEXT</td><td></td><td></td>' +
            '</tr>' +
            '<tr class="BUILDING" style="display: none; "><td>BUILDING</td><td></td><td></td></tr>' +
        '</tbody>' +
    '</table>');
    raises(function() {
        grid.update({'Atari': {}})
    });
    grid.add({'Webkit Linux (dbg)(1)': { actual: 'TEXT'}});
    equal(grid.outerHTML, '<table class="failures">' +
        '<thead><tr><td>type</td><td>release</td><td>debug</td></tr></thead>' +
        '<tbody>' +
            '<tr class="TEXT">' +
                '<td>TEXT</td>' +
                '<td></td>' +
                '<td><a target="_blank" href="http://build.chromium.org/p/chromium.webkit/waterfall?builder=Webkit+Linux+(dbg)(1)"><span class="architecture">64-bit</span><span class="version">lucid</span></a></td>' +
            '</tr>' +
            '<tr class="BUILDING" style="display: none; "><td>BUILDING</td><td></td><td></td></tr>' +
        '</tbody>' +
    '</table>');
    grid.add({'Webkit Mac10.5 (CG)': { actual: 'IMAGE+TEXT'}});
    equal(grid.outerHTML, '<table class="failures">' +
        '<thead><tr><td>type</td><td>release</td><td>debug</td></tr></thead>' +
        '<tbody>' +
            '<tr class="IMAGE+TEXT">' +
                '<td>IMAGE+TEXT</td>' +
                '<td><a target="_blank" href="http://build.chromium.org/p/chromium.webkit/waterfall?builder=Webkit+Mac10.5+(CG)"><span class="version">leopard</span></a></td>' +
                '<td></td>' +
            '</tr>' +
            '<tr class="TEXT">' +
                '<td>TEXT</td>' +
                '<td></td>' +
                '<td><a target="_blank" href="http://build.chromium.org/p/chromium.webkit/waterfall?builder=Webkit+Linux+(dbg)(1)"><span class="architecture">64-bit</span><span class="version">lucid</span></a></td>' +
            '</tr>' +
            '<tr class="BUILDING" style="display: none; "><td>BUILDING</td><td></td><td></td></tr>' +
        '</tbody>' +
    '</table>');
    grid.add({'Webkit Mac10.5 (CG)': { actual: 'IMAGE+TEXT'}});
    equal(grid.outerHTML, '<table class="failures">' +
        '<thead><tr><td>type</td><td>release</td><td>debug</td></tr></thead>' +
        '<tbody>' +
            '<tr class="IMAGE+TEXT">' +
                '<td>IMAGE+TEXT</td>' +
                '<td><a target="_blank" href="http://build.chromium.org/p/chromium.webkit/waterfall?builder=Webkit+Mac10.5+(CG)"><span class="version">leopard</span></a></td>' +
                '<td></td>' +
            '</tr>' +
            '<tr class="TEXT">' +
                '<td>TEXT</td>' +
                '<td></td>' +
                '<td><a target="_blank" href="http://build.chromium.org/p/chromium.webkit/waterfall?builder=Webkit+Linux+(dbg)(1)"><span class="architecture">64-bit</span><span class="version">lucid</span></a></td>' +
            '</tr>' +
            '<tr class="BUILDING" style="display: none; "><td>BUILDING</td><td></td><td></td></tr>' +
        '</tbody>' +
    '</table>');
});

}());
