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
        macOSSequoia: { name: "macos-sequoia", readableName: "macOS Sequoia", order: 1 },
        macOSSonoma: { name: "macos-sonoma", readableName: "macOS Sonoma", order: 2 },
        macOSVentura: { name: "macos-ventura", readableName: "macOS Ventura", order: 3 },
        iOS18Simulator: { name: "ios-simulator-18", readableName: "iOS 18 Simulator", order: 13 },
        iOS18Device: { name: "ios-18", readableName: "iOS 18", order: 14 },
        visionOS2Simulator: { name: "visionos-simulator-2", readableName: "visionOS 2 Simulator", order: 15 },
        visionOS2Device: { name: "visionos-2", readableName: "visionOS 2", order: 16 },
        tvOS18Simulator: { name: "tvos-simulator-18", readableName: "TvOS Simulator 18", order: 20 },
        tvOS18Device: { name: "tvos-18", readableName: "TvOS 18", order: 21 },
        watchOS11Simulator: { name: "watchos-simulator-11", readableName: "WatchOS Simulator 11", order: 32 },
        watchOS11Device: { name: "watchos-11", readableName: "WatchOS 11", order: 33 },
        Windows: { name: "windows", readableName: "Windows", order: 43 },
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
