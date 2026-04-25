/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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
        "Apple-Tahoe-Release-Build": {platform: Dashboard.Platform.macOSTahoe, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-Tahoe-Debug-Build": {platform: Dashboard.Platform.macOSTahoe, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-Tahoe-Debug-WK1-Tests": {platform: Dashboard.Platform.macOSTahoe, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple-Tahoe-Debug-WK2-Tests": {platform: Dashboard.Platform.macOSTahoe, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-Tahoe-Release-WK1-Tests": {platform: Dashboard.Platform.macOSTahoe, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple-Tahoe-Release-WK2-Tests": {platform: Dashboard.Platform.macOSTahoe, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-Tahoe-Debug-AppleSilicon-WK1-Tests": {platform: Dashboard.Platform.macOSTahoe, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1, heading: "Debug AppleSilicon"},
        "Apple-Tahoe-Debug-AppleSilicon-WK2-Tests": {platform: Dashboard.Platform.macOSTahoe, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2, heading: "Debug AppleSilicon"},
        "Apple-Tahoe-Release-AppleSilicon-WK1-Tests": {platform: Dashboard.Platform.macOSTahoe, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1, heading: "Release AppleSilicon"},
        "Apple-Tahoe-Release-AppleSilicon-WK2-Tests": {platform: Dashboard.Platform.macOSTahoe, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2, heading: "Release AppleSilicon"},
        "Apple-Tahoe-Release-WK2-Perf": {platform: Dashboard.Platform.macOSTahoe, debug: false, performance: true, heading: "Performance"},
        "Apple-Tahoe JSC": {platform: Dashboard.Platform.macOSTahoe, heading: "JavaScript", combinedQueues: {
            "Apple-Tahoe-AppleSilicon-Release-Test262-Tests": {heading: "Release arm64 Test262 (Tests)"},
            "Apple-Tahoe-LLINT-CLoop-BuildAndTest": {heading: "LLINT CLoop (BuildAndTest)"},
        }},
        "Apple-Tahoe-World-Leaks": {platform: Dashboard.Platform.macOSTahoe, heading: "World Leaks", combinedQueues: {
            "Apple-Tahoe-Release-World-Leaks-Tests": {heading: "World Leaks (Tests)"},
        }},
        "Apple-Sequoia-Release-Build": {platform: Dashboard.Platform.macOSSequoia, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-Sequoia-Debug-Build": {platform: Dashboard.Platform.macOSSequoia, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-Sequoia-Debug-WK1-Tests": {platform: Dashboard.Platform.macOSSequoia, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple-Sequoia-Debug-WK2-Tests": {platform: Dashboard.Platform.macOSSequoia, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-Sequoia-Release-WK1-Tests": {platform: Dashboard.Platform.macOSSequoia, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple-Sequoia-Release-WK2-Tests": {platform: Dashboard.Platform.macOSSequoia, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-Sequoia-Debug-AppleSilicon-WK2-Tests": {platform: Dashboard.Platform.macOSSequoia, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2, heading: "Debug AppleSilicon"},
        "Apple-Sequoia-Release-AppleSilicon-WK2-Tests": {platform: Dashboard.Platform.macOSSequoia, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2, heading: "Release AppleSilicon"},
        "Apple-Sequoia JSC": {platform: Dashboard.Platform.macOSSequoia, heading: "JavaScript", combinedQueues: {
            "Apple-Sequoia-Debug-Test262-Tests": {heading: "Debug Test262 (Tests)"},
            "Apple-Sequoia-Release-Test262-Tests": {heading: "Release Test262 (Tests)"},
            "Apple-Sequoia-AppleSilicon-O3-Debug-JSC-BuildAndTest": {heading: "O3 Debug arm64 JSC (BuildAndTest)"},
            "Apple-Sequoia-AppleSilicon-Release-JSC-Tests": {heading: "Release arm64 JSC (Tests)"},
            "Apple-Sequoia-Intel-Release-JSC-Tests": {heading: "Release x86_64 JSC (Tests)"},
        }},
        "Apple-iOS-26-Release-Build": {platform: Dashboard.Platform.iOS26Device, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-iOS-26-Simulator-Release-Build": {platform: Dashboard.Platform.iOS26Simulator, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-iOS-26-Simulator-Release-WK2-Tests": {platform: Dashboard.Platform.iOS26Simulator, heading:"iOS Release", debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-iOS-26-Simulator-Debug-Build": {platform: Dashboard.Platform.iOS26Simulator, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-iOS-26-Simulator-Debug-WK2-Tests": {platform: Dashboard.Platform.iOS26Simulator, debug: true, heading:"iOS Debug", tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-iPadOS-26-Simulator-Release-WK2-Tests": {platform: Dashboard.Platform.iOS26Simulator, heading:"iPadOS Release", debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-iPadOS-26-Simulator-Debug-WK2-Tests": {platform: Dashboard.Platform.iOS26Simulator, heading:"iPadOS Debug", debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-visionOS-26-Release-Build": {platform: Dashboard.Platform.visionOS26Device, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-visionOS-26-Simulator-Release-Build": {platform: Dashboard.Platform.visionOS26Simulator, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-visionOS-26-Simulator-Release-WK2-Tests": {platform: Dashboard.Platform.visionOS26Simulator, heading:"visionOS Release", debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-visionOS-26-Simulator-Debug-Build": {platform: Dashboard.Platform.visionOS26Simulator, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-visionOS-26-Simulator-Debug-WK2-Tests": {platform: Dashboard.Platform.visionOS26Simulator, debug: true, heading:"visionOS Debug", tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-tvOS-26-Release-Build": {platform: Dashboard.Platform.tvOS26Device, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-tvOS-Simulator-26-Release-Build": {platform: Dashboard.Platform.tvOS26Simulator, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-watchOS-26-Release-Build": {platform: Dashboard.Platform.watchOS26Device, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple-watchOS-Simulator-26-Release-Build": {platform: Dashboard.Platform.watchOS26Simulator, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Windows-64-bit-Release-Build": {platform: Dashboard.Platform.Windows, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Windows-64-bit-Release-Tests": {platform: Dashboard.Platform.Windows, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Windows-64-bit-Debug-Build": {platform: Dashboard.Platform.Windows, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Windows-64-bit-Debug-Tests": {platform: Dashboard.Platform.Windows, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "WPE-Linux-64-bit-Release-Build": {platform: Dashboard.Platform.LinuxWPE, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "WPE-Linux-64-bit-Release-Tests": {platform: Dashboard.Platform.LinuxWPE, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "WPE-Linux-64-bit-Debug-Build": {platform: Dashboard.Platform.LinuxWPE, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "WPE-Linux-64-bit-Debug-Tests": {platform: Dashboard.Platform.LinuxWPE, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "GTK-Linux-64-bit-Release-Build": {platform: Dashboard.Platform.LinuxGTK, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "GTK-Linux-64-bit-Release-Tests": {platform: Dashboard.Platform.LinuxGTK, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "GTK-Linux-64-bit-Debug-Build": {platform: Dashboard.Platform.LinuxGTK, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "GTK-Linux-64-bit-Debug-Tests": {platform: Dashboard.Platform.LinuxGTK, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "GTK-Linux-64-bit-Release-Perf": {platform: Dashboard.Platform.LinuxGTK, debug: false, performance: true, heading: "Performance"},
        "GTK LTS Builders": {platform: Dashboard.Platform.LinuxGTK, heading: "LTS Builders", combinedQueues: {
            "GTK-Linux-64-bit-Release-Debian-Stable-Build": {heading: "Debian Stable (Build)"},
            "GTK-Linux-64-bit-Release-Ubuntu-LTS-Build": {heading: "Ubuntu LTS (Build)"},
        }},
        "JSCOnly AArch64 Testers": {platform: Dashboard.Platform.LinuxJSCOnly, heading: "AArch64", combinedQueues: {
            "JSCOnly-Linux-AArch64-Release": {heading: "AArch64"},
        }},
        "JSCOnly ARMv7 Testers": {platform: Dashboard.Platform.LinuxJSCOnly, heading: "ARMv7", combinedQueues: {
            "JSCOnly-Linux-ARMv7-Thumb2-Release": {heading: "ARMv7 Thumb2"},
        }},
        "PlayStation-Release-Build": {platform: Dashboard.Platform.PlayStation, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "PlayStation-Debug-Build": {platform: Dashboard.Platform.PlayStation, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
    };

    Buildbot.call(this, "https://build.webkit.org/", queueInfo, {});
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
