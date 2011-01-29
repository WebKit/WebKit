// A basic performance testing harness.

function loadFile(path) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", path, false);
    xhr.send(null);
    return xhr.responseText;
}

var logDiv;

function setupLogging() {
    logDiv = document.createElement("pre");
    document.body.appendChild(logDiv);
}

function log(text) {
    logDiv.innerText += text + "\n";
    window.scrollTo(document.body.height);
}

// FIXME: We should make it possible to configure runCount.
var runCount = 20;
var completedRuns = -1; // Discard the any runs < 0.
var times = [];

function computeAverage(values) {
    var sum = 0;
    for (var i = 0; i < values.length; i++)
        sum += values[i];
    return sum / values.length;
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
    log("stdev " + computeStdev(times));
}

var testFunction;

function runPerformanceTest(testFunction) {
    setupLogging()

    log("Running " + runCount + " times");
    window.testFunction = testFunction;
    runOneTest();
}

function runOneTest() {
    var start = new Date();
    window.testFunction();
    var time = new Date() - start;
    completedRuns++;
    if (completedRuns <= 0) {
        log("Ignoring warm-up run (" + time + ")");
    } else {
        times.push(time);
        log(time);
    }
    if (completedRuns < runCount) {
        window.setTimeout(runOneTest, 0);
    } else {
        logStatistics(times);
    }
}
