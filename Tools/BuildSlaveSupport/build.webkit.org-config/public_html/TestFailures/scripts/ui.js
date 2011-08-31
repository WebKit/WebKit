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

var ui = ui || {};

(function () {

ui.displayURLForBuilder = function(builderName)
{
    return 'http://build.chromium.org/p/chromium.webkit/waterfall?' + $.param({
        'builder': builderName
    });
}

ui.displayNameForBuilder = function(builderName)
{
    return builderName.replace(/Webkit /, '');
}

ui.urlForTest = function(testName)
{
    return 'http://trac.webkit.org/browser/trunk/LayoutTests/' + testName;
}

ui.rolloutReasonForTestNameList = function(testNameList)
{
    return 'Broke:\n' + testNameList.map(function(testName) {
        return '* ' + testName;
    }).join('\n');
}

ui.onebar = base.extends('div', {
    init: function()
    {
        this.id = 'onebar';
        this.innerHTML =
            '<ul>' +
                '<li><a href="#summary">Summary</a></li>' +
                '<li><a href="#results">Results</a></li>' +
                '<li><a href="#commits">Commits</a></li>' +
            '</ul>' +
            '<div id="summary"></div>' +
            '<div id="results"></div>' +
            '<div id="commits">Coming soon...</div>';
        this._tabNames = [
            'summary',
            'results',
            'commits,'
        ]
        this._tabs = $(this).tabs({
            disabled: [1, 2],
        });
    },
    attach: function()
    {
        document.body.insertBefore(this, document.body.firstChild);
    },
    tabNamed: function(tabName)
    {
        return $('#' + tabName, this)[0];
    },
    summary: function()
    {
        return this.tabNamed('summary');
    },
    results: function()
    {
        return this.tabNamed('results');
    },
    select: function(tabName)
    {
        var tabIndex = this._tabNames.indexOf(tabName);
        this._tabs.tabs('enable', tabIndex);
        this._tabs.tabs('select', tabIndex);
    }
});

// FIXME: Loading a module shouldn't set off a timer.  The controller should kick this off.
setInterval(function() {
    Array.prototype.forEach.call(document.querySelectorAll("time.relative"), function(time) {
        time.update && time.update();
    });
}, config.kRelativeTimeUpdateFrequency);

ui.RelativeTime = base.extends('time', {
    init: function()
    {
        this.setDate(new Date());
        this.className = 'relative';
    },
    date: function()
    {
        return this._date;
    },
    update: function()
    {
        this.textContent = base.relativizeTime(this._date);
    },
    setDate: function(date)
    {
        this._date = date;
        this.textContent = base.relativizeTime(date);
    }
});

})();
