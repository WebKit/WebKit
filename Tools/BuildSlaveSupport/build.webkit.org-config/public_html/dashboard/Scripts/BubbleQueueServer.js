/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

BubbleQueueServer = function()
{
    const queueInfo = {
        "commit-queue": {platform: Dashboard.Platform.MacOSXMountainLion, shortName: "commit", title: "Commit Queue"},
        "style-queue": {shortName: "style", title: "Style Checker Queue"},
        "mac-ews": {platform: Dashboard.Platform.MacOSXMountainLion, shortName: "mac", title: "WebKit1\xa0Release\xa0Tests\xa0EWS"},
        "mac-wk2-ews": {platform: Dashboard.Platform.MacOSXMountainLion, shortName: "mac-wk2", title: "WebKit2\xa0Release\xa0Tests\xa0EWS"},
        "win-ews": {platform: Dashboard.Platform.Windows7, shortName: "win", title: "WebKit1\xa0Release\xa0Build\xa0EWS"},
        "gtk-wk2-ews": {platform: Dashboard.Platform.LinuxGTK, shortName: "gtk-wk2", title: "WebKit2\xa0Release\xa0Build\xa0EWS"},
        "efl-wk2-ews": {platform: Dashboard.Platform.LinuxEFL, shortName: "efl-wk2", title: "WebKit2\xa0Release\xa0Build\xa0EWS"}
    };

    BaseObject.call(this);

    this.baseURL = "https://webkit-queues.appspot.com/";
    this.queues = {};

    for (var id in queueInfo)
        this.queues[id] = new BubbleQueue(this, id, queueInfo[id]);
};

BaseObject.addConstructorFunctions(BubbleQueueServer);

BubbleQueueServer.prototype = {
    constructor: BubbleQueueServer,
    __proto__: BaseObject.prototype,

    jsonQueueLengthURL: function(queueID)
    {
        return this.baseURL + "queue-length-json/" + encodeURIComponent(queueID);
    },

    jsonQueueStatusURL: function(queueID)
    {
        return this.baseURL + "queue-status-json/" + encodeURIComponent(queueID);
    },

    queueStatusURL: function(queueID)
    {
        return this.baseURL + "queue-status/" + encodeURIComponent(queueID);
    },
};
