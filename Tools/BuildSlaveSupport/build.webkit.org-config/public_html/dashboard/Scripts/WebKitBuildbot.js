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
        "Apple Mojave Debug (Build)": {platform: Dashboard.Platform.macOSMojave, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple Mojave Release (Build)": {platform: Dashboard.Platform.macOSMojave, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple Mojave Debug WK1 (Tests)": {platform: Dashboard.Platform.macOSMojave, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Mojave Debug WK2 (Tests)": {platform: Dashboard.Platform.macOSMojave, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple Mojave Release WK1 (Tests)": {platform: Dashboard.Platform.macOSMojave, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Mojave Release WK2 (Tests)": {platform: Dashboard.Platform.macOSMojave, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple Mojave (Leaks)": {platform: Dashboard.Platform.macOSMojave, debug: false, leaks: true},
        "Apple High Sierra Debug (Build)": {platform: Dashboard.Platform.macOSHighSierra, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple High Sierra Release (Build)": {platform: Dashboard.Platform.macOSHighSierra, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple High Sierra Debug WK1 (Tests)": {platform: Dashboard.Platform.macOSHighSierra, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple High Sierra Debug WK2 (Tests)": {platform: Dashboard.Platform.macOSHighSierra, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple High Sierra Release WK1 (Tests)": {platform: Dashboard.Platform.macOSHighSierra, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple High Sierra Release WK2 (Perf)": {platform: Dashboard.Platform.macOSHighSierra, debug: false, performance: true, heading: "Performance"},
        "Apple High Sierra Release WK2 (Tests)": {platform: Dashboard.Platform.macOSHighSierra, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple High Sierra JSC": {platform: Dashboard.Platform.macOSHighSierra, heading: "JavaScript", combinedQueues: {
            "Apple High Sierra LLINT CLoop (BuildAndTest)": {heading: "LLINT CLoop (BuildAndTest)"},
            "Apple High Sierra Debug JSC (Tests)": {heading: "Debug JSC (Tests)"},
            "Apple High Sierra Release JSC (Tests)": {heading: "Release JSC (Tests)"},
            "Apple High Sierra Debug Test262 (Tests)": {heading: "Debug Test262 (Tests)"},
            "Apple High Sierra Release Test262 (Tests)": {heading: "Release Test262 (Tests)"},
        }},
        "Apple iOS 12 Release (Build)": {platform: Dashboard.Platform.iOS12Device, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple iOS 12 Simulator Release (Build)": {platform: Dashboard.Platform.iOS12Simulator, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple iOS 12 Simulator Release WK2 (Tests)": {platform: Dashboard.Platform.iOS12Simulator, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple iOS 12 Simulator Debug (Build)": {platform: Dashboard.Platform.iOS12Simulator, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "Apple iOS 12 Simulator Debug WK2 (Tests)": {platform: Dashboard.Platform.iOS12Simulator, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "Apple Win 10 Debug (Build)": {platform: Dashboard.Platform.Windows10, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit },
        "Apple Win 10 Release (Build)": {platform: Dashboard.Platform.Windows10, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple Win 10 Debug (Tests)": {platform: Dashboard.Platform.Windows10, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Win 10 Release (Tests)": {platform: Dashboard.Platform.Windows10, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "Apple Win 7 Release (Build)": {platform: Dashboard.Platform.Windows7, builder: true, architecture: Buildbot.BuildArchitecture.ThirtyTwoBit},
        "Apple Win 7 Release (Tests)": {platform: Dashboard.Platform.Windows7, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "WinCairo 64-bit WKL Release (Build)": {platform: Dashboard.Platform.WinCairo, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "WinCairo 64-bit WKL Release (Tests)": {platform: Dashboard.Platform.WinCairo, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "WinCairo 64-bit WKL Debug (Build)": {platform: Dashboard.Platform.WinCairo, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "WinCairo 64-bit WKL Debug (Tests)": {platform: Dashboard.Platform.WinCairo, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit1},
        "WPE Linux 64-bit Release (Build)": {platform: Dashboard.Platform.LinuxWPE, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "WPE Linux 64-bit Release (Tests)": {platform: Dashboard.Platform.LinuxWPE, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "WPE Linux 64-bit Debug (Build)": {platform: Dashboard.Platform.LinuxWPE, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "WPE Linux 64-bit Debug (Tests)": {platform: Dashboard.Platform.LinuxWPE, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "GTK Linux 64-bit Release (Build)": {platform: Dashboard.Platform.LinuxGTK, debug: false, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "GTK Linux 64-bit Release (Tests)": {platform: Dashboard.Platform.LinuxGTK, debug: false, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "GTK Linux 64-bit Debug (Build)": {platform: Dashboard.Platform.LinuxGTK, debug: true, builder: true, architecture: Buildbot.BuildArchitecture.SixtyFourBit},
        "GTK Linux 64-bit Debug (Tests)": {platform: Dashboard.Platform.LinuxGTK, debug: true, tester: true, testCategory: Buildbot.TestCategory.WebKit2},
        "GTK Linux 64-bit Release (Perf)": {platform: Dashboard.Platform.LinuxGTK, debug: false, performance: true, heading: "Performance"},
        "GTK LTS Builders": {platform: Dashboard.Platform.LinuxGTK, heading: "LTS Builders", combinedQueues: {
            "GTK Linux 64-bit Release Debian Stable (Build)": {heading: "Debian Stable (Build)"},
            "GTK Linux 64-bit Release Ubuntu LTS (Build)": {heading: "Ubuntu LTS (Build)"},
        }},
        "GTK Wayland Testers": {platform: Dashboard.Platform.LinuxGTK, heading: "Wayland", combinedQueues: {
            "GTK Linux 64-bit Release Wayland (Tests)": {heading: "Wayland"},
        }},
        "JSCOnly AArch64 Testers": {platform: Dashboard.Platform.LinuxJSCOnly, heading: "AArch64", combinedQueues: {
            "JSCOnly Linux AArch64 Release": {heading: "AArch64"},
        }},
        "JSCOnly ARMv7 Testers": {platform: Dashboard.Platform.LinuxJSCOnly, heading: "ARMv7", combinedQueues: {
            "JSCOnly Linux ARMv7 Thumb2 Release": {heading: "ARMv7 Thumb2"},
            "JSCOnly Linux ARMv7 Thumb2 SoftFP Release": {heading: "ARMv7 Thumb2 SoftFP"},
        }},
        "JSCOnly MIPS Testers": {platform: Dashboard.Platform.LinuxJSCOnly, heading: "MIPS", combinedQueues: {
            "JSCOnly Linux MIPS32el Release": {heading: "MIPS32el"},
        }},
    };

    Buildbot.call(this, "https://build.webkit.org/", queueInfo, {"USE_BUILDBOT_VERSION_LESS_THAN_09" : true});
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
