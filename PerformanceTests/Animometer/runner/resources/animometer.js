window.benchmarkRunnerClient = {
    iterationCount: 1,
    testsCount: null,
    progressBar: null,
    recordTable: null,
    options: { testInterval: 30000, frameRate: 50, estimatedFrameRate: true, fixTestComplexity : false },
    score: 0,
    _iterationsSamplers: [],
    _resultsTable: null,
    
    willAddTestFrame: function (frame)
    {
        var main = document.querySelector("main");
        var style = getComputedStyle(main);
        frame.style.left = main.offsetLeft + parseInt(style.borderLeftWidth) + parseInt(style.paddingLeft) + "px";
        frame.style.top = main.offsetTop + parseInt(style.borderTopWidth) + parseInt(style.paddingTop) + "px";
    },
    
    didRunTest: function ()
    {
        this.progressBar.incRange();
    },
    
    willStartFirstIteration: function ()
    {
        this._iterationsSamplers = [];
        this._resultsTable = new RecordTable(document.querySelectorAll(".results-table")[0]);
        
        this.progressBar = new ProgressBar(document.getElementById("progress-completed"), this.testsCount);
        this.recordTable = new RecordTable(document.querySelectorAll(".record-table")[0]);
    },
    
    didRunSuites: function (suitesSamplers)
    {
        this._iterationsSamplers.push(suitesSamplers);
    },
    
    didFinishLastIteration: function ()
    {
        this.score = this._resultsTable.showIterations(this._iterationsSamplers, "");
        showResults();
    }
}

function showSection(sectionIdentifier, pushState)
{
    var currentSectionElement = document.querySelector("section.selected");
    console.assert(currentSectionElement);

    var newSectionElement = document.getElementById(sectionIdentifier);
    console.assert(newSectionElement);

    currentSectionElement.classList.remove("selected");
    newSectionElement.classList.add("selected");

    if (pushState)
        history.pushState({section: sectionIdentifier}, document.title);
}

function startBenchmark()
{
    var checkboxes = [ 
        document.getElementById("html-suite"), 
        document.getElementById("canvas-suite"), 
        document.getElementById("svg-suite"),
        document.getElementById("examples-suite"),
        document.getElementById("template-suite"),
    ];
    var enabledSuites = Suites.filter(function (suite, index) { return !suite.disabled && checkboxes[index].checked; });
    var testsCount = enabledSuites.reduce(function (testsCount, suite) { return testsCount + suite.tests.length; }, 0);
    benchmarkRunnerClient.testsCount = benchmarkRunnerClient.iterationCount * testsCount;
    benchmarkRunnerClient.options["testInterval"] = parseInt(document.getElementById("test-interval").value) * 1000;
    benchmarkRunnerClient.options["frameRate"] = parseInt(document.getElementById("frame-rate").value);
    benchmarkRunnerClient.options["estimatedFrameRate"] = document.getElementById("estimated-frame-rate").checked;    
    benchmarkRunnerClient.options["fixTestComplexity"] = document.getElementById("fix-test-complexity").checked;
    benchmarkRunnerClient.options["showRunningResults"] = document.getElementById("show-running-results").checked;
    
    if (!benchmarkRunnerClient.options["showRunningResults"])
        document.getElementById("record").style.display = "none";

    var runner = new BenchmarkRunner(enabledSuites, benchmarkRunnerClient);
    runner.runMultipleIterations(benchmarkRunnerClient.iterationCount);
}

function startTest()
{
    showSection("running");
    startBenchmark();
}

function showResults(score)
{
    var element = document.querySelector("#results > h1");
    element.textContent = "Results:"
    
    var score = benchmarkRunnerClient.score.toFixed(2);
    element.textContent += " [Score = " + score + "]";
        
    showSection("results", true);
}

function showGraph(testName, axes, samples, samplingTimeOffset)
{
    var element = document.querySelector("#graph > h1");
    element.textContent = "Graph:"

    if (testName.length)
        element.textContent += " [test = " + testName + "]";
            
    graph("#graphContainer", new Point(700, 400), new Insets(20, 50, 20, 50), axes, samples, samplingTimeOffset);
    showSection("graph", true);    
}
