/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
"use strict";

class Results {
    constructor(benchmark)
    {
        this._benchmark = benchmark;
        for (let subResult of Results.subResults)
            this[subResult] = new Stats(benchmark.cells[subResult], subResult);
    }
    
    get benchmark() { return this._benchmark; }
    
    reset()
    {
        for (let subResult of Results.subResults)
            this[subResult].reset();
    }
    
    reportRunning()
    {
        if (isInBrowser)
            this._benchmark.cells.message.classList.add('running');
    }
    
    reportDone()
    {
        if (isInBrowser)
            this._benchmark.cells.message.classList.remove('running');
    }
    
    reportResult(times)
    {
        if (times.length < 5)
            throw new Error("We expect >= 5 iterations");

        this.firstIteration.add(times[0]);
        let steadyTimes = times.slice(1).sort((a, b) => b - a); // Sort in reverse order.
        this.averageWorstCase.add((steadyTimes[0] + steadyTimes[1] + steadyTimes[2] + steadyTimes[3]) / 4);
        this.steadyState.add(steadyTimes.reduce((previous, current) => previous + current) / steadyTimes.length);
        this.reportDone();
    }
    
    reportError(message, url, lineNumber)
    {
        if (isInBrowser) {
            this._benchmark.cells.message.classList.remove('running');
            this._benchmark.cells.message.classList.add('failed');
        } else
            print("Failed running benchmark");
    }
}

Results.subResults = ["firstIteration", "averageWorstCase", "steadyState"];
