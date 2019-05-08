/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

(function() {
    if (WI.dontLocalizeUserInterface)
        return;

    let localizedStringsURL = InspectorFrontendHost.localizedStringsURL();
    console.assert(localizedStringsURL);
    if (localizedStringsURL)
        document.write("<script src=\"" + localizedStringsURL + "\"></script>");
})();

WI.unlocalizedString = function(string)
{
    // Intentionally do nothing, since this is for engineering builds
    // (such as in Debug UI) or in text that is standardized in English.
    // For example, CSS property names and values are never localized.
    return string;
};

WI.UIString = function(string, key, comment)
{
    if (WI.dontLocalizeUserInterface)
        return string;

    key = key || string;

    if (window.localizedStrings && key in window.localizedStrings)
        return window.localizedStrings[key];

    if (!window.localizedStrings)
        console.error(`Attempted to load localized string "${key}" before localizedStrings was initialized.`, comment);

    if (!this._missingLocalizedStrings)
        this._missingLocalizedStrings = {};

    if (!(key in this._missingLocalizedStrings)) {
        console.error(`Localized string "${key}" was not found.`, comment);
        this._missingLocalizedStrings[key] = true;
    }

    return "LOCALIZED STRING NOT FOUND";
};
