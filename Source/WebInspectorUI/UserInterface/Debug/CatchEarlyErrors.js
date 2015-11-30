/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

window.onerror = function(message, url, lineNumber, columnNumber) {
    if (!window.__earlyErrors) {
        window.__earlyErrors = [];
        // Early errors like parse errors may happen in the <head>, so attach
        // a body if none exists yet. Code below expects document.body to exist.
        if (!document.body)
            document.write("<body></body></html>");

        // Prevent any event handlers from running in an inconsistent state.
        function stopEventPropagation(event) {
            event.stopPropagation();
        }

        let windowEvents = ["beforecopy", "copy", "click", "dragover", "focus"];
        let documentEvents = ["focus", "blur", "resize", "keydown", "keyup", "mousemove", "pagehide", "contextmenu"];
        for (let name of windowEvents)
            window.addEventListener(name, stopEventPropagation, true);
        for (let name of documentEvents)
            document.addEventListener(name, stopEventPropagation, true);

        // Don't tell InspectorFrontendAPI that loading is done, since it can
        // clear some of the error boilerplate page by accident.

        // Signal that loading is done even though we can't guarantee that
        // evaluating code on the inspector page will do anything useful.
        // Without this, the frontend host may never show the window.
        if (InspectorFrontendHost) {
            InspectorFrontendHost.loaded();
            InspectorFrontendHost.inspectedURLChanged("Internal Error");
        }
    }

    window.__earlyErrors.push({message, url, lineNumber, columnNumber});
    let firstError = window.__earlyErrors[0];

    let formattedErrorDetails = window.__earlyErrors.map((entry) => `${entry.message} (at ${entry.url}:${entry.lineNumber}:${entry.columnNumber})`);
    let detailsForBugReport = formattedErrorDetails.map((line) => ` - ${line}`).join("\n");
    let encodedBugDescription = encodeURIComponent(`Caught errors:\n${detailsForBugReport}`);
    let encodedBugTitle = encodeURIComponent(`Uncaught Exception loading Web Inspector: ${firstError.message}`);
    let prefilledBugReportLink = `https://bugs.webkit.org/enter_bug.cgi?alias=&assigned_to=webkit-unassigned%40lists.webkit.org&attach_text=&blocked=&bug_file_loc=http%3A%2F%2F&bug_severity=Normal&bug_status=NEW&comment=${encodedBugDescription}&component=Web%20Inspector&contenttypeentry=&contenttypemethod=autodetect&contenttypeselection=text%2Fplain&data=&dependson=&description=&flag_type-1=X&flag_type-3=X&form_name=enter_bug&keywords=&op_sys=All&priority=P2&product=WebKit&rep_platform=All&short_desc=${encodedBugTitle}&version=WebKit%20Nightly%20Build`;
    let detailsForHTML = formattedErrorDetails.map((line) => `<li>${line}</li>`).join("\n");
    let errorPageHTML = `<h1>
    <img src="Images/Errors.svg" />
    Web Inspector encountered an error while loading.
    </h1>
    <dl>
        <dt>Why?</dt>
        <dd>Usually, this is caused by a syntax error while modifying the Web Inspector
        UI, or running an updated frontend with out-of-date WebKit build.</dt>
        <dt>I didn't do anything...?</dt>
        <dd>If you don't think you caused this error to happen,
        <a href="${prefilledBugReportLink}">click to file a pre-populated
        bug with this information</a>. It's possible that someone else broke it by accident.</dd>
        <dt>Oops, can I try again?</dt>
        <dd><a href="javascript:window.location.reload()">Click to reload the Inspector</a>
        again after making local changes.</dd>
    </dl>
    <h2>
    <img src="Images/Console.svg" />
    These errors were caught while loading Inspector:
    </h2>
    <p><ul>${detailsForHTML}</ul></p>`;
    document.body.classList.add("caught-early-error");
    document.body.innerHTML = errorPageHTML;
}
