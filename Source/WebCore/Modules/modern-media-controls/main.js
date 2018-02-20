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

const SkipSeconds = 15;
const MinimumSizeToShowAnyControl = 47;
const MaximumSizeToShowSmallProminentControl = 88;

let mediaControlsHost;

// This is called from HTMLMediaElement::ensureMediaControlsInjectedScript().
function createControls(shadowRoot, media, host)
{
    if (host) {
        mediaControlsHost = host;
        iconService.mediaControlsHost = host;
        shadowRoot.appendChild(document.createElement("style")).textContent = host.shadowRootCSSText;
    }

    return new MediaController(shadowRoot, media, host);
}

function UIString(stringToLocalize, replacementString)
{
    let allLocalizedStrings = {};
    try {
        allLocalizedStrings = UIStrings;
    } catch (error) {}

    const localizedString = allLocalizedStrings[stringToLocalize];
    if (!localizedString)
        return stringToLocalize;

    if (replacementString)
        return localizedString.replace("%s", replacementString);

    return localizedString;
}

function formatTimeByUnit(value)
{
    const time = value || 0;
    const absTime = Math.abs(time);
    return {
        seconds: Math.floor(absTime % 60).toFixed(0),
        minutes: Math.floor((absTime / 60) % 60).toFixed(0),
        hours: Math.floor(absTime / (60 * 60)).toFixed(0)
    };
}

function unitizeTime(value, unit)
{
    let returnedUnit = UIString(unit);
    if (value > 1)
        returnedUnit = UIString(`${unit}s`);

    return `${value} ${returnedUnit}`;
}

function formattedStringForDuration(timeInSeconds)
{
    if (mediaControlsHost)
        return mediaControlsHost.formattedStringForDuration(Math.abs(timeInSeconds));
    else
        return "";
}
