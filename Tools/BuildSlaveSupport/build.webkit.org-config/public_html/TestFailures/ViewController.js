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

function ViewController(buildbot) {
    this._buildbot = buildbot;

    var self = this;
    addEventListener('load', function() { self.loaded() }, false);
    addEventListener('hashchange', function() { self.parseHash(location.hash) }, false);
}

ViewController.prototype = {
    loaded: function() {
        this.parseHash(location.hash);
    },

    parseHash: function(hash) {
        var match = /#\/(.*)/.exec(hash);
        if (match)
            this._displayBuilder(this._buildbot.builderNamed(match[1]));
        else
            this._displayTesters();
    },

    _displayBuilder: function(builder) {
        var self = this;
        builder.startFetchingBuildHistory(function(history) {
            var list = document.createElement('ol');
            Object.keys(history).forEach(function(buildName, buildIndex, buildNameArray) {
                if (!Object.keys(history[buildName].tests).length)
                    return;
                var dlItems = [
                    [document.createTextNode('Failed'), self._domForBuildName(builder, buildName)],
                ];
                if (buildIndex + 1 < buildNameArray.length)
                    dlItems.push([document.createTextNode('Passed'), self._domForBuildName(builder, buildNameArray[buildIndex + 1])]);

                var item = document.createElement('li');
                item.appendChild(createDefinitionList(dlItems));
                list.appendChild(item);

                if (history[buildName].tooManyFailures) {
                    var p = document.createElement('p');
                    p.className = 'info';
                    p.appendChild(document.createTextNode('run-webkit-tests exited early due to too many failures/crashes/timeouts'));
                    item.appendChild(p);
                }

                var testList = document.createElement('ol');
                for (var testName in history[buildName].tests) {
                    var testItem = document.createElement('li');
                    testItem.appendChild(self._domForFailedTest(builder, buildName, testName, history[buildName].tests[testName]));
                    testList.appendChild(testItem);
                }
                item.appendChild(testList);
            });

            var header = document.createElement('h1');
            header.appendChild(document.createTextNode(builder.name));
            document.body.innerHTML = '';
            document.title = builder.name;
            document.body.appendChild(header);
            document.body.appendChild(list);
        });
    },

    _displayTesters: function() {
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

        this._buildbot.getTesters(function(testers) {
            testers.forEach(function(tester) {
                tester.getMostRecentCompletedBuildNumber(function(buildNumber) {
                    if (buildNumber < 0)
                        return;
                    tester.getNumberOfFailingTests(buildNumber, function(failureCount, tooManyFailures) {
                        if (failureCount <= 0)
                            return;
                        latestBuildInfos.push({ tester: tester, failureCount: failureCount, tooManyFailures: tooManyFailures });
                        updateList();
                    });
                });
            });

            document.body.innerHTML = '';
            document.title = 'Testers';
            document.body.appendChild(list);
        });
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
        resultsLink.href = builder.resultsDirectoryURL(buildName) + 'results.html';
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

    _domForFailedTest: function(builder, buildName, testName, failureType) {
        var diagnosticInfo = builder.failureDiagnosisTextAndURL(buildName, testName, failureType);

        var result = document.createDocumentFragment();
        result.appendChild(document.createTextNode(testName));
        result.appendChild(document.createTextNode(' ('));

        var textNode = document.createTextNode(diagnosticInfo.text);
        if ('url' in diagnosticInfo) {
            var link = document.createElement('a');
            link.href = diagnosticInfo.url;
            link.appendChild(textNode);
            result.appendChild(link);
        } else
            result.appendChild(textNode);

        result.appendChild(document.createTextNode(')'));

        return result;
    },
};
