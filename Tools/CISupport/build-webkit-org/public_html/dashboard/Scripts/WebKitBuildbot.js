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
        "Apple-Ventura-Release-Build": {platform: Dashboard.Platform.macOSVentura, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-Ventura-Debug-Build": {platform: Dashboard.Platform.macOSVentura, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-Ventura-Debug-WK1-Tests": {platform: Dashboard.Platform.macOSVentura, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple-Ventura-Debug-WK2-Tests": {platform: Dashboard.Platform.macOSVentura, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-Ventura-Release-WK1-Tests": {platform: Dashboard.Platform.macOSVentura, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple-Ventura-Release-WK2-Tests": {platform: Dashboard.Platform.macOSVentura, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-Ventura-Debug-AppleSilicon-WK1-Tests": {platform: Dashboard.Platform.macOSVentura, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1, heading: "Debug AppleSilicon"},
        "Apple-Ventura-Debug-AppleSilicon-WK2-Tests": {platform: Dashboard.Platform.macOSVentura, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2, heading: "Debug AppleSilicon"},
        "Apple-Ventura-Release-AppleSilicon-WK1-Tests": {platform: Dashboard.Platform.macOSVentura, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1, heading: "Release AppleSilicon"},
        "Apple-Ventura-Release-AppleSilicon-WK2-Tests": {platform: Dashboard.Platform.macOSVentura, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2, heading: "Release AppleSilicon"},
        "Apple-Monterey-Release-Build": {platform: Dashboard.Platform.macOSMonterey, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-Monterey-Debug-Build": {platform: Dashboard.Platform.macOSMonterey, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-Monterey-Debug-WK1-Tests": {platform: Dashboard.Platform.macOSMonterey, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple-Monterey-Debug-WK2-Tests": {platform: Dashboard.Platform.macOSMonterey, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-Monterey-Release-WK1-Tests": {platform: Dashboard.Platform.macOSMonterey, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple-Monterey-Release-WK2-Tests": {platform: Dashboard.Platform.macOSMonterey, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-Monterey-Debug-AppleSilicon-WK1-Tests": {platform: Dashboard.Platform.macOSMonterey, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1, heading: "Debug AppleSilicon"},
        "Apple-Monterey-Debug-AppleSilicon-WK2-Tests": {platform: Dashboard.Platform.macOSMonterey, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2, heading: "Debug AppleSilicon"},
        "Apple-Monterey-Release-AppleSilicon-WK1-Tests": {platform: Dashboard.Platform.macOSMonterey, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1, heading: "Release AppleSilicon"},
        "Apple-Monterey-Release-AppleSilicon-WK2-Tests": {platform: Dashboard.Platform.macOSMonterey, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2, heading: "Release AppleSilicon"},
        "Apple-Monterey JSC": {platform: Dashboard.Platform.macOSMonterey, heading: "JavaScript", combinedQueues: {
            "Apple-Monterey-Debug-Test262-Tests": {heading: "Debug Test262 (Tests)"},
            "Apple-Monterey-Release-Test262-Tests": {heading: "Release Test262 (Tests)"},
            "Apple-Monterey-AppleSilicon-Release-Test262-Tests": {heading: "Release arm64 Test262 (Tests)"},
            "Apple-Monterey-AppleSilicon-Debug-JSC-Tests": {heading: "Debug arm64 JSC (Tests)"},
            "Apple-Monterey-AppleSilicon-Release-JSC-Tests": {heading: "Release arm64 JSC (Tests)"},
        }},
        "Apple-BigSur-Release-Build": {platform: Dashboard.Platform.macOSBigSur, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-BigSur-Debug-Build": {platform: Dashboard.Platform.macOSBigSur, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-BigSur-Debug-WK1-Tests": {platform: Dashboard.Platform.macOSBigSur, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple-BigSur-Debug-WK2-Tests": {platform: Dashboard.Platform.macOSBigSur, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-BigSur-Release-WK2-Perf": {platform: Dashboard.Platform.macOSBigSur, debug: false, performance: true, heading: "Performance"},
        "Apple-BigSur-Release-WK1-Tests": {platform: Dashboard.Platform.macOSBigSur, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple-BigSur-Release-WK2-Tests": {platform: Dashboard.Platform.macOSBigSur, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-BigSur JSC": {platform: Dashboard.Platform.macOSBigSur, heading: "JavaScript", combinedQueues: {
            "Apple-BigSur-LLINT-CLoop-BuildAndTest": {heading: "LLINT CLoop (BuildAndTest)"},
            "Apple-BigSur-Debug-JSC-Tests": {heading: "Debug JSC (Tests)"},
            "Apple-BigSur-Release-JSC-Tests": {heading: "Release JSC (Tests)"},
        }},
        "Apple-iOS-16-Release-Build": {platform: Dashboard.Platform.iOS16Device, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-iOS-16-Simulator-Release-Build": {platform: Dashboard.Platform.iOS16Simulator, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-iOS-16-Simulator-Release-arm64-WK2-Tests": {platform: Dashboard.Platform.iOS16Simulator, heading:"iOS Release (arm64)", debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-iOS-16-Simulator-Release-WK2-Tests": {platform: Dashboard.Platform.iOS16Simulator, heading:"iOS Release", debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-iOS-16-Simulator-Debug-Build": {platform: Dashboard.Platform.iOS16Simulator, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-iOS-16-Simulator-Debug-WK2-Tests": {platform: Dashboard.Platform.iOS16Simulator, debug: true, heading:"iOS Debug", tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-iPadOS-16-Simulator-Release-WK2-Tests": {platform: Dashboard.Platform.iOS16Simulator, heading:"iPadOS Release", debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-iPadOS-16-Simulator-Debug-WK2-Tests": {platform: Dashboard.Platform.iOS16Simulator, heading:"iPadOS Debug", debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-tvOS-16-Release-Build": {platform: Dashboard.Platform.tvOS16Device, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-tvOS-Simulator-16-Release-Build": {platform: Dashboard.Platform.tvOS16Simulator, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-watchOS-9-Release-Build": {platform: Dashboard.Platform.watchOS9Device, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple-watchOS-Simulator-9-Release-Build": {platform: Dashboard.Platform.watchOS9Simulator, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-Win-10-Debug-Build": {platform: Dashboard.Platform.Windows10, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit },
        "Apple-Win-10-Release-Build": {platform: Dashboard.Platform.Windows10, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple-Win-10-Debug-Tests": {platform: Dashboard.Platform.Windows10, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple-Win-10-Release-Tests": {platform: Dashboard.Platform.Windows10, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "WinCairo-64-bit-Release-Build": {platform: Dashboard.Platform.WinCairo, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "WinCairo-64-bit-Release-Tests": {platform: Dashboard.Platform.WinCairo, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "WinCairo-64-bit-WKL-Release-Tests": {platform: Dashboard.Platform.WinCairo, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "WinCairo-64-bit-Debug-Build": {platform: Dashboard.Platform.WinCairo, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "WinCairo-64-bit-Debug-Tests": {platform: Dashboard.Platform.WinCairo, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "WinCairo-64-bit-WKL-Debug-Tests": {platform: Dashboard.Platform.WinCairo, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
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
        "GTK Wayland Testers": {platform: Dashboard.Platform.LinuxGTK, heading: "Wayland", combinedQueues: {
            "GTK-Linux-64-bit-Release-Wayland-Tests": {heading: "Wayland"},
        }},
        "JSCOnly AArch64 Testers": {platform: Dashboard.Platform.LinuxJSCOnly, heading: "AArch64", combinedQueues: {
            "JSCOnly-Linux-AArch64-Release": {heading: "AArch64"},
        }},
        "JSCOnly ARMv7 Testers": {platform: Dashboard.Platform.LinuxJSCOnly, heading: "ARMv7", combinedQueues: {
            "JSCOnly-Linux-ARMv7-Thumb2-Release": {heading: "ARMv7 Thumb2"},
        }},
        "JSCOnly MIPS Testers": {platform: Dashboard.Platform.LinuxJSCOnly, heading: "MIPS", combinedQueues: {
            "JSCOnly-Linux-MIPS32el-Release": {heading: "MIPS32el"},
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
