/*
* Copyright (C) 2025 Apple Inc. All rights reserved.
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
* THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Load D3 and data loading utilities for d8

const EXPECTED_LAST_RESULT_LENGTH = 691366;
const EXPECTED_LAST_RESULT_HASH = 144487595;

globalThis.clearTimeout = () => { };

class Benchmark extends StartupBenchmark {
    lastResult;
    totalHash = 0xdeadbeef;
    currentIteration = 0;

    constructor({iterationCount}) {
        super({
            iterationCount,
            expectedCacheCommentCount: 10028,
            sourceCodeReuseCount: 2,
         });
    }

    async init() {
        await super.init();
        this.airportsCsvString = (await JetStream.getString(JetStream.preload.AIRPORTS));
        console.assert(this.airportsCsvString.length == 145493, `Expected this.airportsCsvString.length to be 141490 but got ${this.airportsCsvString.length}`);
        this.usDataJsonString = await JetStream.getString(JetStream.preload.US_DATA);
        console.assert(this.usDataJsonString.length == 2880996, `Expected this.usData.length to be 2880996 but got ${this.usDataJsonString.length}`);
        this.usData = JSON.parse(this.usDataJsonString);
    }

    runIteration() {
        let iterationSourceCode = this.iterationSourceCodes[this.currentIteration];
        if (!iterationSourceCode)
            throw new Error(`Could not find source for iteration ${this.currentIteration}`);
        // Module in sourceCode it assigned to the ReactRenderTest variable.
        let D3Test;
        eval(iterationSourceCode);
        const html = D3Test.runTest(this.airportsCsvString, this.usData);
        const htmlHash = this.quickHash(html);
        this.lastResult = { html, htmlHash };
        this.totalHash ^= this.lastResult.htmlHash;
        this.currentIteration++;
    }

    validate() {
        if (this.lastResult.html.length != EXPECTED_LAST_RESULT_LENGTH)
            throw new Error(`Expected this.lastResult.html.length to be ${EXPECTED_LAST_RESULT_LENGTH} but got ${this.lastResult.length}`);
        if (this.lastResult.htmlHash != EXPECTED_LAST_RESULT_HASH)
            throw new Error(`Expected this.lastResult.htmlHash to be ${EXPECTED_LAST_RESULT_HASH} but got ${this.lastResult.htmlHash}`);
    }
}