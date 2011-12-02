// Copyright (C) 2011 Google Inc. All rights reserved.
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
CHROMIUM_WEBKIT_BUILDER_MASTER = new BuilderMaster('ChromiumWebkit', 'http://build.chromium.org/p/chromium.webkit/builders/');
CHROMIUM_GPU_BUILDER_MASTER = new BuilderMaster('ChromiumGPU', 'http://build.chromium.org/p/chromium.gpu/builders/');
WEBKIT_BUILDER_MASTER = new BuilderMaster('webkit.org', 'http://build.webkit.org/builders/');

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
        if (flags & BuilderGroup.EXPECTATIONS_BUILDER)
            this.expectationsBuilder = builder;
    }, this);
}

BuilderGroup.prototype.setup = function()
{
    // FIXME: instead of copying these to globals, it would be better if
    // the rest of the code read things from the BuilderGroup instance directly
    g_defaultBuilderName = this.defaultBuilder;
    g_expectationsBuilder = this.expectationsBuilder;
    g_builders = this.builders;
};

BuilderGroup.TOT_WEBKIT = true;
BuilderGroup.DEPS_WEBKIT = false;

BuilderGroup.DEFAULT_BUILDER = 1 << 1;
// For expectations builder, list the fastest builder so that we always have the
// most up to date expectations.
BuilderGroup.EXPECTATIONS_BUILDER = 1 << 2;

var BUILDER_TO_MASTER = {};
function associateBuildersWithMaster(builders, master)
{
    builders.forEach(function(builderAndFlags) {
        var builder = builderAndFlags[0];
        BUILDER_TO_MASTER[builder] = master;
    });
}

var CHROMIUM_LAYOUT_DEPS_BUILDERS = [
    ['Webkit Win (deps)', BuilderGroup.DEFAULT_BUILDER],
    ['Webkit Linux (deps)', BuilderGroup.EXPECTATIONS_BUILDER],
    ['Webkit Mac10.6 (deps)'],
    ['Webkit Mac10.6 (CG)(deps)'],
];
associateBuildersWithMaster(CHROMIUM_LAYOUT_DEPS_BUILDERS, CHROMIUM_WEBKIT_BUILDER_MASTER);

var CHROMIUM_LAYOUT_TOT_BUILDERS = [
    ['Webkit Win', BuilderGroup.DEFAULT_BUILDER],
    ['Webkit Vista'],
    ['Webkit Win7'],
    ['Webkit Win (dbg)(1)'],
    ['Webkit Win (dbg)(2)'],
    ['Webkit Linux', BuilderGroup.EXPECTATIONS_BUILDER],
    ['Webkit Linux 32'],
    ['Webkit Linux (dbg)'],
    ['Webkit Mac10.5'],
    ['Webkit Mac10.5 (dbg)(1)'],
    ['Webkit Mac10.5 (dbg)(2)'],
    ['Webkit Mac10.6'],
    ['Webkit Mac10.6 (dbg)'],
    ['Webkit Mac10.5 (CG)'],
    ['Webkit Mac10.5 (CG)(dbg)(1)'],
    ['Webkit Mac10.5 (CG)(dbg)(2)'],
    ['Webkit Mac10.6 (CG)'],
    ['Webkit Mac10.6 (CG)(dbg)']
];
associateBuildersWithMaster(CHROMIUM_LAYOUT_TOT_BUILDERS, CHROMIUM_WEBKIT_BUILDER_MASTER);

var WEBKIT_TOT_BUILDERS = [
    ['Chromium Win Release (Tests)', BuilderGroup.DEFAULT_BUILDER],
    ['Chromium Linux Release (Tests)', BuilderGroup.EXPECTATIONS_BUILDER],
    ['Chromium Mac Release (Tests)'],
    ['SnowLeopard Intel Release (Tests)'],
    ['SnowLeopard Intel Debug (Tests)'],
    ['GTK Linux 32-bit Release'],
    ['GTK Linux 32-bit Debug'],
    ['GTK Linux 64-bit Debug'],
    ['Qt Linux Release']
];
associateBuildersWithMaster(WEBKIT_TOT_BUILDERS, WEBKIT_BUILDER_MASTER);

var CHROMIUM_GPU_MESA_BUILDERS = [
    ['Webkit Win - GPU', BuilderGroup.DEFAULT_BUILDER],
    ['Webkit Vista - GPU'],
    ['Webkit Win7 - GPU'],
    ['Webkit Win (dbg)(1) - GPU'],
    ['Webkit Win (dbg)(2) - GPU'],
    ['Webkit Linux - GPU', BuilderGroup.EXPECTATIONS_BUILDER],
    ['Webkit Linux 32 - GPU'],
    ['Webkit Linux (dbg) - GPU'],
    ['Webkit Mac10.5 - GPU'],
    ['Webkit Mac10.5 (dbg)(1) - GPU'],
    ['Webkit Mac10.5 (dbg)(2) - GPU'],
    ['Webkit Mac10.6 - GPU'],
    ['Webkit Mac10.6 (dbg) - GPU'],
    ['Webkit Mac10.5 (CG) - GPU'],
    ['Webkit Mac10.5 (CG)(dbg)(1) - GPU'],
    ['Webkit Mac10.5 (CG)(dbg)(2) - GPU'],
    ['Webkit Mac10.6 (CG) - GPU'],
    ['Webkit Mac10.6 (CG)(dbg) - GPU']
];
associateBuildersWithMaster(CHROMIUM_GPU_MESA_BUILDERS, CHROMIUM_WEBKIT_BUILDER_MASTER);

var LAYOUT_TESTS_BUILDER_GROUPS = {
    '@DEPS - chromium.org': new BuilderGroup(BuilderGroup.DEPS_WEBKIT, CHROMIUM_LAYOUT_DEPS_BUILDERS),
    '@ToT - chromium.org': new BuilderGroup(BuilderGroup.TOT_WEBKIT, CHROMIUM_LAYOUT_TOT_BUILDERS),
    '@ToT - webkit.org': new BuilderGroup(BuilderGroup.TOT_WEBKIT, WEBKIT_TOT_BUILDERS),
    '@ToT GPU Mesa - chromium.org': new BuilderGroup(BuilderGroup.TOT_WEBKIT, CHROMIUM_GPU_MESA_BUILDERS)
};

var LEGACY_BUILDER_MASTERS_TO_GROUPS = {
    'Chromium': '@DEPS - chromium.org',
    'ChromiumWebkit': '@ToT - chromium.org',
    'webkit.org': '@ToT - webkit.org'
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

var CHROMIUM_GTESTS_TOT_BUILDERS = [
    ['Win (dbg)', BuilderGroup.DEFAULT_BUILDER],
    ['Mac10.6 Tests'],
    ['Linux Tests'],
];
associateBuildersWithMaster(CHROMIUM_GTESTS_TOT_BUILDERS, CHROMIUM_WEBKIT_BUILDER_MASTER);

var CHROMIUM_GTESTS_BUILDER_GROUPS = {
    '@DEPS - chromium.org': new BuilderGroup(BuilderGroup.DEPS_WEBKIT, CHROMIUM_GTESTS_DEPS_BUILDERS),
    '@ToT - chromium.org': new BuilderGroup(BuilderGroup.TOT_WEBKIT, CHROMIUM_GTESTS_TOT_BUILDERS),
};

