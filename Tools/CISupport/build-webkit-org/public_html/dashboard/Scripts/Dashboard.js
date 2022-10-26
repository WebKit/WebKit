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
        macOSVentura: { name: "macos-ventura", readableName: "macOS Ventura", order: 1 },
        macOSMonterey: { name: "macos-monterey", readableName: "macOS Monterey", order: 2 },
        macOSBigSur: { name: "macos-bigsur", readableName: "macOS Big Sur", order: 3 },
        iOS16Simulator: { name: "ios-simulator-16", readableName: "iOS 16 Simulator", order: 13 },
        iOS16Device: { name: "ios-16", readableName: "iOS 16", order: 14 },
        iOS15Simulator: { name: "ios-simulator-15", readableName: "iOS 15 Simulator", order: 15 },
        iOS15Device: { name: "ios-15", readableName: "iOS 15", order: 16 },
        iOS14Simulator: { name: "ios-simulator-14", readableName: "iOS 14 Simulator", order: 17 },
        iOS14Device: { name: "ios-14", readableName: "iOS 14", order: 18 },
        tvOS16Simulator: { name: "tvos-simulator-16", readableName: "TvOS Simulator 16", order: 24 },
        tvOS16Device: { name: "tvos-16", readableName: "TvOS 16", order: 25 },
        tvOS15Simulator: { name: "tvos-simulator-15", readableName: "TvOS Simulator 15", order: 24 },
        tvOS15Device: { name: "tvos-15", readableName: "TvOS 15", order: 25 },
        tvOS14Simulator: { name: "tvos-simulator-14", readableName: "TvOS Simulator 14", order: 26 },
        tvOS14Device: { name: "tvos-14", readableName: "TvOS 14", order: 27 },
        watchOS9Simulator: { name: "watchos-simulator-9", readableName: "WatchOS Simulator 9", order: 40 },
        watchOS9Device: { name: "watchos-9", readableName: "WatchOS 9", order: 41 },
        watchOS8Simulator: { name: "watchos-simulator-8", readableName: "WatchOS Simulator 8", order: 40 },
        watchOS8Device: { name: "watchos-8", readableName: "WatchOS 8", order: 41 },
        watchOS7Simulator: { name: "watchos-simulator-7", readableName: "WatchOS Simulator 7", order: 42 },
        watchOS7Device: { name: "watchos-7", readableName: "WatchOS 7", order: 43 },
        Windows10: { name: "windows-10", readableName: "Windows 10", order: 50 },
        WinCairo: { name: "wincairo-windows-10", readableName: "WinCairo", order: 55 },
        LinuxWPE: { name : "linux-wpe", readableName: "Linux WPE", order: 90 },
        LinuxGTK: { name : "linux-gtk", readableName: "Linux GTK", order: 91 },
        LinuxJSCOnly: { name : "linux-jsconly", readableName: "Linux JSCOnly", order: 92 },
        PlayStation: { name : "playstation", readableName: "PlayStation", order: 100 },
    },
    Branch: {},
    Repository: {
        OpenSource: { name: "openSource", isGit: true, order: 0 },
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
