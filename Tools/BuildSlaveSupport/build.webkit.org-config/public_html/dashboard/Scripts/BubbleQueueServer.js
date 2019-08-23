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
        "commit-queue": {platform: Dashboard.Platform.macOSHighSierra, shortName: "commit", title: "Commit Queue"},
        "jsc-armv7-ews": {platform: Dashboard.Platform.LinuxJSCOnly, shortName: "jsc-armv7", title: "ARMv7\xa0Release\xa0Build\xa0EWS"},
        "jsc-ews": {platform: Dashboard.Platform.macOSMojave, shortName: "jsc", title: "Release\xa0JSC\xa0Tests\xa0EWS"},
        "jsc-mips-ews": {platform: Dashboard.Platform.LinuxJSCOnly, shortName: "jsc-mips-ews", title: "MIPS\xa0Release\xa0Build\xa0EWS"},
        "win-ews": {platform: Dashboard.Platform.Windows10, shortName: "win", title: "WebKit1\xa0Release\xa0Build\xa0EWS"},
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
