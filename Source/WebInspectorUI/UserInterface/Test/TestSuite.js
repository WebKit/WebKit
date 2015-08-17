/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

TestSuite = class TestSuite extends WebInspector.Object
{
    constructor(harness, name) {
        if (!(harness instanceof TestHarness))
            throw new Error("Must pass the test's harness as the first argument.");

        if (typeof name !== "string" || !name.trim().length)
            throw new Error("Tried to create TestSuite without string suite name.");

        super();

        this.name = name;
        this._harness = harness;

        this.testcases = [];
        this.runCount = 0;
        this.failCount = 0;
    }

    // Use this if the test file only has one suite, and no handling
    // of the value returned by runTestCases() is needed.
    runTestCasesAndFinish()
    {
        throw new Error("Must be implemented by subclasses.");
    }

    runTestCases()
    {
        throw new Error("Must be implemented by subclasses.");
    }

    get passCount()
    {
        return this.runCount - this.failCount;
    }

    get skipCount()
    {
        if (this.failCount)
            return this.testcases.length - this.runCount;
        else
            return 0;
    }

    addTestCase(testcase)
    {
        if (!testcase || !(testcase instanceof Object))
            throw new Error("Tried to add non-object test case.");

        if (typeof testcase.name !== "string" || !testcase.name.trim().length)
            throw new Error("Tried to add test case without a name.");

        if (typeof testcase.test !== "function")
            throw new Error("Tried to add test case without `test` function.");

        this.testcases.push(testcase);
    }
};

AsyncTestSuite = class AsyncTestSuite extends TestSuite
{
    runTestCasesAndFinish()
    {
        function finish() {
            this._harness.completeTest();
        }

        this.runTestCases()
            .then(finish.bind(this))
            .catch(finish.bind(this));
    }

    runTestCases()
    {
        if (!this.testcases.length)
            throw new Error("Tried to call runTestCases() for suite with no test cases");
        if (this._startedRunning)
            throw new Error("Tried to call runTestCases() more than once.");

        this._startedRunning = true;

        this._harness.log("");
        this._harness.log("== Running test suite: " + this.name);

        // Avoid adding newlines if nothing was logged.
        let priorLogCount = this._harness.logCount;
        let self = this;
        let result = this.testcases.reduce(function(chain, testcase, i) {
            return chain.then(function() {
                if (i > 0 && priorLogCount + 1 < self._harness.logCount)
                    self._harness.log("");

                priorLogCount = self._harness.logCount;
                self._harness.log("-- Running test case: " + testcase.name);
                self.runCount++;
                return new Promise(testcase.test);
            });
        }, Promise.resolve());

        return result.catch(function(e) {
            self.failCount++;
            let message = e;
            if (e instanceof Error)
                message = e.message;

            if (typeof message !== "string")
                message = JSON.stringify(message);

            self._harness.log("!! EXCEPTION: " + message);
            throw e; // Reject this promise by re-throwing the error.
        });
    }
};

SyncTestSuite = class SyncTestSuite extends TestSuite
{
    runTestCasesAndFinish()
    {
        this.runTestCases();
        this._harness.completeTest();
    }

    runTestCases()
    {
        if (!this.testcases.length)
            throw new Error("Tried to call runTestCases() for suite with no test cases");
        if (this._startedRunning)
            throw new Error("Tried to call runTestCases() more than once.");

        this._startedRunning = true;

        this._harness.log("");
        this._harness.log("== Running test suite: " + this.name);

        let priorLogCount = this._harness.logCount;
        let self = this;
        for (let i = 0; i < this.testcases.length; i++) {
            let testcase = this.testcases[i];
            if (i > 0 && priorLogCount + 1 < this._harness.logCount)
                this._harness.log("");

            priorLogCount = this._harness.logCount;

            this._harness.log("-- Running test case: " + testcase.name);
            self.runCount++;
            try {
                let result = testcase.test.call(null);
                if (result === false) {
                    self.failCount++;
                    return false;
                }
            } catch (e) {
                self.failCount++;
                let message = e;
                if (e instanceof Error)
                    message = e.message;
                else
                    e = new Error(e);

                if (typeof message !== "string")
                    message = JSON.stringify(message);

                this._harness.log("!! EXCEPTION: " + message);
                return false;
            }
        }

        return true;
    }
};
