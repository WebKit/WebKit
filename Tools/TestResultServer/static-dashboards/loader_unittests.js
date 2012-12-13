// Copyright (C) Zan Dobersek <zandobersek@gmail.com>
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

module('loader');

test('loading steps', 1, function() {
    resetGlobals();
    var loadedSteps = [];
    var resourceLoader = new loader.Loader();
    function loadingStep1() {
        loadedSteps.push('step 1');
        resourceLoader.load();
    }
    function loadingStep2() {
        loadedSteps.push('step 2');
        resourceLoader.load();
    }

    var loadingCompleteCallback = resourceLoadingComplete;
    resourceLoadingComplete = function() {
        deepEqual(loadedSteps, ['step 1', 'step 2']);
    }

    try {
        resourceLoader._loadingSteps = [loadingStep1, loadingStep2];
        resourceLoader.load();
    } finally {
        resourceLoadingComplete = loadingCompleteCallback;
    }
});

test('results files loading', 5, function() {
    resetGlobals();
    var expectedLoadedBuilders = ["WebKit Linux", "WebKit Win"];
    var loadedBuilders = [];
    var resourceLoader = new loader.Loader();
    resourceLoader._loadNext = function() {
        deepEqual(loadedBuilders.sort(), expectedLoadedBuilders);
        loadedBuilders.forEach(function(builderName) {
            ok('secondsSinceEpoch' in g_resultsByBuilder[builderName]);
            deepEqual(g_resultsByBuilder[builderName].tests, {});
        });
    }

    var requestFunction = loader.request;
    loader.request = function(url, successCallback, errorCallback) {
        var builderName = /builder=([\w ]+)&/.exec(url)[1];
        loadedBuilders.push(builderName);
        successCallback({responseText: '{"version": 4, "' + builderName + '": {"secondsSinceEpoch": [' + Date.now() + '], "tests": {}}}'});
    }

    g_builders = {"WebKit Linux": true, "WebKit Win": true};

    builders.masters['ChromiumWebkit'] = {'tests': {'layout-tests': {builders: ["WebKit Linux", "WebKit Win"]}}};
    loadBuildersList('@ToT - chromium.org', 'layout-tests');

    try {
        resourceLoader._loadResultsFiles();
    } finally {
        loader.request = requestFunction;
    }
});

test('expectations files loading', 1, function() {
    resetGlobals();
    parseCrossDashboardParameters();
    var expectedLoadedPlatforms = ["chromium", "chromium-android", "efl", "efl-wk1", "efl-wk2", "gtk",
                                   "gtk-wk2", "mac", "mac-lion", "mac-snowleopard", "qt", "win", "wk2"];
    var loadedPlatforms = [];
    var resourceLoader = new loader.Loader();
    resourceLoader._loadNext = function() {
        deepEqual(loadedPlatforms.sort(), expectedLoadedPlatforms);
    }

    var requestFunction = loader.request;
    loader.request = function(url, successCallback, errorCallback) {
        loadedPlatforms.push(/LayoutTests\/platform\/(.+)\/TestExpectations/.exec(url)[1]);
        successCallback({responseText: ''});
    }

    try {
        resourceLoader._loadExpectationsFiles();
    } finally {
        loader.request = requestFunction;
    }
});

test('results file failing to load', 2, function() {
    resetGlobals();
    // FIXME: loader shouldn't depend on state defined in dashboard_base.js.
    g_buildersThatFailedToLoad = [];

    var resourceLoader = new loader.Loader();
    var resourceLoadCount = 0;
    resourceLoader._handleResourceLoad = function() {
        resourceLoadCount++;
    }

    var builder1 = 'builder1';
    g_builders[builder1] = true;
    resourceLoader._handleResultsFileLoadError(builder1);

    var builder2 = 'builder2';
    g_builders[builder2] = true;
    resourceLoader._handleResultsFileLoadError(builder2);

    deepEqual(g_buildersThatFailedToLoad, [builder1, builder2]);
    equal(resourceLoadCount, 2);

})
