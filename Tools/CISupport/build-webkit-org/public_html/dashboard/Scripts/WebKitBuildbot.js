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
        "Apple-Sonoma-Release-Build": {platform: Dashboard.Platform.macOSSonoma, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-Sonoma-Debug-Build": {platform: Dashboard.Platform.macOSSonoma, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-Sonoma-Debug-WK1-Tests": {platform: Dashboard.Platform.macOSSonoma, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple-Sonoma-Debug-WK2-Tests": {platform: Dashboard.Platform.macOSSonoma, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-Sonoma-Release-WK1-Tests": {platform: Dashboard.Platform.macOSSonoma, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple-Sonoma-Release-WK2-Tests": {platform: Dashboard.Platform.macOSSonoma, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-Sonoma-Debug-AppleSilicon-WK1-Tests": {platform: Dashboard.Platform.macOSSonoma, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1, heading: "Debug AppleSilicon"},
        "Apple-Sonoma-Debug-AppleSilicon-WK2-Tests": {platform: Dashboard.Platform.macOSSonoma, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2, heading: "Debug AppleSilicon"},
        "Apple-Sonoma-Release-AppleSilicon-WK1-Tests": {platform: Dashboard.Platform.macOSSonoma, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1, heading: "Release AppleSilicon"},
        "Apple-Sonoma-Release-AppleSilicon-WK2-Tests": {platform: Dashboard.Platform.macOSSonoma, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2, heading: "Release AppleSilicon"},
        "Apple-Sonoma-Release-WK2-Perf": {platform: Dashboard.Platform.macOSSonoma, debug: false, performance: true, heading: "Performance"},
        "Apple-Sonoma JSC": {platform: Dashboard.Platform.macOSSonoma, heading: "JavaScript", combinedQueues: {
            "Apple-Sonoma-AppleSilicon-Release-Test262-Tests": {heading: "Release arm64 Test262 (Tests)"},
            "Apple-Sonoma-LLINT-CLoop-BuildAndTest": {heading: "LLINT CLoop (BuildAndTest)"},
        }},
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
            "Apple-Monterey-AppleSilicon-Debug-JSC-Tests": {heading: "Debug arm64 JSC (Tests)"},
            "Apple-Monterey-AppleSilicon-Release-JSC-Tests": {heading: "Release arm64 JSC (Tests)"},
        }},
        "Apple-iOS-17-Release-Build": {platform: Dashboard.Platform.iOS17Device, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-iOS-17-Simulator-Release-Build": {platform: Dashboard.Platform.iOS17Simulator, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-iOS-17-Simulator-Release-WK2-Tests": {platform: Dashboard.Platform.iOS17Simulator, heading:"iOS Release", debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-iOS-17-Simulator-Debug-Build": {platform: Dashboard.Platform.iOS17Simulator, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-iOS-17-Simulator-Debug-WK2-Tests": {platform: Dashboard.Platform.iOS17Simulator, debug: true, heading:"iOS Debug", tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-iPadOS-17-Simulator-Release-WK2-Tests": {platform: Dashboard.Platform.iOS17Simulator, heading:"iPadOS Release", debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-iPadOS-17-Simulator-Debug-WK2-Tests": {platform: Dashboard.Platform.iOS17Simulator, heading:"iPadOS Debug", debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple-tvOS-17-Release-Build": {platform: Dashboard.Platform.tvOS17Device, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-tvOS-Simulator-17-Release-Build": {platform: Dashboard.Platform.tvOS17Simulator, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple-watchOS-10-Release-Build": {platform: Dashboard.Platform.watchOS10Device, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple-watchOS-Simulator-10-Release-Build": {platform: Dashboard.Platform.watchOS10Simulator, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
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
