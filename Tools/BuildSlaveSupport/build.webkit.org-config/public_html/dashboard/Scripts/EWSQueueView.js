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

EWSQueueView = function(queues)
{
    QueueView.call(this);

    this.queues = queues || [];

    this.queues.forEach(function(queue) {
        if (this.platform && this.platform != queue.platform)
            throw "A EWS view may not contain queues for multiple platforms."
        else
            this.platform = queue.platform;
        queue.addEventListener(EWSQueue.Event.Updated, this._queueUpdated, this);
    }.bind(this));

    this.update();
};

BaseObject.addConstructorFunctions(EWSQueueView);

EWSQueueView.prototype = {
    constructor: EWSQueueView,
    __proto__: QueueView.prototype,

    update: function()
    {
        QueueView.prototype.update.call(this);

        this.element.removeChildren();

        function appendQueue(queue)
        {
            var releaseLabel = document.createElement("a");
            releaseLabel.classList.add("queueLabel");
            releaseLabel.textContent = queue.title;
            releaseLabel.href = queue.statusPage;
            releaseLabel.target = "_blank";
            this.element.appendChild(releaseLabel);

            var patchCount = queue.patchCount;

            var message = patchCount === 1 ? "patch in queue" : "patches in queue";
            var status = new StatusLineView(message, StatusLineView.Status.Neutral, null, patchCount || "0");
            this.element.appendChild(status.element);
        }

        this.queues.forEach(function(queue) {
            appendQueue.call(this, queue);
        }, this);
    },

    _updateQueues: function()
    {
        this.queues.forEach(function(queue) { queue.update(); });
    },

    _queueUpdated: function(event)
    {
        this.updateSoon();
    },
};
