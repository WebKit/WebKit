/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

Dashboard = {
    Platform: {
        macOSCatalina: { name: "macos-catalina", readableName: "macOS Catalina", order: 1 },
        macOSMojave: { name: "macos-mojave", readableName: "macOS Mojave", order: 2 },
        macOSHighSierra: { name: "macos-highsierra", readableName: "macOS High Sierra", order: 3 },
        iOS13Simulator: { name: "ios-simulator-13", readableName: "iOS 13 Simulator", order: 20 },
        iOS13Device: { name: "ios-13", readableName: "iOS 13", order: 25 },
        tvOS13Simulator: { name: "tvos-simulator-13", readableName: "TvOS Simulator 13", order: 26 },
        tvOS13Device: { name: "tvos-13", readableName: "TvOS 13", order: 27 },
        watchOS6Simulator: { name: "watchos-simulator-6", readableName: "WatchOS Simulator 6", order: 28 },
        watchOS6Device: { name: "watchos-6", readableName: "WatchOS 6", order: 29 },
        Windows10: { name: "windows-10", readableName: "Windows 10", order: 30 },
        WinCairo: { name: "wincairo-windows-10", readableName: "WinCairo", order: 50 },
        LinuxWPE: { name : "linux-wpe", readableName: "Linux WPE", order: 90 },
        LinuxGTK: { name : "linux-gtk", readableName: "Linux GTK", order: 91 },
        LinuxJSCOnly: { name : "linux-jsconly", readableName: "Linux JSCOnly", order: 92 },
    },
    Branch: {},
    Repository: {
        OpenSource: { name: "openSource", isSVN: true, order: 0 },
    },
    get sortedPlatforms()
    {
        if (!this._sortedPlatforms)
            this._sortedPlatforms = sortDictionariesByOrder(Dashboard.Platform);
        return this._sortedPlatforms;
    },
    get sortedRepositories()
    {
        if (!this._sortedRepositories)
            this._sortedRepositories = sortDictionariesByOrder(Dashboard.Repository);
        return this._sortedRepositories;
    },
};
