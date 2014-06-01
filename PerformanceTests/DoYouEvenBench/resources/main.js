(function () {

var values = [];
var resultContainer = null;
var title;
var progressContainer;
var progress;
var iterationNumber = 0;
var finishedTestCount = 0;

function addResult(title, value) {
    if (!resultContainer) {
        resultContainer = document.createElement('table');
        var caption = document.createElement('caption');
        caption.textContent = document.title;
        resultContainer.appendChild(caption);
        document.body.appendChild(resultContainer);
    }
    if (!title)
        return;
    var row = document.createElement('tr');
    var th = document.createElement('th');
    th.textContent = title;
    var td = document.createElement('td');
    td.textContent = value;
    row.appendChild(th);
    row.appendChild(td);
    resultContainer.appendChild(row);
}

window.benchmarkClient = {
    iterationCount: 20,
    willRunTest: function () {
        if (!progress) {
            // We don't use the real progress element as some implementations animate it.
            progressContainer = document.createElement('div');
            progressContainer.appendChild(document.createElement('div'));
            progressContainer.id = 'progressContainer';
            document.body.appendChild(progressContainer);
            progress = progressContainer.firstChild;
        }
        addResult();
    },
    didRunTest: function () {
        finishedTestCount++;
        progress.style.width = (finishedTestCount * 100 / this.testsCount) + '%';
    },
    didRunSuites: function (measuredValues) {
        values.push(measuredValues.total);
        iterationNumber++;
        addResult('Iteration ' + iterationNumber, measuredValues.total.toFixed(2) + ' ms');
    },
    didFinishLastIteration: function () {
        var sum = values.reduce(function (a, b) { return a + b; }, 0);
        var arithmeticMean = sum / values.length;
        var meanLabel = arithmeticMean.toFixed(2) + ' ms';
        if (window.Statistics) {
            var delta = Statistics.confidenceIntervalDelta(0.95, values.length, sum, Statistics.squareSum(values));
            var precentDelta = delta * 100 / arithmeticMean;
            meanLabel += ' \xb1 ' + delta.toFixed(2) + ' ms (' + precentDelta.toFixed(2) + '%)';
        }
        addResult('Arithmetic Mean', meanLabel);
        progressContainer.parentNode.removeChild(progressContainer);
    }
}

})();

function startBenchmark() {
    var enabledSuites = Suites.filter(function (suite) { return !suite.disabled });
    var totalSubtestCount = enabledSuites.reduce(function (testsCount, suite) { return testsCount + suite.tests.length; }, 0);
    benchmarkClient.testsCount = benchmarkClient.iterationCount * totalSubtestCount;
    var runner = new BenchmarkRunner(Suites, benchmarkClient);
    runner.runMultipleIterations(benchmarkClient.iterationCount);
}

window.onload = startBenchmark;
