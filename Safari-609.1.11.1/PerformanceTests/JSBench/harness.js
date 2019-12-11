/*
 * Copyright (C) 2011, 2012 Purdue University
 * Written by Gregor Richards
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

(function() {
    // put benchmarks here
    var benchmarks = ["amazon-chrome", "amazon-chrome-win", "amazon-firefox",
                      "amazon-firefox-win", "amazon-safari", "facebook-chrome",
                      "facebook-chrome-win", "facebook-firefox",
                      "facebook-firefox-win", "facebook-safari",
                      "google-chrome", "google-chrome-win", "google-firefox",
                      "google-firefox-win", "google-safari", "twitter-chrome",
                      "twitter-chrome-win", "twitter-firefox",
                      "twitter-firefox-win", "twitter-safari", "yahoo-chrome",
                      "yahoo-chrome-win", "yahoo-firefox", "yahoo-firefox-win",
                      "yahoo-safari"];
    var modes = {
        "*": ["urem"],
        "amazon-firefox": ["urm"],
        "amazon-firefox-win": ["urm"],
        "google-firefox": ["uem"],
        "twitter-chrome-win": ["rem"]
    };
    var minRuns = 23;
    var keepRuns = 20;
    var maxRuns = 100;
    var maxCIM = 0.10;

    // fixups for old engines
    if (!Array.prototype.reduce) {
        Array.prototype.reduce = function(f, val) {
            for (var i = 0; i < this.length; i++) {
                val = f(val, this[i]);
            }
            return val;
        };
    }

    // all test results
    var results = {};

    var curRun = 0;
    var curBenchmark = 0;
    var curMode = 0;

    function getModes(bm) {
        if (bm in modes) return modes[bm];
        return modes["*"];
    }

    // call this to either rerun or go to another page and come back
    function rerun() {
        try {
            if (window.sessionStorage) {
                // store this for the session and go 'round to another page to force uncaching on Chrome
                sessionStorage.JSBNG_harnessState = JSON.stringify({
                    results: results,
                    curRun: curRun,
                    curBenchmark: curBenchmark,
                    curMode: curMode
                });
                window.location.href = window.location.href.replace(/\/[^\/]*$/, "/reload.html");
                return;
            }
        } catch (ex) {}
        runBenchmark();
    }

    // load our current state and run
    function onload() {
        var gob = document.getElementById("go");
        try {
            if (window.sessionStorage && "JSBNG_harnessState" in sessionStorage) {
                var state = JSON.parse(sessionStorage.JSBNG_harnessState);
                results = state.results;
                curRun = state.curRun;
                curBenchmark = state.curBenchmark;
                curMode = state.curMode;
                setTimeout(runBenchmark, 200);
                gob.style.display = "none";
                return;
            }
        } catch (ex) {}
        gob.onclick = function() {
            gob.style.display = "none";
            setTimeout(runBenchmark, 200);
        };
        if (/#run/.test(window.location.href)) {
            gob.style.display = "none";
            setTimeout(runBenchmark, 200);
        }
    }

    function runBenchmark() {
        var output = document.getElementById("output");
        var bmframe = document.getElementById("bmframe");

        // should we stop?
        if (curRun < minRuns) {
            output.innerHTML = (curRun+1) + "/" + minRuns;
        } else {
            // check if our S/M is OK
            var stats = handleResults(false);
            output.innerHTML = (curRun+1) + " " + stats.cim + " (" + maxCIM + ")";
            if (curRun >= maxRuns || stats.cim < maxCIM) {
                bmframe.src = "about:blank";
                bmframe.style.display = "none";
                handleResults(true);
                if (window.sessionStorage) delete sessionStorage.JSBNG_harnessState;
                return;
            }
        }

        // get out our benchmark and mode
        var benchmark = benchmarks[curBenchmark];
        var modes = getModes(benchmark);
        var mode = modes[curMode];
        if (++curMode >= modes.length) {
            curMode = 0;
            curBenchmark++;
        }
        if (curBenchmark >= benchmarks.length) {
            curBenchmark = 0;
            curRun++;
        }

        // make sure we have the results space
        if (!(benchmark in results)) results[benchmark] = {};
        if (!(mode in results[benchmark])) results[benchmark][mode] = [];

        // Expose time measuring function.
        if (window.performance && window.performance.now)
            window.currentTimeInMS = function() { return window.performance.now() };
        else if (typeof preciseTime !== 'undefined')
            window.currentTimeInMS = function() { return preciseTime() * 1000; };
        else
            window.currentTimeInMS = function() { return Date.now(); };

        // set up the receiver
        if (/v/.test(mode)) {
            // verification mode, we only care if there's an error
            window.JSBNG_handleResult = function(res) {
                if (res.error) {
                    if (!("errors" in results)) results.errors = [];
                    results.errors.push(benchmark + "." + mode + ": " + res.msg);
                }
                rerun();
            };
        } else {
            window.JSBNG_handleResult = function(res) {
                if (!res.error) {
                    results[benchmark][mode].push(res.time);
                }
                rerun();
            };
        }

        // then load it
        bmframe.src = benchmark + "/" + mode + ".html";
    }

    // handle all of our results
    function handleResults(pr) {
        var output = document.getElementById("output");
        function print(str) {
            output.appendChild(document.createTextNode(str));
            output.appendChild(document.createElement("br"));
        }
        function printarr(arr) {
            for (var i = 0; i < arr.length; i++) print(arr[i]);
        }
        function percent(num) {
            return (num*100).toFixed(2) + "%";
        }

        // clear out the intermediate results
        if (pr) output.innerHTML = "";

        // totals
        var totals = {
            mean: 1,
            stddev: 1,
            sem: 1,
            ci: 1,
            runs: 0
        };

        // stuff to print later
        var ptotals = [];
        var presults = [];
        var praw = [];
        var spc = "\u00a0\u00a0";
        var spc2 = spc + spc;

        // calculate all the real results
        for (var b = 0; b < benchmarks.length; b++) {
            var benchmark = benchmarks[b];
            var modes = getModes(benchmark);
            if (pr) {
                presults.push(spc + benchmark + ":");
                praw.push(spc + benchmark + ":");
            }

            for (var m = 0; m < modes.length; m++) {
                var mode = modes[m];
                var bmresults = results[benchmark][mode].slice(-keepRuns);
                if (bmresults.length == 0) continue;

                // get the raw results
                var rr = spc2 + mode + ": [";
                for (var i = 0; i < bmresults.length; i++) {
                    if (i != 0) rr += ", ";
                    rr += bmresults[i];
                }
                rr += "]";
                if (pr) praw.push(rr);

                // now get the stats for this run
                var bmstats = stats(bmresults);

                // mul it to the totals
                totals.mean *= bmstats.mean;
                totals.stddev *= bmstats.stddev;
                totals.sem *= bmstats.sem;
                totals.ci *= bmstats.ci;
                totals.runs++;

                // and output it
                if (pr) presults.push(spc2 + mode + ": " +
                    bmstats.mean.toFixed(2) + "ms ± " + percent(bmstats.cim) +
                    " (stddev=" + percent(bmstats.sm) + ", stderr=" +
                    percent(bmstats.semm) + ")");
            }

            if (pr) {
                presults.push("");
                praw.push("");
            }
        }

        // now calculate the totals
        var power = 1 / totals.runs;
        totals.mean = Math.pow(totals.mean, power);
        totals.stddev = Math.pow(totals.stddev, power);
        totals.sm = totals.stddev / totals.mean;
        totals.sem = Math.pow(totals.sem, power);
        totals.semm = totals.sem / totals.mean;
        totals.ci = Math.pow(totals.ci, power);
        totals.cim = totals.ci / totals.mean;
        ptotals.push("Final results:");
        ptotals.push(spc + totals.mean.toFixed(2) + "ms ± " + percent(totals.cim) + " (lower is better)");
        ptotals.push(spc + "Standard deviation = " + percent(totals.sm) + " of mean");
        ptotals.push(spc + "Standard error = " + percent(totals.semm) + " of mean");
        if (totals.cim >= maxCIM)
            ptotals.push(spc + "WARNING: These results are not trustworthy! After " + maxRuns + " runs, 95% confidence interval is still greater than " + percent(maxCIM) + " of the mean!");
        else
            ptotals.push(spc + curRun + " runs");
        ptotals.push("");

        // if there are errors, mark those too
        if ("errors" in results) {
            ptotals.push("ERRORS:");
            for (var i = 0; i < results.errors.length; i++) ptotals.push(spc + results.errors[i]);
            ptotals.push("");
        }

        if (pr) {
            // and print it all out
            printarr(ptotals);
            print("Result breakdown:");
            printarr(presults);
            print("Raw results:");
            printarr(praw);
        }

        return totals;
    }

    // standard t-distribution for normally distributed samples
    var tDistribution = [NaN, NaN, 12.71, 4.30, 3.18, 2.78, 2.57, 2.45, 2.36,
    2.31, 2.26, 2.23, 2.20, 2.18, 2.16, 2.14, 2.13, 2.12, 2.11, 2.10, 2.09,
    2.09, 2.08, 2.07, 2.07, 2.06, 2.06, 2.06, 2.05, 2.05, 2.05, 2.04, 2.04,
    2.04, 2.03, 2.03, 2.03, 2.03, 2.03, 2.02, 2.02, 2.02, 2.02, 2.02, 2.02,
    2.02, 2.01, 2.01, 2.01, 2.01, 2.01, 2.01, 2.01, 2.01, 2.01, 2.00, 2.00,
    2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00,
    2.00, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99,
    1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99,
    1.99, 1.99, 1.99, 1.99, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98,
    1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98,
    1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98,
    1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98,
    1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98,
    1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97,
    1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.96];

    // t distribution
    function tDist(n) {
        if (n >= tDistribution.length)
            return tDistribution[tDistribution.length-1];
        return tDistribution[n];
    }

    // get statistics
    function stats(results) {
        var ret = {};

        function sum(arr) {
            return arr.reduce(function(p, c) { return p + c; }, 0);
        }

        // mean
        ret.mean = sum(results) / results.length;

        // sample stddev
        ret.stddev = Math.sqrt(
            sum(
                results.map(function(e) { return Math.pow(e - ret.mean, 2); })
            ) / (results.length - 1)
        );

        // stddev / mean
        ret.sm = ret.stddev / ret.mean;

        // sample SEM (stderr)
        ret.sem = ret.stddev / Math.sqrt(results.length);

        // sample SEM/mean
        ret.semm = ret.sem / ret.mean;

        // sample 95% confidence interval range
        ret.ci = tDist(results.length) * ret.sem;

        // sample 95% CI / mean
        ret.cim = ret.ci / ret.mean;

        return ret;
    }

    window.onload = onload;
})();
