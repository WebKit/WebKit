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

WI.AuditTestGroup = class AuditTestGroup extends WI.AuditTestBase
{
    constructor(name, tests, {description} = {})
    {
        console.assert(Array.isArray(tests));

        super(name, {description});

        this._tests = tests;

        for (let test of this._tests) {
            test.addEventListener(WI.AuditTestBase.Event.Completed, this._handleTestCompleted, this);
            test.addEventListener(WI.AuditTestBase.Event.Progress, this._handleTestProgress, this);
        }
    }

    // Static

    static async fromPayload(payload)
    {
        if (typeof payload !== "object" || payload === null)
            return null;

        let {type, name, tests, description} = payload;

        if (type !== WI.AuditTestGroup.TypeIdentifier)
            return null;

        if (typeof name !== "string")
            return null;

        if (!Array.isArray(tests))
            return null;

        tests = await Promise.all(tests.map(async (test) => {
            let testCase = await WI.AuditTestCase.fromPayload(test);
            if (testCase)
                return testCase;

            let testGroup = await WI.AuditTestGroup.fromPayload(test);
            if (testGroup)
                return testGroup;

            return null;
        }));
        tests = tests.filter((test) => !!test);
        if (!tests.length)
            return null;

        let options = {};
        if (typeof description === "string")
            options.description = description;

        return new WI.AuditTestGroup(name, tests, options);
    }

    // Public

    get tests() { return this._tests; }

    stop()
    {
        // Called from WI.AuditManager.

        for (let test of this._tests)
            test.stop();

        super.stop();
    }

    clearResult(options = {})
    {
        let cleared = !!this._result;
        for (let test of this._tests) {
            if (test.clearResult(options))
                cleared = true;
        }

        return super.clearResult({
            ...options,
            suppressResultClearedEvent: !cleared,
        });
    }

    toJSON()
    {
        let json = super.toJSON();
        json.tests = this._tests.map((testCase) => testCase.toJSON());
        return json;
    }

    // Protected

    async run()
    {
        let count = this._tests.length;
        for (let index = 0; index < count && this._runningState === WI.AuditManager.RunningState.Active; ++index) {
            let test = this._tests[index];

            await test.start();

            if (test instanceof WI.AuditTestCase)
                this.dispatchEventToListeners(WI.AuditTestBase.Event.Progress, {index, count});
        }

        this._updateResult();
    }

    // Private

    _updateResult()
    {
        let results = this._tests.map((test) => test.result).filter((result) => !!result);
        if (!results.length)
            return;

        this._result = new WI.AuditTestGroupResult(this.name, results, {
            description: this.description,
        });
    }

    _handleTestCompleted(event)
    {
        if (this._runningState === WI.AuditManager.RunningState.Active)
            return;

        this._updateResult();
        this.dispatchEventToListeners(WI.AuditTestBase.Event.Completed);
    }

    _handleTestProgress(event)
    {
        if (this._runningState !== WI.AuditManager.RunningState.Active)
            return;

        let walk = (tests) => {
            let count = 0;
            for (let test of tests) {
                if (test instanceof WI.AuditTestCase)
                    ++count;
                else if (test instanceof WI.AuditTestGroup)
                    count += walk(test.tests);
            }
            return count;
        };

        this.dispatchEventToListeners(WI.AuditTestBase.Event.Progress, {
            index: event.data.index + walk(this.tests.slice(0, this.tests.indexOf(event.target))),
            count: walk(this.tests),
        });
    }
};

WI.AuditTestGroup.TypeIdentifier = "test-group";
