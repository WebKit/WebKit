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

const MinimumSizeToShowAnyControl = 47;
const MaximumSizeToShowSmallProminentControl = 88;

// If running outside the media element's isolated world, polyfill the MediaControlsUtils object:
if (!window.utils) {
    window.utils = {
        formattedStringForDuration: function(duration) {
            return "";
        },
    };
}

// This is called from HTMLMediaElement::ensureMediaControls().
function createControls(shadowRoot, media, host)
{
    if (host) {
        for (let styleSheet of host.shadowRootStyleSheets)
            shadowRoot.appendChild(document.createElement("style")).textContent = styleSheet;
    }

    controller = new MediaController(shadowRoot, media, host);
    if (host)
        host.controller = controller;
    return controller;
}

function UIString(stringToLocalize, ...replacementStrings)
{
    let localizedString = window.UIStrings?.[stringToLocalize] ?? stringToLocalize;

    for (let replacementString of replacementStrings)
        localizedString = localizedString.replace("%s", replacementString);

    return localizedString;
}

function formatTimeByUnit(value)
{
    const time = value || 0;
    const absTime = Math.abs(time);
    const sign = Math.sign(time);
    return {
        seconds: sign * Math.floor(absTime % 60).toFixed(0),
        minutes: sign * Math.floor((absTime / 60) % 60).toFixed(0),
        hours: sign * Math.floor(absTime / (60 * 60)).toFixed(0)
    };
}

function unitizeTime(value, unit)
{
    let returnedUnit = UIString(unit);
    if (value > 1)
        returnedUnit = UIString(`${unit}s`);

    return `${value} ${returnedUnit}`;
}

