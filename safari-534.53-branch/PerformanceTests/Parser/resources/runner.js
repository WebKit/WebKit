function log(text) {
    document.getElementById("log").innerHTML += text + "\n";
    window.scrollTo(0, document.body.height);
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

function logStatistics(times) {
    log("");
    log("avg " + computeAverage(times));
    log("median " + computeMedian(times));
    log("stdev " + computeStdev(times));
    log("min " + computeMin(times));
    log("max " + computeMax(times));
}

function run() {
    var start = new Date();
    for (var i = 0; i < 10; ++i)
        window.runFunction();
    var time = new Date() - start;
    completedRuns++;
    if (completedRuns <= 0) {
        log("Ignoring warm-up run (" + time + ")");
    } else {
        times.push(time);
        log(time);
    }
    if (completedRuns < window.runCount) {
        window.setTimeout(run, 0);
    } else {
        logStatistics(times);
    }
}

function start(runCount, runFunction) {
    window.runCount = runCount;
    window.runFunction = runFunction;

    log("Running " + runCount + " times");
    run();
}
