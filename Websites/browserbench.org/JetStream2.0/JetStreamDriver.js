"use strict";

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

const preloadResources = !isInBrowser;
const measureTotalTimeAsSubtest = false; // Once we move to preloading all resources, it would be good to turn this on.

if (typeof RAMification === "undefined")
    var RAMification = false;

if (typeof testIterationCount === "undefined")
    var testIterationCount = undefined;

// Used for the promise representing the current benchmark run.
this.currentResolve = null;
this.currentReject = null;

const defaultIterationCount = 120;
const defaultWorstCaseCount = 4;

function assert(b, m = "") {
    if (!b)
        throw new Error("Bad assertion: " + m);
}

function firstID(benchmark) {
    return `results-cell-${benchmark.name}-first`;
}

function worst4ID(benchmark) {
    return `results-cell-${benchmark.name}-worst4`;
}

function avgID(benchmark) {
    return `results-cell-${benchmark.name}-avg`;
}

function scoreID(benchmark) {
    return `results-cell-${benchmark.name}-score`;
}

function mean(values) {
    assert(values instanceof Array);
    let sum = 0;
    for (let x of values)
        sum += x;
    return sum / values.length;
}

function geomean(values) {
    assert(values instanceof Array);
    let product = 1;
    for (let x of values)
        product *= x;
    return product ** (1 / values.length);
}

function toScore(timeValue) {
    return 5000 / timeValue;
}

function toTimeValue(score) {
    return 5000 / score;
}

function updateUI() {
    return new Promise((resolve) => {
        if (isInBrowser)
            requestAnimationFrame(() => setTimeout(resolve, 0));
        else
            resolve();
    });
}

function uiFriendlyNumber(num) {
    if (Number.isInteger(num))
        return num;
    return num.toFixed(3);
}

function uiFriendlyDuration(time)
{
    let minutes = time.getMinutes();
    let seconds = time.getSeconds();
    let milliSeconds = time.getMilliseconds();
    let result = "" + minutes + ":";

    result = result + (seconds < 10 ? "0" : "") + seconds + ".";
    result = result + (milliSeconds < 10 ? "00" : (milliSeconds < 100 ? "0" : "")) + milliSeconds;

    return result;
}

const fileLoader = (function() {
    class Loader {
        constructor() {
            this.requests = new Map;
        }

        async _loadInternal(url) {
            if (!isInBrowser)
                return Promise.resolve(readFile(url));

            let fetchResponse = await fetch(new Request(url));
            if (url.indexOf(".js") !== -1)
                return await fetchResponse.text();
            else if (url.indexOf(".wasm") !== -1)
                return await fetchResponse.arrayBuffer();

            throw new Error("should not be reached!");
        }

        async load(url) {
            if (this.requests.has(url))
                return (await this.requests.get(url));

            let promise = this._loadInternal(url);
            this.requests.set(url, promise);
            return (await promise);
        }
    }
    return new Loader;
})();

class Driver {
    constructor() {
        this.benchmarks = [];
    }

    addPlan(plan, BenchmarkClass = DefaultBenchmark) {
        this.benchmarks.push(new BenchmarkClass(plan));
    }

    async start() {
        let statusElement = false;
        let summaryElement = false;
        if (isInBrowser) {
            statusElement = document.getElementById("status");
            summaryElement = document.getElementById("result-summary");
            statusElement.innerHTML = `<label>Running...</label>`;
        } else {
            console.log("Starting JetStream2");
        }

        await updateUI();

        let start = Date.now();
        for (let benchmark of this.benchmarks) {
            benchmark.updateUIBeforeRun();

            await updateUI();

            try {

                await benchmark.run();
            } catch(e) {
                JetStream.reportError(benchmark);
                throw e;
            }

            benchmark.updateUIAfterRun();
        }

        let totalTime = Date.now() - start;
        if (measureTotalTimeAsSubtest) {
            if (isInBrowser)
                document.getElementById("benchmark-total-time-score").innerHTML = uiFriendlyNumber(totalTime);
            else
                console.log("Total time:", uiFriendlyNumber(totalTime));
            allScores.push(totalTime);
        }

        let allScores = [];
        for (let benchmark of this.benchmarks)
            allScores.push(benchmark.score);

        if (isInBrowser) {
            summaryElement.classList.add('done');
            summaryElement.innerHTML = "<div class=\"score\">" + uiFriendlyNumber(geomean(allScores)) + "</div><label>Score</label>";
            statusElement.innerHTML = '';
        } else
            console.log("\nTotal Score: ", uiFriendlyNumber(geomean(allScores)), "\n");

        this.reportScoreToRunBenchmarkRunner();
    }

    runCode(string)
    {
        if (!isInBrowser) {
            let scripts = string;
            let globalObject = runString("");
            globalObject.console = {log:globalObject.print}
            globalObject.top = {
                currentResolve,
                currentReject
            };
            for (let script of scripts)
                globalObject.loadString(script);
            return globalObject;
        }

        var magic = document.getElementById("magic");
        magic.contentDocument.body.textContent = "";
        magic.contentDocument.body.innerHTML = "<iframe id=\"magicframe\" frameborder=\"0\">";

        var magicFrame = magic.contentDocument.getElementById("magicframe");
        magicFrame.contentDocument.open();
        magicFrame.contentDocument.write("<!DOCTYPE html><head><title>benchmark payload</title></head><body>\n" + string + "</body></html>");

        return magicFrame;
    }

    prepareToRun()
    {
        this.benchmarks.sort((a, b) => a.plan.name.toLowerCase() < b.plan.name.toLowerCase() ? 1 : -1);

        let text = "";
        let newBenchmarks = [];
        for (let benchmark of this.benchmarks) {
            let id = JSON.stringify(benchmark.constructor.scoreDescription());
            let description = JSON.parse(id);

            newBenchmarks.push(benchmark);
            let scoreIds = benchmark.scoreIdentifiers()
            let overallScoreId = scoreIds.pop();

            if (isInBrowser) {
                text +=
                    `<div class="benchmark" id="benchmark-${benchmark.name}">
                    <h3 class="benchmark-name"><a href="in-depth.html#${benchmark.name}">${benchmark.name}</a></h3>
                    <h4 class="score" id="${overallScoreId}">___</h4><p>`;
                for (let i = 0; i < scoreIds.length; i++) {
                    let id = scoreIds[i];
                    let label = description[i];
                    text += `<span class="result"><span id="${id}">___</span><label>${label}</label></span>`
                }
                text += `</p></div>`;
            }
        }

        if (!isInBrowser)
            return;

        for (let f = 0; f < 5; f++)
            text += `<div class="benchmark fill"></div>`;

        let timestamp = Date.now();
        document.getElementById('jetstreams').style.backgroundImage = `url('jetstreams.svg?${timestamp}')`;
        let resultsTable = document.getElementById("results");
        resultsTable.innerHTML = text;

        document.getElementById("magic").textContent = "";
        document.addEventListener('keypress', function (e) {
            if (e.which === 13)
                JetStream.start();
        });
    }

    reportError(benchmark)
    {
        for (let id of benchmark.scoreIdentifiers())
            document.getElementById(id).innerHTML = "error";
    }

    async initialize() {
        await this.fetchResources();
        this.prepareToRun();
        if (isInBrowser && window.location.search == '?report=true') {
            setTimeout(() => this.start(), 4000);
        }
    }

    async fetchResources() {
        for (let benchmark of this.benchmarks)
            await benchmark.fetchResources();

        if (!isInBrowser)
            return;

        let statusElement = document.getElementById("status");
        statusElement.classList.remove('loading');
        statusElement.innerHTML = `<a href="javascript:JetStream.start()" class="button">Start Test</a>`;
        statusElement.onclick = () => {
            statusElement.onclick = null;
            JetStream.start();
            return false;
        }
    }

    async reportScoreToRunBenchmarkRunner()
    {
        if (!isInBrowser)
            return;

        if (window.location.search !== '?report=true')
            return;

        let results = {};
        for (let benchmark of this.benchmarks) {
            const subResults = {}
            const subTimes = benchmark.subTimes();
            for (const name in subTimes) {
                subResults[name] = {"metrics": {"Time": {"current": [toTimeValue(subTimes[name])]}}};
            }
            results[benchmark.name] = {
                "metrics" : {
                    "Score" : {"current" : [benchmark.score]},
                    "Time": ["Geometric"],
                },
                "tests": subResults,
            };;
        }

        results = {"JetStream2.0": {"metrics" : {"Score" : ["Geometric"]}, "tests" : results}};

        const content = JSON.stringify(results);
        await fetch("/report", {
            method: "POST",
            heeaders: {
                "Content-Type": "application/json",
                "Content-Length": content.length,
                "Connection": "close",
            },
            body: content,
        });
    }
};

class Benchmark {
    constructor(plan)
    {
        this.plan = plan;
        this.iterations = testIterationCount || plan.iterations || defaultIterationCount;
        this.isAsync = !!plan.isAsync;

        this.scripts = null;

        this._resourcesPromise = null;
        this.fetchResources();
    }

    get name() { return this.plan.name; }

    get runnerCode() {
        return `
            let __benchmark = new Benchmark(${this.iterations});
            let results = [];
            for (let i = 0; i < ${this.iterations}; i++) {
                if (__benchmark.prepareForNextIteration)
                    __benchmark.prepareForNextIteration();

                let start = Date.now();
                __benchmark.runIteration();
                let end = Date.now();

                results.push(Math.max(1, end - start));
            }
            if (__benchmark.validate)
                __benchmark.validate();
            top.currentResolve(results);`;
    }

    processResults() {
        throw new Error("Subclasses need to implement this");
    }

    get score() {
        throw new Error("Subclasses need to implement this");
    }

    get prerunCode() { return null; }

    async run() {
        let code;
        if (isInBrowser)
            code = "";
        else
            code = [];

        let addScript = (text) => {
            if (isInBrowser)
                code += `<script>${text}</script>`;
            else
                code.push(text);
        };

        let addScriptWithURL = (url) => {
            if (isInBrowser)
                code += `<script src="${url}"></script>`;
            else
                assert(false, "Should not reach here in CLI");
        };

        addScript(`const isInBrowser = ${isInBrowser}; let performance = {now: Date.now.bind(Date)};`);

        if (!!this.plan.deterministicRandom) {
            addScript(`
                Math.random = (function() {
                    var seed = 49734321;
                    return function() {
                        // Robert Jenkins' 32 bit integer hash function.
                        seed = ((seed + 0x7ed55d16) + (seed << 12))  & 0xffffffff;
                        seed = ((seed ^ 0xc761c23c) ^ (seed >>> 19)) & 0xffffffff;
                        seed = ((seed + 0x165667b1) + (seed << 5))   & 0xffffffff;
                        seed = ((seed + 0xd3a2646c) ^ (seed << 9))   & 0xffffffff;
                        seed = ((seed + 0xfd7046c5) + (seed << 3))   & 0xffffffff;
                        seed = ((seed ^ 0xb55a4f09) ^ (seed >>> 16)) & 0xffffffff;
                        return (seed & 0xfffffff) / 0x10000000;
                    };
                })();
            `);

        }

        if (this.plan.preload) {
            let str = "";
            for (let [variableName, blobUrl] of this.preloads)
                str += `const ${variableName} = "${blobUrl}";\n`;
            addScript(str);
        }

        let prerunCode = this.prerunCode;
        if (prerunCode)
            addScript(prerunCode);

        if (preloadResources) {
            assert(this.scripts && this.scripts.length === this.plan.files.length);

            for (let text of this.scripts)
                addScript(text);
        } else {
            for (let file of this.plan.files)
                addScriptWithURL(file);
        }

        let promise = new Promise((resolve, reject) => {
            currentResolve = resolve;
            currentReject = reject;
        });

        if (isInBrowser) {
            code = `
                <script> window.onerror = top.currentReject; </script>
                ${code}
            `;
        }
        addScript(this.runnerCode);

        this.startTime = new Date();

        if (RAMification)
            resetMemoryPeak();

        let magicFrame;
        try {
            magicFrame = JetStream.runCode(code);
        } catch(e) {
            console.log("Error in runCode: ", e);
            throw e;
        }
        let results = await promise;

        this.endTime = new Date();

        if (RAMification) {
            let memoryFootprint = MemoryFootprint();
            this.currentFootprint = memoryFootprint.current;
            this.peakFootprint = memoryFootprint.peak;
        }

        this.processResults(results);
        if (isInBrowser)
            magicFrame.contentDocument.close();
    }

    fetchResources() {
        if (this._resourcesPromise)
            return this._resourcesPromise;

        let filePromises = preloadResources ? this.plan.files.map((file) => fileLoader.load(file)) : [];
        let preloads = [];
        let preloadVariableNames = [];

        if (isInBrowser && this.plan.preload) {
            for (let prop of Object.getOwnPropertyNames(this.plan.preload)) {
                preloadVariableNames.push(prop);
                preloads.push(this.plan.preload[prop]);
            }
        }

        preloads = preloads.map((file) => fileLoader.load(file));

        let p1 = Promise.all(filePromises).then((texts) => {
            if (!preloadResources)
                return;
            this.scripts = [];
            assert(texts.length === this.plan.files.length);
            for (let text of texts)
                this.scripts.push(text);
        });

        let p2 = Promise.all(preloads).then((data) => {
            this.preloads = [];
            this.blobs = [];
            for (let i = 0; i < data.length; ++i) {
                let item = data[i];

                let blob;
                if (typeof item === "string") {
                    blob = new Blob([item], {type : 'application/javascript'});
                } else if (item instanceof ArrayBuffer) {
                    blob = new Blob([item], {type : 'application/octet-stream'});
                } else
                    throw new Error("Unexpected item!");

                this.blobs.push(blob);
                this.preloads.push([preloadVariableNames[i], URL.createObjectURL(blob)]);
            }
        });

        this._resourcesPromise = Promise.all([p1, p2]);
        return this._resourcesPromise;
    }

    static scoreDescription() { throw new Error("Must be implemented by subclasses."); }
    scoreIdentifiers() { throw new Error("Must be implemented by subclasses"); }

    updateUIBeforeRun() {
        if (!isInBrowser) {
            console.log(`Running ${this.name}:`);
            return;
        }

        let containerUI = document.getElementById("results");
        let resultsBenchmarkUI = document.getElementById(`benchmark-${this.name}`);
        containerUI.insertBefore(resultsBenchmarkUI, containerUI.firstChild);
        resultsBenchmarkUI.classList.add("benchmark-running");

        for (let id of this.scoreIdentifiers())
            document.getElementById(id).innerHTML = "...";
    }

    updateUIAfterRun() {
        if (!isInBrowser)
            return;

        let benchmarkResultsUI = document.getElementById(`benchmark-${this.name}`);
        benchmarkResultsUI.classList.remove("benchmark-running");
        benchmarkResultsUI.classList.add("benchmark-done");

    }
};

class DefaultBenchmark extends Benchmark {
    constructor(...args) {
        super(...args);

        this.worstCaseCount = this.plan.worstCaseCount || defaultWorstCaseCount;
        this.firstIteration = null;
        this.worst4 = null;
        this.average = null;
    }

    processResults(results) {
        function copyArray(a) {
            let result = [];
            for (let x of a)
                result.push(x);
            return result;
        }
        results = copyArray(results);

        this.firstIteration = toScore(results[0]);

        results = results.slice(1);
        results.sort((a, b) => a < b ? 1 : -1);
        for (let i = 0; i + 1 < results.length; ++i)
            assert(results[i] >= results[i + 1]);

        let worstCase = [];
        for (let i = 0; i < this.worstCaseCount; ++i)
            worstCase.push(results[i]);
        this.worst4 = toScore(mean(worstCase));
        this.average = toScore(mean(results));
    }

    get score() {
        return geomean([this.firstIteration, this.worst4, this.average]);
    }

    subTimes() {
        return {
            "First": this.firstIteration,
            "Worst": this.worst4,
            "Average": this.average,
        };
    }

    static scoreDescription() {
        return ["First", "Worst", "Average", "Score"];
    }

    scoreIdentifiers() {
        return [firstID(this), worst4ID(this), avgID(this), scoreID(this)];
    }

    updateUIAfterRun() {
        super.updateUIAfterRun();

        if (isInBrowser) {
            document.getElementById(firstID(this)).innerHTML = uiFriendlyNumber(this.firstIteration);
            document.getElementById(worst4ID(this)).innerHTML = uiFriendlyNumber(this.worst4);
            document.getElementById(avgID(this)).innerHTML = uiFriendlyNumber(this.average);
            document.getElementById(scoreID(this)).innerHTML = uiFriendlyNumber(this.score);
            return;
        }

        print("    Startup:", uiFriendlyNumber(this.firstIteration));
        print("    Worst Case:", uiFriendlyNumber(this.worst4));
        print("    Average:", uiFriendlyNumber(this.average));
        print("    Score:", uiFriendlyNumber(this.score));
        if (RAMification) {
            print("    Current Footprint:", uiFriendlyNumber(this.currentFootprint));
            print("    Peak Footprint:", uiFriendlyNumber(this.peakFootprint));
        }
        print("    Wall time:", uiFriendlyDuration(new Date(this.endTime - this.startTime)));
    }
}

class AsyncBenchmark extends DefaultBenchmark {
    get runnerCode() {
        return `
        async function doRun() {
            let __benchmark = new Benchmark();
            let results = [];
            for (let i = 0; i < ${this.iterations}; i++) {
                let start = Date.now();
                await __benchmark.runIteration();
                let end = Date.now();
                results.push(Math.max(1, end - start));
            }
            if (__benchmark.validate)
                __benchmark.validate();
            top.currentResolve(results);
        }
        doRun();`
    }
};

class WSLBenchmark extends Benchmark {
    constructor(...args) {
        super(...args);

        this.stdlib = null;
        this.mainRun = null;
    }

    processResults(results) {
        this.stdlib = toScore(results[0]);
        this.mainRun = toScore(results[1]);
    }

    get score() {
        return geomean([this.stdlib, this.mainRun]);
    }

    get runnerCode() {
        return `
            let benchmark = new Benchmark();
            let results = [];
            {
                let start = Date.now();
                benchmark.buildStdlib();
                results.push(Date.now() - start);
            }

            {
                let start = Date.now();
                benchmark.run();
                results.push(Date.now() - start);
            }

            top.currentResolve(results);
            `;
    }

    subTimes() {
        return {
            "Stdlib": this.stdlib,
            "MainRun": this.mainRun,
        };
    }

    static scoreDescription() {
        return ["Stdlib", "MainRun", "Score"];
    }

    scoreIdentifiers() {
        return ["wsl-stdlib-score", "wsl-tests-score", "wsl-score-score"];
    }

    updateUIAfterRun() {
        super.updateUIAfterRun();

        if (isInBrowser) {
            document.getElementById("wsl-stdlib-score").innerHTML = uiFriendlyNumber(this.stdlib);
            document.getElementById("wsl-tests-score").innerHTML = uiFriendlyNumber(this.mainRun);
            document.getElementById("wsl-score-score").innerHTML = uiFriendlyNumber(this.score);
            return;
        }

        print("    Stdlib:", uiFriendlyNumber(this.stdlib));
        print("    Tests:", uiFriendlyNumber(this.mainRun));
        print("    Score:", uiFriendlyNumber(this.score));
        if (RAMification) {
            print("    Current Footprint:", uiFriendlyNumber(this.currentFootprint));
            print("    Peak Footprint:", uiFriendlyNumber(this.peakFootprint));
        }
        print("    Wall time:", uiFriendlyDuration(new Date(this.endTime - this.startTime)));
    }
};

class WasmBenchmark extends Benchmark {
    constructor(...args) {
        super(...args);

        this.startupTime = null;
        this.runTime = null;
    }

    processResults(results) {
        this.startupTime = toScore(results[0]);
        this.runTime = toScore(results[1]);
    }

    get score() {
        return geomean([this.startupTime, this.runTime]);
    }

    get wasmPath() {
        return this.plan.wasmPath;
    }

    get prerunCode() {
        let str = `
            let verbose = false;

            let compileTime = null;
            let runTime = null;

            let globalObject = this;

            globalObject.benchmarkTime = Date.now.bind(Date);

            globalObject.reportCompileTime = (t) => {
                if (compileTime !== null)
                    throw new Error("called report compile time twice");
                compileTime = t;
            };

            globalObject.reportRunTime = (t) => {
                if (runTime !== null)
                    throw new Error("called report run time twice")
                runTime = t;
                top.currentResolve([compileTime, runTime]);
            };

            abort = quit = function() {
                if (verbose)
                    console.log('Intercepted quit/abort');
            };

            oldPrint = globalObject.print;
            globalObject.print = globalObject.printErr = (...args) => {
                if (verbose)
                    console.log('Intercepted print: ', ...args);
            };

            let Module = {
                preRun: [],
                postRun: [],
                print: function() { },
                printErr: function() { },
                setStatus: function(text) {
                },
                totalDependencies: 0,
                monitorRunDependencies: function(left) {
                    this.totalDependencies = Math.max(this.totalDependencies, left);
                    Module.setStatus(left ? 'Preparing... (' + (this.totalDependencies-left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
                }
            };
            globalObject.Module = Module;
            `;
        return str;
    }

    get runnerCode() {
        let str = "";
        if (isInBrowser) {
            str += `
                var xhr = new XMLHttpRequest();
                xhr.open('GET', wasmBlobURL, true);
                xhr.responseType = 'arraybuffer';
                xhr.onload = function() {
                    Module.wasmBinary = xhr.response;
                    doRun();
                };
                xhr.send(null);
            `;
        } else {
            str += `
            Module.wasmBinary = read("${this.wasmPath}", "binary");
            globalObject.read = (...args) => {
                console.log("should not be inside read: ", ...args);
                throw new Error;
            };

            Module.setStatus = null;
            Module.monitorRunDependencies = null;

            Promise.resolve(42).then(() => {
                try {
                    doRun();
                } catch(e) {
                    console.log("error running wasm:", e);
                    throw e;
                }
            })
            `;
        }
        return str;
    }

    subTimes() {
        return {
            "Startup": this.startupTime,
            "Runtime": this.runTime,
        };
    }

    static scoreDescription() {
        return ["Startup", "Runtime", "Score"];
    }

    get startupID() {
        return `wasm-startup-id${this.name}`;
    }
    get runID() {
        return `wasm-run-id${this.name}`;
    }
    get scoreID() {
        return `wasm-score-id${this.name}`;
    }

    scoreIdentifiers() {
        return [this.startupID, this.runID, this.scoreID];
    }

    updateUIAfterRun() {
        super.updateUIAfterRun();

        if (isInBrowser) {
            document.getElementById(this.startupID).innerHTML = uiFriendlyNumber(this.startupTime);
            document.getElementById(this.runID).innerHTML = uiFriendlyNumber(this.runTime);
            document.getElementById(this.scoreID).innerHTML = uiFriendlyNumber(this.score);
            return;
        }
        print("    Startup:", uiFriendlyNumber(this.startupTime));
        print("    Run time:", uiFriendlyNumber(this.runTime));
        if (RAMification) {
            print("    Current Footprint:", uiFriendlyNumber(this.currentFootprint));
            print("    Peak Footprint:", uiFriendlyNumber(this.peakFootprint));
        }
        print("    Score:", uiFriendlyNumber(this.score));
    }
};

const ARESGroup = Symbol.for("ARES");
const CDJSGroup = Symbol.for("CDJS");
const CodeLoadGroup = Symbol.for("CodeLoad");
const LuaJSFightGroup = Symbol.for("LuaJSFight");
const OctaneGroup = Symbol.for("Octane");
const RexBenchGroup = Symbol.for("RexBench");
const SeaMonsterGroup = Symbol.for("SeaMonster");
const SimpleGroup = Symbol.for("Simple");
const SunSpiderGroup = Symbol.for("SunSpider");
const WasmGroup = Symbol.for("Wasm");
const WorkerTestsGroup = Symbol.for("WorkerTests");
const WSLGroup = Symbol.for("WSL");
const WTBGroup = Symbol.for("WTB");


let testPlans = [
    // ARES
    {
        name: "Air",
        files: [
            "./ARES-6/Air/symbols.js"
            , "./ARES-6/Air/tmp_base.js"
            , "./ARES-6/Air/arg.js"
            , "./ARES-6/Air/basic_block.js"
            , "./ARES-6/Air/code.js"
            , "./ARES-6/Air/frequented_block.js"
            , "./ARES-6/Air/inst.js"
            , "./ARES-6/Air/opcode.js"
            , "./ARES-6/Air/reg.js"
            , "./ARES-6/Air/stack_slot.js"
            , "./ARES-6/Air/tmp.js"
            , "./ARES-6/Air/util.js"
            , "./ARES-6/Air/custom.js"
            , "./ARES-6/Air/liveness.js"
            , "./ARES-6/Air/insertion_set.js"
            , "./ARES-6/Air/allocate_stack.js"
            , "./ARES-6/Air/payload-gbemu-executeIteration.js"
            , "./ARES-6/Air/payload-imaging-gaussian-blur-gaussianBlur.js"
            , "./ARES-6/Air/payload-airjs-ACLj8C.js"
            , "./ARES-6/Air/payload-typescript-scanIdentifier.js"
            , "./ARES-6/Air/benchmark.js"
        ],
        testGroup: ARESGroup
    },
    {
        name: "Basic",
        files: [
            "./ARES-6/Basic/ast.js"
            , "./ARES-6/Basic/basic.js"
            , "./ARES-6/Basic/caseless_map.js"
            , "./ARES-6/Basic/lexer.js"
            , "./ARES-6/Basic/number.js"
            , "./ARES-6/Basic/parser.js"
            , "./ARES-6/Basic/random.js"
            , "./ARES-6/Basic/state.js"
            , "./ARES-6/Basic/util.js"
            , "./ARES-6/Basic/benchmark.js"
        ],
        testGroup: ARESGroup
    },
    {
        name: "ML",
        files: [
            "./ARES-6/ml/index.js"
            , "./ARES-6/ml/benchmark.js"
        ],
        iterations: 60,
        testGroup: ARESGroup
    },
    {
        name: "Babylon",
        files: [
            "./ARES-6/Babylon/index.js"
            , "./ARES-6/Babylon/benchmark.js"
        ],
        preload: {
            airBlob: "./ARES-6/Babylon/air-blob.js",
            basicBlob: "./ARES-6/Babylon/basic-blob.js",
            inspectorBlob: "./ARES-6/Babylon/inspector-blob.js",
            babylonBlob: "./ARES-6/Babylon/babylon-blob.js"
        },
        testGroup: ARESGroup
    },
    // CDJS
    {
        name: "cdjs",
        files: [
            "./cdjs/constants.js"
            , "./cdjs/util.js"
            , "./cdjs/red_black_tree.js"
            , "./cdjs/call_sign.js"
            , "./cdjs/vector_2d.js"
            , "./cdjs/vector_3d.js"
            , "./cdjs/motion.js"
            , "./cdjs/reduce_collision_set.js"
            , "./cdjs/simulator.js"
            , "./cdjs/collision.js"
            , "./cdjs/collision_detector.js"
            , "./cdjs/benchmark.js"
        ],
        iterations: 60,
        worstCaseCount: 3,
        testGroup: CDJSGroup
    },
    // CodeLoad
    {
        name: "first-inspector-code-load",
        files: [
            "./code-load/code-first-load.js"
        ],
        preload: {
            inspectorPayloadBlob: "./code-load/inspector-payload-minified.js"
        },
        testGroup: CodeLoadGroup
    },
    {
        name: "multi-inspector-code-load",
        files: [
            "./code-load/code-multi-load.js"
        ],
        preload: {
            inspectorPayloadBlob: "./code-load/inspector-payload-minified.js"
        },
        testGroup: CodeLoadGroup
    },
    // Octane
    {
        name: "Box2D",
        files: [
            "./Octane/box2d.js"
        ],
        deterministicRandom: true,
        testGroup: OctaneGroup
    },
    {
        name: "octane-code-load",
        files: [
            "./Octane/code-first-load.js"
        ],
        deterministicRandom: true,
        testGroup: OctaneGroup
    },
    {
        name: "crypto",
        files: [
            "./Octane/crypto.js"
        ],
        deterministicRandom: true,
        testGroup: OctaneGroup
    },
    {
        name: "delta-blue",
        files: [
            "./Octane/deltablue.js"
        ],
        deterministicRandom: true,
        testGroup: OctaneGroup
    },
    {
        name: "earley-boyer",
        files: [
            "./Octane/earley-boyer.js"
        ],
        deterministicRandom: true,
        testGroup: OctaneGroup
    },
    {
        name: "gbemu",
        files: [
            "./Octane/gbemu-part1.js"
            , "./Octane/gbemu-part2.js"
        ],
        deterministicRandom: true,
        testGroup: OctaneGroup
    },
    {
        name: "mandreel",
        files: [
            "./Octane/mandreel.js"
        ],
        iterations: 80,
        deterministicRandom: true,
        testGroup: OctaneGroup
    },
    {
        name: "navier-stokes",
        files: [
            "./Octane/navier-stokes.js"
        ],
        deterministicRandom: true,
        testGroup: OctaneGroup
    },
    {
        name: "pdfjs",
        files: [
            "./Octane/pdfjs.js"
        ],
        deterministicRandom: true,
        testGroup: OctaneGroup
    },
    {
        name: "raytrace",
        files: [
            "./Octane/raytrace.js"
        ],
        deterministicRandom: true,
        testGroup: OctaneGroup
    },
    {
        name: "regexp",
        files: [
            "./Octane/regexp.js"
        ],
        deterministicRandom: true,
        testGroup: OctaneGroup
    },
    {
        name: "richards",
        files: [
            "./Octane/richards.js"
        ],
        deterministicRandom: true,
        testGroup: OctaneGroup
    },
    {
        name: "splay",
        files: [
            "./Octane/splay.js"
        ],
        deterministicRandom: true,
        testGroup: OctaneGroup
    },
    {
        name: "typescript",
        files: [
            "./Octane/typescript-compiler.js"
            , "./Octane/typescript-input.js"
            , "./Octane/typescript.js"
        ],
        iterations: 15,
        worstCaseCount: 2,
        deterministicRandom: true,
        testGroup: OctaneGroup
    },
    {
        name: "octane-zlib",
        files: [
            "./Octane/zlib-data.js"
            , "./Octane/zlib.js"
        ],
        iterations: 15,
        worstCaseCount: 2,
        deterministicRandom: true,
        testGroup: OctaneGroup
    },
    // RexBench
    {
        name: "FlightPlanner",
        files: [
            "./RexBench/FlightPlanner/airways.js"
            , "./RexBench/FlightPlanner/waypoints.js"
            , "./RexBench/FlightPlanner/flight_planner.js"
            , "./RexBench/FlightPlanner/expectations.js"
            , "./RexBench/FlightPlanner/benchmark.js"
        ],
        testGroup: RexBenchGroup
    },
    {
        name: "OfflineAssembler",
        files: [
            "./RexBench/OfflineAssembler/registers.js"
            , "./RexBench/OfflineAssembler/instructions.js"
            , "./RexBench/OfflineAssembler/ast.js"
            , "./RexBench/OfflineAssembler/parser.js"
            , "./RexBench/OfflineAssembler/file.js"
            , "./RexBench/OfflineAssembler/LowLevelInterpreter.js"
            , "./RexBench/OfflineAssembler/LowLevelInterpreter32_64.js"
            , "./RexBench/OfflineAssembler/LowLevelInterpreter64.js"
            , "./RexBench/OfflineAssembler/InitBytecodes.js"
            , "./RexBench/OfflineAssembler/expected.js"
            , "./RexBench/OfflineAssembler/benchmark.js"
        ],
        iterations: 80,
        testGroup: RexBenchGroup
    },
    {
        name: "UniPoker",
        files: [
            "./RexBench/UniPoker/poker.js"
            , "./RexBench/UniPoker/expected.js"
            , "./RexBench/UniPoker/benchmark.js"
        ],
        deterministicRandom: true,
        testGroup: RexBenchGroup
    },
    // Simple
    {
        name: "async-fs",
        files: [
            "./simple/file-system.js"
        ],
        iterations: 40,
        worstCaseCount: 3,
        benchmarkClass: AsyncBenchmark,
        testGroup: SimpleGroup
    },
    {
        name: "float-mm.c",
        files: [
            "./simple/float-mm.c.js"
        ],
        iterations: 15,
        worstCaseCount: 2,
        testGroup: SimpleGroup
    },
    {
        name: "hash-map",
        files: [
            "./simple/hash-map.js"
        ],
        testGroup: SimpleGroup
    },
    // SeaMonster
    {
        name: "ai-astar",
        files: [
            "./SeaMonster/ai-astar.js"
        ],
        testGroup: SeaMonsterGroup
    },
    {
        name: "gaussian-blur",
        files: [
            "./SeaMonster/gaussian-blur.js"
        ],
        testGroup: SeaMonsterGroup
    },
    {
        name: "stanford-crypto-aes",
        files: [
            "./SeaMonster/sjlc.js"
            , "./SeaMonster/stanford-crypto-aes.js"
        ],
        testGroup: SeaMonsterGroup
    },
    {
        name: "stanford-crypto-pbkdf2",
        files: [
            "./SeaMonster/sjlc.js"
            , "./SeaMonster/stanford-crypto-pbkdf2.js"
        ],
        testGroup: SeaMonsterGroup
    },
    {
        name: "stanford-crypto-sha256",
        files: [
            "./SeaMonster/sjlc.js"
            , "./SeaMonster/stanford-crypto-sha256.js"
        ],
        testGroup: SeaMonsterGroup
    },
    {
        name: "json-stringify-inspector",
        files: [
            "./SeaMonster/inspector-json-payload.js"
            , "./SeaMonster/json-stringify-inspector.js"
        ],
        iterations: 20,
        worstCaseCount: 2,
        testGroup: SeaMonsterGroup
    },
    {
        name: "json-parse-inspector",
        files: [
            "./SeaMonster/inspector-json-payload.js"
            , "./SeaMonster/json-parse-inspector.js"
        ],
        iterations: 20,
        worstCaseCount: 2,
        testGroup: SeaMonsterGroup
    },
    // Wasm
    {
        name: "HashSet-wasm",
        wasmPath: "./wasm/HashSet.wasm",
        files: [
            "./wasm/HashSet.js"
        ],
        preload: {
            wasmBlobURL: "./wasm/HashSet.wasm"
        },
        benchmarkClass: WasmBenchmark,
        testGroup: WasmGroup
    },
    {
        name: "tsf-wasm",
        wasmPath: "./wasm/tsf.wasm",
        files: [
            "./wasm/tsf.js"
        ],
        preload: {
            wasmBlobURL: "./wasm/tsf.wasm"
        },
        benchmarkClass: WasmBenchmark,
        testGroup: WasmGroup
    },
    {
        name: "quicksort-wasm",
        wasmPath: "./wasm/quicksort.wasm",
        files: [
            "./wasm/quicksort.js"
        ],
        preload: {
            wasmBlobURL: "./wasm/quicksort.wasm"
        },
        benchmarkClass: WasmBenchmark,
        testGroup: WasmGroup
    },
    {
        name: "gcc-loops-wasm",
        wasmPath: "./wasm/gcc-loops.wasm",
        files: [
            "./wasm/gcc-loops.js"
        ],
        preload: {
            wasmBlobURL: "./wasm/gcc-loops.wasm"
        },
        benchmarkClass: WasmBenchmark,
        testGroup: WasmGroup
    },
    {
        name: "richards-wasm",
        wasmPath: "./wasm/richards.wasm",
        files: [
            "./wasm/richards.js"
        ],
        preload: {
            wasmBlobURL: "./wasm/richards.wasm"
        },
        benchmarkClass: WasmBenchmark,
        testGroup: WasmGroup
    },
    // WorkerTests
    {
        name: "bomb-workers",
        files: [
            "./worker/bomb.js"
        ],
        iterations: 80,
        preload: {
            rayTrace3D: "./worker/bomb-subtests/3d-raytrace.js"
            , accessNbody: "./worker/bomb-subtests/access-nbody.js"
            , morph3D: "./worker/bomb-subtests/3d-morph.js"
            , cube3D: "./worker/bomb-subtests/3d-cube.js"
            , accessFunnkuch: "./worker/bomb-subtests/access-fannkuch.js"
            , accessBinaryTrees: "./worker/bomb-subtests/access-binary-trees.js"
            , accessNsieve: "./worker/bomb-subtests/access-nsieve.js"
            , bitopsBitwiseAnd: "./worker/bomb-subtests/bitops-bitwise-and.js"
            , bitopsNsieveBits: "./worker/bomb-subtests/bitops-nsieve-bits.js"
            , controlflowRecursive: "./worker/bomb-subtests/controlflow-recursive.js"
            , bitops3BitBitsInByte: "./worker/bomb-subtests/bitops-3bit-bits-in-byte.js"
            , botopsBitsInByte: "./worker/bomb-subtests/bitops-bits-in-byte.js"
            , cryptoAES: "./worker/bomb-subtests/crypto-aes.js"
            , cryptoMD5: "./worker/bomb-subtests/crypto-md5.js"
            , cryptoSHA1: "./worker/bomb-subtests/crypto-sha1.js"
            , dateFormatTofte: "./worker/bomb-subtests/date-format-tofte.js"
            , dateFormatXparb: "./worker/bomb-subtests/date-format-xparb.js"
            , mathCordic: "./worker/bomb-subtests/math-cordic.js"
            , mathPartialSums: "./worker/bomb-subtests/math-partial-sums.js"
            , mathSpectralNorm: "./worker/bomb-subtests/math-spectral-norm.js"
            , stringBase64: "./worker/bomb-subtests/string-base64.js"
            , stringFasta: "./worker/bomb-subtests/string-fasta.js"
            , stringValidateInput: "./worker/bomb-subtests/string-validate-input.js"
            , stringTagcloud: "./worker/bomb-subtests/string-tagcloud.js"
            , stringUnpackCode: "./worker/bomb-subtests/string-unpack-code.js"
            , regexpDNA: "./worker/bomb-subtests/regexp-dna.js"
        },
        benchmarkClass: AsyncBenchmark,
        testGroup: WorkerTestsGroup
    },
    {
        name: "segmentation",
        files: [
            "./worker/segmentation.js"
        ],
        preload: {
            asyncTaskBlob: "./worker/async-task.js"
        },
        iterations: 36,
        worstCaseCount: 3,
        benchmarkClass: AsyncBenchmark,
        testGroup: WorkerTestsGroup
    },
    // WSL
    {
        name: "WSL",
        files: ["./WSL/Node.js" ,"./WSL/Type.js" ,"./WSL/ReferenceType.js" ,"./WSL/Value.js" ,"./WSL/Expression.js" ,"./WSL/Rewriter.js" ,"./WSL/Visitor.js" ,"./WSL/CreateLiteral.js" ,"./WSL/CreateLiteralType.js" ,"./WSL/PropertyAccessExpression.js" ,"./WSL/AddressSpace.js" ,"./WSL/AnonymousVariable.js" ,"./WSL/ArrayRefType.js" ,"./WSL/ArrayType.js" ,"./WSL/Assignment.js" ,"./WSL/AutoWrapper.js" ,"./WSL/Block.js" ,"./WSL/BoolLiteral.js" ,"./WSL/Break.js" ,"./WSL/CallExpression.js" ,"./WSL/CallFunction.js" ,"./WSL/Check.js" ,"./WSL/CheckLiteralTypes.js" ,"./WSL/CheckLoops.js" ,"./WSL/CheckRecursiveTypes.js" ,"./WSL/CheckRecursion.js" ,"./WSL/CheckReturns.js" ,"./WSL/CheckUnreachableCode.js" ,"./WSL/CheckWrapped.js" ,"./WSL/Checker.js" ,"./WSL/CloneProgram.js" ,"./WSL/CommaExpression.js" ,"./WSL/ConstexprFolder.js" ,"./WSL/ConstexprTypeParameter.js" ,"./WSL/Continue.js" ,"./WSL/ConvertPtrToArrayRefExpression.js" ,"./WSL/DereferenceExpression.js" ,"./WSL/DoWhileLoop.js" ,"./WSL/DotExpression.js" ,"./WSL/DoubleLiteral.js" ,"./WSL/DoubleLiteralType.js" ,"./WSL/EArrayRef.js" ,"./WSL/EBuffer.js" ,"./WSL/EBufferBuilder.js" ,"./WSL/EPtr.js" ,"./WSL/EnumLiteral.js" ,"./WSL/EnumMember.js" ,"./WSL/EnumType.js" ,"./WSL/EvaluationCommon.js" ,"./WSL/Evaluator.js" ,"./WSL/ExpressionFinder.js" ,"./WSL/ExternalOrigin.js" ,"./WSL/Field.js" ,"./WSL/FindHighZombies.js" ,"./WSL/FlattenProtocolExtends.js" ,"./WSL/FlattenedStructOffsetGatherer.js" ,"./WSL/FloatLiteral.js" ,"./WSL/FloatLiteralType.js" ,"./WSL/FoldConstexprs.js" ,"./WSL/ForLoop.js" ,"./WSL/Func.js" ,"./WSL/FuncDef.js" ,"./WSL/FuncInstantiator.js" ,"./WSL/FuncParameter.js" ,"./WSL/FunctionLikeBlock.js" ,"./WSL/HighZombieFinder.js" ,"./WSL/IdentityExpression.js" ,"./WSL/IfStatement.js" ,"./WSL/IndexExpression.js" ,"./WSL/InferTypesForCall.js" ,"./WSL/Inline.js" ,"./WSL/Inliner.js" ,"./WSL/InstantiateImmediates.js" ,"./WSL/IntLiteral.js" ,"./WSL/IntLiteralType.js" ,"./WSL/Intrinsics.js" ,"./WSL/LateChecker.js" ,"./WSL/Lexer.js" ,"./WSL/LexerToken.js" ,"./WSL/LiteralTypeChecker.js" ,"./WSL/LogicalExpression.js" ,"./WSL/LogicalNot.js" ,"./WSL/LoopChecker.js" ,"./WSL/MakeArrayRefExpression.js" ,"./WSL/MakePtrExpression.js" ,"./WSL/NameContext.js" ,"./WSL/NameFinder.js" ,"./WSL/NameResolver.js" ,"./WSL/NativeFunc.js" ,"./WSL/NativeFuncInstance.js" ,"./WSL/NativeType.js" ,"./WSL/NativeTypeInstance.js" ,"./WSL/NormalUsePropertyResolver.js" ,"./WSL/NullLiteral.js" ,"./WSL/NullType.js" ,"./WSL/OriginKind.js" ,"./WSL/OverloadResolutionFailure.js" ,"./WSL/Parse.js" ,"./WSL/Prepare.js" ,"./WSL/Program.js" ,"./WSL/ProgramWithUnnecessaryThingsRemoved.js" ,"./WSL/PropertyResolver.js" ,"./WSL/Protocol.js" ,"./WSL/ProtocolDecl.js" ,"./WSL/ProtocolFuncDecl.js" ,"./WSL/ProtocolRef.js" ,"./WSL/PtrType.js" ,"./WSL/ReadModifyWriteExpression.js" ,"./WSL/RecursionChecker.js" ,"./WSL/RecursiveTypeChecker.js" ,"./WSL/ResolveNames.js" ,"./WSL/ResolveOverloadImpl.js" ,"./WSL/ResolveProperties.js" ,"./WSL/ResolveTypeDefs.js" ,"./WSL/Return.js" ,"./WSL/ReturnChecker.js" ,"./WSL/ReturnException.js" ,"./WSL/StandardLibrary.js" ,"./WSL/StatementCloner.js" ,"./WSL/StructLayoutBuilder.js" ,"./WSL/StructType.js" ,"./WSL/Substitution.js" ,"./WSL/SwitchCase.js" ,"./WSL/SwitchStatement.js" ,"./WSL/SynthesizeEnumFunctions.js" ,"./WSL/SynthesizeStructAccessors.js" ,"./WSL/TrapStatement.js" ,"./WSL/TypeDef.js" ,"./WSL/TypeDefResolver.js" ,"./WSL/TypeOrVariableRef.js" ,"./WSL/TypeParameterRewriter.js" ,"./WSL/TypeRef.js" ,"./WSL/TypeVariable.js" ,"./WSL/TypeVariableTracker.js" ,"./WSL/TypedValue.js" ,"./WSL/UintLiteral.js" ,"./WSL/UintLiteralType.js" ,"./WSL/UnificationContext.js" ,"./WSL/UnreachableCodeChecker.js" ,"./WSL/VariableDecl.js" ,"./WSL/VariableRef.js" ,"./WSL/VisitingSet.js" ,"./WSL/WSyntaxError.js" ,"./WSL/WTrapError.js" ,"./WSL/WTypeError.js" ,"./WSL/WhileLoop.js" ,"./WSL/WrapChecker.js", "./WSL/Test.js"],
        benchmarkClass: WSLBenchmark,
        testGroup: WSLGroup
    }
];

// LuaJSFight tests
let luaJSFightTests = [
    "hello_world"
    , "list_search"
    , "lists"
    , "string_lists"
];
for (let test of luaJSFightTests) {
    testPlans.push({
        name: `${test}-LJF`,
        files: [
            `./LuaJSFight/${test}.js`
        ],
        testGroup: LuaJSFightGroup
    });
}

// SunSpider tests
let sunSpiderTests = [
    "3d-cube"
    , "3d-raytrace"
    , "base64"
    , "crypto-aes"
    , "crypto-md5"
    , "crypto-sha1"
    , "date-format-tofte"
    , "date-format-xparb"
    , "n-body"
    , "regex-dna"
    , "string-unpack-code"
    , "tagcloud"
];
for (let test of sunSpiderTests) {
    testPlans.push({
        name: `${test}-SP`,
        files: [
            `./SunSpider/${test}.js`
        ],
        testGroup: SunSpiderGroup
    });
}

// WTB (Web Tooling Benchmark) tests
let WTBTests = [
    "acorn"
    , "babylon"
    , "chai"
    , "coffeescript"
    , "espree"
    , "jshint"
    , "lebab"
    , "prepack"
    , "uglify-js"
];
for (let name of WTBTests) {
    testPlans.push({
        name: `${name}-wtb`,
        files: [
            isInBrowser ? "./web-tooling-benchmark/browser.js" : "./web-tooling-benchmark/cli.js"
            , `./web-tooling-benchmark/${name}.js`
        ],
        iterations: 5,
        worstCaseCount: 1,
        testGroup: WTBGroup
    });
}


let testsByName = new Map();
let testsByGroup = new Map();

for (let plan of testPlans) {
    let testName = plan.name;

    if (testsByName.has(plan.name))
        throw "Duplicate test plan with name \"" + testName + "\"";
    else
        testsByName.set(testName, plan);

    let group = plan.testGroup;

    if (testsByGroup.has(group))
        testsByGroup.get(group).push(testName);
    else
        testsByGroup.set(group, [testName]);
}

this.JetStream = new Driver();

function addTestByName(testName)
{
    let plan = testsByName.get(testName);

    if (plan)
        JetStream.addPlan(plan, plan.benchmarkClass);
    else
        throw "Couldn't find test named \"" +  testName + "\"";
}

function addTestsByGroup(group)
{
    let testList = testsByGroup.get(group);

    if (!testList)
        throw "Couldn't find test group named: \"" + Symbol.keyFor(group) + "\"";

    for (let testName of testList)
        addTestByName(testName);
}

function processTestList(testList)
{
    let tests = [];

    if (testList instanceof Array)
        tests = testList;
    else
        tests = testList.split(/[\s,]/);

    for (let testName of tests) {
        let groupTest = testsByGroup.get(Symbol.for(testName));

        if (groupTest) {
            for (let testName of groupTest)
                addTestByName(testName);
        } else
            addTestByName(testName);
    }
}

let runOctane = true;
let runARES = true;
let runWSL = true;
let runRexBench = true;
let runWTB = true;
let runSunSpider = true;
let runSimple = true;
let runCDJS = true;
let runWorkerTests = !!isInBrowser;
let runSeaMonster = true;
let runCodeLoad = true;
let runWasm = true;

if (false) {
    runOctane = false;
    runARES = false;
    runWSL = false;
    runRexBench = false;
    runWTB = false;
    runSunSpider = false;
    runSimple = false;
    runCDJS = false;
    runWorkerTests = false;
    runSeaMonster = false;
    runCodeLoad = false;
    runWasm = false;
}

if (typeof testList !== "undefined") {
    processTestList(testList);
} else {
    if (runARES)
        addTestsByGroup(ARESGroup);

    if (runCDJS)
        addTestsByGroup(CDJSGroup);

    if (runCodeLoad)
        addTestsByGroup(CodeLoadGroup);

    if (runOctane)
        addTestsByGroup(OctaneGroup);

    if (runRexBench)
        addTestsByGroup(RexBenchGroup);

    if (runSeaMonster)
        addTestsByGroup(SeaMonsterGroup);

    if (runSimple)
        addTestsByGroup(SimpleGroup);

    if (runSunSpider)
        addTestsByGroup(SunSpiderGroup);

    if (runWasm)
        addTestsByGroup(WasmGroup);

    if (runWorkerTests)
        addTestsByGroup(WorkerTestsGroup);

    if (runWSL)
        addTestsByGroup(WSLGroup);

    if (runWTB)
        addTestsByGroup(WTBGroup);
}
