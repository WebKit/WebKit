
var PerfTestRunner = {};

PerfTestRunner.log = function (text) {
    if (!document.getElementById("log")) {
        var pre = document.createElement('pre');
        pre.id = 'log';
        document.body.appendChild(pre);
    }
    document.getElementById("log").innerHTML += text + "\n";
    window.scrollTo(0, document.body.height);
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

PerfTestRunner.computeStatistics = function (times) {
    var data = times.slice();

    // Add values from the smallest to the largest to avoid the loss of significance
    data.sort();

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

    return result;
}

PerfTestRunner.logStatistics = function (times) {
    this.log("");
    var statistics = this.computeStatistics(times);
    this.printStatistics(statistics);
}

PerfTestRunner.printStatistics = function (statistics) {
    this.log("");
    this.log("avg " + statistics.mean);
    this.log("median " + statistics.median);
    this.log("stdev " + statistics.stdev);
    this.log("min " + statistics.min);
    this.log("max " + statistics.max);
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
        this.logStatistics(this._times);
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

    this._completedRuns++;
    if (this._completedRuns <= 0)
        this.log("Ignoring warm-up run (" + time + ")");
    else {
        this._times.push(time);
        this.log(time);
    }
    this._runLoop();
}

PerfTestRunner.run = function (runFunction, loopsPerRun, runCount, doneFunction) {
    this._runFunction = runFunction;
    this._loopsPerRun = loopsPerRun || 10;
    this._runCount = runCount || 20;
    this._doneFunction = doneFunction || function () {};
    this._completedRuns = -1;
    this.customRunFunction = null;
    this._times = [];

    this.log("Running " + this._runCount + " times");
    this._runLoop();
}

if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
    layoutTestController.dumpAsText();
}
