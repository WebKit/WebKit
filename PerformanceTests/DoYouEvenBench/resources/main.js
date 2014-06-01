window.benchmarkClient = {
    iterationCount: 20,
    _timeValues: [],
    _finishedTestCount: 0,
    _iterationNumber: 0,
    _progress: null,
    _progressCompleted: null,
    _resultContainer: null,
    willRunTest: function () { },
    didRunTest: function () {
        this._finishedTestCount++;
        this._progressCompleted.style.width = (this._finishedTestCount * 100 / this.testsCount) + '%';
    },
    didRunSuites: function (measuredValues) {
        this._timeValues.push(measuredValues.total);
        this._iterationNumber++;
        this._addResult('Iteration ' + this._iterationNumber, measuredValues.total.toFixed(2) + ' ms');
    },
    willStartFirstIteration: function () {
        // We don't use the real progress element as some implementations animate it.
        this._progress = document.createElement('div');
        this._progress.appendChild(document.createElement('div'));
        this._progress.id = 'progressContainer';
        document.body.appendChild(this._progress);
        this._progressCompleted = this._progress.firstChild;

        this._resultContainer = document.createElement('table');
        var caption = document.createElement('caption');
        caption.textContent = document.title;
        this._resultContainer.appendChild(caption);
        document.body.appendChild(this._resultContainer);
    },
    didFinishLastIteration: function () {
        var values = this._timeValues;
        var sum = values.reduce(function (a, b) { return a + b; }, 0);
        var arithmeticMean = sum / values.length;
        var meanLabel = arithmeticMean.toFixed(2) + ' ms';
        if (window.Statistics) {
            var delta = Statistics.confidenceIntervalDelta(0.95, values.length, sum, Statistics.squareSum(values));
            var precentDelta = delta * 100 / arithmeticMean;
            meanLabel += ' \xb1 ' + delta.toFixed(2) + ' ms (' + precentDelta.toFixed(2) + '%)';
        }
        this._addResult('Arithmetic Mean', meanLabel);
        this._progress.parentNode.removeChild(this._progress);
    },
    _addResult: function (title, value) {
        var row = document.createElement('tr');
        var th = document.createElement('th');
        th.textContent = title;
        var td = document.createElement('td');
        td.textContent = value;
        row.appendChild(th);
        row.appendChild(td);
        this._resultContainer.appendChild(row);
    }
}

function startBenchmark() {
    var enabledSuites = Suites.filter(function (suite) { return !suite.disabled });
    var totalSubtestCount = enabledSuites.reduce(function (testsCount, suite) { return testsCount + suite.tests.length; }, 0);
    benchmarkClient.testsCount = benchmarkClient.iterationCount * totalSubtestCount;
    var runner = new BenchmarkRunner(Suites, benchmarkClient);
    runner.runMultipleIterations(benchmarkClient.iterationCount);
}

window.onload = startBenchmark;
