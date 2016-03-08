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
        "Apple El Capitan Debug (Build)": {platform: Dashboard.Platform.MacOSXElCapitan, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple El Capitan Release (Build)": {platform: Dashboard.Platform.MacOSXElCapitan, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple El Capitan Release (32-bit Build)": {platform: Dashboard.Platform.MacOSXElCapitan, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple El Capitan Debug WK1 (Tests)": {platform: Dashboard.Platform.MacOSXElCapitan, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple El Capitan Debug WK2 (Tests)": {platform: Dashboard.Platform.MacOSXElCapitan, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple El Capitan Release WK1 (Tests)": {platform: Dashboard.Platform.MacOSXElCapitan, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple El Capitan Release WK2 (Tests)": {platform: Dashboard.Platform.MacOSXElCapitan, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple El Capitan Release WK2 (Perf)": {platform: Dashboard.Platform.MacOSXElCapitan, debug: false, performance: true, heading: "Performance"},
        "Apple El Capitan (Leaks)": {platform: Dashboard.Platform.MacOSXElCapitan, debug: false, leaks: true},
        "Apple El Capitan JSC": {platform: Dashboard.Platform.MacOSXElCapitan, heading: "JavaScript", combinedQueues: {
            "Apple El Capitan 32-bit JSC (BuildAndTest)": {heading: "32-bit JSC (BuildAndTest)"},
            "Apple El Capitan LLINT CLoop (BuildAndTest)": {heading: "LLINT CLoop (BuildAndTest)"},
            "Apple El Capitan Debug JSC (Tests)": {heading: "Debug JSC (Tests)"},
            "Apple El Capitan Release JSC (Tests)": {heading: "Release JSC (Tests)"},
        }},
        "Apple Yosemite Debug (Build)": {platform: Dashboard.Platform.MacOSXYosemite, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple Yosemite Release (Build)": {platform: Dashboard.Platform.MacOSXYosemite, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple Yosemite Release (32-bit Build)": {platform: Dashboard.Platform.MacOSXYosemite, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple Yosemite Debug WK1 (Tests)": {platform: Dashboard.Platform.MacOSXYosemite, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Yosemite Debug WK2 (Tests)": {platform: Dashboard.Platform.MacOSXYosemite, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple Yosemite Release WK1 (Tests)": {platform: Dashboard.Platform.MacOSXYosemite, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Yosemite Release WK2 (Tests)": {platform: Dashboard.Platform.MacOSXYosemite, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple Yosemite Release WK2 (Perf)": {platform: Dashboard.Platform.MacOSXYosemite, debug: false, performance: true, heading: "Performance"},
        "Apple Yosemite (Leaks)": {platform: Dashboard.Platform.MacOSXYosemite, debug: true, leaks: true},
        "Apple Yosemite JSC": {platform: Dashboard.Platform.MacOSXYosemite, heading: "JavaScript", combinedQueues: {
            "Apple Yosemite 32-bit JSC (BuildAndTest)": {heading: "32-bit JSC (BuildAndTest)"},
            "Apple Yosemite LLINT CLoop (BuildAndTest)": {heading: "LLINT CLoop (BuildAndTest)"},
            "Apple Yosemite Debug JSC (Tests)": {heading: "Debug JSC (Tests)"},
            "Apple Yosemite Release JSC (Tests)": {heading: "Release JSC (Tests)"},
        }},
        "Apple iOS 9 Release (Build)": {platform: Dashboard.Platform.iOS9Device, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.Universal},
        "Apple iOS 9 Simulator Release (Build)": {platform: Dashboard.Platform.iOS9Simulator, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple iOS 9 Simulator Release WK1 (Tests)": {platform: Dashboard.Platform.iOS9Simulator, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple iOS 9 Simulator Release WK2 (Tests)": {platform: Dashboard.Platform.iOS9Simulator, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple iOS 9 Simulator Debug (Build)": {platform: Dashboard.Platform.iOS9Simulator, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple iOS 9 Simulator Debug WK1 (Tests)": {platform: Dashboard.Platform.iOS9Simulator, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple iOS 9 Simulator Debug WK2 (Tests)": {platform: Dashboard.Platform.iOS9Simulator, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple Win Debug (Build)": {platform: Dashboard.Platform.Windows7, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple Win Release (Build)": {platform: Dashboard.Platform.Windows7, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple Win 7 Debug (Tests)": {platform: Dashboard.Platform.Windows7, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Win 7 Release (Tests)": {platform: Dashboard.Platform.Windows7, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "GTK Linux 64-bit Release (Build)": {platform: Dashboard.Platform.LinuxGTK, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "GTK Linux 64-bit Release (Tests)": {platform: Dashboard.Platform.LinuxGTK, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "GTK Linux 64-bit Debug (Build)": {platform: Dashboard.Platform.LinuxGTK, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "GTK Linux 64-bit Debug (Tests)": {platform: Dashboard.Platform.LinuxGTK, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "GTK Linux 64-bit Release (Perf)": {platform: Dashboard.Platform.LinuxGTK, debug: false, performance: true, heading: "Performance"},
        "EFL Linux 64-bit Release WK2": {platform: Dashboard.Platform.LinuxEFL, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "EFL Linux 64-bit Release WK2 (Perf)": {platform: Dashboard.Platform.LinuxEFL, performance: true, heading: "Performance"}
    };

    Buildbot.call(this, "https://build.webkit.org/", queueInfo);
};

BaseObject.addConstructorFunctions(WebKitBuildbot);

WebKitBuildbot.prototype = {
    constructor: WebKitBuildbot,
    __proto__: Buildbot.prototype,
    performanceDashboardURL:  "https://perf.webkit.org",

    get defaultBranches()
    {
        return [{ repository: Dashboard.Repository.OpenSource, name: "trunk" }];
    }
};
