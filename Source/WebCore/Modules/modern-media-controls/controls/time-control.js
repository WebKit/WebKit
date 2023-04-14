/*
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

const TenMinutes = 10 * 60;
const OneHour = 6 * TenMinutes;
const TenHours = 10 * OneHour;
const MinimumScrubberWidth = 120;
const ScrubberMargin = 5;

class TimeControl extends LayoutItem
{

    constructor(layoutDelegate)
    {
        super({
            element: `<div class="time-control"></div>`,
            layoutDelegate
        });

        this._timeLabelsAttachment = TimeControl.TimeLabelsAttachment.Side;

        this._shouldShowDurationTimeLabel = this.layoutTraits.supportsDurationTimeLabel();

        this.elapsedTimeLabel = new TimeLabel(TimeLabel.Type.Elapsed);
        this.scrubber = new Slider(this.layoutDelegate, "scrubber");
        if (this._shouldShowDurationTimeLabel)
            this.durationTimeLabel = new TimeLabel(TimeLabel.Type.Duration);
        this.remainingTimeLabel = new TimeLabel(TimeLabel.Type.Remaining);

        this.activityIndicator = new LayoutNode(`<div class="activity-indicator"></div>`);
        this.activityIndicator.width = 14;
        this.activityIndicator.height = 14;
        for (let segmentClassName of ["n", "ne", "e", "se", "s", "sw", "w", "nw"])
            this.activityIndicator.element.appendChild(document.createElement("div")).className = segmentClassName;

        this._duration = 0;
        this._currentTime = 0;
        this._loading = false;

        if (this._shouldShowDurationTimeLabel) {
            this.durationTimeLabel.element.addEventListener("click", this);
            this.remainingTimeLabel.element.addEventListener("click", this);
        }
    }

    // Public

    set duration(duration)
    {
        if (this._duration === duration)
            return;

        this._duration = duration;
        this.needsLayout = true;
    }

    set currentTime(currentTime)
    {
        if (this._currentTime === currentTime)
            return;

        this._currentTime = currentTime;
        this.needsLayout = true;
    }

    get loading()
    {
        return this._loading;
    }

    set loading(flag)
    {
        if (this._loading === flag)
            return;

        this._loading = flag;
        this.scrubber.disabled = flag;
        this.needsLayout = true;
    }

    get minimumWidth()
    {
        this._performIdealLayout();

        if (this._timeLabelsDisplayOnScrubberSide)
            return MinimumScrubberWidth + this._scrubberMargin + this._durationOrRemainingTimeLabel().width;
        return MinimumScrubberWidth;
    }

    get idealMinimumWidth()
    {
        this._performIdealLayout();
        if (this._timeLabelsDisplayOnScrubberSide)
            return this.elapsedTimeLabel.width + MinimumScrubberWidth + (2 * this._scrubberMargin) + this._durationOrRemainingTimeLabel().width;
        return MinimumScrubberWidth;
    }

    get timeLabelsAttachment()
    {
        return this._timeLabelsAttachment;
    }

    set timeLabelsAttachment(attachment)
    {
        if (this._timeLabelsAttachment == attachment)
            return;

        this._timeLabelsAttachment = attachment;
        this.needsLayout = true;
    }

    // Protected

    layout()
    {
        super.layout();
        this._performIdealLayout();

        if (this._loading || !this._timeLabelsDisplayOnScrubberSide)
            return;

        // If the scrubber width is larger or equal to the minimal scrubber width, this means we determined during ideal layout
        // that there is enough space to fit both the elapsed time label and either the duration or remaining time label. Otherwise
        // it must be hidden.
        this.elapsedTimeLabel.visible = this.scrubber.width >= MinimumScrubberWidth;
        this._performLayoutWithRightTimeLabel(this._durationOrRemainingTimeLabel());
    }

    handleEvent(event)
    {
        switch (event.type) {
        case "click":
            switch (event.target) {
            case this.durationTimeLabel.element:
                this._shouldShowDurationTimeLabel = false;
                this.needsLayout = true;
                break;

            case this.remainingTimeLabel.element:
                if (this._canShowDurationTimeLabel) {
                    this._shouldShowDurationTimeLabel = true;
                    this.needsLayout = true;
                }
                break;
            }
        }
    }

    // Private

    get _scrubberMargin()
    {
        return this.computedValueForStylePropertyInPx("--scrubber-margin");
    }

    get _timeLabelsDisplayOnScrubberSide()
    {
        return this._timeLabelsAttachment == TimeControl.TimeLabelsAttachment.Side;
    }

    get _canShowDurationTimeLabel()
    {
        return this.elapsedTimeLabel.visible;
    }

    _durationOrRemainingTimeLabel()
    {
        return (this._canShowDurationTimeLabel && this._shouldShowDurationTimeLabel) ? this.durationTimeLabel : this.remainingTimeLabel;
    }

    _performIdealLayout()
    {
        // During an ideal layout, we want the elapsed time label to be visible so we only account for _shouldShowDurationTimeLabel
        // to determine whether to show the duration or remaining time label.
        const durationOrRemainingTimeLabel = this._shouldShowDurationTimeLabel ? this.durationTimeLabel : this.remainingTimeLabel;

        // We can only show the duration time label if the elapsed time label is visible.
        // During an ideal layout, we must assume the elapsed time label is visible.
        if (this._loading)
            durationOrRemainingTimeLabel.setValueWithNumberOfDigits(NaN, 4);
        else {
            const shouldShowZeroDurations = isNaN(this._duration) || this._duration === Number.POSITIVE_INFINITY;

            let numberOfDigitsForTimeLabels;
            if (this._duration < TenMinutes)
                numberOfDigitsForTimeLabels = 3;
            else if (shouldShowZeroDurations || this._duration < OneHour)
                numberOfDigitsForTimeLabels = 4;
            else if (this._duration < TenHours)
                numberOfDigitsForTimeLabels = 5;
            else
                numberOfDigitsForTimeLabels = 6;

            // Even though we only care about the metrics of durationOrRemainingTimeLabel, we set the values for both
            // labels here since this is the only place where we set such values.
            this.elapsedTimeLabel.setValueWithNumberOfDigits(shouldShowZeroDurations ? 0 : this._currentTime, numberOfDigitsForTimeLabels);
            this.remainingTimeLabel.setValueWithNumberOfDigits(shouldShowZeroDurations ? 0 : this._currentTime - this._duration, numberOfDigitsForTimeLabels);
            if (this.durationTimeLabel)
                this.durationTimeLabel.setValueWithNumberOfDigits(shouldShowZeroDurations ? 0 : this._duration, numberOfDigitsForTimeLabels);
        }

        if (this._duration)
            this.scrubber.value = this._currentTime / this._duration;

        this._performLayoutWithRightTimeLabel(durationOrRemainingTimeLabel);
    }

    _performLayoutWithRightTimeLabel(rightTimeLabel)
    {
        const scrubberMargin = this._scrubberMargin;

        this.scrubber.x = (() => {
            if (this._loading)
                return this.activityIndicator.width + scrubberMargin;
            if (this._timeLabelsDisplayOnScrubberSide && this.elapsedTimeLabel.visible)
                return this.elapsedTimeLabel.width + scrubberMargin;
            return 0;
        })();

        this.scrubber.width = (() => {
            if (this._timeLabelsDisplayOnScrubberSide)
                return this.width - this.scrubber.x - scrubberMargin - rightTimeLabel.width;
            return this.width;
        })();

        rightTimeLabel.x = (() => {
            if (this._timeLabelsDisplayOnScrubberSide)
                return this.scrubber.x + this.scrubber.width + scrubberMargin;
            return this.width - rightTimeLabel.width;
        })()

        this.children = [this._loading ? this.activityIndicator : this.elapsedTimeLabel, this.scrubber, rightTimeLabel];
    }

    updateScrubberLabel()
    {
        this.scrubber.inputAccessibleLabel = this.elapsedTimeLabel.value;
    }

}

TimeControl.TimeLabelsAttachment = {
    Above: 1 << 0,
    Side:  1 << 1
};
