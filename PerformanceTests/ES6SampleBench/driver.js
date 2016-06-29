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

class Driver {
    constructor(triggerCell, magicCell, summaryCell, key)
    {
        if (!magicCell)
            throw new Error("Need magic cell");
        if (!summaryCell)
            throw new Error("Need summary cell");
        
        this._benchmarks = new Map();
        this._triggerCell = triggerCell;
        this._magicCell = magicCell;
        this._summary = new Stats(summaryCell);
        this._key = key;
        window[key] = this;
    }
    
    addBenchmark(benchmark)
    {
        this._benchmarks.set(benchmark, new Results(benchmark));
    }
    
    start(numIterations)
    {
        this._triggerCellSaved = this._triggerCell.innerHTML;
        this._updateIterations();
        
        this._summary.reset();
        for (let [benchmark, results] of this._benchmarks)
            results.reset();
        this._isRunning = true;
        this._numIterations = numIterations;
        this._iterator = null;
        this._iterate();
    }
    
    reportResult(...args)
    {
        this._benchmarks.get(this._benchmark).reportResult(...args);
        this._recomputeSummary();
        this._iterate();
    }
    
    reportError(...args)
    {
        this._benchmarks.get(this._benchmark).reportError(...args);
        this._recomputeSummary();
        this._iterate();
    }
    
    _recomputeSummary()
    {
        class Geomean {
            constructor()
            {
                this._count = 0;
                this._sum = 0;
            }
            
            add(value)
            {
                this._count++;
                this._sum += Math.log(value);
            }
            
            get result()
            {
                return Math.exp(this._sum * (1 / this._count));
            }
        }
        
        let statses = [];
        for (let results of this._benchmarks.values()) {
            for (let subResult of Results.subResults)
                statses.push(results[subResult]);
        }
        
        let numIterations = Math.min(...statses.map(stats => stats.numIterations));
        let data = new Array(numIterations);
        for (let i = 0; i < data.length; ++i)
            data[i] = new Geomean();
        
        for (let stats of statses) {
            for (let i = 0; i < data.length; ++i)
                data[i].add(stats.valueForIteration(i));
        }
        
        let geomeans = data.map(geomean => geomean.result);
        
        this._summary.reset(...geomeans);
    }
    
    _iterate()
    {
        this._benchmark = this._iterator ? this._iterator.next().value : null;
        if (!this._benchmark) {
            if (!this._numIterations) {
                this._triggerCell.innerHTML = this._triggerCellSaved;
                return;
            }
            this._numIterations--;
            this._updateIterations();
            this._iterator = this._benchmarks.keys();
            this._benchmark = this._iterator.next().value;
        }
        
        this._benchmarks.get(this._benchmark).reportRunning();
        
        window.setTimeout(() => {
            if (!this._isRunning)
                return;
            
            this._magicCell.contentDocument.body.textContent = "";
            this._magicCell.contentDocument.body.innerHTML = "<iframe id=\"magicFrame\" frameborder=\"0\">";
            
            let magicFrame = this._magicCell.contentDocument.getElementById("magicFrame");
            magicFrame.contentDocument.open();
            magicFrame.contentDocument.write(
                `<!DOCTYPE html><head><title>benchmark payload</title></head><body><script>` +
                `window.onerror = top.${this._key}.reportError;\n` +
                `function reportResult()\n` +
                `{\n` +
                `    var driver = top.${this._key};\n` +
                `    driver.reportResult.apply(driver, arguments);\n` +
                `}\n` +
                `</script>\n` +
                `${this._benchmark.code}</body></html>`);
        }, 100);
    }
    
    _updateIterations()
    {
        this._triggerCell.innerHTML = "Running... (" + (this._numIterations + 1) + " to go)";
    }
}

