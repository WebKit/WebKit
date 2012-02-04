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

ui.urlForFlakinessDashboard = function(testNameList)
{
    var testsParameter = testNameList.join(',');
    return 'http://test-results.appspot.com/dashboards/flakiness_dashboard.html#tests=' + encodeURIComponent(testsParameter);
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
                '<li><a href="#unexpected">Unexpected Failures</a></li>' +
                '<li><a href="#results">Results</a></li>' +
            '</ul>' +
            '<div id="unexpected"></div>' +
            '<div id="failures"></div>' +
            '<div id="results"></div>';
        this._tabNames = [
            'unexpected',
            'results',
        ]
        this._tabs = $(this).tabs({
            disabled: [1],
        });
        if (config.kExperimentalFeatures) {
            this._tabs.tabs('add', '#failures', 'All Failures (experimental)');
            this._tabNames.push('failures');
        }
    },
    attach: function()
    {
        document.body.insertBefore(this, document.body.firstChild);
    },
    tabNamed: function(tabName)
    {
        if (this._tabNames.indexOf(tabName) == -1)
            return null;
        tab = document.getElementById(tabName);
        // We perform this sanity check below to make sure getElementById
        // hasn't given us a node in some other unrelated part of the document.
        // that shouldn't happen normally, but it could happen if an attacker
        // has somehow sneakily added a node to our document.
        if (tab.parentNode != this)
            return null;
        return tab;
    },
    unexpected: function()
    {
        return this.tabNamed('unexpected');
    },
    failures: function()
    {
        return this.tabNamed('failures');
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
        this.className = 'relative';
    },
    date: function()
    {
        return this._date;
    },
    update: function()
    {
        this.textContent = this._date ? base.relativizeTime(this._date) : '';
    },
    setDate: function(date)
    {
        this._date = date;
        this.update();
    }
});

ui.MessageBox = base.extends('div',  {
    init: function(title, message)
    {
        this._content = document.createElement('div');
        this.appendChild(this._content);
        this.addMessage(message);
        document.body.appendChild(this);
        $(this).dialog({
            resizable: false,
            width: $(window).width() * 0.80,  // FIXME: We should have CSS do this work for us.
        });
        $('.ui-dialog-title', this.parentNode).text(title);
        $(this).bind('dialogclose', function() {
            $(this).detach();
        }.bind(this));
    },
    close: function()
    {
        $(this).dialog('close');
    },
    addMessage: function(message)
    {
        var element = document.createElement('div');
        $(element).addClass('message').text(message);
        this._content.appendChild(element);
    },
    // FIXME: It's unclear whether this code could live here or in a controller.
    addFinalMessage: function(message)
    {
        this.addMessage(message);
        this.appendChild(new ui.actions.List([new ui.actions.Close()]));
        $(this).bind('close', this.close.bind(this));
    }
});

})();
