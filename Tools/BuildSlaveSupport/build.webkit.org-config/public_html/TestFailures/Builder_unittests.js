/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

module("Builder");

test("getNumberOfFailingTests shouldn't include leaks", 4, function() {
    var mockBuildbot = {};
    mockBuildbot.baseURL = 'http://example.com/';

    var builder = new Builder('test builder', mockBuildbot);

    var realGetResource = window.getResource;
    window.getResource = function(url, successCallback, errorCallback) {
        var mockXHR = {};
        mockXHR.responseText = JSON.stringify({
            steps: [
                {
                    name: 'layout-test',
                    isStarted: true,
                    results: [
                        2,
                        [
                            "2178 total leaks found!", 
                            "2 test cases (<1%) had incorrect layout", 
                            "2 test cases (<1%) crashed",
                        ],
                    ],
                },
            ],
        });

        successCallback(mockXHR);
    };

    // FIXME: It's lame to be modifying singletons like this. We should get rid
    // of our singleton usage entirely!
    var realPersistentCache = window.PersistentCache;
    window.PersistentCache = {
        contains: function() { return false; },
        set: function() { },
        get: function() { },
    };

    builder.getNumberOfFailingTests(1, function(failureCount, tooManyFailures) {
        equal(failureCount, 4);
        equal(tooManyFailures, false);
    });

    window.getResource = realGetResource;
    equal(window.getResource, realGetResource);
    window.PersistentCache = realPersistentCache;
    equal(window.PersistentCache, realPersistentCache);
});

})();
