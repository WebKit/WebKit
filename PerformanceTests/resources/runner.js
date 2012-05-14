// There are tests for computeStatistics() located in LayoutTests/fast/harness/perftests

var PerfTestRunner = {};

// To make the benchmark results predictable, we replace Math.random with a
// 100% deterministic alternative.
PerfTestRunner.randomSeed = PerfTestRunner.initialRandomSeed = 49734321;

PerfTestRunner.resetRandomSeed = function() {
    PerfTestRunner.randomSeed = PerfTestRunner.initialRandomSeed
}

PerfTestRunner.random = Math.random = function() {
    // Robert Jenkins' 32 bit integer hash function.
    var randomSeed = PerfTestRunner.randomSeed;
    randomSeed = ((randomSeed + 0x7ed55d16) + (randomSeed << 12))  & 0xffffffff;
    randomSeed = ((randomSeed ^ 0xc761c23c) ^ (randomSeed >>> 19)) & 0xffffffff;
    randomSeed = ((randomSeed + 0x165667b1) + (randomSeed << 5))   & 0xffffffff;
    randomSeed = ((randomSeed + 0xd3a2646c) ^ (randomSeed << 9))   & 0xffffffff;
    randomSeed = ((randomSeed + 0xfd7046c5) + (randomSeed << 3))   & 0xffffffff;
    randomSeed = ((randomSeed ^ 0xb55a4f09) ^ (randomSeed >>> 16)) & 0xffffffff;
    PerfTestRunner.randomSeed = randomSeed;
    return (randomSeed & 0xfffffff) / 0x10000000;
};

PerfTestRunner.log = function (text) {
    if (!document.getElementById("log")) {
        var pre = document.createElement('pre');
        pre.id = 'log';
        document.body.appendChild(pre);
    }
    document.getElementById("log").innerHTML += text + "\n";
    window.scrollTo(0, document.body.height);
}

PerfTestRunner.info = function (text) {
    this.log("Info: " + text);
}

PerfTestRunner.logInfo = function (text) {
    if (!window.layoutTestController)
        this.log(text);
}

PerfTestRunner.loadFile = function (path) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", path, false);
    xhr.send(null);
    return xhr.responseText;
}

PerfTestRunner.computeStatistics = function (times, unit) {
    var data = times.slice();

    // Add values from the smallest to the largest to avoid the loss of significance
    data.sort(function(a,b){return a-b;});

    var middle = Math.floor(data.length / 2);
    var result = {
        min: data[0],
        max: data[data.length - 1],
        median: data.length % 2 ? data[middle] : (data[middle - 1] + data[middle]) / 2,
    };

    // Compute the mean and variance using a numerically stable algorithm.
    var squareSum = 0;
    result.mean = data[0];
    result.sum = data[0];
    for (var i = 1; i < data.length; ++i) {
        var x = data[i];
        var delta = x - result.mean;
        var sweep = i + 1.0;
        result.mean += delta / sweep;
        result.sum += x;
        squareSum += delta * delta * (i / sweep);
    }
    result.variance = squareSum / data.length;
    result.stdev = Math.sqrt(result.variance);
    result.unit = unit || "ms";

    return result;
}

PerfTestRunner.logStatistics = function (times) {
    this.log("");
    var statistics = this.computeStatistics(times, this.unit);
    this.printStatistics(statistics);
}

PerfTestRunner.printStatistics = function (statistics) {
    this.log("");
    this.log("avg " + statistics.mean + " " + statistics.unit);
    this.log("median " + statistics.median + " " + statistics.unit);
    this.log("stdev " + statistics.stdev + " " + statistics.unit);
    this.log("min " + statistics.min + " " + statistics.unit);
    this.log("max " + statistics.max + " " + statistics.unit);
}

PerfTestRunner.gc = function () {
    if (window.GCController)
        window.GCController.collect();
    else {
        function gcRec(n) {
            if (n < 1)
                return {};
            var temp = {i: "ab" + i + (i / 100000)};
            temp += "foo";
            gcRec(n-1);
        }
        for (var i = 0; i < 1000; i++)
            gcRec(10);
    }
}

PerfTestRunner._runLoop = function () {
    if (this._completedRuns < this._runCount) {
        this.gc();
        window.setTimeout(function () { PerfTestRunner._runner(); }, 0);
    } else {
        this.logStatistics(this._results);
        this._doneFunction();
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    }
}

PerfTestRunner._runner = function () {
    var start = Date.now();
    var totalTime = 0;

    for (var i = 0; i < this._loopsPerRun; ++i) {
        var returnValue = this._runFunction.call(window);
        if (returnValue - 0 === returnValue) {
            if (returnValue <= 0)
                this.log("runFunction returned a non-positive value: " + returnValue);
            totalTime += returnValue;
        }
    }

    // Assume totalTime can never be zero when _runFunction returns a number.
    var time = totalTime ? totalTime : Date.now() - start;

    this.ignoreWarmUpAndLog(time);
    this._runLoop();
}

PerfTestRunner.ignoreWarmUpAndLog = function (result) {
    this._completedRuns++;

    var labeledResult = result + " " + this.unit;
    if (this._completedRuns <= 0)
        this.log("Ignoring warm-up run (" + labeledResult + ")");
    else {
        this._results.push(result);
        this.log(labeledResult);
    }
}

PerfTestRunner.initAndStartLoop = function() {
    this._completedRuns = -1;
    this.customRunFunction = null;
    this._results = [];
    this.log("Running " + this._runCount + " times");
    this._runLoop();
}

PerfTestRunner.run = function (runFunction, loopsPerRun, runCount, doneFunction) {
    this._runFunction = runFunction;
    this._loopsPerRun = loopsPerRun || 10;
    this._runCount = runCount || 20;
    this._doneFunction = doneFunction || function () {};
    this.unit = 'ms';
    this.initAndStartLoop();
}

PerfTestRunner.runPerSecond = function (test) {
    this._doneFunction = function () { if (test.done) test.done(); };
    this._runCount = test.runCount || 20;
    this._callsPerIteration = 1;
    this.unit = 'runs/s';

    this._test = test;
    this._runner = this._perSecondRunner;
    this.initAndStartLoop();
}

PerfTestRunner._perSecondRunner = function () {
    var timeToRun = this._test.timeToRun || 750;
    var totalTime = 0;
    var i = 0;
    var callsPerIteration = this._callsPerIteration;

    if (this._test.setup)
        this._test.setup();

    while (totalTime < timeToRun) {
        totalTime += this._perSecondRunnerIterator(callsPerIteration);
        i += callsPerIteration;
        if (this._completedRuns <= 0 && totalTime < 100)
            callsPerIteration = Math.max(10, 2 * callsPerIteration);
    }
    this._callsPerIteration = callsPerIteration;

    this.ignoreWarmUpAndLog(i * 1000 / totalTime);
    this._runLoop();
}

PerfTestRunner._perSecondRunnerIterator = function (callsPerIteration) {
    var startTime = Date.now();
    for (var i = 0; i < callsPerIteration; i++)
        this._test.run();
    return Date.now() - startTime;
}

if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
    layoutTestController.dumpAsText();
}
