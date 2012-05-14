/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

(function () {

module("checkout");

test("subversionURLForTest", 1, function() {
    equals(checkout.subversionURLForTest("path/to/test.html"), "http://svn.webkit.org/repository/webkit/trunk/LayoutTests/path/to/test.html");
});

test("updateExpectations", 4, function() {
    var simulator = new NetworkSimulator();

    // FIXME: This leaks state into g_haveSeenCheckoutAvailable, which is global.
    simulator.ajax = function(options) { options.success.call(); },

    simulator.post = function(url, data, callback)
    {
        equals(url, 'http://127.0.0.1:8127/updateexpectations');
        equals(data, '[{"builderName":"WebKit Linux","testName":"another/test.svg"}]');
        simulator.scheduleCallback(callback);
    };

    simulator.runTest(function() {
        checkout.updateExpectations([{
            'builderName': 'WebKit Linux',
            'testName': 'another/test.svg',
        }], function() {
            ok(true);
        }, function() {
            ok(false);
        });
    });
});

test("optimizeBaselines", 3, function() {
    var simulator = new NetworkSimulator();
    simulator.post = function(url, callback)
    {
        equals(url, 'http://127.0.0.1:8127/optimizebaselines?test=another%2Ftest.svg&suffixes=txt%2Cpng');
        simulator.scheduleCallback(callback);
    };

    simulator.runTest(function() {
        checkout.optimizeBaselines('another/test.svg', 'txt,png', function() {
            ok(true);
        });
    });
});

test("rebaseline", 6, function() {
    var simulator = new NetworkSimulator();

    var requestedURLs = [];
    simulator.post = function(url, callback)
    {
        requestedURLs.push(url);
        simulator.scheduleCallback(callback);
    };
    simulator.ajax = function(options)
    {
        if (options.url.indexOf('/ping') != -1)
            ok(false, 'Recieved non-ping ajax request.');
        simulator.scheduleCallback(options.success);
    };

    var kExpectedTestNameProgressStack = [
        'fast/test.html',
        'another/test.svg',
        'another/test.svg', // This is the first one.
    ];

    simulator.runTest(function() {
        checkout.rebaseline([{
            'builderName': 'WebKit Linux',
            'testName': 'another/test.svg',
            'failureInfoList': ['IMAGE'],
        }, {
            'builderName': 'WebKit Mac10.6',
            'testName': 'another/test.svg',
            'failureInfoList': ['IMAGE', 'TEXT', 'IMAGE+TEXT'],
        }, {
            'builderName': 'Webkit Vista',
            'testName': 'fast/test.html',
            'failureInfoList': ['IMAGE+TEXT'],
        }], function() {
            ok(true);
        }, function(failureInfo) {
            equals(failureInfo.testName, kExpectedTestNameProgressStack.pop());
        }, function() {
            ok(false, 'Checkout should be available.');
        });
    });

    deepEqual(requestedURLs, [
        "http://127.0.0.1:8127/rebaseline?builder=WebKit+Linux&test=another%2Ftest.svg&suffixes=png",
        "http://127.0.0.1:8127/rebaseline?builder=WebKit+Mac10.6&test=another%2Ftest.svg&suffixes=png%2Ctxt",
        "http://127.0.0.1:8127/rebaseline?builder=Webkit+Vista&test=fast%2Ftest.html&suffixes=txt%2Cpng",
        "http://127.0.0.1:8127/optimizebaselines?test=another%2Ftest.svg&suffixes=png%2Ctxt",
        "http://127.0.0.1:8127/optimizebaselines?test=fast%2Ftest.html&suffixes=png%2Ctxt"
    ]);
});

})();
