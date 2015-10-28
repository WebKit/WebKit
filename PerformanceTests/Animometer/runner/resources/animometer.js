window.benchmarkRunnerClient = {
    iterationCount: 1,
    testsCount: null,
    progressBar: null,
    recordTable: null,
    options: { testInterval: 30000, frameRate: 50, estimatedFrameRate: true, fixTestComplexity : false },
    score: 0,
    _resultsDashboard: null,
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
        this._resultsDashboard = new ResultsDashboard();
        this._resultsTable = new ResultsTable(document.querySelector(".results-table"), Headers);
        
        this.progressBar = new ProgressBar(document.getElementById("progress-completed"), this.testsCount);
        this.recordTable = new ResultsTable(document.querySelector(".record-table"), Headers);
    },
    
    didRunSuites: function (suitesSamplers)
    {
        this._resultsDashboard.push(suitesSamplers);
    },
    
    didFinishLastIteration: function ()
    {
        var json = this._resultsDashboard.toJSON(true, true);
        this._resultsTable.showIterations(json[Strings["JSON_RESULTS"][0]]);
        
        var element = document.querySelector("#json > textarea");
        element.innerHTML = JSON.stringify(json[Strings["JSON_RESULTS"][0]][0], function(key, value) { 
            if (typeof value == "number")
                return value.toFixed(2);
            return value;
        }, 4);
        
        this.score = json[Strings["JSON_SCORE"]];
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
    var enabledSuites = [];

    localStorage.clear();
    var suiteItems = document.querySelectorAll("#suites > ul > li");
    for (var i = 0; i < suiteItems.length; ++i) {
        var suiteItem = suiteItems[i];
        var suiteCheckbox = suiteItem.querySelector("input");

        if (!suiteCheckbox.checked)
            continue;

        var enabledTests = [];
        var testCheckboxes = suiteItem.querySelector("ul").querySelectorAll("input");
        for (var j = 0; j < testCheckboxes.length; ++j) {
            var testCheckbox = testCheckboxes[j];
            if (!testCheckbox.checked)
                continue;

            enabledTests.push(testCheckbox.test);
            localStorage.setItem(localStorageNameForTest(suiteCheckbox.suite, testCheckbox.test), +testCheckbox.checked);
        }

        enabledSuites.push(new Suite(suiteCheckbox.suite.name, enabledTests));
    }

    localStorage.setItem("test-interval", document.getElementById("test-interval").value);

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

function showResults()
{
    var element = document.querySelector("#results > h1");
    element.textContent = Strings["TEXT_RESULTS"][0] + ":";
    
    var score = benchmarkRunnerClient.score.toFixed(2);
    element.textContent += " [Score = " + score + "]";
        
    showSection("results", true);
}

function showJson()
{
    var element = document.querySelector("#json > h1");
    element.textContent = Strings["TEXT_RESULTS"][2] + ":";
    
    var score = benchmarkRunnerClient.score.toFixed(2);
    element.textContent += " [Score = " + score + "]";

    showSection("json", true);
}

function showTestGraph(testName, axes, samples, samplingTimeOffset)
{
    var element = document.querySelector("#test-graph > h1");
    element.textContent = Strings["TEXT_RESULTS"][1] + ":";

    if (testName.length)
        element.textContent += " [test = " + testName + "]";
            
    graph("#graphContainer", new Point(700, 400), new Insets(20, 50, 20, 50), axes, samples, samplingTimeOffset);
    showSection("test-graph", true);    
}

function showTestJSON(testName, testResults)
{
    var element = document.querySelector("#test-json > h1");
    element.textContent = Strings["TEXT_RESULTS"][2] + ":";

    if (testName.length)
        element.textContent += " [test = " + testName + "]";
            
    var element = document.querySelector("#test-json > textarea");
    element.innerHTML = JSON.stringify(testResults, function(key, value) { 
        if (typeof value == "number")
            return value.toFixed(2);
        return value;
    }, 4);

    showSection("test-json", true);    
}

function initialize() {
    populateSettings();

    var toggleTestsCheckbox = document.getElementById("toggleTests");
    toggleTestsCheckbox.onchange = function(event) {
        if (event.target.checked)
            document.getElementById("suites").classList.add("showTests");
        else
            document.getElementById("suites").classList.remove("showTests");
    };
}

function updateSuiteSelection(event) {
    var selected = event.target.checked;
    var testCheckboxes = event.target.parentNode.parentNode.querySelector("ul").querySelectorAll("input");
    for (var i = 0; i < testCheckboxes.length; ++i) {
        testCheckboxes[i].checked = selected;
    }
}

function updateTestSelection(event) {
    var testsList = event.target.parentNode.parentNode.parentNode;
    var suiteCheckbox = testsList.parentNode.querySelector("label > input");

    updateSuiteCheckbox(testsList, suiteCheckbox);
}

function updateSuiteCheckbox(testsList, suiteCheckbox) {
    var numberEnabledTests = 0;
    var testCheckboxes = testsList.querySelectorAll("input");
    var totalCheckboxes = testCheckboxes.length;
    for (var i = 0; i < totalCheckboxes; ++i) {
        if (testCheckboxes[i].checked)
            ++numberEnabledTests;
    }
    suiteCheckbox.checked = numberEnabledTests > 0;
    suiteCheckbox.indeterminate = numberEnabledTests > 0 && numberEnabledTests < totalCheckboxes;
}

function localStorageNameForTest(suite, test) {
    return suite.name + "/" + test.name;
}

function populateSettings() {
    var suitesDiv = document.getElementById("suites");

    var suitesList = document.createElement("ul");
    suitesDiv.appendChild(suitesList);

    Suites.forEach(function(suite) {
        var suiteItem = document.createElement("li");
        suitesList.appendChild(suiteItem);

        var suiteLabel = document.createElement("label");
        suiteItem.appendChild(suiteLabel);

        var suiteCheckbox = document.createElement("input");
        suiteCheckbox.setAttribute("type", "checkbox");
        suiteCheckbox.suite = suite;
        suiteCheckbox.onchange = updateSuiteSelection;
        suiteLabel.appendChild(suiteCheckbox);
        suiteLabel.appendChild(document.createTextNode(" " + suite.name));

        var testsList = document.createElement("ul");
        suiteItem.appendChild(testsList);

        suite.tests.forEach(function(test) {
            var testItem = document.createElement("li");
            testsList.appendChild(testItem);

            var testLabel = document.createElement("label");
            testItem.appendChild(testLabel);

            var testCheckbox = document.createElement("input");
            testCheckbox.setAttribute("type", "checkbox");
            testCheckbox.test = test;
            testCheckbox.onchange = updateTestSelection;
            if (+localStorage.getItem(localStorageNameForTest(suite, test)))
                testCheckbox.checked = true;
            testLabel.appendChild(testCheckbox);
            testLabel.appendChild(document.createTextNode(" " + test.name));
        });

        updateSuiteCheckbox(testsList, suiteCheckbox);
    });

    var interval = localStorage.getItem("test-interval");
    if (interval) {
        document.getElementById("test-interval").value = interval;
    }
}
document.addEventListener("DOMContentLoaded", initialize);
