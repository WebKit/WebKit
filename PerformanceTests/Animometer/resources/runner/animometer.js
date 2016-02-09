ResultsDashboard = Utilities.createClass(
    function()
    {
        this._iterationsSamplers = [];
        this._processedData = undefined;
    }, {

    push: function(suitesSamplers)
    {
        this._iterationsSamplers.push(suitesSamplers);
    },

    _processData: function()
    {
        var iterationsResults = [];
        var iterationsScores = [];

        this._iterationsSamplers.forEach(function(iterationSamplers, index) {
            var suitesResults = {};
            var suitesScores = [];

            for (var suiteName in iterationSamplers) {
                var suite = suiteFromName(suiteName);
                var suiteSamplerData = iterationSamplers[suiteName];

                var testsResults = {};
                var testsScores = [];

                for (var testName in suiteSamplerData) {
                    testsResults[testName] = suiteSamplerData[testName];
                    testsScores.push(testsResults[testName][Strings.json.score]);
                }

                suitesResults[suiteName] =  {};
                suitesResults[suiteName][Strings.json.score] = Statistics.geometricMean(testsScores);
                suitesResults[suiteName][Strings.json.results.tests] = testsResults;
                suitesScores.push(suitesResults[suiteName][Strings.json.score]);
            }

            iterationsResults[index] = {};
            iterationsResults[index][Strings.json.score] = Statistics.geometricMean(suitesScores);
            iterationsResults[index][Strings.json.results.suites] = suitesResults;
            iterationsScores.push(iterationsResults[index][Strings.json.score]);
        });

        this._processedData = {};
        this._processedData[Strings.json.score] = Statistics.sampleMean(iterationsScores.length, iterationsScores.reduce(function(a, b) { return a * b; }));
        this._processedData[Strings.json.results.iterations] = iterationsResults;
    },

    get data()
    {
        if (this._processedData)
            return this._processedData;
        this._processData();
        return this._processedData;
    },

    get score()
    {
        return this.data[Strings.json.score];
    }
});

ResultsTable = Utilities.createClass(
    function(element, headers)
    {
        this.element = element;
        this._headers = headers;

        this._flattenedHeaders = [];
        this._headers.forEach(function(header) {
            if (header.children)
                this._flattenedHeaders = this._flattenedHeaders.concat(header.children);
            else
                this._flattenedHeaders.push(header);
        }, this);

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
            var th = Utilities.createElement("th", {}, row);
            if (header.title != Strings.text.results.graph)
                th.textContent = header.title;
            if (header.children)
                th.colSpan = header.children.length;
        });
    },

    _addGraphButton: function(td, testName, testResults)
    {
        var data = testResults[Strings.json.samples];
        if (!data)
            return;

        var button = Utilities.createElement("button", { class: "small-button" }, td);

        button.addEventListener("click", function() {
            var score = testResults[Strings.json.score].toFixed(2);
            var complexity = testResults[Strings.json.experiments.complexity];
            var mean = [
                "mean: ",
                complexity[Strings.json.measurements.average].toFixed(2),
                " ± ",
                complexity[Strings.json.measurements.stdev].toFixed(2),
                " (",
                complexity[Strings.json.measurements.percent].toFixed(2),
                "%), worst 5%: ",
                complexity[Strings.json.measurements.concern].toFixed(2)].join("");

            var graphData = {
                axes: [Strings.text.experiments.complexity, Strings.text.experiments.frameRate],
                mean: [
                    testResults[Strings.json.experiments.complexity][Strings.json.measurements.average],
                    testResults[Strings.json.experiments.frameRate][Strings.json.measurements.average]
                ],
                samples: data,
                samplingTimeOffset: testResults[Strings.json.samplingTimeOffset]
            }
            if (testResults[Strings.json.targetFPS])
                graphData.targetFPS = testResults[Strings.json.targetFPS];
            benchmarkController.showTestGraph(testName, score, mean, graphData);
        });

        button.textContent = Strings.text.results.graph + "...";
    },

    _isNoisyMeasurement: function(jsonExperiment, data, measurement, options)
    {
        const percentThreshold = 10;
        const averageThreshold = 2;

        if (measurement == Strings.json.measurements.percent)
            return data[Strings.json.measurements.percent] >= percentThreshold;

        if (jsonExperiment == Strings.json.experiments.frameRate && measurement == Strings.json.measurements.average)
            return Math.abs(data[Strings.json.measurements.average] - options["frame-rate"]) >= averageThreshold;

        return false;
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

        var isNoisy = false;
        [Strings.json.experiments.complexity, Strings.json.experiments.frameRate].forEach(function (experiment) {
            var data = testResults[experiment];
            for (var measurement in data) {
                if (this._isNoisyMeasurement(experiment, data, measurement, options))
                    isNoisy = true;
            }
        }, this);

        this._flattenedHeaders.forEach(function (header) {
            var className = "";
            if (header.className) {
                if (typeof header.className == "function")
                    className = header.className(testResults, options);
                else
                    className = header.className;
            }

            if (header.title == Strings.text.testName) {
                var titleClassName = className;
                if (isNoisy)
                    titleClassName += " noisy-results";
                var td = Utilities.createElement("td", { class: titleClassName }, row);
                td.textContent = testName;
                return;
            }

            var td = Utilities.createElement("td", { class: className }, row);
            if (header.title == Strings.text.results.graph) {
                this._addGraphButton(td, testName, testResults);
            } else if (!("text" in header)) {
                td.textContent = testResults[header.title];
            } else if (typeof header.text == "string") {
                var data = testResults[header.text];
                if (typeof data == "number")
                    data = data.toFixed(2);
                td.textContent = data;
            } else {
                td.textContent = header.text(testResults, testName);
            }
        }, this);
    },

    _addSuite: function(suiteName, suiteResults, options)
    {
        for (var testName in suiteResults[Strings.json.results.tests]) {
            var testResults = suiteResults[Strings.json.results.tests][testName];
            this._addTest(testName, testResults, options);
        }
    },

    _addIteration: function(iterationResult, options)
    {
        for (var suiteName in iterationResult[Strings.json.results.suites]) {
            this._addEmptyRow();
            this._addSuite(suiteName, iterationResult[Strings.json.results.suites][suiteName], options);
        }
    },

    showIterations: function(iterationsResults, options)
    {
        this.clear();
        this._addHeader();

        iterationsResults.forEach(function(iterationResult) {
            this._addIteration(iterationResult, options);
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
        this.results = new ResultsDashboard();
    },

    didRunSuites: function(suitesSamplers)
    {
        this.results.push(suitesSamplers);
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

    populateTable: function(tableIdentifier, headers, data)
    {
        var table = new ResultsTable(document.getElementById(tableIdentifier), headers);
        table.showIterations(data, benchmarkRunnerClient.options);
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
            "kalman-measurement-error": 4
        };
        this._startBenchmark(Suites, options, "test-container");
    },

    showResults: function()
    {
        if (!this.addedKeyEvent) {
            document.addEventListener("keypress", this.selectResults, false);
            this.addedKeyEvent = true;
        }

        sectionsManager.setSectionScore("results", benchmarkRunnerClient.results.score.toFixed(2));
        var data = benchmarkRunnerClient.results.data[Strings.json.results.iterations];
        sectionsManager.populateTable("results-header", Headers.testName, data);
        sectionsManager.populateTable("results-score", Headers.score, data);
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
