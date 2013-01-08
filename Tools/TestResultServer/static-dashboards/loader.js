// Copyright (C) 2012 Google Inc. All rights reserved.
// Copyright (C) 2012 Zan Dobersek <zandobersek@gmail.com>
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//         * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//         * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//         * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

var loader = loader || {};

(function() {

var TEST_RESULTS_SERVER = 'http://test-results.appspot.com/';
var CHROMIUM_EXPECTATIONS_URL = 'http://svn.webkit.org/repository/webkit/trunk/LayoutTests/platform/chromium/TestExpectations';

function pathToBuilderResultsFile(builderName) {
    return TEST_RESULTS_SERVER + 'testfile?builder=' + builderName +
           '&master=' + builderMaster(builderName).name +
           '&testtype=' + g_crossDashboardState.testType + '&name=';
}

loader.request = function(url, success, error, opt_isBinaryData)
{
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    if (opt_isBinaryData)
        xhr.overrideMimeType('text/plain; charset=x-user-defined');
    xhr.onreadystatechange = function(e) {
        if (xhr.readyState == 4) {
            if (xhr.status == 200)
                success(xhr);
            else
                error(xhr);
        }
    }
    xhr.send();
}

loader.Loader = function()
{
    this._loadingSteps = [
        this._loadBuildersList,
        this._loadResultsFiles,
        this._loadExpectationsFiles,
    ];

    this._buildersThatFailedToLoad = [];
    this._staleBuilders = [];
}

loader.Loader.prototype = {
    load: function()
    {
        this._loadNext();
    },
    _loadNext: function()
    {
        var loadingStep = this._loadingSteps.shift();
        if (!loadingStep) {
            resourceLoadingComplete(this._getLoadingErrorMessages());
            return;
        }
        loadingStep.apply(this);
    },
    _loadBuildersList: function()
    {
        loadBuildersList(g_crossDashboardState.group, g_crossDashboardState.testType);
        this._loadNext();
    },
    _loadResultsFiles: function()
    {
        parseParameters();

        for (var builderName in currentBuilders())
            this._loadResultsFileForBuilder(builderName);
    },
    _loadResultsFileForBuilder: function(builderName)
    {
        var resultsFilename;
        if (isTreeMap())
            resultsFilename = 'times_ms.json';
        else if (g_crossDashboardState.showAllRuns)
            resultsFilename = 'results.json';
        else
            resultsFilename = 'results-small.json';

        var resultsFileLocation = pathToBuilderResultsFile(builderName) + resultsFilename;
        loader.request(resultsFileLocation,
                partial(function(loader, builderName, xhr) {
                    loader._handleResultsFileLoaded(builderName, xhr.responseText);
                }, this, builderName),
                partial(function(loader, builderName, xhr) {
                    loader._handleResultsFileLoadError(builderName);
                }, this, builderName));
    },
    _handleResultsFileLoaded: function(builderName, fileData)
    {
        if (isTreeMap())
            this._processTimesJSONData(builderName, fileData);
        else
            this._processResultsJSONData(builderName, fileData);

        // We need this work-around for webkit.org/b/50589.
        if (!g_resultsByBuilder[builderName]) {
            this._handleResultsFileLoadError(builderName);
            return;
        }

        this._handleResourceLoad();
    },
    _processTimesJSONData: function(builderName, fileData)
    {
        // FIXME: We should probably include the builderName in the JSON
        // rather than relying on only loading one JSON file per page.
        g_resultsByBuilder[builderName] = JSON.parse(fileData);
    },
    _processResultsJSONData: function(builderName, fileData)
    {
        var builds = JSON.parse(fileData);

        var json_version = builds['version'];
        for (var builderName in builds) {
            if (builderName == 'version')
                continue;

            // If a test suite stops being run on a given builder, we don't want to show it.
            // Assume any builder without a run in two weeks for a given test suite isn't
            // running that suite anymore.
            // FIXME: Grab which bots run which tests directly from the buildbot JSON instead.
            var lastRunSeconds = builds[builderName].secondsSinceEpoch[0];
            if ((Date.now() / 1000) - lastRunSeconds > ONE_WEEK_SECONDS)
                continue;

            if ((Date.now() / 1000) - lastRunSeconds > ONE_DAY_SECONDS)
                this._staleBuilders.push(builderName);

            if (json_version >= 4)
                builds[builderName][TESTS_KEY] = flattenTrie(builds[builderName][TESTS_KEY]);
            g_resultsByBuilder[builderName] = builds[builderName];
        }
    },
    _handleResultsFileLoadError: function(builderName)
    {
        console.error('Failed to load results file for ' + builderName + '.');

        // FIXME: loader shouldn't depend on state defined in dashboard_base.js.
        this._buildersThatFailedToLoad.push(builderName);

        // Remove this builder from builders, so we don't try to use the
        // data that isn't there.
        delete currentBuilders()[builderName];

        // Change the default builder name if it has been deleted.
        if (g_defaultDashboardSpecificStateValues.builder == builderName) {
            var defaultBuilderName = currentBuilderGroup().defaultBuilder();
            g_defaultDashboardSpecificStateValues.builder = defaultBuilderName;
            if (!defaultBuilderName) {
                var error = 'No tests results found for ' + g_crossDashboardState.testType + '. Reload the page to try fetching it again.';
                console.error(error);
                addError(error);
            }
       }

        // Proceed as if the resource had loaded.
        this._handleResourceLoad();
    },
    _handleResourceLoad: function()
    {
        if (this._haveResultsFilesLoaded())
            this._loadNext();
    },
    _haveResultsFilesLoaded: function()
    {
        for (var builder in currentBuilders()) {
            if (!g_resultsByBuilder[builder])
                return false;
        }
        return true;
    },
    _loadExpectationsFiles: function()
    {
        if (!isFlakinessDashboard() && !g_crossDashboardState.useTestData) {
            this._loadNext();
            return;
        }

        var expectationsFilesToRequest = {};
        traversePlatformsTree(function(platform, platformName) {
            if (platform.fallbackPlatforms)
                platform.fallbackPlatforms.forEach(function(fallbackPlatform) {
                    var fallbackPlatformObject = platformObjectForName(fallbackPlatform);
                    if (fallbackPlatformObject.expectationsDirectory && !(fallbackPlatform in expectationsFilesToRequest))
                        expectationsFilesToRequest[fallbackPlatform] = EXPECTATIONS_URL_BASE_PATH + fallbackPlatformObject.expectationsDirectory + '/TestExpectations';
                });

            if (platform.expectationsDirectory)
                expectationsFilesToRequest[platformName] = EXPECTATIONS_URL_BASE_PATH + platform.expectationsDirectory + '/TestExpectations';
        });

        for (platformWithExpectations in expectationsFilesToRequest)
            loader.request(expectationsFilesToRequest[platformWithExpectations],
                    partial(function(loader, platformName, xhr) {
                        g_expectationsByPlatform[platformName] = getParsedExpectations(xhr.responseText);

                        delete expectationsFilesToRequest[platformName];
                        if (!Object.keys(expectationsFilesToRequest).length)
                            loader._loadNext();
                    }, this, platformWithExpectations),
                    partial(function(platformName, xhr) {
                        console.error('Could not load expectations file for ' + platformName);
                    }, platformWithExpectations));
    },
    _getLoadingErrorMessages: function()
    {
        var errorMsgs = '';
        if (this._buildersThatFailedToLoad.length)
            errorMsgs += 'ERROR: Failed to get data from ' + this._buildersThatFailedToLoad.toString() + '.<br>';

        if (this._staleBuilders.length)
            errorMsgs +='ERROR: Data from ' + this._staleBuilders.toString() + ' is more than 1 day stale.<br>';

        return errorMsgs;
    }
}

})();
