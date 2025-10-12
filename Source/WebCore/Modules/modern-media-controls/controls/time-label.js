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

const MinusSignWidthsForDigits = {
    3: 6,
    4: 5,
    5: 6,
    6: 5
};

const WidthsForDigits = {
    3: 27,
    4: 35,
    5: 46,
    6: 54
}

class TimeLabel extends LayoutNode
{

    constructor(type)
    {
        super(`<div role="text" class="time-label"></div>`);

        this._type = type;
        this.value = 0;
        this.numberOfDigits = 4;

        switch (this._type) {
        case TimeLabel.Type.Elapsed:
            this.element.setAttribute("id", "time-label-elapsed");
            break;

        case TimeLabel.Type.Remaining:
            this.element.setAttribute("id", "time-label-remaining");
            break;

        case TimeLabel.Type.Duration:
            this.element.setAttribute("id", "time-label-duration");
            break;
        }
    }

    // Public

    get value()
    {
        return this._value;
    }
    set value(value)
    {
        this._value = value;
        this.markDirtyProperty("value");
    }

    get numberOfDigits()
    {
        return this._numberOfDigits;
    }
    set numberOfDigits(numberOfDigits)
    {
        if (this._numberOfDigits === numberOfDigits)
            return;

        if (numberOfDigits < 4 || numberOfDigits > 6)
            throw new RangeError("TimeLabel numerOfDigits must be between 4 and 6.");

        this._numberOfDigits = numberOfDigits;

        let formatterOptions = {
            style: "digital",
            fractionalDigits: 0,
            hoursDisplay: "auto",
            minutesDisplay: "always",
            secondsDisplay: "always",
        };
        if (numberOfDigits >= 5)
            formatterOptions.hoursDisplay = "always";
        if (numberOfDigits == 6)
            formatterOptions.hours = "2-digit";

        this._formatter = new Intl.DurationFormat(undefined, formatterOptions);

        this.markDirtyProperty("value");
    }

    setValueWithNumberOfDigits(value, numberOfDigits)
    {
        this.value = value;
        this.numberOfDigits = numberOfDigits;
    }

    // Protected

    commitProperty(propertyName)
    {
        if (propertyName === "value") {
            this.element.textContent = this._formattedTime();

            const timeAsString = utils.formattedStringForDuration(this.value);
            switch (this._type) {
            case TimeLabel.Type.Elapsed:
                this.element.setAttribute("aria-label", UIString("Elapsed: %s", timeAsString));
                break;

            case TimeLabel.Type.Remaining:
                this.element.setAttribute("aria-label", UIString("Remaining: %s", timeAsString));
                break;

            case TimeLabel.Type.Duration:
                this.element.setAttribute("aria-label", UIString("Duration: %s", timeAsString));
                break;
            }

            if (this.parent instanceof TimeControl)
                this.parent.updateScrubberLabel();
        }
        else
            super.commitProperty(propertyName);
    }

    // Private

    _formattedTime()
    {
        if (isNaN(this._value))
            return "--:--";
        
        return this._formatter.format(formatTimeByUnit(this._value));
    }

}

function doubleDigits(x)
{
    if (x < 10)
        return `0${x}`;
    return `${x}`;
}

TimeLabel.Type = {
    Elapsed: 0,
    Remaining: 1,
    Duration: 2,
};
