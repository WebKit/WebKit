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

var g_info = null;

var g_updateTimerId = 0;
var g_buildersFailing = null;

var g_unexpectedFailures = null;
var g_failingBuilders = null;

function update()
{
    // FIXME: This should be a button with a progress element.
    var updating = new ui.notifications.Info('Updating ...');

    g_info.add(updating);

    builders.buildersFailingStepRequredForTestCoverage(g_failingBuilders.update.bind(g_failingBuilders));

    base.callInParallel([model.updateRecentCommits, model.updateResultsByBuilder], function() {
        model.analyzeUnexpectedFailures(g_unexpectedFailures.update.bind(g_unexpectedFailures), function() {
            g_unexpectedFailures.purge();
            updating.dismiss();
        });
    });
}

$(document).ready(function() {
    g_updateTimerId = window.setInterval(update, config.kUpdateFrequency);

    checkout.registerUnavailableCheckoutCallback(function() {
        alert('Please run "webkit-patch garden-o-matic" to enable this feature.');
    });

    var actions = new ui.notifications.Stream();
    g_unexpectedFailures = new controllers.UnexpectedFailures(actions);

    g_info = new ui.notifications.Stream();
    g_failingBuilders = new controllers.FailingBuilders(g_info);

    document.body.insertBefore(actions, document.body.firstChild);
    document.body.insertBefore(g_info, document.body.firstChild);

    // FIXME: This should be an Action object.
    var button = document.body.insertBefore(document.createElement('button'), document.body.firstChild);
    button.addEventListener("click", update);
    button.textContent = 'update';

    update();
});

})();
