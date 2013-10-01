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

BuildbotQueueView = function(debugQueues, releaseQueues)
{
    BaseObject.call(this);

    this.releaseQueues = releaseQueues || [];
    this.debugQueues = debugQueues || [];

    this.element = document.createElement("div");
    this.element.classList.add("queue-view");
    this.element.__queueView = this;

    this.releaseQueues.forEach(function(queue) {
        queue.addEventListener(BuildbotQueue.Event.IterationsAdded, this._queueIterationsAdded, this);
    }.bind(this));

    this.debugQueues.forEach(function(queue) {
        queue.addEventListener(BuildbotQueue.Event.IterationsAdded, this._queueIterationsAdded, this);
    }.bind(this));

    setInterval(this._updateQueues.bind(this), BuildbotQueueView.UpdateInterval);
};

BaseObject.addConstructorFunctions(BuildbotQueueView);

BuildbotQueueView.UpdateInterval = 45000; // 45 seconds
BuildbotQueueView.UpdateSoonTimeout = 1000; // 1 second

BuildbotQueueView.prototype = {
    constructor: BuildbotQueueView,
    __proto__: BaseObject.prototype,

    updateSoon: function()
    {
        if (this._updateTimeout)
            return;
        this._updateTimeout = setTimeout(this.update.bind(this), BuildbotQueueView.UpdateSoonTimeout);
    },

    update: function()
    {
        if (this._updateTimeout) {
            clearTimeout(this._updateTimeout);
            delete this._updateTimeout;
        }

        // Implemented by subclasses.
    },

    revisionLinksForIteration: function(iteration)
    {
        function linkForRevision(revision, internal)
        {
            var linkElement = document.createElement("a");
            linkElement.href = iteration.queue.buildbot.tracRevisionURL(revision, internal);
            linkElement.textContent = "r" + revision;
            linkElement.target = "_new";
            return linkElement;
        }

        console.assert(iteration.openSourceRevision);
        var openSourceLink = linkForRevision(iteration.openSourceRevision, false);

        if (!iteration.internalRevision)
            return openSourceLink;

        var internalLink = linkForRevision(iteration.internalRevision, true);

        var fragment = document.createDocumentFragment();
        fragment.appendChild(openSourceLink);
        fragment.appendChild(document.createTextNode(" \uff0b "));
        fragment.appendChild(internalLink);
        return fragment;
    },

    _updateQueues: function()
    {
        this.releaseQueues.forEach(function(queue) { queue.update(); });
        this.debugQueues.forEach(function(queue) { queue.update(); });
    },

    _queueIterationsAdded: function(event)
    {
        this.updateSoon();

        event.data.addedIterations.forEach(function(iteration) {
            iteration.addEventListener(BuildbotIteration.Event.Updated, this._iterationUpdated, this);
        }.bind(this));
    },

    _iterationUpdated: function(event)
    {
        this.updateSoon();
    }
};
