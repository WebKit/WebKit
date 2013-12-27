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

EWSQueue = function(ews, id, info)
{
    BaseObject.call(this);

    console.assert(ews);
    console.assert(id);

    this.ews = ews;
    this.id = id;
    this.title = info.title || "\xa0";

    this.platform = info.platform.name || "unknown";

    this.update();
};

BaseObject.addConstructorFunctions(EWSQueue);

EWSQueue.Event = {
    Updated: "updated"
};

EWSQueue.prototype = {
    constructor: EWSQueue,
    __proto__: BaseObject.prototype,

    get baseURL()
    {
        return this.ews.baseURL + "queue-status-json/" + encodeURIComponent(this.id) + "-ews";
    },

    get statusPage()
    {
        return this.ews.baseURL + "queue-status/" + encodeURIComponent(this.id) + "-ews";
    },

    get patchCount()
    {
        return this._patchCount;
    },

    update: function()
    {
        JSON.load(this.baseURL, function(data) {
            var newPatchCount = data.queue.length;
            if (this._patchCount == newPatchCount)
                return;
            this._patchCount = newPatchCount;

            this.dispatchEventToListeners(EWSQueue.Event.Updated, null);
        }.bind(this));
    }
};
