/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

// Sync with time-control.css:
const MinimumTimeControlWidth = 160;
const IdealMinimumTimeControlWidth = 200;

class TimeControl extends LayoutItem
{

    constructor(layoutDelegate)
    {
        super({
            element: `<div class="time-control"></div>`,
            layoutDelegate
        });

        this.style = TimeControl.Style.Side;

        this._shouldShowDurationTimeLabel = this.layoutTraits.supportsDurationTimeLabel();

        this.elapsedTimeLabel = new TimeLabel(TimeLabel.Type.Elapsed);
        this.scrubber = new Slider(this.layoutDelegate, "scrubber");
        this.durationTimeLabel = new TimeLabel(TimeLabel.Type.Duration);
        this.remainingTimeLabel = new TimeLabel(TimeLabel.Type.Remaining);

        this.activityIndicator = new LayoutNode(`<div class="activity-indicator"></div>`);
        for (let segmentClassName of ["n", "ne", "e", "se", "s", "sw", "w", "nw"])
            this.activityIndicator.element.appendChild(document.createElement("div")).className = segmentClassName;

        this._duration = 0;
        this._currentTime = 0;
        this._loading = false;
        this._supportsSeeking = true;

        let children = [this._loading ? this.activityIndicator : this.elapsedTimeLabel, this.scrubber, this.remainingTimeLabel]
        if (this._shouldShowDurationTimeLabel) {
            this.element.classList.add("duration");
            children.push(this.durationTimeLabel);
            this.durationTimeLabel.element.addEventListener("click", this);
            this.remainingTimeLabel.element.addEventListener("click", this);
        }
        this.children = children;
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
        this.scrubber.disabled = this._loading || !this._supportsSeeking;
        this.needsLayout = true;
    }

    get supportsSeeking()
    {
        return this._supportsSeeking;
    }
    
    set supportsSeeking(flag)
    {
        if (this._supportsSeeking === flag)
            return;

        this._supportsSeeking = flag;
        this.scrubber.disabled = this._loading || !this._supportsSeeking;
        this.needsLayout = true;
    }

    get minimumWidth()
    {
        return MinimumTimeControlWidth;
    }

    get idealMinimumWidth()
    {
        return IdealMinimumTimeControlWidth;
    }

    // Protected

    layout()
    {
        super.layout();
        this._performIdealLayout();
    }

    handleEvent(event)
    {
        switch (event.type) {
        case "click":
            switch (event.target) {
            case this.durationTimeLabel.element:
            case this.remainingTimeLabel.element:
                this._toggleDurationRemainingLabel()
                break;
            }
        }
    }

    // Private

    _performIdealLayout()
    {
        if (this._loading) {
            this.durationTimeLabel.value = NaN;
            this.remainingTimeLabel.value = NaN;
        } else {
            const shouldShowZeroDurations = isNaN(this._duration) || this._duration > maxNonLiveDuration;

            if (shouldShowZeroDurations) {
                this.elapsedTimeLabel.value = 0;
                this.durationTimeLabel.value = 0;
                this.remainingTimeLabel.value = 0;

                this.elapsedTimeLabel.numberOfDigits = 4;
                this.durationTimeLabel.numberOfDigits = 4;
                this.remainingTimeLabel.numberOfDigits = 4;
            } else {
                let numberOfDigitsForTimeLabels;
                if (this._duration < OneHour)
                    numberOfDigitsForTimeLabels = 4;
                else if (this._duration < TenHours)
                    numberOfDigitsForTimeLabels = 5;
                else
                    numberOfDigitsForTimeLabels = 6;

                this.elapsedTimeLabel.value = this._currentTime;
                this.durationTimeLabel.value = this._duration;
                this.remainingTimeLabel.value = this._currentTime - this._duration;

                this.elapsedTimeLabel.numberOfDigits = numberOfDigitsForTimeLabels;
                this.durationTimeLabel.numberOfDigits = numberOfDigitsForTimeLabels;
                this.remainingTimeLabel.numberOfDigits = numberOfDigitsForTimeLabels;
            }
        }

        if (this._duration)
            this.scrubber.value = this._currentTime / this._duration;
    }

    updateScrubberLabel()
    {
        this.scrubber.inputAccessibleLabel = this.elapsedTimeLabel.value;
    }

    _toggleDurationRemainingLabel()
    {
        if (!this._shouldShowDurationTimeLabel)
            return;

        this.element.classList.toggle("duration");
        this.element.classList.toggle("remaining");
    }
}

TimeControl.Style = {
    Above: "above",
    Side:  "side",
    Below: "below"
};
