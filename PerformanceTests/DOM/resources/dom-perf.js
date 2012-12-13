/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// A framework for running the benchmark suites and computing a score based
// on timing measurements.
//
// Time measurements are computed by summing individual measurements in a
// test's run() function.  Because Javascript generally has a 1ms timer
// granularity, tests should be careful to take at least 2ms to run.  Tests
// which run in less than 1ms are invalid.  Some older browsers have less
// timer resolution, as low as 15ms.  On these systems tests which take less
// than 15ms to run are invalid.
//
// Scores for a benchmark suite are calculated as the geometric mean across
// all benchmarks which comprise that suite.
//
// Overall score is calculated as the geometric mean across all benchmark
// suites.

// Benchmark Object.
// A benchmark is a test which can be run as part of a BenchmarkSuite.
//
// Each test provides the following properties:
//    name
//      The name of the benchmark.
//    run
//      The work function which is measured
//    opt_setup (optional)
//      A function which is run before each benchmark iteration.
//    opt_cleanup (optional)
//      A function to cleanup any side-effects of a benchmark.
//    opt_shareSetup (optional)
//      A flag indicating whether the setup function should be run once or with
//      each benchmark iteration.  (default is false)
//

// Add each method to Arrays, to allow easy iteration over elements
if(!Array.prototype.forEach) {
    // forEach's calling syntax is:
    //   [].forEach(func, scope);
    // registered callbacks always get 3 args:
    //   [].forEach(function(item, index, fullArray){});
    Array.prototype.forEach = function(callback, scope) {
        callback = hitch(scope, callback);
        for (var i = 0, len = this.length; i < len; i++)
            callback(this[i], i, this);
    };
}

byId = function(id, doc) {
    if (typeof id == "string")
        return (doc||document).getElementById(id);
    return id;
};

// A utility object for measuring time intervals.
function Interval() {
  var start_ = 0;
  var stop_ = 0;
  this.start = function() { start_ = PerfTestRunner.now(); };
  this.stop = function() { stop_ = PerfTestRunner.now(); };
  this.microseconds = function() { return (stop_ - start_) * 1000; };
}

// A stub profiler object.
function PseudoProfiler() {}
PseudoProfiler.prototype.start = function() {};
PseudoProfiler.prototype.stop = function() {};

var chromiumProfilerInitOnce = false;   // Chromium profiler initialization.

// Cross-platform function to get the profiler object.
// For chromium, returns a Profiler object which can be used during the run.
// For other browsers, returns a PseudoProfiler object.
function GetProfiler() {
    if (window.chromium && window.chromium.Profiler) {
        var profiler = new window.chromium.Profiler();
        if (!chromiumProfilerInitOnce) {
            chromiumProfilerInitOnce = true;
            profiler.stop();
            if (profiler.clear)
                profiler.clear();
            if (profiler.setThreadName)
                profiler.setThreadName("domperf javascript");
        }
        return profiler;
    }
    return new PseudoProfiler();
}

function Benchmark(name, run, opt_setup, opt_cleanup, opt_shareSetup) {
    this.name = name;
    this.timeToRun = 100; // ms.
    // Tests like DOM/DOMWalk.html are too fast and need to be ran multiple times.
    // 100 ms was chosen since it was long enough to make DOMWalk and other fast tests stable
    // but was short enough not to make other slow tests run multiple times.
    this.run = run;
    this.setup = opt_setup;
    this.cleanup = opt_cleanup;
    this.shareSetup = opt_shareSetup;
}

// BenchmarkSuite
// A group of benchmarks that can be run together.
function BenchmarkSuite(name, benchmarks) {
    this.name = name;
    this.benchmarks = benchmarks;
    for (var i = 0; i < this.benchmarks.length; i++)
        this.benchmarks[i].suite = this;
    this.suiteFile = this.currentSuiteFile;
}

// This computes the amount of overhead is associated with the call to the test
// function and getting the date. 
BenchmarkSuite.start = PerfTestRunner.now();

BenchmarkSuite.Math = new (function() {
    // Computes the geometric mean of a set of numbers.
    // nulls in numbers will be ignored
    // minNumber is optional (defaults to 0.001) anything smaller than this
    // will be changed to this value, eliminating infinite results
    // mapFn is an optional arg that will be used as a map function on numbers
    this.GeometricMean = function(numbers, minNumber, mapFn) {
        if (mapFn)
            numbers = dojo.map(numbers, mapFn);
        var log = 0;
        var nNumbers = 0;
        for (var i = 0, n = numbers.length; i < n; i++) {
            var number = numbers[i];
            if (number) {
                if (number < minNumber)
                    number = minNumber;
                nNumbers++;
                log += Math.log(number);
            }
        }
        return Math.pow(Math.E, log / nNumbers);
    };

    // Compares two objects using built-in JavaScript operators.
    this.ascend = function(a, b) {
        if (a < b)
          return -1;
        else if (a > b)
          return 1;
        return 0;
    };
});

// Benchmark results hold the benchmark and the measured time used to run the
// benchmark. The benchmark score is computed later once a full benchmark suite
// has run to completion.
function BenchmarkResult(benchmark, times, error, benchmarkContent) {
    this.benchmark = benchmark;
    this.error = error;
    this.times = times;

    this.countNodes = function(parent) {
        var nDescendants = 0;
        for (var child = parent.firstChild; child; child = child.nextSibling)
            nDescendants += countNodes(child);
        return nDescendants + 1;
    };

    if (benchmarkContent) {
        var nNodes = countNodes(benchmarkContent) - 1;
        if (nNodes > 0) {
            this.html = benchmarkContent.innerHTML;
            this.nNodes = nNodes;
        }
    }
    if (!error) {
        var statistics = PerfTestRunner.computeStatistics(times);
        this.min = statistics.min;
        this.max = statistics.max;
        this.median = statistics.median;
        this.mean = statistics.mean;
        this.sum = statistics.sum;
        this.variance = statistics.variance;
        this.stdev = statistics.stdev;
    }

    // Convert results to numbers. Used by the geometric mean computation.
    this.valueOf = function() { return this.time; };
}

// Runs a single benchmark and computes the average time it takes to run a
// single iteration.
BenchmarkSuite.prototype.RunSingle = function(benchmark, times) {
    var elapsed = 0;
    var start = PerfTestRunner.now();
    var runInterval = new Interval();
    var setupReturn = null;
    var runReturn = null;
    var time;
    var totalTime = 0;
    var nZeros = 0;
    var error = null;
    var profiler = GetProfiler();

    for (var n = 0; !error && totalTime < benchmark.timeToRun;  n++) {
        if (this.benchmarkContent)
            this.benchmarkContentHolder.removeChild(this.benchmarkContent);
        this.benchmarkContent = this.benchmarkContentProto.cloneNode();
        this.benchmarkContentHolder.appendChild(this.benchmarkContent);
        PerfTestRunner.gc();

        try {
            if (benchmark.setup) {
                if (!setupReturn || !benchmark.shareSetup)
                    setupReturn = benchmark.setup();
            }

            profiler.start();
            runInterval.start();
            runReturn = benchmark.run(setupReturn);
            runInterval.stop();
            profiler.stop();
            time = runInterval.microseconds() / 1000;
            if (time > 0) {
                times.push(time);
                elapsed += time;
            } else
                times.push(0);
            if (benchmark.cleanup)
                benchmark.cleanup(runReturn);
        } catch (e) {
            error = e;
        }
        totalTime = PerfTestRunner.now() - start;
  }

    var result = new BenchmarkResult(benchmark, times, error, null);
    if (this.benchmarkContent) {
        this.benchmarkContentHolder.removeChild(this.benchmarkContent);
        this.benchmarkContent = null;
    }
    return result;
};

BenchmarkSuite.prototype.generateTree = function(parent, width, depth) {
    var id = parent.id;
    if (depth !== 0) {
        var middle = Math.floor(width / 2);
        for (var i = 0; i < width; i++) {
            if (i == middle) {
                var divNode = document.createElement("div");
                // TODO:[dave] this causes us to have potentially very long
                // ids. We might want to encode these values into a smaller string
                divNode.id = id + ':(' + i + ', 0)';
                parent.appendChild(divNode);
                this.generateTree(divNode, width, depth - 1);
            } else {
                var p = parent;
                for (var j = 0; j < i; j++) {
                    var divNode = document.createElement("div");
                    divNode.id = id + ':(' + i + ',' + j + ')';
                    p.appendChild(divNode);
                    p = divNode;
                }
                var span = document.createElement("span");
                span.appendChild(document.createTextNode(p.id));
                p.appendChild(span);
            }
        }
    }
};

// Generates a DOM tree (doesn't insert it into the document).
// The width and depth arguments help shape the "bushiness" of the full tree.
// The approach is to recursively generate a set of subtrees. At each root we
// generate width trees, of increasing depth, from 1 to width.  Then we recurse
// from the middle child of this subtree. We do this up to depth times.  reps
// allows us to duplicate a set of trees, without additional recursions. 
BenchmarkSuite.prototype.generateDOMTree = function(width, depth, reps) {
    var top = document.createElement("div");

    top.id = "test";
    for (var i = 0; i < reps; i++) {
        var divNode = document.createElement("div");
        divNode.id = "test" + i;
        this.generateTree(divNode, width, depth);
        top.appendChild(divNode);
    }
    return top;
};

// Generate a small sized tree.
// 92 span leaves, max depth: 23, avg depth: 14
BenchmarkSuite.prototype.generateSmallTree = function() {
    return this.generateDOMTree(14, 12, 10);
};

// Generate a medium sized tree.
// 1320 span leaves, max depth: 27, avg depth: 16
BenchmarkSuite.prototype.generateMediumTree = function() {
    return this.generateDOMTree(19, 13, 9);
};

// Generate a large sized tree.
// 2600 span leaves, max depth: 55, avg depth: 30
BenchmarkSuite.prototype.generateLargeTree = function() {
    return this.generateDOMTree(26, 26, 4);
};

function runBenchmarkSuite(suite, iterationCount) {
    PerfTestRunner.measureTime({run: function () {
        var container = document.getElementById('container');
        var content = document.getElementById('benchmark_content');
        suite.benchmarkContentHolder = container;
        suite.benchmarkContentProto = content;
        var totalMeanTime = 0;
        for (var j = 0; j < suite.benchmarks.length; j++) {
            var result = suite.RunSingle(suite.benchmarks[j], []);
            if (result.error)
                log(result.error);
            else
                totalMeanTime += result.mean;
        }
        return totalMeanTime;
    },
    iterationCount: iterationCount,
    done: function () {
        var container = document.getElementById('container');
        if (container.firstChild)
            container.removeChild(container.firstChild);
    }});
}
