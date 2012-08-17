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

BuilderMaster.prototype.logPath = function(builder, buildNumber)
{
    return this.basePath + 'builders/' + builder + '/builds/' + buildNumber;
};

BuilderMaster.prototype.builderJsonPath = function()
{
    return this.basePath + 'json/builders';
};

CHROMIUM_BUILDER_MASTER = new BuilderMaster('Chromium', 'http://build.chromium.org/p/chromium/');
CHROMIUM_WIN_BUILDER_MASTER = new BuilderMaster('ChromiumWin', 'http://build.chromium.org/p/chromium.win/');
CHROMIUM_MAC_BUILDER_MASTER = new BuilderMaster('ChromiumMac', 'http://build.chromium.org/p/chromium.mac/');
CHROMIUM_LINUX_BUILDER_MASTER = new BuilderMaster('ChromiumLinux', 'http://build.chromium.org/p/chromium.linux/');
CHROMIUMOS_BUILDER_MASTER = new BuilderMaster('ChromiumChromiumOS', 'http://build.chromium.org/p/chromium.chromiumos/');
CHROMIUM_GPU_BUILDER_MASTER = new BuilderMaster('ChromiumGPU', 'http://build.chromium.org/p/chromium.gpu/');
CHROMIUM_GPU_FYI_BUILDER_MASTER = new BuilderMaster('ChromiumGPUFYI', 'http://build.chromium.org/p/chromium.gpu.fyi/');
CHROMIUM_PERF_AV_BUILDER_MASTER = new BuilderMaster('ChromiumPerfAv', 'http://build.chromium.org/p/chromium.perf_av/');
CHROMIUM_WEBKIT_BUILDER_MASTER = new BuilderMaster('ChromiumWebkit', 'http://build.chromium.org/p/chromium.webkit/');
WEBKIT_BUILDER_MASTER = new BuilderMaster('webkit.org', 'http://build.webkit.org/');

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
    this.groups = 0;
    this.expectedGroups = 0;
}

BuilderGroup.prototype.setbuilder = function(builder, flags) {
    this.builders[builder] = builder.replace(/[ .()]/g, '_');
    // FIXME: Remove this at some point, we don't actually use DEFAULT_BUILDER
    //        in any meaningful way anymore.  We always just default to the
    //        first builder in alphabetical order.
    if (flags & BuilderGroup.DEFAULT_BUILDER)
        this.defaultBuilder = builder;
};

BuilderGroup.prototype.append = function(builders) {
    builders.forEach(function(builderAndFlags) {
        var builder = builderAndFlags[0];
        var flags = builderAndFlags[1];
        this.setbuilder(builder, flags);
    }, this);
    this.groups += 1;
};

BuilderGroup.prototype.loaded = function() {
    return this.groups >= this.expectedGroups;
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

function doXHR(url, onLoad, builderGroups, groupName)
{
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onload = function() {
        if (xhr.status == 200)
            onLoad(JSON.parse(xhr.response));
        else
            onErrorLoadingBuilderList(url, builderGroups, groupName);
    };
    xhr.onerror = function() { onErrorLoadingBuilderList(url, builderGroups, groupName); };
    xhr.send();
}

function requestBuilderList(builderGroups, builderFilter, master, groupName, builderGroup)
{
    if (!builderGroups[groupName])
        builderGroups[groupName] = builderGroup;
    var onLoad = partial(onBuilderListLoad, builderGroups, builderFilter, master, groupName);
    doXHR(master.builderJsonPath(), onLoad, builderGroups, groupName);
    builderGroups[groupName].expectedGroups += 1;
}

function isChromiumDepsGpuTestRunner(builder)
{
    return true;
}

function isChromiumDepsFyiGpuTestRunner(builder)
{
    // FIXME: This is kind of wonky, but there's not really a better pattern.
    return builder.indexOf('(') != -1;
}

function isChromiumTipOfTreeGpuTestRunner(builder)
{
    return builder.indexOf('GPU') != -1;
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
    return (builder.indexOf('Webkit') != -1 && builder.indexOf('Builder') == -1 && builder.indexOf('(deps)') == -1 && builder.indexOf('ASAN') == -1) || builder.indexOf('(Content Shell)') != -1;
}

function isChromiumWebkitDepsTestRunner(builder)
{
    return builder.indexOf('Webkit') != -1 && builder.indexOf('Builder') == -1 && builder.indexOf('(deps)') != -1;
}

function isChromiumDepsGTestRunner(builder)
{
    return builder.indexOf('Tests') != -1 && builder.indexOf('Chrome Frame') == -1;
}

function isChromiumDepsCrosGTestRunner(builder)
{
    return builder.indexOf('Tests') != -1;
}

function isChromiumTipOfTreeGTestRunner(builder)
{
    return !isChromiumTipOfTreeGpuTestRunner(builder) && builder.indexOf('Builder') == -1 && builder.indexOf('Perf') == -1 &&
         builder.indexOf('Webkit') == -1 && builder.indexOf('Valgrind') == -1 && builder.indexOf('Chrome Frame') == -1;
}

function isChromiumDepsAVTestRunner(builder)
{
    return builder.indexOf('Builder') == -1;
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

function onBuilderListLoad(builderGroups, builderFilter, master, groupName, json)
{
    var builders = generateBuildersFromBuilderList(Object.keys(json), builderFilter);
    associateBuildersWithMaster(builders, master);
    builderGroups[groupName].append(builders);
    if (builderGroups[groupName].loaded())
        g_handleBuildersListLoaded();
}

function onErrorLoadingBuilderList(url, builderGroups, groupName)
{
    builderGroups[groupName].groups += 1;
    console.log('Could not load list of builders from ' + url + '. Try reloading.');
}

function loadBuildersList(groupName, testType) {
    switch (testType) {
    case 'gl_tests':
    case 'gpu_tests':
        switch(groupName) {
        case '@DEPS - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.DEPS_WEBKIT);
            requestBuilderList(CHROMIUM_GPU_TESTS_BUILDER_GROUPS, isChromiumDepsGpuTestRunner, CHROMIUM_GPU_BUILDER_MASTER, groupName, builderGroup);
            break;

        case '@DEPS FYI - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.DEPS_WEBKIT);
            requestBuilderList(CHROMIUM_GPU_TESTS_BUILDER_GROUPS, isChromiumDepsFyiGpuTestRunner, CHROMIUM_GPU_FYI_BUILDER_MASTER, groupName, builderGroup);
            break;

        case '@ToT - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.TOT_WEBKIT);
            requestBuilderList(CHROMIUM_GPU_TESTS_BUILDER_GROUPS, isChromiumTipOfTreeGpuTestRunner, CHROMIUM_WEBKIT_BUILDER_MASTER, groupName, builderGroup);
            break;
        }
        break;

    case 'layout-tests':
        switch(groupName) {
        case '@ToT - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.TOT_WEBKIT);
            requestBuilderList(LAYOUT_TESTS_BUILDER_GROUPS, isChromiumWebkitTipOfTreeTestRunner, CHROMIUM_WEBKIT_BUILDER_MASTER, groupName, builderGroup);
            break;

        case '@ToT - webkit.org':
            var builderGroup = new BuilderGroup(BuilderGroup.TOT_WEBKIT);
            requestBuilderList(LAYOUT_TESTS_BUILDER_GROUPS, isWebkitTestRunner, WEBKIT_BUILDER_MASTER, groupName, builderGroup);
            break;

        case '@DEPS - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.DEPS_WEBKIT);
            requestBuilderList(LAYOUT_TESTS_BUILDER_GROUPS, isChromiumWebkitDepsTestRunner, CHROMIUM_WEBKIT_BUILDER_MASTER, groupName, builderGroup);
            requestBuilderList(LAYOUT_TESTS_BUILDER_GROUPS, isChromiumDepsAVTestRunner, CHROMIUM_PERF_AV_BUILDER_MASTER, groupName, builderGroup);
            break;
        }
        break;

    default:
        switch(groupName) {
        case '@DEPS - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.DEPS_WEBKIT);
            requestBuilderList(CHROMIUM_GTESTS_BUILDER_GROUPS, isChromiumDepsGTestRunner, CHROMIUM_BUILDER_MASTER, groupName, builderGroup);
            requestBuilderList(CHROMIUM_GTESTS_BUILDER_GROUPS, isChromiumDepsGTestRunner, CHROMIUM_WIN_BUILDER_MASTER, groupName, builderGroup);
            requestBuilderList(CHROMIUM_GTESTS_BUILDER_GROUPS, isChromiumDepsGTestRunner, CHROMIUM_MAC_BUILDER_MASTER, groupName, builderGroup);
            requestBuilderList(CHROMIUM_GTESTS_BUILDER_GROUPS, isChromiumDepsGTestRunner, CHROMIUM_LINUX_BUILDER_MASTER, groupName, builderGroup);
            break;

        case '@DEPS CrOS - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.DEPS_WEBKIT);
            requestBuilderList(CHROMIUM_GTESTS_BUILDER_GROUPS, isChromiumDepsCrosGTestRunner, CHROMIUMOS_BUILDER_MASTER, groupName, builderGroup);
            break;

        case '@ToT - chromium.org':
            var builderGroup = new BuilderGroup(BuilderGroup.TOT_WEBKIT);
            requestBuilderList(CHROMIUM_GTESTS_BUILDER_GROUPS, isChromiumTipOfTreeGTestRunner, CHROMIUM_WEBKIT_BUILDER_MASTER, groupName, builderGroup);
            break;
        }
        break;
    }
}

var LAYOUT_TESTS_BUILDER_GROUPS = {
    '@ToT - chromium.org': null,
    '@ToT - webkit.org': null,
    '@DEPS - chromium.org': null,
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
