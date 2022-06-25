/*
 * Copyright (C) 2012 Purdue University
 * Written by Gregor Richards
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

(function() {
    /* This kind of test is very difficult to generalize, so we just avoid
     * browsers known not to work */
    var browserMin = {
        "mozilla": "2.0",
        "webkit": "536.7",
        "v8": "10.0",
        "msie": "9.0",
        "opera": "11.60"
    };

    /* Since Chrome doesn't use the same JS engine as WebKit, it actually has
     * different dependencies */
    var rchrome = /Chrome[\/]([\w.]+)/;
    var match = rchrome.exec(navigator.userAgent);
    if (match) {
        delete $.browser.webkit;
        $.browser.v8 = true;
        $.browser.version = match[1];
    }

    var browserName = {
        "mozilla": "Mozilla",
        "webkit": "WebKit",
        "v8": "Chrome",
        "msie": "Microsoft Internet Explorer",
        "opera": "Opera"
    };

    var versionName = {
        "mozilla": "2.0 (e.g. Firefox 4.0)"
    };

    var real = "harness.html";

    // version-number-based comparison
    function versionCheck(min, real) {
        var minComps = min.split(".");
        var realComps = real.split(".");
        while (realComps.length < minComps.length) realComps.push("0");

        for (var i = 0; i < minComps.length; i++) {
            var min = parseInt(minComps[i]);
            var real = parseInt(realComps[i]);
            if (real < min) return false;
            else if (real > min) return true;
        }

        return true;
    }

    // test the browser version
    window.onload = function() {
        var badBrowser = false;
        var goodVersion = 0;
        var badVersion = 0;
        for (var browser in browserMin) {
            var version = browserMin[browser];
            if ($.browser[browser]) {
                if (!versionCheck(version, $.browser.version)) {
                    badBrowser = browser;
                    goodVersion = version;
                    if (badBrowser in versionName) goodVersion = versionName[badBrowser];
                    badVersion = $.browser.version;
                }
            }
        }

        var output = document.getElementById("output");
        if (badBrowser) {
            // we don't like your browser
            output.innerHTML =
                "Sorry, but your browser is not supported. The minimum version of <b>" +
                browserName[badBrowser] +
                "</b> required to run these benchmarks is <b>" +
                goodVersion +
                "</b> (You are using <b>" +
                badVersion +
                "</b>). If you would like to attempt to run them anyway, <a href=\"" +
                real +
                "\">click here</a>, but it is highly likely that they will not " +
                "run, and if they do, the results may be unreliable.";

        } else {
            output.innerHTML =
                "Loading benchmarks ...";
            window.location.href = real;

        }
    };
})();
