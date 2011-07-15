/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

// FIXME: These should probably move to some WebKitBugzilla class (or similar).
const BugzillaConstants = {
    Component: {
        ToolsTests: 'Tools / Tests',
    },

    Keyword: {
        LayoutTestFailure: 'LayoutTestFailure',
        MakingBotsRed: 'MakingBotsRed',
        Regression: 'Regression',
    },

    OperatingSystem: {
        Leopard: 'Mac OS X 10.5',
        SnowLeopard: 'Mac OS X 10.6',
        Windows7: 'Windows 7',
        WindowsXP: 'Windows XP',
    },

    Platform: {
        Macintosh: 'Macintosh',
        PC: 'PC',
    },

    Product: {
        WebKit: 'WebKit',
    },

    Version: {
        Nightly: '528+ (Nightly Build)',
    },
};

function FailingTestsBugForm(bugzilla, trac, tester, failingBuildName, passingBuildName, failingTests) {
    NewBugForm.call(this, bugzilla);

    this._trac = trac;
    this._tester = tester;
    this._failingBuildName = failingBuildName;
    this._passingBuildName = passingBuildName;
    this._failingTests = failingTests;

    this.component = BugzillaConstants.Component.ToolsTests;
    this.description = this._createBugDescription();
    // FIXME: When a newly-added test has been failing since its introduction, it isn't really a
    // "regression". We should use different keywords in that case. <http://webkit.org/b/61645>
    this.keywords = [
        BugzillaConstants.Keyword.LayoutTestFailure,
        BugzillaConstants.Keyword.MakingBotsRed,
        BugzillaConstants.Keyword.Regression
    ].join(', ');
    this.operatingSystem = this._computeOperatingSystem();
    this.platform = this._computePlatform();
    this.product = BugzillaConstants.Product.WebKit;
    this.title = this._createBugTitle();
    this.url = this._failingResultsHTMLURL();
    this.version = BugzillaConstants.Version.Nightly;
}

FailingTestsBugForm.prototype = {
    domElement: function() {
        var form = NewBugForm.prototype.domElement.call(this);
        form.className = 'new-bug-form';
        form.target = '_blank';
        return form;
    },

    _computeOperatingSystem: function() {
        if (/Windows 7/.test(this._tester.name))
            return BugzillaConstants.OperatingSystem.Windows7;
        if (/Windows XP/.test(this._tester.name))
            return BugzillaConstants.OperatingSystem.WindowsXP;
        if (/SnowLeopard/.test(this._tester.name))
            return BugzillaConstants.OperatingSystem.SnowLeopard;
        if (/Leopard/.test(this._tester.name))
            return BugzillaConstants.OperatingSystem.Leopard;
        return '';
    },

    _computePlatform: function() {
        if (/Windows/.test(this._tester.name))
            return BugzillaConstants.Platform.PC;
        if (/Leopard/.test(this._tester.name))
            return BugzillaConstants.Platform.Macintosh;
        return '';
    },

    _createBugDescription: function() {
        var firstSuspectRevision = this._passingRevision() ? this._passingRevision() + 1 : this._failingRevision();
        var lastSuspectRevision = this._failingRevision();

        var endOfFirstSentence;
        if (this._passingBuildName) {
            endOfFirstSentence = 'started failing on ' + this._tester.name;
            if (firstSuspectRevision === lastSuspectRevision)
                endOfFirstSentence += ' in r' + firstSuspectRevision + ' <' + this._trac.changesetURL(firstSuspectRevision) + '>';
            else
                endOfFirstSentence += ' between r' + firstSuspectRevision + ' and r' + lastSuspectRevision + ' (inclusive)';
        } else
            endOfFirstSentence = (this._failingTests.length === 1 ? 'has' : 'have') + ' been failing on ' + this._tester.name + ' since at least r' + firstSuspectRevision + ' <' + this._trac.changesetURL(firstSuspectRevision) + '>';
        var description;
        if (this._failingTests.length === 1)
            description = this._failingTests[0] + ' ' + endOfFirstSentence + '.\n\n';
        else if (this._failingTests.length === 2)
            description = this._failingTests.join(' and ') + ' ' + endOfFirstSentence + '.\n\n';
        else {
            description = 'The following tests ' + endOfFirstSentence + ':\n\n'
                + this._failingTests.map(function(test) { return '    ' + test }).join('\n')
                + '\n\n';
        }
        if (firstSuspectRevision !== lastSuspectRevision)
            description += this._trac.logURL('trunk', firstSuspectRevision, lastSuspectRevision) + '\n\n';
        if (this._passingBuildName)
            description += this._tester.resultsPageURL(this._passingBuildName) + ' passed\n';
        description += this._failingResultsHTMLURL() + ' failed\n';

        return description;
    },

    _createBugTitle: function() {
        // FIXME: When a newly-added test has been failing since its introduction, it isn't really a
        // "regression". We should use a different title in that case. <http://webkit.org/b/61645>

        var titlePrefix = 'REGRESSION (' + this._regressionRangeString() + '): ';
        var titleSuffix = ' failing on ' + this._tester.name;
        var title = titlePrefix + this._failingTests.join(', ') + titleSuffix;

        if (title.length <= Bugzilla.maximumBugTitleLength)
            return title;

        var pathPrefix = longestCommonPathPrefix(this._failingTests);
        if (pathPrefix) {
            title = titlePrefix + this._failingTests.length + ' ' + pathPrefix + ' tests' + titleSuffix;
            if (title.length <= Bugzilla.maximumBugTitleLength)
                return title;
        }

        title = titlePrefix + this._failingTests.length + ' tests' + titleSuffix;

        console.assert(title.length <= Bugzilla.maximumBugTitleLength);
        return title;
    },

    _failingResultsHTMLURL: function() {
        return this._tester.resultsPageURL(this._failingBuildName);
    },

    _failingRevision: function() {
        return this._tester.buildbot.parseBuildName(this._failingBuildName).revision;
    },

    _passingRevision: function() {
        if (!this._passingBuildName)
            return null;
        return this._tester.buildbot.parseBuildName(this._passingBuildName).revision;
    },

    _regressionRangeString: function() {
        var failingRevision = this._failingRevision();
        var passingRevision = this._passingRevision();
        if (!passingRevision || failingRevision - passingRevision <= 1)
            return 'r' + failingRevision;
        return 'r' + passingRevision + '-r' + failingRevision;
    },
};

FailingTestsBugForm.prototype.__proto__ = NewBugForm.prototype;
