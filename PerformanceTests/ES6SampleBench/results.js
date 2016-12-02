/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
            this._benchmark.cells.message.innerHTML = "Running...";
    }
    
    reportDone()
    {
        if (isInBrowser)
            this._benchmark.cells.message.innerHTML = "";
    }
    
    reportResult(times)
    {
        function averageAbovePercentile(numbers, percentile) {
            // Don't change the original array.
            numbers = numbers.slice();
            
            // Sort in ascending order.
            numbers.sort(function(a, b) { return a - b; });
            
            // Now the elements we want are at the end. Keep removing them until the array size shrinks too much.
            // Examples assuming percentile = 99:
            //
            // - numbers.length starts at 100: we will remove just the worst entry and then not remove anymore,
            //   since then numbers.length / originalLength = 0.99.
            //
            // - numbers.length starts at 1000: we will remove the ten worst.
            //
            // - numbers.length starts at 10: we will remove just the worst.
            var numbersWeWant = [];
            var originalLength = numbers.length;
            while (numbers.length / originalLength > percentile / 100)
                numbersWeWant.push(numbers.pop());
            
            var sum = 0;
            for (var i = 0; i < numbersWeWant.length; ++i)
                sum += numbersWeWant[i];
            
            var result = sum / numbersWeWant.length;
            
            // Do a sanity check.
            if (numbers.length && result < numbers[numbers.length - 1]) {
                throw "Sanity check fail: the worst case result is " + result +
                    " but we didn't take into account " + numbers;
            }
            
            return result;
        }

        this.firstIteration.add(times[0]);
        let steadyTimes = times.slice(1);
        this.averageWorstCase.add(averageAbovePercentile(steadyTimes, 98));
        this.steadyState.add(steadyTimes.reduce((previous, current) => previous + current) / steadyTimes.length);
        this.reportDone();
    }
    
    reportError(message, url, lineNumber)
    {
        for (let subResult of Results.subResults)
            this[subResult].reportResult(Stats.error);
        if (isInBrowser)
            this._benchmark.cells.message.innerHTML = url + ":" + lineNumber + ": " + message;
    }
}

Results.subResults = ["firstIteration", "averageWorstCase", "steadyState"];
