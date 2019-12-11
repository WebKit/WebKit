/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

MetricsBubbleView = function(analyzer, queue)
{
    BaseObject.call(this);

    this.element = document.createElement("div");
    this.element.classList.add("queue-view");

    this._results = null;

    this._queue = queue;

    analyzer.addEventListener(Analyzer.Event.Starting, this._clearResults, this);
    analyzer.addEventListener(Analyzer.Event.QueueResults, this._queueResultsAdded, this);
    
    this._updateSoon();
};

MetricsBubbleView.UpdateSoonTimeout = 100; // 100 ms

BaseObject.addConstructorFunctions(MetricsBubbleView);

MetricsBubbleView.prototype = {
    constructor: MetricsBubbleView,
    __proto__: BaseObject.prototype,

    _clearResults: function()
    {
        this._results = null;
        this._updateSoon();
    },

    _queueResultsAdded: function(event)
    {
        if (this._queue.id !== event.data.queueID)
            return;

        this._results = event.data;
        this._updateSoon();
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

        function pluralizeMinutes(intervalInSeconds)
        {
            if (intervalInSeconds < 60)
                return "less than a minute";
            else if (intervalInSeconds < 120)
                return "1\xa0minute";
            else
                return Math.round(intervalInSeconds / 60) + "\xa0minutes";
        }

        function formatPercentage(fraction)
        {
            if (fraction > 0 && fraction < 0.01)
                return "< 1%";
            else
                return Math.round(fraction * 100) + "%";
        }

        if (!this._results)
            return;

        if (this._queue.id === "style-queue") {
            addDivider(this.element);
            addLine(this.element, "Time to result:");
            addLine(this.element, "- median: " + pluralizeMinutes(this._results.medianTotalTimeInSeconds) + ";");
            addLine(this.element, "- average: " + pluralizeMinutes(this._results.averageTotalTimeInSeconds) + ".");
        } else {
            if (this._results.totalPatches !== this._results.patchesWithRetriesCount) {
                addDivider(this.element);
                var numberOfPatchesThatHadFinalResultsAtFirstTry = this._results.totalPatches - this._results.patchesWithRetriesCount - this._results.patchesThatDidNotCompleteCount + this._results.patchesThatSpinnedAndDidNotCompleteCount;
                var text = formatPercentage(numberOfPatchesThatHadFinalResultsAtFirstTry / this._results.totalPatches) + " of patches ";
                text += (this._queue.id === "commit-queue") ? "were landed or rejected" : "had final results";
                text += " at first try. Time to result:"
                addLine(this.element, text);
                addLine(this.element, "- median: " + pluralizeMinutes(this._results.medianTotalTimeForPatchesThatWereNotRetriedInSeconds) + ";");
                addLine(this.element, "- average: " + pluralizeMinutes(this._results.averageTotalTimeForPatchesThatWereNotRetriedInSeconds) + ".");
                if (this._results.patchesThatDidNotApplyCount !== this._results.patchesThatSpinnedAndCeasedToApplyCount) {
                    if (this._results.patchesThatDidNotApplyCount - this._results.patchesThatSpinnedAndCeasedToApplyCount === numberOfPatchesThatHadFinalResultsAtFirstTry)
                        addLine(this.element, "None of these patches applied to trunk.");
                    else
                        addLine(this.element, "This includes " + formatPercentage((this._results.patchesThatDidNotApplyCount - this._results.patchesThatSpinnedAndCeasedToApplyCount) / this._results.totalPatches) + " that did not apply to trunk.");
                }
            }

            if (this._results.patchesThatDidNotCompleteCount !== this._results.patchesThatSpinnedAndDidNotCompleteCount) {
                addDivider(this.element);
                addLine(this.element, formatPercentage((this._results.patchesThatDidNotCompleteCount - this._results.patchesThatSpinnedAndDidNotCompleteCount) / this._results.totalPatches) + " of patches ceased to be eligible for processing before the first try finished.");
            }

            if (this._results.patchesWithRetriesCount) {
                addDivider(this.element);
                var text = formatPercentage(this._results.patchesWithRetriesCount / this._results.totalPatches) + " of patches had to be retried";
                if (this._results.patchesThatSpinnedAndDidNotCompleteCount) {
                    text += ", including " + formatPercentage(this._results.patchesThatSpinnedAndDidNotCompleteCount / this._results.totalPatches) + " that kept being retried until the patch became ineligible for processing";
                    if (this._results.patchesThatSpinnedAndCeasedToApplyCount)
                        text += ", and " + formatPercentage(this._results.patchesThatSpinnedAndCeasedToApplyCount / this._results.totalPatches) + " that kept being retried until the patch ceased to apply to trunk.";
                    else
                        text += ".";
                } else if (this._results.patchesThatSpinnedAndCeasedToApplyCount)
                    text += ", including " + formatPercentage(this._results.patchesThatSpinnedAndCeasedToApplyCount / this._results.totalPatches) + " that were spinning until the patch ceased to apply to trunk.";
                else
                    text += ".";
                if (this._results.patchesThatSpinnedAndPassedOrFailedCount) {
                    text += (this._results.patchesWithRetriesCount === this._results.patchesThatSpinnedAndPassedOrFailedCount) ? " All of them" : " " + formatPercentage(this._results.patchesThatSpinnedAndPassedOrFailedCount / this._results.totalPatches);
                    text += " finally produced a result, which took:";
                    addLine(this.element, text);
                    addLine(this.element, "- median: " + pluralizeMinutes(this._results.medianTotalTimeForPatchesThatSpinnedAndPassedOrFailedInSeconds) + ";");
                    addLine(this.element, "- average: " + pluralizeMinutes(this._results.averageTotalTimeForPatchesThatSpinnedAndPassedOrFailedInSeconds) + ".");
                } else
                    addLine(this.element, text);
            }
        }

        addDivider(this.element);
        addLine(this.element, "Wait time before processing started:");
        addLine(this.element, "- median: " + pluralizeMinutes(this._results.medianWaitTimeInSeconds) + ";");
        addLine(this.element, "- average: " + pluralizeMinutes(this._results.averageWaitTimeInSeconds) + ".");
        addLine(this.element, "- worst: " + pluralizeMinutes(this._results.maximumWaitTimeInSeconds) + ".");
        if (this._results.medianWaitTimeInSeconds < 3 * 60 && this._results.patchesThatWaitedMoreThan3MinutesCount)
            addLine(this.element, formatPercentage(this._results.patchesThatWaitedMoreThan3MinutesCount / this._results.totalPatches) + " of patches had a wait time of more than 3 minutes.");

        if (this._results.patchesThatCausedInternalError.length) {
            addDivider(this.element);
            if (this._results.patchesThatCausedInternalError.length === 1)
                addError(this.element, "One patch caused internal error");
            else
                addError(this.element, this._results.patchesThatCausedInternalError.length + "\xa0patches caused internal error, please see patch numbers in console log.");
            console.log("Patches that caused internal error for " + this._results.queueID + ": " + this._results.patchesThatCausedInternalError);
        }
    },

    _updateSoon: function()
    {
        setTimeout(this._update.bind(this), MetricsBubbleView.UpdateSoonTimeout);
    },
};
