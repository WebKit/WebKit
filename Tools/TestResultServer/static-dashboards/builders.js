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

function LOAD_BUILDBOT_DATA(builderData)
{
    builders.masters = {};
    builderData.forEach(function(master) {
        builders.masters[master.name] = new builders.BuilderMaster(master.name, master.url, master.tests);
    })
}

var builders = builders || {};

(function() {

// FIXME: Move some of this loading logic into loader.js.

builders._loadScript = function(url, success, error)
{
    var script = document.createElement('script');
    script.src = url;
    script.onload = success;
    script.onerror = error;
    document.head.appendChild(script);
}

builders._requestBuilders = function()
{
    var buildersUrl = 'builders.jsonp';
    builders._loadScript(buildersUrl, function() {}, function() {
        console.error('Could not load ' + buildersUrl);
    });
}


builders.BuilderMaster = function(name, basePath, tests)
{
    this.name = name;
    this.basePath = basePath;
    this.tests = tests;
}

builders.BuilderMaster.prototype = {
    logPath: function(builder, buildNumber)
    {
        return this.basePath + '/builders/' + builder + '/builds/' + buildNumber;
    },
    builderJsonPath: function()
    {
        return this.basePath + '/json/builders';
    },
}

builders._requestBuilders();

})();

// FIXME: Move everything below into the anonymous namespace above.

CHROMIUM_WIN_BUILDER_MASTER = 'ChromiumWin';
CHROMIUM_MAC_BUILDER_MASTER = 'ChromiumMac';
CHROMIUM_LINUX_BUILDER_MASTER = 'ChromiumLinux';
CHROMIUMOS_BUILDER_MASTER = 'ChromiumChromiumOS';
CHROMIUM_GPU_BUILDER_MASTER = 'ChromiumGPU';
CHROMIUM_GPU_FYI_BUILDER_MASTER = 'ChromiumGPUFYI';
CHROMIUM_PERF_AV_BUILDER_MASTER = 'ChromiumPerfAv';
CHROMIUM_WEBKIT_BUILDER_MASTER = 'ChromiumWebkit';
WEBKIT_BUILDER_MASTER = 'webkit.org';

var LEGACY_BUILDER_MASTERS_TO_GROUPS = {
    'Chromium': '@DEPS - chromium.org',
    'ChromiumWin': '@DEPS - chromium.org',
    'ChromiumMac': '@DEPS - chromium.org',
    'ChromiumLinux': '@DEPS - chromium.org',
    'ChromiumChromiumOS': '@DEPS CrOS - chromium.org',
    'ChromiumGPU': '@DEPS - chromium.org',
    'ChromiumGPUFYI': '@DEPS FYI - chromium.org',
    'ChromiumPerfAv': '@DEPS - chromium.org',
    'ChromiumWebkit': '@ToT - chromium.org',
    'webkit.org': '@ToT - webkit.org'
};

function BuilderGroup(isToTWebKit)
{
    this.isToTWebKit = isToTWebKit;
    // Map of builderName (the name shown in the waterfall) to builderPath (the
    // path used in the builder's URL)
    this.builders = {};
}

BuilderGroup.prototype.append = function(builders) {
    builders.forEach(function(builderName) {
        this.builders[builderName] = builderName.replace(/[ .()]/g, '_');
    }, this);
};

BuilderGroup.prototype.defaultBuilder = function()
{
    for (var builder in this.builders)
        return builder;
    console.error('There are no builders in this builder group.');
}

BuilderGroup.prototype.master = function()
{
    return builderMaster(this.defaultBuilder());
}

BuilderGroup.TOT_WEBKIT = true;
BuilderGroup.DEPS_WEBKIT = false;

var BUILDER_TO_MASTER = {};

function builderMaster(builderName)
{
    return BUILDER_TO_MASTER[builderName];
}

function requestBuilderList(builderGroups, masterName, groupName, builderGroup, testType, opt_builderFilter)
{
    if (!builderGroups[groupName])
        builderGroups[groupName] = builderGroup;
    var master = builders.masters[masterName];
    var builderList = master.tests[testType].builders;
    if (opt_builderFilter)
        builderList = builderList.filter(opt_builderFilter);
    builderList.forEach(function(builderName) {
        BUILDER_TO_MASTER[builderName] = master;
    });
    builderGroups[groupName].append(builderList);
}

function isChromiumContentShellTestRunner(builder)
{
    return builder.indexOf('(Content Shell)') != -1;
}

function isChromiumWebkitTipOfTreeTestRunner(builder)
{
    // FIXME: Remove the Android check once the android tests bot is actually uploading results.
    return builder.indexOf('ASAN') == -1 && !isChromiumContentShellTestRunner(builder) && builder.indexOf('Android') == -1 && !isChromiumWebkitDepsTestRunner(builder);
}

function isChromiumWebkitDepsTestRunner(builder)
{
    return builder.indexOf('(deps)') != -1;
}

// FIXME: Look into whether we can move the grouping logic into builders.jsonp and get rid of this code.
function loadBuildersList(groupName, testType) {
    switch (testType) {
    case 'gl_tests':
    case 'gpu_tests':
        switch(groupName) {
        case '@DEPS - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.DEPS_WEBKIT);
            requestBuilderList(CHROMIUM_GPU_TESTS_BUILDER_GROUPS, CHROMIUM_GPU_BUILDER_MASTER, groupName, builderGroup, testType);
            break;

        case '@DEPS FYI - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.DEPS_WEBKIT);
            requestBuilderList(CHROMIUM_GPU_TESTS_BUILDER_GROUPS, CHROMIUM_GPU_FYI_BUILDER_MASTER, groupName, builderGroup, testType);
            break;

        case '@ToT - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.TOT_WEBKIT);
            requestBuilderList(CHROMIUM_GPU_TESTS_BUILDER_GROUPS, CHROMIUM_WEBKIT_BUILDER_MASTER, groupName, builderGroup, testType);
            break;
        }
        break;

    case 'layout-tests':
        switch(groupName) {
        case 'Content Shell @ToT - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.TOT_WEBKIT);
            requestBuilderList(LAYOUT_TESTS_BUILDER_GROUPS, CHROMIUM_WEBKIT_BUILDER_MASTER, groupName, builderGroup, testType, isChromiumContentShellTestRunner);
            break;

        case '@ToT - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.TOT_WEBKIT);
            requestBuilderList(LAYOUT_TESTS_BUILDER_GROUPS, CHROMIUM_WEBKIT_BUILDER_MASTER, groupName, builderGroup, testType, isChromiumWebkitTipOfTreeTestRunner);
            break;

        case '@ToT - webkit.org':
            var builderGroup = new BuilderGroup(BuilderGroup.TOT_WEBKIT);
            requestBuilderList(LAYOUT_TESTS_BUILDER_GROUPS, WEBKIT_BUILDER_MASTER, groupName, builderGroup, testType);
            break;

        case '@DEPS - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.DEPS_WEBKIT);
            requestBuilderList(LAYOUT_TESTS_BUILDER_GROUPS, CHROMIUM_WEBKIT_BUILDER_MASTER, groupName, builderGroup, testType, isChromiumWebkitDepsTestRunner);
            requestBuilderList(LAYOUT_TESTS_BUILDER_GROUPS, CHROMIUM_PERF_AV_BUILDER_MASTER, groupName, builderGroup, testType);
            break;
        }
        break;

    case 'test_shell_tests':
    case 'webkit_unit_tests':
        switch(groupName) {
        case '@ToT - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.TOT_WEBKIT);
            requestBuilderList(TEST_SHELL_TESTS_BUILDER_GROUPS, CHROMIUM_WEBKIT_BUILDER_MASTER, groupName, builderGroup, testType, isChromiumWebkitTipOfTreeTestRunner);
            break;

        case '@DEPS - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.DEPS_WEBKIT);
            requestBuilderList(TEST_SHELL_TESTS_BUILDER_GROUPS, CHROMIUM_WEBKIT_BUILDER_MASTER, groupName, builderGroup, testType, isChromiumWebkitDepsTestRunner);
            break;
        }
        break;    

    default:
        switch(groupName) {
        case '@DEPS - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.DEPS_WEBKIT);
            requestBuilderList(CHROMIUM_GTESTS_BUILDER_GROUPS, CHROMIUM_WIN_BUILDER_MASTER, groupName, builderGroup, testType);
            requestBuilderList(CHROMIUM_GTESTS_BUILDER_GROUPS, CHROMIUM_MAC_BUILDER_MASTER, groupName, builderGroup, testType);
            requestBuilderList(CHROMIUM_GTESTS_BUILDER_GROUPS, CHROMIUM_LINUX_BUILDER_MASTER, groupName, builderGroup, testType);
            break;

        case '@DEPS CrOS - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.DEPS_WEBKIT);
            requestBuilderList(CHROMIUM_GTESTS_BUILDER_GROUPS, CHROMIUMOS_BUILDER_MASTER, groupName, builderGroup, testType);
            break;

        case '@ToT - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.TOT_WEBKIT);
            requestBuilderList(CHROMIUM_GTESTS_BUILDER_GROUPS, CHROMIUM_WEBKIT_BUILDER_MASTER, groupName, builderGroup, testType);
            break;
        }
        break;
    }
}

var TEST_SHELL_TESTS_BUILDER_GROUPS = {
    '@ToT - chromium.org': null,
    '@DEPS - chromium.org': null,
};

var LAYOUT_TESTS_BUILDER_GROUPS = {
    '@ToT - chromium.org': null,
    '@ToT - webkit.org': null,
    '@DEPS - chromium.org': null,
    'Content Shell @ToT - chromium.org': null,
};

var CHROMIUM_GPU_TESTS_BUILDER_GROUPS = {
    '@DEPS - chromium.org': null,
    '@DEPS FYI - chromium.org': null,
    '@ToT - chromium.org': null,
};

var CHROMIUM_GTESTS_BUILDER_GROUPS = {
    '@DEPS - chromium.org': null,
    '@DEPS CrOS - chromium.org': null,
    '@ToT - chromium.org': null,
};
