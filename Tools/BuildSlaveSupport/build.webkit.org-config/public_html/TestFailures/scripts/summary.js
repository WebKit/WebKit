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

(function() {

var g_actions = new ui.notifications.Stream();
var g_info = new ui.notifications.Stream();

var g_updateTimerId = 0;
var g_testFailures = new base.UpdateTracker();
var g_buildersFailing = null;

function update()
{
    // FIXME: This should be a button with a progress element.
    var updating = new ui.notifications.Info("Updating ...");
    g_info.add(updating);

    builders.buildersFailingStepRequredForTestCoverage(function(builderNameList) {
        if (builderNameList.length == 0) {
            if (g_buildersFailing) {
                g_buildersFailing.dismiss();
                g_buildersFailing = null;
            }
            return;
        }
        if (!g_buildersFailing) {
            g_buildersFailing = new ui.notifications.BuildersFailing();
            g_info.add(g_buildersFailing);
        }
        // FIXME: We should provide regression ranges for the failing builders.
        // This doesn't seem to happen often enough to worry too much about that, however.
        g_buildersFailing.setFailingBuilders(builderNameList);
    });

    base.callInParallel([model.updateRecentCommits, model.updateResultsByBuilder], function() {
        model.analyzeUnexpectedFailures(function(failureAnalysis) {
            var key = failureAnalysis.newestPassingRevision + "+" + failureAnalysis.oldestFailingRevision;
            var failure = g_testFailures.get(key);
            if (!failure) {
                failure = new ui.notifications.TestFailures();
                model.commitDataListForRevisionRange(failureAnalysis.newestPassingRevision + 1, failureAnalysis.oldestFailingRevision).forEach(function(commitData) {
                    failure.addCommitData(commitData);
                });
                g_actions.add(failure);
            }
            failure.addFailureAnalysis(failureAnalysis);
            g_testFailures.update(key, failure);
        }, function() {
            g_testFailures.purge(function(failure) {
                failure.dismiss();
            });
            updating.dismiss();
        });
    });
}

$(document).ready(function() {
    g_updateTimerId = window.setInterval(update, config.kUpdateFrequency);
    document.body.insertBefore(g_actions, document.body.firstChild);
    document.body.insertBefore(g_info, document.body.firstChild);
    var button = document.body.insertBefore(document.createElement("button"), document.body.firstChild);
    button.addEventListener("click", update);
    button.textContent = 'update';
    update();
});

})();
