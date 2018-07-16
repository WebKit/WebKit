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

 WI.AuditReport = class AuditReport
{
    constructor(representedTest)
    {
        console.assert(representedTest instanceof WI.AuditTestCase || representedTest instanceof WI.AuditTestSuite);
        
        this._results = [];
        this._isWritable = true;
        this._representedTestCases = (representedTest instanceof WI.AuditTestCase) ? [representedTest] : [...representedTest.testCases];
        this._representedTestSuite = (representedTest instanceof WI.AuditTestSuite) ? representedTest : null;
    }

    // Public

    get representedTestCases() { return this._representedTestCases.slice(); }
    get representedTestSuite() { return this._representedTestSuite; }
    get resultsData() { return this._results.slice(); }
    get isWritable() { return this._isWritable; }

    get failedCount() {
        return this._results.slice().filter(result => result.failed).length;
    }

    addResult(auditResult)
    {
        if (!this._isWritable)
            return;

        console.assert(auditResult instanceof WI.AuditResult);
        this._results.push(auditResult);
    }

    close()
    {
        this._isWritable = false;
    }
}
