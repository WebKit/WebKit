/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

function enterFullscreen() {

    // Session::fullscreenWindow maps result "entered", "unsupported" and "failed" (with a reason) to WebDriver results.
    let callback = arguments[0];

    // This is also false when Permissions-Policy disallows fullscreen, which we report as unsupported too.
    if (!document.fullscreenEnabled) {
        callback({ result: "unsupported" });
        return;
    }

    if (document.fullscreenElement) {
        callback({ result: "entered" });
        return;
    }

    let fullscreenChangeListener, fullscreenErrorListener;
    let finish = (result, reason) => {
        document.removeEventListener("fullscreenchange", fullscreenChangeListener);
        document.documentElement.removeEventListener("fullscreenerror", fullscreenErrorListener);
        callback({ result, reason });
    };
    fullscreenChangeListener = (e) => {
        // Some other element went fullscreen, we didn't request it.
        if (e.target !== document.documentElement)
            return;

        if (!document.fullscreenElement)
            return;

        finish("entered");
    };
    fullscreenErrorListener = (e) => {
        // Some other element caused an error, we didn't request it.
        if (e.target !== document.documentElement)
            return;

        finish("failed", "fullscreenerror event");
    };

    let requested = false;
    let request = () => {
        if (requested)
            return;
        requested = true;

        // The document fires change events, but the fullscreen element fires error events.
        document.addEventListener("fullscreenchange", fullscreenChangeListener);
        document.documentElement.addEventListener("fullscreenerror", fullscreenErrorListener);
        // Events can be missed (a refusal without an error event, or an enter and exit before the listener runs),
        // so settle on the promise too.
        document.documentElement.requestFullscreen().then(
            () => finish(document.fullscreenElement ? "entered" : "failed", "requestFullscreen() resolved without entering fullscreen"),
            (error) => finish("failed", error.message));
    };

    // A hidden document can't go fullscreen, and a restored window's page can still be hidden for a moment.
    // Don't wait long, since the forced user activation expires.
    if (document.hidden) {
        document.addEventListener("visibilitychange", request, { once: true });
        setTimeout(request, 2000);
    } else
        request();
}
