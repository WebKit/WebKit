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
        macOSMojave: { name: "macos-mojave", readableName: "macOS Mojave", order: 6 },
        macOSHighSierra: { name: "macos-highsierra", readableName: "macOS High Sierra", order: 7 },
        macOSSierra: { name: "macos-sierra", readableName: "macOS Sierra", order: 8 },
        iOS12Simulator: { name: "ios-simulator-12", readableName: "iOS 12 Simulator", order: 20 },
        iOS12Device: { name: "ios-12", readableName: "iOS 12", order: 25 },
        Windows10: { name: "windows-10", readableName: "Windows 10", order: 30 },
        Windows7: { name: "windows-7", readableName: "Windows 7", order: 35 },
        WinCairo: { name: "wincairo-windows-10", readableName: "WinCairo", order: 50 },
        LinuxWPE: { name : "linux-wpe", readableName: "Linux WPE", order: 90 },
        LinuxGTK: { name : "linux-gtk", readableName: "Linux GTK", order: 91 }
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
