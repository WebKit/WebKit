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
        this._view.setResultsByTest(resultsByTest);
        // FIXME: Wire up some actions.
    },
});

var FailureStreamController = base.extends(Object, {
    _resultsFilter: null,
    _keyFor: function(failureAnalysis) { throw "Not implemented!"; },
    _createFailureView: function(failureAnalysis) { throw "Not implemented!"; },

    init: function(view)
    {
        this._view = view;
        this._testFailures = new base.UpdateTracker();
    },
    update: function(failureAnalysis)
    {
        var key = this._keyFor(failureAnalysis);
        var failure = this._testFailures.get(key);
        if (!failure) {
            failure = this._createFailureView(failureAnalysis);
            this._view.add(failure);
            $(failure).bind('examine', function() {
                this.onExamine(failure);
            }.bind(this));
            $(failure).bind('rebaseline', function() {
                this.onRebaseline(failure);
            }.bind(this));
        }
        failure.addFailureAnalysis(failureAnalysis);
        this._testFailures.update(key, failure);
        return failure;
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
        var failuresByTest = base.filterDictionary(
            this._resultsFilter(model.state.resultsByBuilder),
            function(key) {
                return testNameList.indexOf(key) != -1;
            });

        var controller = new controllers.ResultsDetails(resultsView, failuresByTest);

        // FIXME: This doesn't belong here.
        var onebar = $('#onebar')[0];
        var resultsContainer = onebar.results();
        $(resultsContainer).empty().append(resultsView);
        onebar.select('results');
    },
    onRebaseline: function(failures)
    {
        failureInfoList = base.flattenArray(failures.testNameList().map(model.unexpectedFailureInfoForTestName));
        checkout.rebaseline(failureInfoList, function() {
            // FIXME: We should have a better dialog than this!
            alert('Rebaseline done! Please land with "webkit-patch land-cowboy".');
        });
    }
});

controllers.UnexpectedFailures = base.extends(FailureStreamController, {
    _resultsFilter: results.expectedOrUnexpectedFailuresByTest,

    _impliedFirstFailingRevision: function(failureAnalysis)
    {
        return failureAnalysis.newestPassingRevision + 1;
    },
    _keyFor: function(failureAnalysis)
    {
        return failureAnalysis.newestPassingRevision + "+" + failureAnalysis.oldestFailingRevision;
    },
    _createFailureView: function(failureAnalysis)
    {
        var failure = new ui.notifications.FailingTestsSummary();
        model.commitDataListForRevisionRange(this._impliedFirstFailingRevision(failureAnalysis), failureAnalysis.oldestFailingRevision).forEach(function(commitData) {
            var suspiciousCommit = failure.addCommitData(commitData);
            $(suspiciousCommit).bind('rollout', function() {
                this.onRollout(commitData.revision, failure.testNameList());
            }.bind(this));
            $(failure).bind('blame', function() {
                this.onBlame(failure, commitData);
            }.bind(this));
        }, this);

        return failure;
    },
    update: function(failureAnalysis)
    {
        var failure = FailureStreamController.prototype.update.call(this, failureAnalysis);
        failure.updateBuilderResults(model.buildersInFlightForRevision(this._impliedFirstFailingRevision(failureAnalysis)));
    },
    onBlame: function(failure, commitData)
    {
        failure.pinToCommitData(commitData);
        $('.action', failure).each(function() {
            // FIXME: This isn't the right way of finding and disabling this action.
            if (this.textContent == 'Blame')
                this.disabled = true;
        });
    },
    onRollout: function(revision, testNameList)
    {
        checkout.rollout(revision, ui.rolloutReasonForTestNameList(testNameList), $.noop);
    }
});

controllers.Failures = base.extends(FailureStreamController, {
    _resultsFilter: results.expectedOrUnexpectedFailuresByTest,

    _keyFor: function(failureAnalysis)
    {
        return base.dirName(failureAnalysis.testName);
    },
    _createFailureView: function(failureAnalysis)
    {
        return new ui.notifications.FailingTests();
    },
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
