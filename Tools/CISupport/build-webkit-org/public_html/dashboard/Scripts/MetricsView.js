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

MetricsView = function(analyzer, queues)
{
    BaseObject.call(this);

    this.element = document.createElement("div");
    this.element.classList.add("queue-view");

    this._results = { };

    this._debugQueues = queues.filter(function(queue) { return queue.platform && queue.debug; });
    this._releaseQueues = queues.filter(function(queue) { return queue.platform && !queue.debug; });
    this._aggregatePseudoQueues = queues.filter(function(queue) { return queue.platform === undefined; });

    if (this._aggregatePseudoQueues.length)
        this.element.classList.add("aggregate-queue-view");

    this._hasMultipleDebugQueues = this._debugQueues.length > 1;
    this._hasMultipleReleaseQueues = this._releaseQueues.length > 1;

    analyzer.addEventListener(Analyzer.Event.Starting, this._clearResults, this);
    analyzer.addEventListener(Analyzer.Event.QueueResults, this._queueResultsAdded, this);
    
    this._updateSoon();
};

MetricsView.UpdateSoonTimeout = 100; // 100 ms

BaseObject.addConstructorFunctions(MetricsView);

MetricsView.prototype = {
    constructor: MetricsView,
    __proto__: BaseObject.prototype,

    _clearResults: function()
    {
        this._results = { };
        this._updateSoon();
    },

    _queueResultsAdded: function(event)
    {
        if (this._debugQueues.some(function(queue) { return queue.id === event.data.queueID; })
        || this._releaseQueues.some(function(queue) { return queue.id === event.data.queueID; })
        || this._aggregatePseudoQueues.some(function(queue) { return queue.id === event.data.queueID; })) {
            this._results[event.data.queueID] = event.data;
            this._updateSoon();
        };
    },

    _update: function()
    {
        this.element.removeChildren();

        function addLine(element, text)
        {
            var line = document.createElement("div");
            line.textContent = text;
            line.classList.add("result-line");
            element.appendChild(line);
        }

        function addError(element, text)
        {
            addLine(element, text);
            element.lastChild.classList.add("error-line");
        }

        function addDivider(element)
        {
            var divider = document.createElement("div");
            divider.classList.add("divider");
            element.appendChild(divider);
        }

        function appendQueueResults(queue)
        {
            if (!(queue.id in this._results))
                return;

            var result = this._results[queue.id];

            var isAggregateQueue = -1 !== this._aggregatePseudoQueues.indexOf(queue);

            if (result.buildbotRangeError && !isAggregateQueue)
                addError(this.element, result.buildbotRangeErrorText);

            if ("percentageOfGreen" in result)
                addLine(this.element, "Green " + Math.round(result.percentageOfGreen * 100) / 100 + "% of time");

            if (isAggregateQueue && result.longestStretchOfRed) {
                addLine(this.element, "Longest red: " + Math.round(result.longestStretchOfRed / 60) + " minutes");
                addDivider(this.element);
            } else if ("percentageOfGreen" in result)
                addDivider(this.element);

            if (queue.builder || isAggregateQueue) {
                addLine(this.element, "Time from commit: ");
                addLine(this.element, "Average: " + Math.round(result.averageSecondsFromCommit / 60) + " minutes");
                addLine(this.element, "Median: " + Math.round(result.medianSecondsFromCommit / 60) + " minutes");
                addLine(this.element, "Worst: " + Math.round(result.worstSecondsFromCommit / 60) + " minutes (r" + result.revisionWithWorstTimeFromCommit + ")");
            } else {
                // Time from commit is pretty useless for tester bots.
                addLine(this.element, "Time on the bot: ");
                addLine(this.element, "Average: " + Math.round(result.averageSecondsOwnTime / 60) + " minutes");
                addLine(this.element, "Median: " + Math.round(result.medianSecondsOwnTime / 60) + " minutes");
                addLine(this.element, "Worst: " + Math.round(result.worstSecondsOwnTime / 60) + " minutes (r" + result.revisionWithWorstOwnTime + ")");
            }
        }

        function appendBuildArchitecture(queues, showArchitecture)
        {
            queues.forEach(function(queue) {
                var labelText = queue.debug ? "Debug" : "Release";
                if (showArchitecture) {
                    if (queue.architecture === Buildbot.BuildArchitecture.Universal)
                        labelText += " (Universal)";
                    else if (queue.architecture === Buildbot.BuildArchitecture.SixtyFourBit)
                        labelText += " (64-bit)";
                    else if (queue.architecture === Buildbot.BuildArchitecture.ThirtyTwoBit)
                        labelText += " (32-bit)";
                }
                var releaseLabel = document.createElement("a");
                releaseLabel.classList.add("queueLabel");
                releaseLabel.textContent = labelText;
                releaseLabel.href = queue.overviewURL;
                releaseLabel.target = "_blank";
                this.element.appendChild(releaseLabel);

                appendQueueResults.call(this, queue);
            }.bind(this));
        }

        appendBuildArchitecture.call(this, this._releaseQueues, this._hasMultipleReleaseQueues);
        appendBuildArchitecture.call(this, this._debugQueues, this._hasMultipleDebugQueues);
        
        this._aggregatePseudoQueues.forEach(function(queue) {
            appendQueueResults.call(this, queue);
        }.bind(this));
    },

    _updateSoon: function()
    {
        setTimeout(this._update.bind(this), MetricsView.UpdateSoonTimeout);
    },
};
