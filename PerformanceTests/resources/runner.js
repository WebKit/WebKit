function log(text) {
    document.getElementById("log").innerHTML += text + "\n";
    window.scrollTo(0, document.body.height);
}

function logInfo(text) {
    if (!window.layoutTestController)
        log(text);
}

function loadFile(path) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", path, false);
    xhr.send(null);
    return xhr.responseText;
}

var runCount = -1;
var runFunction = function() {};
var completedRuns = -1; // Discard the any runs < 0.
var times = [];

function computeAverage(values) {
    var sum = 0;
    for (var i = 0; i < values.length; i++)
        sum += values[i];
    return sum / values.length;
}

function computeMax(values) {
    var max = values.length ? values[0] : 0;
    for (var i = 1; i < values.length; i++) {
        if (max < values[i])
            max = values[i];
    }
    return max;
}

function computeMedian(values) {
    values.sort(function(a, b) { return a - b; });
    var len = values.length;
    if (len % 2)
        return values[(len-1)/2];
    return (values[len/2-1] + values[len/2]) / 2;
}

function computeMin(values) {
    var min = values.length ? values[0] : 0;
    for (var i = 1; i < values.length; i++) {
        if (min > values[i])
            min = values[i];
    }
    return min;
}

function computeStdev(values) {
    var average = computeAverage(values);
    var sumOfSquaredDeviations = 0;
    for (var i = 0; i < values.length; ++i) {
        var deviation = values[i] - average;
        sumOfSquaredDeviations += deviation * deviation;
    }
    return Math.sqrt(sumOfSquaredDeviations / values.length);
}

function printStatistics(stats, printFunction)
{
    printFunction("");
    printFunction("avg " + stats.avg);
    printFunction("median " + stats.median);
    printFunction("stdev " + stats.stdev);
    printFunction("min " + stats.min);
    printFunction("max " + stats.max);
}

function logStatistics(times) {
    printStatistics({
        avg: computeAverage(times),
        median: computeMedian(times),
        stdev: computeStdev(times),
        min: computeMin(times),
        max: computeMax(times)
    }, log);
}

function gc() {
    if (window.GCController)
        GCController.collect();
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

function runLoop()
{
    if (window.completedRuns < window.runCount) {
        gc();
        window.setTimeout(run, 0);
    } else {
        logStatistics(times);
        window.doneFunction();
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    }
}

function run() {
    if (window.customRunFunction)
        var time = window.customRunFunction();
    else {
        var start = new Date();
        for (var i = 0; i < window.loopsPerRun; ++i)
            window.runFunction();
        var time = new Date() - start;
    }

    window.completedRuns++;
    if (window.completedRuns <= 0) {
        log("Ignoring warm-up run (" + time + ")");
    } else {
        times.push(time);
        log(time);
    }
    runLoop()
}

function start(runCount, runFunction, loopsPerRun, doneFunction) {
    window.runCount = runCount;
    window.runFunction = runFunction;
    window.loopsPerRun = loopsPerRun || 10;
    window.doneFunction = doneFunction || function() {};

    log("Running " + runCount + " times");
    runLoop();
}

function startCustom(runCount, customRunFunction, doneFunction) {
    window.runCount = runCount;
    window.customRunFunction = customRunFunction;
    window.loopsPerRun = 1;
    window.doneFunction = doneFunction || function() {};

    log("Running " + runCount + " times");
    runLoop();
}

if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
    layoutTestController.dumpAsText();
}
