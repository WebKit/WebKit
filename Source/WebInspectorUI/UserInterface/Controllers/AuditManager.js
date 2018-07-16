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

 WI.AuditManager = class AuditManager extends WI.Object
{
    constructor()
    {
        super();

        this._testSuiteConstructors = [];
        this._reports = new Map;
        
        // Transforming all the constructors into AuditTestSuite instances.
        this._testSuites = this._testSuiteConstructors.map(suite =>  {
            let newTestSuite = new suite;

            if (!newTestSuite instanceof WI.AuditTestSuite)
                throw new Error("Audit test suites must be of instance WI.AuditTestSuite.");

            return newTestSuite;
        });
    }

    // Public

    get testSuites() { return this._testSuites.slice(); }
    get reports() { return [...this._reports.values()]; }

    async runAuditTestByRepresentedObject(representedObject)
    {
        let auditReport = new WI.AuditReport(representedObject);

        if (representedObject instanceof WI.AuditTestCase) {
            let auditResult = await this._runTestCase(representedObject);
            auditReport.addResult(auditResult);
        } else if (representedObject instanceof WI.AuditTestSuite) {
            let testCases = representedObject.testCases;            
            // Start reducing from testCases[0].
            let result = testCases.slice(1).reduce((chain, testCase, index) => {
                if (testCase.setup) {
                    let setup = testCase.setup.call(testCase, testCase.suite)
                    if (testCase.setup[Symbol.toStringTag] === "AsyncFunction")
                        return setup;
                    else
                        return new Promise(setup);
                }

                chain = chain.then((auditResult) => {
                    auditReport.addResult(auditResult);
                    return this._runTestCase(testCase);
                });

                if (testCase.tearDown) {
                    let tearDown = testCase.tearDown.call(testCase, testCase.suite)
                    if (testCase.tearDown[Symbol.toStringTag] === "AsyncFunction")
                        return tearDown;
                    else
                        return new Promise(tearDown);
                }
                return chain;
            }, this._runTestCase(testCases[0]));

            let lastAuditResult = await result;
            auditReport.addResult(lastAuditResult);

            // Make AuditReport read-only after all the AuditResults have been received.
            auditReport.close();
        }

        this._reports.set(representedObject.id, auditReport);
        this.dispatchEventToListeners(WI.AuditManager.Event.NewReportAdded, {auditReport});

        return auditReport;
    }

    addTestSuite(auditTestSuiteConstructor)
    {
        if (this._testSuiteConstructors.indexOf(auditTestSuiteConstructor) >= 0)
            throw new Error(`class ${auditTestSuiteConstructor.name} already exists.`);

        let auditTestSuite = new auditTestSuiteConstructor;
        this._testSuiteConstructors.push(auditTestSuiteConstructor);
        this._testSuites.push(auditTestSuite);
    }

    reportForId(reportId)
    {
        return this._reports.get(reportId);
    }

    removeAllReports()
    {
        this._reports.clear();
    }

    // Private

    async _runTestCase(testCase)
    {
        let didRaiseException = false;
        let result;
        this.dispatchEventToListeners(WI.AuditManager.Event.TestStarted, {test: testCase});
        try {
            result = await testCase.test.call(testCase, testCase.suite);
        } catch (resultData) {
            result = resultData;
            didRaiseException = true;
        }
        this.dispatchEventToListeners(WI.AuditManager.Event.TestEnded, {test: testCase});
        return new WI.AuditResult(testCase, {result}, didRaiseException);
    }
}

WI.AuditManager.Event = {
    TestStarted: Symbol("test-started"),
    TestEnded: Symbol("test-ended")
}
