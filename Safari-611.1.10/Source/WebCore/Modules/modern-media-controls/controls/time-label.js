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
        this.setValueWithNumberOfDigits(0, 4);
    }

    // Public

    get value()
    {
        return this._value;
    }

    setValueWithNumberOfDigits(value, numberOfDigits)
    {
        this._value = value;
        this._numberOfDigits = numberOfDigits;
        this.width = WidthsForDigits[this._numberOfDigits] + (this._type === TimeLabel.Types.Remaining && !isNaN(this._value) ? MinusSignWidthsForDigits[this._numberOfDigits] : 0);
        this.markDirtyProperty("value");
    }

    // Protected

    commitProperty(propertyName)
    {
        if (propertyName === "value") {
            this.element.textContent = this._formattedTime();
            const timeAsString = formattedStringForDuration(this.value);
            const ariaLabel = (this._type === TimeLabel.Types.Remaining) ? UIString("Remaining") : UIString("Elapsed");
            this.element.setAttribute("aria-label", `${ariaLabel}: ${timeAsString}`);
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
        
        const time = formatTimeByUnit(this._value);

        let timeComponents;
        if (this._numberOfDigits == 3)
            timeComponents = [time.minutes, doubleDigits(time.seconds)];
        else if (this._numberOfDigits == 4)
            timeComponents = [doubleDigits(time.minutes), doubleDigits(time.seconds)];
        else if (this._numberOfDigits == 5)
            timeComponents = [time.hours, doubleDigits(time.minutes), doubleDigits(time.seconds)];
        else if (this._numberOfDigits == 6)
            timeComponents = [doubleDigits(time.hours), doubleDigits(time.minutes), doubleDigits(time.seconds)];

        return (this._type === TimeLabel.Types.Remaining ? "-" : "") + timeComponents.join(":");
    }

}

function doubleDigits(x)
{
    if (x < 10)
        return `0${x}`;
    return `${x}`;
}

TimeLabel.Types = {
    Elapsed: 0,
    Remaining: 1
};
