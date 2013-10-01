/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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

WebKitBuildbot = function()
{
    const queueInfo = {
        "Apple Lion Release (Build)": {platform: Buildbot.Platform.MacOSXLion, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple Lion Debug (Build)": {platform: Buildbot.Platform.MacOSXLion, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple MountainLion Release (Build)": {platform: Buildbot.Platform.MacOSXMountainLion, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple MountainLion Release (32-bit Build)": {platform: Buildbot.Platform.MacOSXMountainLion, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple MountainLion Debug (Build)": {platform: Buildbot.Platform.MacOSXMountainLion, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple Lion Release WK1 (Tests)": {platform: Buildbot.Platform.MacOSXLion, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Lion Debug WK1 (Tests)": {platform: Buildbot.Platform.MacOSXLion, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Lion Release WK2 (Tests)": {platform: Buildbot.Platform.MacOSXLion, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple Lion Debug WK2 (Tests)": {platform: Buildbot.Platform.MacOSXLion, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple MountainLion Release WK1 (Tests)": {platform: Buildbot.Platform.MacOSXMountainLion, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple MountainLion Debug WK1 (Tests)": {platform: Buildbot.Platform.MacOSXMountainLion, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple MountainLion Release WK2 (Tests)": {platform: Buildbot.Platform.MacOSXMountainLion, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple MountainLion Debug WK2 (Tests)": {platform: Buildbot.Platform.MacOSXMountainLion, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple Win Debug (Build)": {platform: Buildbot.Platform.Windows7, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple Win Release (Build)": {platform: Buildbot.Platform.Windows7, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple Win 7 Debug (Tests)": {platform: Buildbot.Platform.Windows7, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Win 7 Release (Tests)": {platform: Buildbot.Platform.Windows7, tester: true, testCategory: Buildbot.TestCategory.WebKit1}
    };

    Buildbot.call(this, "http://build.webkit.org/", queueInfo);
};

BaseObject.addConstructorFunctions(WebKitBuildbot);

WebKitBuildbot.prototype = {
    constructor: WebKitBuildbot,
    __proto__: Buildbot.prototype,

    tracRevisionURL: function(revision)
    {
        return "http://trac.webkit.org/changeset/" + revision;
    },

    buildLogURLForIteration: function(iteration)
    {
        return this.baseURL + "builders/" + encodeURIComponent(iteration.queue.id) + "/builds/" + iteration.id + "/steps/compile-webkit/logs/stdio/text";
    },

    layoutTestResultsURLForIteration: function(iteration)
    {
        return this.baseURL + "results/" + encodeURIComponent(iteration.queue.id) + "/" + encodeURIComponent("r" + iteration.openSourceRevision + " (" + iteration.id + ")") + "/results.html";
    },

    javascriptTestResultsURLForIteration: function(iteration)
    {
        return this.baseURL + "builders/" + encodeURIComponent(iteration.queue.id) + "/builds/" + iteration.id + "/steps/jscore-test/logs/actual.html";
    }
};
