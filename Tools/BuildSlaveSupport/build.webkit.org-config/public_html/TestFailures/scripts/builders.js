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

function isStepRequredForTestCoverage(step)
{
    switch(step.name) {
    case kUpdateStepName:
    case kUpdateScriptsStepName:
    case kCompileStepName:
    case kWebKitTestsStepName:
        return true;
    default:
        return false;
    }
}

function didFail(step)
{
    if (step.name == kWebKitTestsStepName) {
        // run-webkit-tests fails to generate test coverage when it crashes or hangs.
        return step.text.indexOf(kCrashedOrHungOutputMarker) != -1;
    }
    // FIXME: Is this the correct way to test for failure?
    return step.results[0] > 0;
}

function didFailStepRequredForTestCoverage(buildInfo)
{
    return buildInfo.steps.filter(isStepRequredForTestCoverage).filter(didFail).length > 0;
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
    var buildInfoByBuilder = {};
    var builderNames = Object.keys(config.kBuilders);
    var requestTracker = new base.RequestTracker(builderNames.length, callback, [buildInfoByBuilder]);
    net.get(kChromiumBuildBotURL + '/json/builders', function(builderStatus) {
        $.each(builderNames, function(index, builderName) {
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

builders.buildersFailingStepRequredForTestCoverage = function(callback)
{
    fetchMostRecentBuildInfoByBuilder(function(buildInfoByBuilder) {
        var builderNameList = [];
        $.each(buildInfoByBuilder, function(builderName, buildInfo) {
            if (!buildInfo)
                return;
            if (didFailStepRequredForTestCoverage(buildInfo))
                builderNameList.push(builderName);
        });
        callback(builderNameList);
    });
};

})();
