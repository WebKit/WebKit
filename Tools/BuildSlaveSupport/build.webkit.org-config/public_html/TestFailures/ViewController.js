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

function ViewController(buildbot, bugzilla, trac) {
    this._buildbot = buildbot;
    this._bugzilla = bugzilla;
    this._trac = trac;
    this._navigationID = 0;

    var self = this;
    addEventListener('load', function() { self.loaded() }, false);
    addEventListener('hashchange', function() { self.parseHash(location.hash) }, false);
}

ViewController.prototype = {
    loaded: function() {
        this._mainContentElement = document.createElement('div');
        document.body.appendChild(this._mainContentElement);
        document.body.appendChild(this._domForAuxiliaryUIElements());

        this.parseHash(location.hash);
    },

    parseHash: function(hash) {
        ++this._navigationID;

        var match = /#\/(.*)/.exec(hash);
        if (match)
            this._displayBuilder(this._buildbot.builderNamed(decodeURIComponent(match[1])));
        else
            this._displayTesters();
    },

    _displayBuilder: function(builder) {
        var navigationID = this._navigationID;
        var self = this;
        (new LayoutTestHistoryAnalyzer(builder)).start(function(data, stillFetchingData) {
            if (self._navigationID !== navigationID) {
                // The user has navigated somewhere else. Stop loading data about this tester.
                return false;
            }

            var list = document.createElement('ol');
            list.id = 'failure-history';
            Object.keys(data.history).forEach(function(buildName, buildIndex, buildNameArray) {
                var failingTestNames = Object.keys(data.history[buildName].tests);
                if (!failingTestNames.length)
                    return;

                var item = document.createElement('li');
                list.appendChild(item);

                var testList = document.createElement('ol');
                item.appendChild(testList);

                testList.className = 'test-list';
                for (var testName in data.history[buildName].tests) {
                    var testItem = document.createElement('li');
                    testItem.appendChild(self._domForFailedTest(builder, buildName, testName, data.history[buildName].tests[testName]));
                    testList.appendChild(testItem);
                }

                if (data.history[buildName].tooManyFailures) {
                    var p = document.createElement('p');
                    p.className = 'info';
                    p.appendChild(document.createTextNode('run-webkit-tests exited early due to too many failures/crashes/timeouts'));
                    item.appendChild(p);
                }

                var passingBuildName;
                if (buildIndex + 1 < buildNameArray.length)
                    passingBuildName = buildNameArray[buildIndex + 1];

                item.appendChild(self._domForRegressionRange(builder, passingBuildName, buildName));

                if (passingBuildName || !stillFetchingData)
                    item.appendChild(self._domForNewAndExistingBugs(builder, buildName, passingBuildName, failingTestNames));
            });

            var header = document.createElement('h1');
            header.appendChild(document.createTextNode(builder.name));
            self._mainContentElement.innerHTML = '';
            document.title = builder.name;
            self._mainContentElement.appendChild(header);
            self._mainContentElement.appendChild(list);
            self._mainContentElement.appendChild(self._domForPossiblyFlakyTests(builder, data.possiblyFlaky));

            if (!stillFetchingData)
                PersistentCache.prune();

            return true;
        });
    },

    _displayTesters: function() {
        var navigationID = this._navigationID;

        var list = document.createElement('ul');
        var latestBuildInfos = [];

        function updateList() {
            latestBuildInfos.sort(function(a, b) { return a.tester.name.localeCompare(b.tester.name) });
            while (list.firstChild)
                list.removeChild(list.firstChild);
            latestBuildInfos.forEach(function(buildInfo) {
                var link = document.createElement('a');
                link.href = '#/' + buildInfo.tester.name;
                link.appendChild(document.createTextNode(buildInfo.tester.name));

                var item = document.createElement('li');
                item.appendChild(link);
                if (buildInfo.tooManyFailures)
                    item.appendChild(document.createTextNode(' (too many failures/crashes/timeouts)'));
                else
                    item.appendChild(document.createTextNode(' (' + buildInfo.failureCount + ' failing test' + (buildInfo.failureCount > 1 ? 's' : '') + ')'));
                list.appendChild(item);
            });
        }

        var self = this;
        this._buildbot.getTesters(function(testers) {
            if (self._navigationID !== navigationID) {
                // The user has navigated somewhere else.
                return;
            }
            testers.forEach(function(tester) {
                tester.getMostRecentCompletedBuildNumber(function(buildNumber) {
                    if (self._navigationID !== navigationID)
                        return;
                    if (buildNumber < 0)
                        return;
                    tester.getNumberOfFailingTests(buildNumber, function(failureCount, tooManyFailures) {
                        if (self._navigationID !== navigationID)
                            return;
                        if (failureCount <= 0)
                            return;
                        latestBuildInfos.push({ tester: tester, failureCount: failureCount, tooManyFailures: tooManyFailures });
                        updateList();
                    });
                });
            });

            self._mainContentElement.innerHTML = '';
            document.title = 'Testers';
            self._mainContentElement.appendChild(list);
        });
    },

    _domForRegressionRange: function(builder, passingBuildName, failingBuildName) {
        var result = document.createDocumentFragment();

        var dlItems = [
            [document.createTextNode('Failed'), this._domForBuildName(builder, failingBuildName)],
        ];
        if (passingBuildName)
            dlItems.push([document.createTextNode('Passed'), this._domForBuildName(builder, passingBuildName)]);
        result.appendChild(createDefinitionList(dlItems));

        if (!passingBuildName)
            return result;

        var firstSuspectRevision = this._buildbot.parseBuildName(passingBuildName).revision + 1;
        var lastSuspectRevision = this._buildbot.parseBuildName(failingBuildName).revision;

        if (firstSuspectRevision === lastSuspectRevision)
            return result;

        var link = document.createElement('a');
        result.appendChild(link);

        link.href = this._trac.logURL('trunk', firstSuspectRevision, lastSuspectRevision, true);
        link.appendChild(document.createTextNode('View regression range in Trac'));

        return result;
    },

    _domForAuxiliaryUIElements: function() {
        if (!this._bugzilla)
            return document.createDocumentFragment();

        var aside = document.createElement('aside');
        aside.appendChild(document.createTextNode('Something not working? Have an idea to improve this page? '));
        var link = document.createElement('a');
        aside.appendChild(link);

        link.appendChild(document.createTextNode('File a bug!'));
        var queryParameters = {
            product: 'WebKit',
            component: 'Tools / Tests',
            version: '528+ (Nightly build)',
            bug_file_loc: location.href,
            cc: 'aroben@apple.com',
            short_desc: 'TestFailures page needs more unicorns!',
        };
        link.href = addQueryParametersToURL(this._bugzilla.baseURL + 'enter_bug.cgi', queryParameters);
        link.target = '_blank';

        return aside;
    },

    _domForBuildName: function(builder, buildName) {
        var parsed = this._buildbot.parseBuildName(buildName);

        var sourceLink = document.createElement('a');
        sourceLink.href = 'http://trac.webkit.org/changeset/' + parsed.revision;
        sourceLink.appendChild(document.createTextNode('r' + parsed.revision));

        var buildLink = document.createElement('a');
        buildLink.href = builder.buildURL(parsed.buildNumber);
        buildLink.appendChild(document.createTextNode(parsed.buildNumber));

        var resultsLink = document.createElement('a');
        resultsLink.href = builder.resultsPageURL(buildName);
        resultsLink.appendChild(document.createTextNode('results.html'));

        var result = document.createDocumentFragment();
        result.appendChild(sourceLink);
        result.appendChild(document.createTextNode(' ('));
        result.appendChild(buildLink);
        result.appendChild(document.createTextNode(') ('));
        result.appendChild(resultsLink);
        result.appendChild(document.createTextNode(')'));

        return result;
    },

    _domForFailedTest: function(builder, buildName, testName, testResult) {
        var result = document.createDocumentFragment();
        result.appendChild(document.createTextNode(testName));
        result.appendChild(document.createTextNode(' ('));
        result.appendChild(this._domForFailureDiagnosis(builder, buildName, testName, testResult));
        result.appendChild(document.createTextNode(')'));
        return result;
    },

    _domForFailureDiagnosis: function(builder, buildName, testName, testResult) {
        var diagnosticInfo = builder.failureDiagnosisTextAndURL(buildName, testName, testResult);
        if (!diagnosticInfo)
            return document.createTextNode(testResult.failureType);

        var textNode = document.createTextNode(diagnosticInfo.text);
        if (!('url' in diagnosticInfo))
            return textNode;

        var link = document.createElement('a');
        link.href = diagnosticInfo.url;
        link.appendChild(textNode);
        return link;
    },

    _domForNewAndExistingBugs: function(tester, failingBuildName, passingBuildName, failingTests) {
        var result = document.createDocumentFragment();

        if (!this._bugzilla)
            return result;

        var container = document.createElement('p');
        result.appendChild(container);

        container.className = 'existing-and-new-bugs';

        var bugsContainer = document.createElement('div');
        container.appendChild(bugsContainer);

        bugsContainer.appendChild(document.createTextNode('Searching for bugs related to ' + (failingTests.length > 1 ? 'these tests' : 'this test') + '\u2026'));

        this._bugzilla.quickSearch('ALL ' + failingTests.join('|'), function(bugs) {
            if (!bugs.length) {
                bugsContainer.parentNode.removeChild(bugsContainer);
                return;
            }

            while (bugsContainer.firstChild)
                bugsContainer.removeChild(bugsContainer.firstChild);

            bugsContainer.appendChild(document.createTextNode('Existing bugs related to ' + (failingTests.length > 1 ? 'these tests' : 'this test') + ':'));

            var list = document.createElement('ul');
            bugsContainer.appendChild(list);

            list.className = 'existing-bugs-list';

            function bugToListItem(bug) {
                var link = document.createElement('a');
                link.href = bug.url;
                link.appendChild(document.createTextNode(bug.title));

                var item = document.createElement('li');
                item.appendChild(link);

                return item;
            }

            var openBugs = bugs.filter(function(bug) { return Bugzilla.isOpenStatus(bug.status) });
            var closedBugs = bugs.filter(function(bug) { return !Bugzilla.isOpenStatus(bug.status) });

            list.appendChildren(openBugs.map(bugToListItem));

            if (!closedBugs.length)
                return;

            var item = document.createElement('li');
            list.appendChild(item);

            item.appendChild(document.createTextNode('Closed bugs:'));

            var closedList = document.createElement('ul');
            item.appendChild(closedList);

            closedList.appendChildren(closedBugs.map(bugToListItem));
        });

        var parsedFailingBuildName = this._buildbot.parseBuildName(failingBuildName);
        var regressionRangeString = 'r' + parsedFailingBuildName.revision;
        if (passingBuildName) {
            var parsedPassingBuildName = this._buildbot.parseBuildName(passingBuildName);
            if (parsedFailingBuildName.revision - parsedPassingBuildName.revision > 1)
                regressionRangeString = 'r' + parsedPassingBuildName.revision + '-' + regressionRangeString;
        }

        // FIXME: Some of this code should move into a new method on the Bugzilla class.

        // FIXME: When a newly-added test has been failing since its introduction, it isn't really a
        // "regression". We should use a different title and keywords in that case.
        // <http://webkit.org/b/61645>

        var titlePrefix = 'REGRESSION (' + regressionRangeString + '): ';
        var titleSuffix = ' failing on ' + tester.name;
        var title = titlePrefix + failingTests.join(', ') + titleSuffix;
        if (title.length > Bugzilla.maximumBugTitleLength) {
            var pathPrefix = longestCommonPathPrefix(failingTests);
            if (pathPrefix)
                title = titlePrefix + failingTests.length + ' ' + pathPrefix + ' tests' + titleSuffix;
            if (title.length > Bugzilla.maximumBugTitleLength)
                title = titlePrefix + failingTests.length + ' tests' + titleSuffix;
        }
        console.assert(title.length <= Bugzilla.maximumBugTitleLength);

        var firstSuspectRevision = parsedPassingBuildName ? parsedPassingBuildName.revision + 1 : parsedFailingBuildName.revision;
        var lastSuspectRevision = parsedFailingBuildName.revision;

        var endOfFirstSentence;
        if (passingBuildName) {
            endOfFirstSentence = 'started failing on ' + tester.name;
            if (firstSuspectRevision === lastSuspectRevision)
                endOfFirstSentence += ' in r' + firstSuspectRevision + ' <' + this._trac.changesetURL(firstSuspectRevision) + '>';
            else
                endOfFirstSentence += ' between r' + firstSuspectRevision + ' and r' + lastSuspectRevision + ' (inclusive)';
        } else
            endOfFirstSentence = (failingTests.length === 1 ? 'has' : 'have') + ' been failing on ' + tester.name + ' since at least r' + firstSuspectRevision + ' <' + this._trac.changesetURL(firstSuspectRevision) + '>';

        var description;
        if (failingTests.length === 1)
            description = failingTests[0] + ' ' + endOfFirstSentence + '.\n\n';
        else if (failingTests.length === 2)
            description = failingTests.join(' and ') + ' ' + endOfFirstSentence + '.\n\n';
        else {
            description = 'The following tests ' + endOfFirstSentence + ':\n\n'
                + failingTests.map(function(test) { return '    ' + test }).join('\n')
                + '\n\n';
        }
        if (firstSuspectRevision !== lastSuspectRevision)
            description += this._trac.logURL('trunk', firstSuspectRevision, lastSuspectRevision) + '\n\n';
        if (passingBuildName)
            description += encodeURI(tester.resultsPageURL(passingBuildName)) + ' passed\n';
        var failingResultsHTML = tester.resultsPageURL(failingBuildName);
        description += encodeURI(failingResultsHTML) + ' failed\n';

        var formData = {
            product: 'WebKit',
            version: '528+ (Nightly build)',
            component: 'Tools / Tests',
            keywords: 'LayoutTestFailure, MakingBotsRed, Regression',
            short_desc: title,
            comment: description,
            bug_file_loc: failingResultsHTML,
        };

        if (/Windows/.test(tester.name)) {
            formData.rep_platform = 'PC';
            if (/Windows 7/.test(tester.name))
                formData.op_sys = 'Windows 7';
            else if (/Windows XP/.test(tester.name))
                formData.op_sys = 'Windows XP';
        } else if (/Leopard/.test(tester.name)) {
            formData.rep_platform = 'Macintosh';
            if (/SnowLeopard/.test(tester.name))
                formData.op_sys = 'Mac OS X 10.6';
            else
                formData.op_sys = 'Mac OS X 10.5';
        }

        var form = document.createElement('form');
        result.appendChild(form);
        form.className = 'new-bug-form';
        form.method = 'POST';
        form.action = this._bugzilla.baseURL + 'enter_bug.cgi';
        form.target = '_blank';

        for (var key in formData) {
            var input = document.createElement('input');
            input.type = 'hidden';
            input.name = key;
            input.value = formData[key];
            form.appendChild(input);
        }

        var link = document.createElement('a');
        container.appendChild(link);

        link.addEventListener('click', function(event) { form.submit(); event.preventDefault(); });
        link.href = '#';
        link.appendChild(document.createTextNode('File bug for ' + (failingTests.length > 1 ? 'these failures' : 'this failure')));

        return result;
    },

    _domForPossiblyFlakyTests: function(builder, possiblyFlakyTestData) {
        var result = document.createDocumentFragment();
        var flakyTests = Object.keys(possiblyFlakyTestData);
        if (!flakyTests.length)
            return result;

        var flakyHeader = document.createElement('h2');
        result.appendChild(flakyHeader);
        flakyHeader.appendChild(document.createTextNode('Possibly Flaky Tests'));

        var flakyList = document.createElement('ol');
        result.appendChild(flakyList);

        var self = this;
        flakyList.appendChildren(sorted(flakyTests).map(function(testName) {
            var item = document.createElement('li');
            item.appendChild(document.createTextNode(testName));
            var historyList = document.createElement('ol');
            item.appendChild(historyList);
            historyList.appendChildren(possiblyFlakyTestData[testName].map(function(historyItem) {
                var item = document.createElement('li');
                item.appendChild(self._domForBuildName(builder, historyItem.build));
                item.appendChild(document.createTextNode(': '));
                item.appendChild(self._domForFailureDiagnosis(builder, historyItem.build, testName, historyItem.result));
                return item;
            }));
            return item;
        }));

        return result;
    },
};
