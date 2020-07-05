/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

// La Banque Postale uses a fixed list of iOS versions that support safe area insets (versions 11
// through 13 at the time of writing). Since iOS 14 is not in this list, the app fails to apply top
// and bottom safe area insets to its web content. Work around this by adding the 'device-ios12p'
// class to <body> as if the iOS major version were 13 (the app currently uses 'device-ios12p' to
// represent both iOS 12 and iOS 13). This quirk will be disabled on versions of La Banque Postale
// that link against the iOS 14 SDK (or later).

(() => {
    const deviceReady = () => {
        // Only add 'device-ios12p' if <body> exists, contains the 'device-ios' class, and doesn't
        // contain any other 'device-ios<version>' classes.
        const bodyClasses = Array.from(document.body?.classList || []);
        if (bodyClasses.includes('device-ios') && !bodyClasses.some((c) => /^device-ios\d/.test(c)))
            document.body.classList.add('device-ios12p');
    };

    window.addEventListener('load', () => document.addEventListener('deviceready', deviceReady));
})();
