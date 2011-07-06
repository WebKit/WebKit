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

function LayoutTestResultsLoader(builder) {
    this._builder = builder;
}

LayoutTestResultsLoader.prototype = {
    start: function(buildName, callback, errorCallback) {
        var cacheKey = 'LayoutTestResultsLoader.' + this._builder.name + '.' + buildName;
        const currentCachedDataVersion = 2;
        if (PersistentCache.contains(cacheKey)) {
            var cachedData = PersistentCache.get(cacheKey);
            if (cachedData.version === currentCachedDataVersion) {
                callback(cachedData.tests, cachedData.tooManyFailures);
                return;
            }
        }

        var result = { tests: {}, tooManyFailures: false, version: currentCachedDataVersion };

        var parsedBuildName = this._builder.buildbot.parseBuildName(buildName);

        // http://webkit.org/b/62380 was fixed in r89610.
        var resultsHTMLSupportsTooManyFailuresInfo = parsedBuildName.revision >= 89610;

        var self = this;

        function fetchAndParseResultsHTMLAndCallCallback(callback) {
            getResource(self._builder.resultsPageURL(buildName), function(xhr) {
                var parseResult = (new ORWTResultsParser()).parse(xhr.responseText);
                result.tests = parseResult.tests;
                if (resultsHTMLSupportsTooManyFailuresInfo)
                    result.tooManyFailures = parseResult.tooManyFailures;

                PersistentCache.set(cacheKey, result);
                callback(result.tests, result.tooManyFailures);
            },
            function(xhr) {
                // We failed to fetch results.html. run-webkit-tests must have aborted early.
                PersistentCache.set(cacheKey, result);
                errorCallback(result.tests, result.tooManyFailures);
            });
        }

        if (resultsHTMLSupportsTooManyFailuresInfo) {
            fetchAndParseResultsHTMLAndCallCallback(callback);
            return;
        }

        self._builder.getNumberOfFailingTests(parsedBuildName.buildNumber, function(failingTestCount, tooManyFailures) {
            result.tooManyFailures = tooManyFailures;

            if (failingTestCount < 0) {
                // The number of failing tests couldn't be determined.
                PersistentCache.set(cacheKey, result);
                errorCallback(result.tests, result.tooManyFailures);
                return;
            }

            if (!failingTestCount) {
                // All tests passed.
                PersistentCache.set(cacheKey, result);
                errorCallback(result.tests, result.tooManyFailures);
                return;
            }

            // Find out which tests failed.
            fetchAndParseResultsHTMLAndCallCallback(callback);
        });
    },
};
