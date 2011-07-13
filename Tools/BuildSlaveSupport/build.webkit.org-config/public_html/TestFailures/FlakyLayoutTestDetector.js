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

function FlakyLayoutTestDetector() {
    this._tests = {};
}

FlakyLayoutTestDetector.prototype = {
    incorporateTestResults: function(buildName, failingTests, tooManyFailures) {
        var newFlakyTests = [];

        if (tooManyFailures) {
            // Something was going horribly wrong during this test run. We shouldn't assume that any
            // passes/failures are due to flakiness.
            return newFlakyTests;
        }

        // Record failing tests.
        for (var testName in failingTests) {
            if (!(testName in this._tests)) {
                this._tests[testName] = {
                    state: this._states.LastSeenFailing,
                    history: [],
                };
            }

            var testData = this._tests[testName];
            testData.history.push({ build: buildName, result: failingTests[testName] });

            if (testData.state === this._states.LastSeenPassing) {
                testData.state = this._states.PossiblyFlaky;
                newFlakyTests.push(testName);
            }
        }

        // Record passing tests.
        for (var testName in this._tests) {
            if (testName in failingTests)
                continue;

            var testData = this._tests[testName];
            testData.history.push({ build: buildName, result: { failureType: 'pass' } });

            if (testData.state === this._states.LastSeenFailing)
                testData.state = this._states.LastSeenPassing;
        }

        return newFlakyTests;
    },

    allFailures: function(testName) {
        if (!(testName in this._tests))
            return null;

        return this._tests[testName].history.filter(function(historyItem) { return historyItem.result.failureType !== 'pass' });
    },

    get possiblyFlakyTests() {
        var self = this;
        return Object.keys(self._tests).filter(function(testName) { return self._tests[testName].state === self._states.PossiblyFlaky });
    },

    _states: {
        LastSeenFailing: 0,
        LastSeenPassing: 1,
        PossiblyFlaky: 2,
    },
};
