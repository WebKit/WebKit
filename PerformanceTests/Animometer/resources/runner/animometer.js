window.benchmarkRunnerClient = {
    iterationCount: 1,
    options: null,
    results: null,

    initialize: function(suites, options)
    {
        this.testsCount = this.iterationCount * suites.reduce(function (count, suite) { return count + suite.tests.length; }, 0);
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
            "estimated-frame-rate": true
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
