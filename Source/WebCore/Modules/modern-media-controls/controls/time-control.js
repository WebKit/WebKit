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

        this.elapsedTimeLabel = new TimeLabel(TimeLabel.Types.Elapsed);
        this.scrubber = new Slider("scrubber");
        this.remainingTimeLabel = new TimeLabel(TimeLabel.Types.Remaining);

        this.activityIndicator = new LayoutNode(`<div class="activity-indicator"></div>`);
        this.activityIndicator.width = 14;
        this.activityIndicator.height = 14;
        for (let segmentClassName of ["n", "ne", "e", "se", "s", "sw", "w", "nw"])
            this.activityIndicator.element.appendChild(document.createElement("div")).className = segmentClassName;

        this._duration = 0;
        this._currentTime = 0;
        this._loading = false;
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
        return MinimumScrubberWidth + ScrubberMargin + this.remainingTimeLabel.width;
    }

    get idealMinimumWidth()
    {
        this._performIdealLayout();
        return this.elapsedTimeLabel.width + ScrubberMargin + MinimumScrubberWidth + ScrubberMargin + this.remainingTimeLabel.width;
    }

    // Protected

    layout()
    {
        super.layout();
        this._performIdealLayout();

        if (this._loading)
            return;

        if (this.scrubber.width >= MinimumScrubberWidth) {
            this.elapsedTimeLabel.visible = true;
            return;
        }

        // We drop the elapsed time label if width is constrained and we can't guarantee
        // the scrubber minimum size otherwise.
        this.scrubber.x = 0;
        this.scrubber.width = this.width - ScrubberMargin - this.remainingTimeLabel.width;
        this.remainingTimeLabel.x = this.scrubber.x + this.scrubber.width + ScrubberMargin;
        this.elapsedTimeLabel.visible = false;
    }

    // Private

    _performIdealLayout()
    {
        if (this._loading)
            this.remainingTimeLabel.setValueWithNumberOfDigits(NaN, 4);
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

            this.elapsedTimeLabel.setValueWithNumberOfDigits(shouldShowZeroDurations ? 0 : this._currentTime, numberOfDigitsForTimeLabels);
            this.remainingTimeLabel.setValueWithNumberOfDigits(shouldShowZeroDurations ? 0 : (this._currentTime - this._duration), numberOfDigitsForTimeLabels);
        }

        if (this._duration)
            this.scrubber.value = this._currentTime / this._duration;

        this.scrubber.x = (this._loading ? this.activityIndicator.width : this.elapsedTimeLabel.width) + ScrubberMargin;
        this.scrubber.width = this.width - this.scrubber.x - ScrubberMargin - this.remainingTimeLabel.width;
        this.remainingTimeLabel.x = this.scrubber.x + this.scrubber.width + ScrubberMargin;

        this.children = [this._loading ? this.activityIndicator : this.elapsedTimeLabel, this.scrubber, this.remainingTimeLabel];
    }

    updateScrubberLabel()
    {
        this.scrubber.inputAccessibleLabel = this.elapsedTimeLabel.value;
    }

}
