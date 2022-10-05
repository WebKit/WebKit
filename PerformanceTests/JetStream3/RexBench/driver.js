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

class Driver {
    constructor(statusCell, triggerCell, triggerLink, magicCell, summaryCell, key)
    {
        this._benchmarks = new Map();
        this._statusCell = statusCell;
        this._triggerCell = triggerCell;
        this._triggerLink = triggerLink;
        this._magicCell = magicCell;
        this._summary = new Stats(summaryCell, "summary");
        this._key = key;
        if (isInBrowser)
            window[key] = this;
    }
    
    addBenchmark(benchmark)
    {
        this._benchmarks.set(benchmark, new Results(benchmark));
    }
    
    readyTrigger() 
    {
        if (isInBrowser) {
            this._triggerCell.addEventListener('click', this._triggerLink);
            this._triggerCell.classList.add('ready');
        }
    }
    
    disableTrigger() 
    {
        if (isInBrowser) {
            this._triggerCell.removeEventListener('click', this._triggerLink);
            this._triggerCell.classList.remove('ready');
        }
    }
    
    start(numIterations)
    {
        this.disableTrigger();
        this._updateIterations();

        this._summary.reset();
        for (let [benchmark, results] of this._benchmarks)
            results.reset();
        this._isRunning = true;
        this._startIterations = this._numIterations = numIterations;
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
        if (isInBrowser)
            console.log("Error encountered: ", ...args);

        this._benchmarks.get(this._benchmark).reportError(...args);

        if (isInBrowser) {
            this._statusCell.innerHTML = "Test failure \u2014 error in console";
            this._statusCell.classList.add("failed");
        } else
            print("Test failure");
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
                if (isInBrowser) {
                    this._statusCell.innerHTML = "Restart";
                    this.readyTrigger();
                } else
                    print("Success! Benchmark is now finished.");
                return;
            }
            this._numIterations--;
            this._updateIterations();
            this._iterator = this._benchmarks.keys();
            this._benchmark = this._iterator.next().value;
        }
        
        this._benchmarks.get(this._benchmark).reportRunning();
        
        let benchmark = this._benchmark;
        if (isInBrowser) {
            window.setTimeout(() => {
                if (!this._isRunning)
                    return;
                
                this._magicCell.contentDocument.body.textContent = "";
                this._magicCell.contentDocument.body.innerHTML = "<iframe id=\"magicFrame\" frameborder=\"0\">";
                
                let magicFrame = this._magicCell.contentDocument.getElementById("magicFrame");
                magicFrame.contentDocument.open();
                magicFrame.contentDocument.write(
                    `<!DOCTYPE html><head><title>benchmark payload</title></head><body><script>` +
                    `window.onerror = top.${this._key}.reportError.bind(top.${this._key});\n` +
                    `function reportResult()\n` +
                    `{\n` +
                    `    var driver = top.${this._key};\n` +
                    `    driver.reportResult.apply(driver, arguments);\n` +
                    `}\n` +
                    `</script>\n` +
                    `${this._benchmark.code}</body></html>`);
            }, 100);
        } else {
            Promise.resolve(20).then(() => {
                if (!this._isRunning)
                    return;
                
                try {
                    print(`Running... ${this._benchmark.name} ( ${this._numIterations + 1}  to go)`);
                    benchmark.run();
                    print("\n");
                } catch(e) {
                    print(e);
                    print(e.stack);
                }
            });
        }
    }
    
    _updateIterations()
    {
        if (isInBrowser) {
            this._statusCell.innerHTML = "Running Tests... " + ( this._startIterations - this._numIterations ) + "/" + this._startIterations;
        }
        
    }
}
