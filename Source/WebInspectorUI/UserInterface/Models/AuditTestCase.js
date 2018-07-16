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

 WI.AuditTestCase = class AuditTestCase extends WI.Object
{
    constructor(suite, name, test, setup, tearDown, errorDetails = {})
    {
        console.assert(suite instanceof WI.AuditTestSuite);
        console.assert(typeof(name) === "string");
        
        if (setup)
            console.assert(setup instanceof Function);

        if (tearDown)
            console.assert(tearDown instanceof Function);

        if (test[Symbol.toStringTag] !== "AsyncFunction")
            throw new Error("Test functions must be async functions.");

        super();
        this._id = Symbol(name);
        
        this._suite = suite;
        this._name = name;
        this._test = test;
        this._setup = setup;
        this._tearDown = tearDown;
        this._errorDetails = errorDetails;
    }

    // Public

    get id() { return this._id; }
    get name() { return this._name; }
    get suite() { return this._suite; }
    get test() { return this._test; }
    get setup() { return this._setup; }
    get tearDown() { return this._tearDown; }
    get errorDetails() { return this._errorDetails; }
}
