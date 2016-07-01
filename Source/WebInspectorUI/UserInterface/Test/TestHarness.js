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

TestHarness = class TestHarness extends WebInspector.Object
{
    constructor()
    {
        super();

        this._logCount = 0;
    }

    completeTest()
    {
        throw new Error("Must be implemented by subclasses.");
    }

    addResult()
    {
        throw new Error("Must be implemented by subclasses.");
    }

    debugLog()
    {
        throw new Error("Must be implemented by subclasses.");
    }

    evaluateInPage(string, callback)
    {
        throw new Error("Must be implemented by subclasses.");
    }

    debug()
    {
        throw new Error("Must be implemented by subclasses.");
    }

    createAsyncSuite(name)
    {
        return new AsyncTestSuite(this, name);
    }

    createSyncSuite(name)
    {
        return new SyncTestSuite(this, name);
    }

    get logCount()
    {
        return this._logCount;
    }

    log(message)
    {
        ++this._logCount;

        if (this.forceDebugLogging)
            this.debugLog(message);
        else
            this.addResult(message);
    }

    assert(condition, message)
    {
        if (condition)
            return;

        let stringifiedMessage = TestHarness.messageAsString(message);
        this.log("ASSERT: " + stringifiedMessage);
    }

    expectThat(condition, message)
    {
        if (condition)
            this.pass(message);
        else
            this.fail(message);
    }

    pass(message)
    {
        let stringifiedMessage = TestHarness.messageAsString(message);
        this.log("PASS: " + stringifiedMessage);
    }

    fail(message)
    {
        let stringifiedMessage = TestHarness.messageAsString(message);
        this.log("FAIL: " + stringifiedMessage);
    }

    // Protected

    static messageAsString(message)
    {
        if (message instanceof Element)
            return message.textContent;
        
        return (typeof message !== "string") ? JSON.stringify(message) : message;
    }
};
