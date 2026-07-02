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
        macOSTahoe: { name: "macos-tahoe", readableName: "macOS Tahoe", order: 1 },
        macOSSequoia: { name: "macos-sequoia", readableName: "macOS Sequoia", order: 2 },
        iOS26Simulator: { name: "ios-simulator-26", readableName: "iOS 26 Simulator", order: 13 },
        iOS26Device: { name: "ios-26", readableName: "iOS 26", order: 14 },
        visionOS26Simulator: { name: "visionos-simulator-26", readableName: "visionOS 26 Simulator", order: 15 },
        visionOS26Device: { name: "visionos-26", readableName: "visionOS 26", order: 16 },
        tvOS26Simulator: { name: "tvos-simulator-26", readableName: "tvOS 26 Simulator", order: 20 },
        tvOS26Device: { name: "tvos-26", readableName: "tvOS 26", order: 21 },
        watchOS26Simulator: { name: "watchos-simulator-26", readableName: "watchOS 26 Simulator", order: 32 },
        watchOS26Device: { name: "watchos-26", readableName: "watchOS 26", order: 33 },
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
