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
        macOSSonoma: { name: "macos-sonoma", readableName: "macOS Sonoma", order: 1 },
        macOSVentura: { name: "macos-ventura", readableName: "macOS Ventura", order: 2 },
        macOSMonterey: { name: "macos-monterey", readableName: "macOS Monterey", order: 3 },
        iOS17Simulator: { name: "ios-simulator-17", readableName: "iOS 17 Simulator", order: 13 },
        iOS17Device: { name: "ios-17", readableName: "iOS 17", order: 14 },
        tvOS17Simulator: { name: "tvos-simulator-17", readableName: "TvOS Simulator 17", order: 20 },
        tvOS17Device: { name: "tvos-17", readableName: "TvOS 17", order: 21 },
        watchOS10Simulator: { name: "watchos-simulator-10", readableName: "WatchOS Simulator 10", order: 32 },
        watchOS10Device: { name: "watchos-10", readableName: "WatchOS 10", order: 33 },
        Windows10: { name: "windows-10", readableName: "Windows 10", order: 38 },
        WinCairo: { name: "wincairo-windows-10", readableName: "WinCairo", order: 43 },
        LinuxWPE: { name : "linux-wpe", readableName: "Linux WPE", order: 78 },
        LinuxGTK: { name : "linux-gtk", readableName: "Linux GTK", order: 79 },
        LinuxJSCOnly: { name : "linux-jsconly", readableName: "Linux JSCOnly", order: 80 },
        PlayStation: { name : "playstation", readableName: "PlayStation", order: 88 },
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
