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

WebInspector.Platform = {
    name: InspectorFrontendHost.platform(),
    isLegacyMacOS: false,
    isNightlyBuild: false,
    version: {
        base: 0,
        release: 0,
        name: ""
    }
};

(function () {
    // Check for a nightly build by looking for a plus in the version number and a small number of stylesheets (indicating combined resources).
    var versionMatch = / AppleWebKit\/([^ ]+)/.exec(navigator.userAgent);
    if (versionMatch && versionMatch[1].indexOf("+") !== -1 && document.styleSheets.length < 10)
        WebInspector.Platform.isNightlyBuild = true;

    var isLegacyMacOS = false;
    var osVersionMatch = / Mac OS X (\d+)_(\d+)/.exec(navigator.appVersion);
    if (osVersionMatch && osVersionMatch[1] === "10") {
        WebInspector.Platform.version.base = 10;
        switch(osVersionMatch[2]) {
            case "10":
                WebInspector.Platform.version.name = "yosemite";
                WebInspector.Platform.version.release = 10;
                break;
            case "9":
                WebInspector.Platform.version.name = "mavericks";
                WebInspector.Platform.version.release = 9;
                WebInspector.Platform.isLegacyMacOS = true;
                break;
            case "8":
                WebInspector.Platform.version.name = "mountain-lion";
                WebInspector.Platform.version.release = 8;
                WebInspector.Platform.isLegacyMacOS = true;
                break;
        }
    }
})();
