// Copyright (C) 2012 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
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

// @fileoverview File that lists builders, their masters, and logical groupings
// of them.

function BuilderMaster(name, basePath)
{
    this.name = name;
    this.basePath = basePath;
}

BuilderMaster.prototype.getLogPath = function(builder, buildNumber)
{
    return this.basePath + builder + '/builds/' + buildNumber;
};

CHROMIUM_BUILDER_MASTER = new BuilderMaster('Chromium', 'http://build.chromium.org/p/chromium/builders/');
CHROMIUMOS_BUILDER_MASTER = new BuilderMaster('ChromiumChromiumOS', 'http://build.chromium.org/p/chromium.chromiumos/builders/');
CHROMIUM_GPU_BUILDER_MASTER = new BuilderMaster('ChromiumGPU', 'http://build.chromium.org/p/chromium.gpu/builders/');
CHROMIUM_GPU_FYI_BUILDER_MASTER = new BuilderMaster('ChromiumGPUFYI', 'http://build.chromium.org/p/chromium.gpu.fyi/builders/');
CHROMIUM_WEBKIT_BUILDER_MASTER = new BuilderMaster('ChromiumWebkit', 'http://build.chromium.org/p/chromium.webkit/builders/');
WEBKIT_BUILDER_MASTER = new BuilderMaster('webkit.org', 'http://build.webkit.org/builders/');

var LEGACY_BUILDER_MASTERS_TO_GROUPS = {
    'Chromium': '@DEPS - chromium.org',
    'ChromiumChromiumOS': '@DEPS CrOS - chromium.org',
    'ChromiumGPU': '@DEPS - chromium.org',
    'ChromiumGPUFYI': '@DEPS FYI - chromium.org',
    'ChromiumWebkit': '@ToT - chromium.org',
    'webkit.org': '@ToT - webkit.org'
};

function BuilderGroup(isToTWebKit, builders)
{
    this.isToTWebKit = isToTWebKit;
    // Map of builderName (the name shown in the waterfall) to builderPath (the
    // path used in the builder's URL)
    this.builders = {};
    builders.forEach(function(builderAndFlags) {
        var builder = builderAndFlags[0];
        var flags = builderAndFlags[1];

        this.builders[builder] = builder.replace(/[ .()]/g, '_');
        if (flags & BuilderGroup.DEFAULT_BUILDER)
            this.defaultBuilder = builder;
    }, this);
}

BuilderGroup.prototype.setup = function()
{
    // FIXME: instead of copying these to globals, it would be better if
    // the rest of the code read things from the BuilderGroup instance directly
    g_defaultBuilderName = this.defaultBuilder;
    g_builders = this.builders;
};

BuilderGroup.TOT_WEBKIT = true;
BuilderGroup.DEPS_WEBKIT = false;
BuilderGroup.DEFAULT_BUILDER = 1 << 1;

var BUILDER_TO_MASTER = {};
function associateBuildersWithMaster(builders, master)
{
    builders.forEach(function(builderAndFlags) {
        var builder = builderAndFlags[0];
        BUILDER_TO_MASTER[builder] = master;
    });
}

function jsonRequest(url, onload, onerror)
{
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onload = function() {
        if (xhr.status == 200)
            onload(JSON.parse(xhr.response));
        else
            onerror(url);
    };
    xhr.onerror = function() { onerror(url); };
    xhr.send();
}

function isWebkitTestRunner(builder)
{
    if (builder.indexOf('Tests') != -1) {
        // Apple Windows bots still run old-run-webkit-tests, so they don't upload data.
        return builder.indexOf('Windows') == -1 || (builder.indexOf('Qt') != -1 && builder.indexOf('Chromium') != -1);
    }
    return builder.indexOf('GTK') != -1 || builder == 'Qt Linux Release';
}

function isChromiumWebkitTipOfTreeTestRunner(builder)
{
    return builder.indexOf('Webkit') != -1 && builder.indexOf('Builder') == -1 && builder.indexOf('(deps)') == -1;
}

function isChromiumWebkitDepsTestRunner(builder)
{
    return builder.indexOf('Webkit') != -1 && builder.indexOf('Builder') == -1 && builder.indexOf('(deps)') != -1;
}

function generateBuildersFromBuilderList(builderList, filter)
{
    return builderList.filter(filter).map(function(tester, index) {
        var builder = [tester];
        if (!index)
            builder.push(BuilderGroup.DEFAULT_BUILDER);
        return builder;
    });
}

var WEBKIT_DOT_ORG_BUILDER_JSON_URL = 'http://build.webkit.org/json/builders';
var CHROMIUM_CANARY_BUILDER_JSON_URL = 'http://build.chromium.org/p/chromium.webkit/json/builders';

function onLayoutTestBuilderListLoad(builderFilter, master, groupName, groupEnum, json)
{
    var builders = generateBuildersFromBuilderList(Object.keys(json), builderFilter);
    associateBuildersWithMaster(builders, master);
    LAYOUT_TESTS_BUILDER_GROUPS[groupName] = new BuilderGroup(BuilderGroup.groupEnum, builders);
    g_handleBuildersListLoaded();
}

function onErrorLoadingBuilderList(url)
{
    alert('Could not load list of builders from ' + url + '. Try reloading.');
}

function loadBuildersList(group, testType) {
    if (testType != 'layout-tests') {
        // FIXME: Load builder lists for non-layout-tests dynamically as well.
        return;
    }
    var onLoad, url;
    switch(group) {
    case '@ToT - chromium.org':
        onLoad = partial(onLayoutTestBuilderListLoad, isChromiumWebkitTipOfTreeTestRunner, CHROMIUM_WEBKIT_BUILDER_MASTER, group, BuilderGroup.TOT_WEBKIT);
        url = CHROMIUM_CANARY_BUILDER_JSON_URL;
        break;

    case '@ToT - webkit.org':
        onLoad = partial(onLayoutTestBuilderListLoad, isWebkitTestRunner, WEBKIT_BUILDER_MASTER, group, BuilderGroup.TOT_WEBKIT);
        url = WEBKIT_DOT_ORG_BUILDER_JSON_URL;
        break;

    case '@DEPS - chromium.org':
        onLoad = partial(onLayoutTestBuilderListLoad, isChromiumWebkitDepsTestRunner, CHROMIUM_WEBKIT_BUILDER_MASTER, group, BuilderGroup.DEPS_WEBKIT);
        url = CHROMIUM_CANARY_BUILDER_JSON_URL;
        break;
    }

    jsonRequest(url, onLoad, onErrorLoadingBuilderList);
}

var LAYOUT_TESTS_BUILDER_GROUPS = {
    '@ToT - chromium.org': null,
    '@ToT - webkit.org': null,
    '@DEPS - chromium.org': null,
}

var CHROMIUM_GPU_GTESTS_DEPS_BUILDERS = [
    ['Win7 Release (NVIDIA)', BuilderGroup.DEFAULT_BUILDER],
    ['Win7 Debug (NVIDIA)'],
    ['Mac Release (Intel)'],
    ['Mac Debug (Intel)'],
    ['Linux Release (NVIDIA)'],
    ['Linux Debug (NVIDIA)'],
];
associateBuildersWithMaster(CHROMIUM_GPU_GTESTS_DEPS_BUILDERS, CHROMIUM_GPU_BUILDER_MASTER);

var CHROMIUM_GPU_FYI_GTESTS_DEPS_BUILDERS = [
    ['Win7 Release (ATI)', BuilderGroup.DEFAULT_BUILDER],
    ['Win7 Release (Intel)'],
    ['WinXP Release (NVIDIA)'],
    ['WinXP Debug (NVIDIA)'],
    ['Mac Release (ATI)'],
    ['Linux Release (ATI)'],
    ['Linux Release (Intel)'],
    ['Win7 Audio'],
    ['Linux Audio'],
];
associateBuildersWithMaster(CHROMIUM_GPU_FYI_GTESTS_DEPS_BUILDERS, CHROMIUM_GPU_FYI_BUILDER_MASTER);

var CHROMIUM_GPU_GTESTS_TOT_BUILDERS = [
    ['GPU Win7 (NVIDIA)', BuilderGroup.DEFAULT_BUILDER],
    ['GPU Win7 (dbg) (NVIDIA)'],
    ['GPU Mac'],
    ['GPU Mac (dbg)'],
    ['GPU Linux (NVIDIA)'],
    ['GPU Linux (dbg) (NVIDIA)'],
];
associateBuildersWithMaster(CHROMIUM_GPU_GTESTS_TOT_BUILDERS, CHROMIUM_WEBKIT_BUILDER_MASTER);

var CHROMIUM_GPU_TESTS_BUILDER_GROUPS = {
    '@DEPS - chromium.org': new BuilderGroup(BuilderGroup.DEPS_WEBKIT, CHROMIUM_GPU_GTESTS_DEPS_BUILDERS),
    '@DEPS FYI - chromium.org': new BuilderGroup(BuilderGroup.DEPS_WEBKIT, CHROMIUM_GPU_FYI_GTESTS_DEPS_BUILDERS),
    '@ToT - chromium.org': new BuilderGroup(BuilderGroup.TOT_WEBKIT, CHROMIUM_GPU_GTESTS_TOT_BUILDERS)
};

var CHROMIUM_GTESTS_DEPS_BUILDERS = [
    ['Win', BuilderGroup.DEFAULT_BUILDER],
    ['Mac'],
    ['Linux'],
    ['Linux x64'],
    ['XP Tests (1)'],
    ['XP Tests (2)'],
    ['XP Tests (3)'],
    ['Vista Tests (1)'],
    ['Vista Tests (2)'],
    ['Vista Tests (3)'],
    ['Win7 Tests (1)'],
    ['Win7 Tests (2)'],
    ['Win7 Tests (3)'],
    ['Win7 Sync'],
    ['XP Tests (dbg)(1)'],
    ['XP Tests (dbg)(2)'],
    ['XP Tests (dbg)(3)'],
    ['XP Tests (dbg)(4)'],
    ['XP Tests (dbg)(5)'],
    ['XP Tests (dbg)(6)'],
    ['Win7 Tests (dbg)(1)'],
    ['Win7 Tests (dbg)(2)'],
    ['Win7 Tests (dbg)(3)'],
    ['Win7 Tests (dbg)(4)'],
    ['Win7 Tests (dbg)(5)'],
    ['Win7 Tests (dbg)(6)'],
    ['Interactive Tests (dbg)'],
    ['Win Aura'],
    ['Mac10.5 Tests (1)'],
    ['Mac10.5 Tests (2)'],
    ['Mac10.5 Tests (3)'],
    ['Mac10.6 Tests (1)'],
    ['Mac10.6 Tests (2)'],
    ['Mac10.6 Tests (3)'],
    ['Mac10.6 Sync'],
    ['Mac 10.5 Tests (dbg)(1)'],
    ['Mac 10.5 Tests (dbg)(2)'],
    ['Mac 10.5 Tests (dbg)(3)'],
    ['Mac 10.5 Tests (dbg)(4)'],
    ['Mac 10.6 Tests (dbg)(1)'],
    ['Mac 10.6 Tests (dbg)(2)'],
    ['Mac 10.6 Tests (dbg)(3)'],
    ['Mac 10.6 Tests (dbg)(4)'],
    ['Linux Tests x64'],
    ['Linux Sync'],
    ['Linux Tests (dbg)(1)'],
    ['Linux Tests (dbg)(2)'],
    ['Linux Tests (dbg)(shared)'],
    ['Linux Tests (Aura dbg)'],
];
associateBuildersWithMaster(CHROMIUM_GTESTS_DEPS_BUILDERS, CHROMIUM_BUILDER_MASTER);

var CHROMIUMOS_GTESTS_DEPS_BUILDERS = [
    ['Linux ChromiumOS Tests (1)', BuilderGroup.DEFAULT_BUILDER],
    ['Linux ChromiumOS Tests (2)'],
    ['Linux ChromiumOS Tests (dbg)(1)'],
    ['Linux ChromiumOS Tests (dbg)(2)'],
    ['Linux ChromiumOS Tests (dbg)(3)'],
];
associateBuildersWithMaster(CHROMIUMOS_GTESTS_DEPS_BUILDERS, CHROMIUMOS_BUILDER_MASTER);

var CHROMIUM_GTESTS_TOT_BUILDERS = [
    ['Win (dbg)', BuilderGroup.DEFAULT_BUILDER],
    ['Mac10.6 Tests'],
    ['Linux Tests'],
];
associateBuildersWithMaster(CHROMIUM_GTESTS_TOT_BUILDERS, CHROMIUM_WEBKIT_BUILDER_MASTER);

var CHROMIUM_GTESTS_BUILDER_GROUPS = {
    '@DEPS - chromium.org': new BuilderGroup(BuilderGroup.DEPS_WEBKIT, CHROMIUM_GTESTS_DEPS_BUILDERS),
    '@DEPS CrOS - chromium.org': new BuilderGroup(BuilderGroup.DEPS_WEBKIT, CHROMIUMOS_GTESTS_DEPS_BUILDERS),
    '@ToT - chromium.org': new BuilderGroup(BuilderGroup.TOT_WEBKIT, CHROMIUM_GTESTS_TOT_BUILDERS),
};

