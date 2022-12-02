/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 
const bd1 = new BigDenary('8965168485622506189945604.1235068121348084163185216');
const bd2 = new BigDenary('2480986213549488579706531.6546845013548451265890628');
const bdEquals = (a, b) => a.equals(b);

function areFirstAndLastResultsEqual(bdThunk, areEqual = bdEquals) {
    const firstResult = bdThunk();

    var lastResult;
    for (var i = 0; i < 1e4; i++)
        lastResult = bdThunk();

    return areEqual(firstResult, lastResult);
}

class Benchmark {
    constructor() {
        this._verbose = false;
        this._allFirstAndLastResultsAreEqual = true;
    }

    runIteration() {
        this._allFirstAndLastResultsAreEqual &&=
            areFirstAndLastResultsEqual(() => bd1.plus(bd2)) &&
            areFirstAndLastResultsEqual(() => bd1.minus(bd2)) &&
            areFirstAndLastResultsEqual(() => bd1.negated()) &&
            areFirstAndLastResultsEqual(() => bd1.comparedTo(bd2), Object.is) &&
            areFirstAndLastResultsEqual(() => bd1.multipliedBy(bd2)) &&
            areFirstAndLastResultsEqual(() => bd1.dividedBy(bd2));
    }

    validate() {
        if (!this._allFirstAndLastResultsAreEqual)
            throw new Error("Expected all first and last results to be equal, but they aren't.");
    }
}
