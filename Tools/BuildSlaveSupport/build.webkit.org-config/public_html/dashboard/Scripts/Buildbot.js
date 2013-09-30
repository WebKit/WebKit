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

Buildbot = function(baseURL, queuesInfo)
{
    BaseObject.call(this);

    console.assert(baseURL);
    console.assert(queuesInfo);

    this.baseURL = baseURL;
    this.queues = {};

    for (var id in queuesInfo)
        this.queues[id] = new BuildbotQueue(this, id, queuesInfo[id]);
};

BaseObject.addConstructorFunctions(Buildbot);

// Ordered importance/recency.
Buildbot.Platform = {
    MacOSXMavericks: "mac-os-x-mavericks",
    MacOSXMountainLion: "mac-os-x-mountain-lion",
    MacOSXLion: "mac-os-x-lion",
    Windows8: "windows-8",
    Windows7: "windows-7",
    WindowsXP: "windows-xp",
    LinuxQt: "linux-qt",
    LinuxGTK: "linux-gtk",
    LinuxEFL: "linux-efl",
};

// Ordered importance.
Buildbot.TestCategory = {
    WebKit2: "webkit-2",
    WebKit1: "webkit-1"
};

// Ordered importance.
Buildbot.BuildArchitecture = {
    Universal: "universal",
    SixtyFourBit: "sixty-four-bit",
    ThirtyTwoBit: "thirty-two-bit"
};

Buildbot.prototype = {
    constructor: Buildbot,
    __proto__: BaseObject.prototype,
};
