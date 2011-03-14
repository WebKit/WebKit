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

function RecentBuildsLoader(didLoadRecentBuildsCallback) {
    this._didLoadRecentBuildsCallback = didLoadRecentBuildsCallback;
}

RecentBuildsLoader.prototype = {
    start: function(builderName, maximumNumberOfBuilds) {
        var url = this._buildbotBaseURL + "/json/builders/" + builderName + "/builds/?";
        url += range(maximumNumberOfBuilds).map(function(n) { return "select=-" + (n + 1); }).join("&");
        var self = this;
        getResource(url, function(xhr) {
            var data = JSON.parse(xhr.responseText);
            var builds = [];
            Object.keys(data).forEach(function(buildNumber) {
                var build = data[buildNumber];

                var buildInfo = {
                    revision: build.sourceStamp.changes[0].rev,
                    leakCount: 0,
                    url: null,
                };
                for (var stepIndex = 0; stepIndex < build.steps.length; ++stepIndex) {
                    var step = build.steps[stepIndex];
                    if (step.name === "layout-test") {
                        if (!("results" in step))
                            continue;
                        // The first item of results is a status code, the second is the status text.
                        var strings = step.results[1];
                        for (var stringIndex = 0; stringIndex < strings.length; ++stringIndex) {
                            var match = /^(\d+) total leaks found!$/.exec(strings[stringIndex]);
                            if (!match)
                                continue;
                            buildInfo.leakCount = parseInt(match[1], 10);
                            break;
                        }
                    } else if (step.name === "MasterShellCommand") {
                        if (!("urls" in step))
                            return;
                        if (!("view results" in step.urls))
                            return;
                        buildInfo.url = self._buildbotBaseURL + step.urls["view results"] + "/";
                    }

                    if (buildInfo.leakCount && buildInfo.url) {
                        builds.push(buildInfo);
                        break;
                    }
                }
            });
            self._didLoadRecentBuildsCallback(builds);
        });
    },

    _buildbotBaseURL: "http://build.webkit.org/",
};
