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

class TimeLabel extends LayoutNode
{

    constructor()
    {
        super(`<div class="time-label"></div>`);

        this.value = 0;
    }

    // Public

    get value()
    {
        return this._value;
    }

    set value(value)
    {
        if (value === this._value)
            return;

        this._value = value;
        this.markDirtyProperty("value");
    }

    // Protected

    commitProperty(propertyName)
    {
        if (propertyName === "value")
            this.element.textContent = this._formattedTime();
        else
            super.commitProperty(propertyName);
    }

    // Private

    _formattedTime()
    {
        const time = this._value || 0;
        const absTime = Math.abs(time);
        const intSeconds = Math.floor(absTime % 60).toFixed(0);
        const intMinutes = Math.floor((absTime / 60) % 60).toFixed(0);
        const intHours = Math.floor(absTime / (60 * 60)).toFixed(0);

        const timeStrings = [intMinutes, intSeconds];
        if (intHours > 0)
            timeStrings.unshift(intHours);

        const sign = time < 0 ? "-" : "";
        return sign + timeStrings.map(x => `00${x}`.slice(-2)).join(":");
    }

}
