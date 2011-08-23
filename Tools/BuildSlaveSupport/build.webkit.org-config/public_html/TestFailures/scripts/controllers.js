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

var controllers = controllers || {};

(function(){

controllers.ResultsDetails = base.extends(Object, {
    init: function(view, resultsByTest)
    {
        this._view = view;
        this._resultsByTest = resultsByTest;

        this._view.setTestList(Object.keys(this._resultsByTest));
        $(this._view).bind('testselected', this.onTestSelected.bind(this));
        $(this._view).bind('builderselected', this.onBuilderSelected.bind(this));
        $(this._view).bind('rebaseline', this.onRebaseline.bind(this));
        // FIXME: Bind the next/previous events.
    },
    _failureInfoForTestAndBuilder: function(testName, builderName)
    {
        return {
            'testName': testName,
            'builderName': builderName,
            'failureTypeList': results.failureTypeList(this._resultsByTest[testName][builderName].actual)
        }
    },
    dismiss: function()
    {
        $(this._view).detach();
    },
    showTest: function(testName)
    {
        var builderNameList = Object.keys(this._resultsByTest[testName]);
        this._view.setBuilderList(builderNameList)
        this._view.showResults(this._failureInfoForTestAndBuilder(testName, builderNameList[0]));
    },
    onTestSelected: function(event, testName)
    {
        this.showTest(testName)
    },
    onBuilderSelected: function() {
        this._view.showResults(this._failureInfoForTestAndBuilder(this._view.currentTestName(), this._view.currentBuilderName()));
    },
    onRebaseline: function() {
        var testName = this._view.currentTestName();
        var builderName = this._view.currentBuilderName();
        model.queueForRebaseline({
            'testName': testName,
            'builderName': builderName
        });
    }
});

controllers.UnexpectedFailures = base.extends(Object, {
    init: function(view)
    {
        this._view = view;
        this._testFailures = new base.UpdateTracker();
    },
    update: function(failureAnalysis)
    {
        var key = failureAnalysis.newestPassingRevision + "+" + failureAnalysis.oldestFailingRevision;
        var failure = this._testFailures.get(key);
        if (!failure) {
            failure = new ui.notifications.TestFailures();
            model.commitDataListForRevisionRange(failureAnalysis.newestPassingRevision + 1, failureAnalysis.oldestFailingRevision).forEach(function(commitData) {
                failure.addCommitData(commitData);
            });
            this._view.add(failure);
            $(failure).bind('examine', function() {
                this.onExamine(failure);
            }.bind(this));
        }
        failure.addFailureAnalysis(failureAnalysis);
        this._testFailures.update(key, failure);
    },
    purge: function() {
        this._testFailures.purge(function(failure) {
            failure.dismiss();
        });
    },
    onExamine: function(failures)
    {
        var resultsView = new ui.results.View({
            fetchResultsURLs: results.fetchResultsURLs
        });

        var testNameList = failures.testNameList();
        var failuresByTest = results.unexpectedFailuresByTest(model.state.resultsByBuilder)
        var controller = new controllers.ResultsDetails(resultsView, base.filterDictionary(failuresByTest, function(key) {
            return testNameList.indexOf(testNameList) != -1;
        }));

        // FIXME: This doesn't belong here. Also, we need some way to call controller.dismiss().
        document.body.appendChild(resultsView);
    }
});

controllers.FailingBuilders = base.extends(Object, {
    init: function(view)
    {
        this._view = view;
        this._notification = null;
    },
    update: function(builderNameList)
    {
        if (builderNameList.length == 0) {
            if (this._notification) {
                this._notification.dismiss();
                this._notification = null;
            }
            return;
        }
        if (!this._notification) {
            this._notification = new ui.notifications.BuildersFailing();
            this._view.add(this._notification);
        }
        // FIXME: We should provide regression ranges for the failing builders.
        // This doesn't seem to happen often enough to worry too much about that, however.
        this._notification.setFailingBuilders(builderNameList);
    }
});

})();
