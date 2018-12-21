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
        "commit-queue": {platform: Dashboard.Platform.macOSSierra, shortName: "commit", title: "Commit Queue"},
        "style-queue": {shortName: "style", title: "Style Checker Queue"},
        "gtk-wk2-ews": {platform: Dashboard.Platform.LinuxGTK, shortName: "gtk-wk2", title: "WebKit2\xa0Release\xa0Build\xa0EWS"},
        "ios-ews": {platform: Dashboard.Platform.iOS12Device, shortName: "ios", title: "Release\xa0Build\xa0EWS"},
        "ios-sim-ews": {platform: Dashboard.Platform.iOS12Simulator, shortName: "ios-sim", title: "WebKit2\xa0Release\xa0Tests\xa0EWS"},
        "jsc-armv7-ews": {platform: Dashboard.Platform.LinuxJSCOnly, shortName: "jsc-armv7", title: "ARMv7\xa0Release\xa0Build\xa0EWS"},
        "jsc-ews": {platform: Dashboard.Platform.macOSSierra, shortName: "jsc", title: "Release\xa0JSC\xa0Tests\xa0EWS"},
        "jsc-mips-ews": {platform: Dashboard.Platform.LinuxJSCOnly, shortName: "jsc-mips-ews", title: "MIPS\xa0Release\xa0Build\xa0EWS"},
        "mac-ews": {platform: Dashboard.Platform.macOSSierra, shortName: "mac", title: "WebKit1\xa0Release\xa0Tests\xa0EWS"},
        "mac-wk2-ews": {platform: Dashboard.Platform.macOSSierra, shortName: "mac-wk2", title: "WebKit2\xa0Release\xa0Tests\xa0EWS"},
        "mac-debug-ews": {platform: Dashboard.Platform.macOSSierra, shortName: "mac-debug", title: "WebKit1\xa0Debug\xa0Tests\xa0EWS"},
        "mac-32bit-ews": {platform: Dashboard.Platform.macOSHighSierra, shortName: "mac-32bit", title: "Release\xa032\u2011bit\xa0Build\xa0EWS"},
        "bindings-ews": {platform: Dashboard.Platform.macOSHighSierra, shortName: "bindings", title: "Bindings\xa0EWS"},
        "webkitpy-ews": {platform: Dashboard.Platform.macOSHighSierra, shortName: "webkitpy", title: "Webkitpy\xa0EWS"},
        "win-ews": {platform: Dashboard.Platform.Windows7, shortName: "win", title: "WebKit1\xa0Release\xa0Build\xa0EWS"},
        "wincairo-ews": {platform: Dashboard.Platform.WinCairo, shortName: "wincairo", title: "WebKit1\xa0Release\xa0Build\xa0EWS"},
        "wpe-ews": {platform: Dashboard.Platform.LinuxWPE, shortName: "wpe", title: "WebKit2\xa0Release\xa0Build\xa0EWS"},
    };

    BaseObject.call(this);

    this.baseURL = "https://webkit-queues.webkit.org/";
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

    jsonProcessingTimesURL: function(fromTime, toTime)
    {
        return this.baseURL + "processing-times-json/" + [fromTime.getUTCFullYear(), fromTime.getUTCMonth() + 1, fromTime.getUTCDate(), fromTime.getUTCHours(), fromTime.getUTCMinutes(), fromTime.getUTCSeconds()].join("-")
            + "-" + [toTime.getUTCFullYear(), toTime.getUTCMonth() + 1, toTime.getUTCDate(), toTime.getUTCHours(), toTime.getUTCMinutes(), toTime.getUTCSeconds()].join("-");
    },

    queueStatusURL: function(queueID)
    {
        return this.baseURL + "queue-status/" + encodeURIComponent(queueID);
    },

    // Retrieves information about all patches that were submitted in the time range:
    // {
    //     patch_id_1: {
    //         queue_name_1: {
    //             date: <date/time when the patch was submitted to the queue>,
    //             retry_count: <number of times a bot had to bail out and drop the lock, for another bot to start from scratch>,
    //             wait_duration: <how long it took before a bot first locked the patch for processing>,
    //             process_duration: <how long it took from end of wait to finish, only valid for finished patches. Includes wait time between retries>
    //             final_message: <(pass|fail|not processed|could not apply|internal error|in progress)>
    //         },
    //         ...
    //     },
    //     ...
    // }
    loadProcessingTimes: function(fromTime, toTime, callback)
    {
        JSON.load(this.jsonProcessingTimesURL(fromTime, toTime), function(data) {
            for (patch in data) {
                for (queue in patch)
                    queue.date = new Date(queue.date);
            }
            callback(data, fromTime, toTime);
        }.bind(this));
    },
};
