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

var builders = builders || {};

(function() {

var kChromiumBuildBotURL = 'http://build.chromium.org/p/chromium.webkit';

var kUpdateStepName = 'update';
var kUpdateScriptsStepName = 'update_scripts';
var kCompileStepName = 'compile';
var kWebKitTestsStepName = 'webkit_tests';

var kCrashedOrHungOutputMarker = 'crashed or hung';

function urlForBuildInfo(builderName, buildNumber)
{
    return kChromiumBuildBotURL + '/json/builders/' + encodeURIComponent(builderName) + '/builds/' + encodeURIComponent(buildNumber);
}

function didFail(step)
{
    if (step.name == kWebKitTestsStepName) {
        // run-webkit-tests fails to generate test coverage when it crashes or hangs.
        return step.text.indexOf(kCrashedOrHungOutputMarker) != -1;
    }
    return step.results[0] > 0;
}

function failingSteps(buildInfo)
{
    return buildInfo.steps.filter(didFail);
}

function mostRecentCompletedBuildNumber(individualBuilderStatus)
{
    if (!individualBuilderStatus)
        return null;

    for (var i = individualBuilderStatus.cachedBuilds.length - 1; i >= 0; --i) {
        var buildNumber = individualBuilderStatus.cachedBuilds[i];
        if (individualBuilderStatus.currentBuilds.indexOf(buildNumber) == -1)
            return buildNumber;
    }

    return null;
}

var g_buildInfoCache = new base.AsynchronousCache(function(key, callback) {
    var explodedKey = key.split('\n');
    net.get(urlForBuildInfo(explodedKey[0], explodedKey[1]), callback);
});

function fetchMostRecentBuildInfoByBuilder(callback)
{
    net.get(kChromiumBuildBotURL + '/json/builders', function(builderStatus) {
        var buildInfoByBuilder = {};
        var builderNames = Object.keys(builderStatus);
        var requestTracker = new base.RequestTracker(builderNames.length, callback, [buildInfoByBuilder]);
        builderNames.forEach(function(builderName) {
            // FIXME: Should garden-o-matic show these? I can imagine showing the deps bots being useful at least so
            // that the gardener only need to look at garden-o-matic and never at the waterfall. Not really sure who
            // watches the GPU bots.
            if (builderName.indexOf('GPU') != -1 || builderName.indexOf('deps') != -1) {
                requestTracker.requestComplete();
                return;
            }

            var buildNumber = mostRecentCompletedBuildNumber(builderStatus[builderName]);
            if (!buildNumber) {
                buildInfoByBuilder[builderName] = null;
                requestTracker.requestComplete();
                return;
            }

            g_buildInfoCache.get(builderName + '\n' + buildNumber, function(buildInfo) {
                buildInfoByBuilder[builderName] = buildInfo;
                requestTracker.requestComplete();
            });
        });
    });
}

builders.buildersFailingNonLayoutTests = function(callback)
{
    fetchMostRecentBuildInfoByBuilder(function(buildInfoByBuilder) {
        var failureList = {};
        $.each(buildInfoByBuilder, function(builderName, buildInfo) {
            if (!buildInfo)
                return;
            var failures = failingSteps(buildInfo);
            if (failures.length)
                failureList[builderName] = failures.map(function(failure) { return failure.name; });
        });
        callback(failureList);
    });
};

})();
