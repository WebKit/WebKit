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
        "Apple Sierra Debug (Build)": {platform: Dashboard.Platform.macOSSierra, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple Sierra Release (Build)": {platform: Dashboard.Platform.macOSSierra, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple Sierra Release (32-bit Build)": {platform: Dashboard.Platform.macOSSierra, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple Sierra Debug WK1 (Tests)": {platform: Dashboard.Platform.macOSSierra, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Sierra Debug WK2 (Tests)": {platform: Dashboard.Platform.macOSSierra, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple Sierra Release WK1 (Tests)": {platform: Dashboard.Platform.macOSSierra, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Sierra Release WK2 (Tests)": {platform: Dashboard.Platform.macOSSierra, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple Sierra (Leaks)": {platform: Dashboard.Platform.macOSSierra, debug: false, leaks: true},
        "Apple Sierra JSC": {platform: Dashboard.Platform.macOSSierra, heading: "JavaScript", combinedQueues: {
            "Apple Sierra 32-bit JSC (BuildAndTest)": {heading: "32-bit JSC (BuildAndTest)"},
            "Apple Sierra LLINT CLoop (BuildAndTest)": {heading: "LLINT CLoop (BuildAndTest)"},
            "Apple Sierra Debug JSC (Tests)": {heading: "Debug JSC (Tests)"},
            "Apple Sierra Release JSC (Tests)": {heading: "Release JSC (Tests)"},
        }},
        "Apple El Capitan Debug (Build)": {platform: Dashboard.Platform.MacOSXElCapitan, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple El Capitan Release (Build)": {platform: Dashboard.Platform.MacOSXElCapitan, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple El Capitan Release (32-bit Build)": {platform: Dashboard.Platform.MacOSXElCapitan, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple El Capitan Debug WK1 (Tests)": {platform: Dashboard.Platform.MacOSXElCapitan, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple El Capitan Debug WK2 (Tests)": {platform: Dashboard.Platform.MacOSXElCapitan, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple El Capitan Release WK1 (Tests)": {platform: Dashboard.Platform.MacOSXElCapitan, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple El Capitan Release WK2 (Tests)": {platform: Dashboard.Platform.MacOSXElCapitan, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple El Capitan Release WK2 (Perf)": {platform: Dashboard.Platform.MacOSXElCapitan, debug: false, performance: true, heading: "Performance"},
        "Apple El Capitan JSC": {platform: Dashboard.Platform.MacOSXElCapitan, heading: "JavaScript", combinedQueues: {
            "Apple El Capitan 32-bit JSC (BuildAndTest)": {heading: "32-bit JSC (BuildAndTest)"},
            "Apple El Capitan LLINT CLoop (BuildAndTest)": {heading: "LLINT CLoop (BuildAndTest)"},
            "Apple El Capitan Debug JSC (Tests)": {heading: "Debug JSC (Tests)"},
            "Apple El Capitan Release JSC (Tests)": {heading: "Release JSC (Tests)"},
        }},
        "Apple iOS 10 Release (Build)": {platform: Dashboard.Platform.iOS10Device, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.Universal},
        "Apple iOS 10 Simulator Release (Build)": {platform: Dashboard.Platform.iOS10Simulator, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple iOS 10 Simulator Release WK1 (Tests)": {platform: Dashboard.Platform.iOS10Simulator, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple iOS 10 Simulator Release WK2 (Tests)": {platform: Dashboard.Platform.iOS10Simulator, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple iOS 10 Simulator Debug (Build)": {platform: Dashboard.Platform.iOS10Simulator, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple iOS 10 Simulator Debug WK1 (Tests)": {platform: Dashboard.Platform.iOS10Simulator, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple iOS 10 Simulator Debug WK2 (Tests)": {platform: Dashboard.Platform.iOS10Simulator, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple Win Debug (Build)": {platform: Dashboard.Platform.Windows7, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple Win Release (Build)": {platform: Dashboard.Platform.Windows7, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple Win 7 Debug (Tests)": {platform: Dashboard.Platform.Windows7, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Win 7 Release (Tests)": {platform: Dashboard.Platform.Windows7, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "GTK Linux 64-bit Release (Build)": {platform: Dashboard.Platform.LinuxGTK, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "GTK Linux 64-bit Release (Tests)": {platform: Dashboard.Platform.LinuxGTK, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "GTK Linux 64-bit Debug (Build)": {platform: Dashboard.Platform.LinuxGTK, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "GTK Linux 64-bit Debug (Tests)": {platform: Dashboard.Platform.LinuxGTK, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "GTK Linux 64-bit Release (Perf)": {platform: Dashboard.Platform.LinuxGTK, debug: false, performance: true, heading: "Performance"}
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
