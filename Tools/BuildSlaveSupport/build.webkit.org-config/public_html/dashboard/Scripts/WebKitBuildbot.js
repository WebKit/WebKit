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
        "Apple Mavericks Debug (Build)": {platform: Dashboard.Platform.MacOSXMavericks, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple Mavericks Release (Build)": {platform: Dashboard.Platform.MacOSXMavericks, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple Mavericks Debug WK1 (Tests)": {platform: Dashboard.Platform.MacOSXMavericks, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Mavericks Debug WK2 (Tests)": {platform: Dashboard.Platform.MacOSXMavericks, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple Mavericks Release WK1 (Tests)": {platform: Dashboard.Platform.MacOSXMavericks, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Mavericks Release WK2 (Tests)": {platform: Dashboard.Platform.MacOSXMavericks, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple Mavericks Release (Perf)": {platform: Dashboard.Platform.MacOSXMavericks, debug: false, performance: true},
        "Apple Yosemite Debug (Build)": {platform: Dashboard.Platform.MacOSXYosemite, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple Yosemite Release (Build)": {platform: Dashboard.Platform.MacOSXYosemite, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple Yosemite Release (32-bit Build)": {platform: Dashboard.Platform.MacOSXYosemite, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple Yosemite Debug WK1 (Tests)": {platform: Dashboard.Platform.MacOSXYosemite, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Yosemite Debug WK2 (Tests)": {platform: Dashboard.Platform.MacOSXYosemite, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple Yosemite Release WK1 (Tests)": {platform: Dashboard.Platform.MacOSXYosemite, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Yosemite Release WK2 (Tests)": {platform: Dashboard.Platform.MacOSXYosemite, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple Yosemite Release (Perf)": {platform: Dashboard.Platform.MacOSXYosemite, debug: false, performance: true},
        "Apple Win Debug (Build)": {platform: Dashboard.Platform.Windows7, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple Win Release (Build)": {platform: Dashboard.Platform.Windows7, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple Win 7 Debug (Tests)": {platform: Dashboard.Platform.Windows7, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Win 7 Release (Tests)": {platform: Dashboard.Platform.Windows7, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "GTK Linux 64-bit Release (Build)": {platform: Dashboard.Platform.LinuxGTK, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "GTK Linux 64-bit Release (Tests)": {platform: Dashboard.Platform.LinuxGTK, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "GTK Linux 64-bit Debug (Build)": {platform: Dashboard.Platform.LinuxGTK, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "GTK Linux 64-bit Debug (Tests)": {platform: Dashboard.Platform.LinuxGTK, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "GTK Linux 64-bit Release (Perf)": {platform: Dashboard.Platform.LinuxGTK, debug: false, performance: true},
        "EFL Linux 64-bit Release WK2": {platform: Dashboard.Platform.LinuxEFL, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "EFL Linux 64-bit Release WK2 (Perf)": {platform: Dashboard.Platform.LinuxEFL, performance: true}
    };

    Buildbot.call(this, "https://build.webkit.org/", queueInfo);
};

BaseObject.addConstructorFunctions(WebKitBuildbot);

WebKitBuildbot.prototype = {
    constructor: WebKitBuildbot,
    __proto__: Buildbot.prototype,
    performanceDashboardURL:  "https://perf.webkit.org",

    layoutTestResultsDirectoryURLForIteration: function(iteration)
    {
        return this.baseURL + "results/" + encodeURIComponent(iteration.queue.id) + "/" + encodeURIComponent("r" + iteration.openSourceRevision + " (" + iteration.id + ")");
    }
};
