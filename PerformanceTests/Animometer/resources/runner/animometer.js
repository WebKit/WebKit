ResultsDashboard = Utilities.createClass(
    function(options, testData)
    {
        this._iterationsSamplers = [];
        this._options = options;
        this._results = null;
        if (testData) {
            this._iterationsSamplers = testData;
            this._processData();
        }
    }, {

    push: function(suitesSamplers)
    {
        this._iterationsSamplers.push(suitesSamplers);
    },

    _processData: function()
    {
        this._results = {};
        this._results[Strings.json.results.iterations] = [];

        var iterationsScores = [];
        this._iterationsSamplers.forEach(function(iteration, index) {
            var testsScores = [];

            var result = {};
            this._results[Strings.json.results.iterations][index] = result;

            var suitesResult = {};
            result[Strings.json.results.tests] = suitesResult;

            for (var suiteName in iteration) {
                var suiteData = iteration[suiteName];

                var suiteResult = {};
                suitesResult[suiteName] = suiteResult;

                for (var testName in suiteData) {
                    if (!suiteData[testName][Strings.json.result])
                        this.calculateScore(suiteData[testName]);

                    suiteResult[testName] = suiteData[testName][Strings.json.result];
                    delete suiteData[testName][Strings.json.result];

                    testsScores.push(suiteResult[testName][Strings.json.score]);
                }
            }

            result[Strings.json.score] = Statistics.geometricMean(testsScores);
            iterationsScores.push(result[Strings.json.score]);
        }, this);

        this._results[Strings.json.score] = Statistics.sampleMean(iterationsScores.length, iterationsScores.reduce(function(a, b) { return a + b; }));
    },

    calculateScore: function(data)
    {
        var result = {};
        data[Strings.json.result] = result;
        var samples = data[Strings.json.samples];

        function findRegression(series) {
            var minIndex = Math.round(.025 * series.length);
            var maxIndex = Math.round(.975 * (series.length - 1));
            var minComplexity = series[minIndex].complexity;
            var maxComplexity = series[maxIndex].complexity;
            if (Math.abs(maxComplexity - minComplexity) < 20 && maxIndex - minIndex < 20) {
                minIndex = 0;
                maxIndex = series.length - 1;
                minComplexity = series[minIndex].complexity;
                maxComplexity = series[maxIndex].complexity;
            }

            return {
                minComplexity: minComplexity,
                maxComplexity: maxComplexity,
                samples: series.slice(minIndex, maxIndex + 1),
                regression: new Regression(
                    series,
                    function (datum, i) { return datum[i].complexity; },
                    function (datum, i) { return datum[i].frameLength; },
                    minIndex, maxIndex)
            };
        }

        var complexitySamples;
        [Strings.json.complexity, Strings.json.complexityAverage].forEach(function(seriesName) {
            if (!(seriesName in samples))
                return;

            var regression = {};
            result[seriesName] = regression;
            var regressionResult = findRegression(samples[seriesName]);
            if (seriesName == Strings.json.complexity)
                complexitySamples = regressionResult.samples;
            var calculation = regressionResult.regression;
            regression[Strings.json.regressions.segment1] = [
                [regressionResult.minComplexity, calculation.s1 + calculation.t1 * regressionResult.minComplexity],
                [calculation.complexity, calculation.s1 + calculation.t1 * calculation.complexity]
            ];
            regression[Strings.json.regressions.segment2] = [
                [calculation.complexity, calculation.s2 + calculation.t2 * calculation.complexity],
                [regressionResult.maxComplexity, calculation.s2 + calculation.t2 * regressionResult.maxComplexity]
            ];
            regression[Strings.json.complexity] = calculation.complexity;
            regression[Strings.json.measurements.stdev] = Math.sqrt(calculation.error / samples[seriesName].length);
        });

        if (this._options["adjustment"] == "ramp") {
            var timeComplexity = new Experiment;
            data[Strings.json.controller].forEach(function(regression) {
                timeComplexity.sample(regression[Strings.json.complexity]);
            });

            var experimentResult = {};
            result[Strings.json.controller] = experimentResult;
            experimentResult[Strings.json.score] = timeComplexity.mean();
            experimentResult[Strings.json.measurements.average] = timeComplexity.mean();
            experimentResult[Strings.json.measurements.stdev] = timeComplexity.standardDeviation();
            experimentResult[Strings.json.measurements.percent] = timeComplexity.percentage();

            result[Strings.json.complexity][Strings.json.bootstrap] = Regression.bootstrap(complexitySamples, 2500, function(resample) {
                    resample.sort(function(a, b) {
                        return a.complexity - b.complexity;
                    });

                    var regressionResult = findRegression(resample);
                    return regressionResult.regression.complexity;
                }, .95);
            result[Strings.json.score] = result[Strings.json.complexity][Strings.json.bootstrap].median;

        } else {
            var marks = data[Strings.json.marks];
            var samplingStartIndex = 0, samplingEndIndex = -1;
            if (Strings.json.samplingStartTimeOffset in marks)
                samplingStartIndex = marks[Strings.json.samplingStartTimeOffset].index;
            if (Strings.json.samplingEndTimeOffset in marks)
                samplingEndIndex = marks[Strings.json.samplingEndTimeOffset].index;

            var averageComplexity = new Experiment;
            var averageFrameLength = new Experiment;
            samples[Strings.json.controller].forEach(function (sample, i) {
                if (i >= samplingStartIndex && (samplingEndIndex == -1 || i < samplingEndIndex)) {
                    averageComplexity.sample(sample.complexity);
                    if (sample.smoothedFrameLength && sample.smoothedFrameLength != -1)
                        averageFrameLength.sample(sample.smoothedFrameLength);
                }
            });

            var experimentResult = {};
            result[Strings.json.controller] = experimentResult;
            experimentResult[Strings.json.measurements.average] = averageComplexity.mean();
            experimentResult[Strings.json.measurements.concern] = averageComplexity.concern(Experiment.defaults.CONCERN);
            experimentResult[Strings.json.measurements.stdev] = averageComplexity.standardDeviation();
            experimentResult[Strings.json.measurements.percent] = averageComplexity.percentage();

            experimentResult = {};
            result[Strings.json.frameLength] = experimentResult;
            experimentResult[Strings.json.measurements.average] = 1000 / averageFrameLength.mean();
            experimentResult[Strings.json.measurements.concern] = averageFrameLength.concern(Experiment.defaults.CONCERN);
            experimentResult[Strings.json.measurements.stdev] = averageFrameLength.standardDeviation();
            experimentResult[Strings.json.measurements.percent] = averageFrameLength.percentage();

            result[Strings.json.score] = averageComplexity.score(Experiment.defaults.CONCERN);
        }
    },

    get data()
    {
        return this._iterationsSamplers;
    },

    get results()
    {
        if (this._results)
            return this._results[Strings.json.results.iterations];
        this._processData();
        return this._results[Strings.json.results.iterations];
    },

    get options()
    {
        return this._options;
    },

    get score()
    {
        if (this._results)
            return this._results[Strings.json.score];
        this._processData();
        return this._results[Strings.json.score];
    }
});

ResultsTable = Utilities.createClass(
    function(element, headers)
    {
        this.element = element;
        this._headers = headers;

        this._flattenedHeaders = [];
        this._headers.forEach(function(header) {
            if (header.disabled)
                return;

            if (header.children)
                this._flattenedHeaders = this._flattenedHeaders.concat(header.children);
            else
                this._flattenedHeaders.push(header);
        }, this);

        this._flattenedHeaders = this._flattenedHeaders.filter(function (header) {
            return !header.disabled;
        });

        this.clear();
    }, {

    clear: function()
    {
        this.element.innerHTML = "";
    },

    _addHeader: function()
    {
        var thead = Utilities.createElement("thead", {}, this.element);
        var row = Utilities.createElement("tr", {}, thead);

        this._headers.forEach(function (header) {
            if (header.disabled)
                return;

            var th = Utilities.createElement("th", {}, row);
            if (header.title != Strings.text.graph)
                th.textContent = header.title;
            if (header.children)
                th.colSpan = header.children.length;
        });
    },

    _addEmptyRow: function()
    {
        var row = Utilities.createElement("tr", {}, this.element);
        this._flattenedHeaders.forEach(function (header) {
            return Utilities.createElement("td", { class: "suites-separator" }, row);
        });
    },

    _addTest: function(testName, testResults, options)
    {
        var row = Utilities.createElement("tr", {}, this.element);

        this._flattenedHeaders.forEach(function (header) {
            var td = Utilities.createElement("td", {}, row);
            if (header.title == Strings.text.testName) {
                td.textContent = testName;
            } else if (header.text) {
                var data = testResults[header.text];
                if (typeof data == "number")
                    data = data.toFixed(2);
                td.textContent = data;
            }
        }, this);
    },

    _addIteration: function(iterationResult, iterationData, options)
    {
        var testsResults = iterationResult[Strings.json.results.tests];
        for (var suiteName in testsResults) {
            this._addEmptyRow();
            var suiteResult = testsResults[suiteName];
            var suiteData = iterationData[suiteName];
            for (var testName in suiteResult)
                this._addTest(testName, suiteResult[testName], options, suiteData[testName]);
        }
    },

    showIterations: function(dashboard)
    {
        this.clear();
        this._addHeader();

        var iterationsResults = dashboard.results;
        iterationsResults.forEach(function(iterationResult, index) {
            this._addIteration(iterationResult, dashboard.data[index], dashboard.options);
        }, this);
    }
});

window.benchmarkRunnerClient = {
    iterationCount: 1,
    options: null,
    results: null,

    initialize: function(suites, options)
    {
        this.options = options;
    },

    willStartFirstIteration: function()
    {
        this.results = new ResultsDashboard(this.options);
    },

    didRunSuites: function(suitesSamplers)
    {
        this.results.push(suitesSamplers);
    },

    didRunTest: function(testData)
    {
        this.results.calculateScore(testData);
    },

    didFinishLastIteration: function()
    {
        benchmarkController.showResults();
    }
};

window.sectionsManager =
{
    showSection: function(sectionIdentifier, pushState)
    {
        var currentSectionElement = document.querySelector("section.selected");
        console.assert(currentSectionElement);

        var newSectionElement = document.getElementById(sectionIdentifier);
        console.assert(newSectionElement);

        currentSectionElement.classList.remove("selected");
        newSectionElement.classList.add("selected");

        if (pushState)
            history.pushState({section: sectionIdentifier}, document.title);
    },

    setSectionScore: function(sectionIdentifier, score, mean)
    {
        document.querySelector("#" + sectionIdentifier + " .score").textContent = score;
        if (mean)
            document.querySelector("#" + sectionIdentifier + " .mean").innerHTML = mean;
    },

    populateTable: function(tableIdentifier, headers, dashboard)
    {
        var table = new ResultsTable(document.getElementById(tableIdentifier), headers);
        table.showIterations(dashboard);
    }
};

window.benchmarkController = {
    _startBenchmark: function(suites, options, frameContainerID)
    {
        benchmarkRunnerClient.initialize(suites, options);
        var frameContainer = document.getElementById(frameContainerID);
        var runner = new BenchmarkRunner(suites, frameContainer, benchmarkRunnerClient);
        runner.runMultipleIterations();

        sectionsManager.showSection("test-container");
    },

    startBenchmark: function()
    {
        var options = {
            "test-interval": 10,
            "display": "minimal",
            "adjustment": "adaptive",
            "frame-rate": 50,
            "kalman-process-error": 1,
            "kalman-measurement-error": 4,
            "time-measurement": "performance"
        };
        this._startBenchmark(Suites, options, "test-container");
    },

    showResults: function()
    {
        if (!this.addedKeyEvent) {
            document.addEventListener("keypress", this.selectResults, false);
            this.addedKeyEvent = true;
        }

        var dashboard = benchmarkRunnerClient.results;

        sectionsManager.setSectionScore("results", dashboard.score.toFixed(2));
        sectionsManager.populateTable("results-header", Headers.testName, dashboard);
        sectionsManager.populateTable("results-score", Headers.score, dashboard);
        sectionsManager.showSection("results", true);
    },

    selectResults: function(event)
    {
        if (event.charCode != 115) // 's'
            return;

        event.target.selectRange = ((event.target.selectRange || 0) + 1) % 3;

        var selection = window.getSelection();
        selection.removeAllRanges();
        var range = document.createRange();
        switch (event.target.selectRange) {
            case 0: {
                range.setStart(document.querySelector("#results .score"), 0);
                range.setEndAfter(document.querySelector("#results-score > tr:last-of-type"), 0);
                break;
            }
            case 1: {
                range.selectNodeContents(document.querySelector("#results .score"));
                break;
            }
            case 2: {
                range.selectNode(document.getElementById("results-score"));
                break;
            }
        }
        selection.addRange(range);
    }
};
