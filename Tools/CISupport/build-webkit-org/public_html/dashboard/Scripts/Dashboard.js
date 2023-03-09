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
        iOS16Simulator: { name: "ios-simulator-16", readableName: "iOS 16 Simulator", order: 12 },
        iOS16Device: { name: "ios-16", readableName: "iOS 16", order: 13 },
        tvOS16Simulator: { name: "tvos-simulator-16", readableName: "TvOS Simulator 16", order: 19 },
        tvOS16Device: { name: "tvos-16", readableName: "TvOS 16", order: 20 },
        watchOS9Simulator: { name: "watchos-simulator-9", readableName: "WatchOS Simulator 9", order: 31 },
        watchOS9Device: { name: "watchos-9", readableName: "WatchOS 9", order: 32 },
        Windows10: { name: "windows-10", readableName: "Windows 10", order: 37 },
        WinCairo: { name: "wincairo-windows-10", readableName: "WinCairo", order: 42 },
        LinuxWPE: { name : "linux-wpe", readableName: "Linux WPE", order: 77 },
        LinuxGTK: { name : "linux-gtk", readableName: "Linux GTK", order: 78 },
        LinuxJSCOnly: { name : "linux-jsconly", readableName: "Linux JSCOnly", order: 79 },
        PlayStation: { name : "playstation", readableName: "PlayStation", order: 87 },
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
