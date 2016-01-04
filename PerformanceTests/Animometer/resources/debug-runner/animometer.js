Utilities.extendObject(window.benchmarkRunnerClient, {
    testsCount: null,
    progressBar: null,

    initialize: function(suites, options)
    {
        this.testsCount = this.iterationCount * suites.reduce(function (count, suite) { return count + suite.tests.length; }, 0);
        this.options = options;
    },

    willStartFirstIteration: function ()
    {
        this.results = new ResultsDashboard();
        this.progressBar = new ProgressBar(document.getElementById("progress-completed"), this.testsCount);
    },

    didRunTest: function()
    {
        this.progressBar.incrementRange();
    }
});

Utilities.extendObject(window.sectionsManager, {
    setSectionHeader: function(sectionIdentifier, title)
    {
        document.querySelector("#" + sectionIdentifier + " h1").textContent = title;
    }
});

window.optionsManager =
{
    valueForOption: function(name)
    {
        var formElement = document.forms["benchmark-options"].elements[name];
        if (formElement.type == "checkbox")
            return formElement.checked;
        return formElement.value;
    },

    updateUIFromLocalStorage: function()
    {
        var formElements = document.forms["benchmark-options"].elements;

        for (var i = 0; i < formElements.length; ++i) {
            var formElement = formElements[i];
            var name = formElement.id || formElement.name;
            var type = formElement.type;

            var value = localStorage.getItem(name);
            if (value === null)
                continue;

            if (type == "number")
                formElements[name].value = +value;
            else if (type == "checkbox")
                formElements[name].checked = value == "true";
            else if (type == "radio")
                formElements[name].value = value;
        }
    },

    updateLocalStorageFromUI: function()
    {
        var formElements = document.forms["benchmark-options"].elements;
        var options = {};

        for (var i = 0; i < formElements.length; ++i) {
            var formElement = formElements[i];
            var name = formElement.id || formElement.name;
            var type = formElement.type;

            if (type == "number")
                options[name] = formElement.value;
            else if (type == "checkbox")
                options[name] = formElement.checked;
            else if (type == "radio")
                options[name] = formElements[name].value;

            localStorage.setItem(name, options[name]);
        }

        return options;
    }
}

window.suitesManager =
{
    _treeElement: function()
    {
        return document.querySelector("#suites > .tree");
    },

    _suitesElements: function()
    {
        return document.querySelectorAll("#suites > ul > li");
    },

    _checkboxElement: function(element)
    {
        return element.querySelector("input[type='checkbox']:not(.expand-button)");
    },

    _editElement: function(element)
    {
        return element.querySelector("input[type='number']");
    },

    _editsElements: function()
    {
        return document.querySelectorAll("#suites input[type='number']");
    },

    _localStorageNameForTest: function(suiteName, testName)
    {
        return suiteName + "/" + testName;
    },

    _updateSuiteCheckboxState: function(suiteCheckbox)
    {
        var numberEnabledTests = 0;
        suiteCheckbox.testsElements.forEach(function(testElement) {
            var testCheckbox = this._checkboxElement(testElement);
            if (testCheckbox.checked)
                ++numberEnabledTests;
        }, this);
        suiteCheckbox.checked = numberEnabledTests > 0;
        suiteCheckbox.indeterminate = numberEnabledTests > 0 && numberEnabledTests < suiteCheckbox.testsElements.length;
    },

    _updateStartButtonState: function()
    {
        var suitesElements = this._suitesElements();
        var startButton = document.querySelector("#intro button");

        for (var i = 0; i < suitesElements.length; ++i) {
            var suiteElement = suitesElements[i];
            var suiteCheckbox = this._checkboxElement(suiteElement);

            if (suiteCheckbox.checked) {
                startButton.disabled = false;
                return;
            }
        }

        startButton.disabled = true;
    },

    _onChangeSuiteCheckbox: function(event)
    {
        var selected = event.target.checked;
        event.target.testsElements.forEach(function(testElement) {
            var testCheckbox = this._checkboxElement(testElement);
            testCheckbox.checked = selected;
        }, this);
        this._updateStartButtonState();
    },

    _onChangeTestCheckbox: function(event)
    {
        var suiteCheckbox = event.target.suiteCheckbox;
        this._updateSuiteCheckboxState(suiteCheckbox);
        this._updateStartButtonState();
    },

    _createSuiteElement: function(treeElement, suite, id)
    {
        var suiteElement = DocumentExtension.createElement("li", {}, treeElement);
        var expand = DocumentExtension.createElement("input", { type: "checkbox",  class: "expand-button", id: id }, suiteElement);
        var label = DocumentExtension.createElement("label", { class: "tree-label", for: id }, suiteElement);

        var suiteCheckbox = DocumentExtension.createElement("input", { type: "checkbox" }, label);
        suiteCheckbox.suite = suite;
        suiteCheckbox.onchange = this._onChangeSuiteCheckbox.bind(this);
        suiteCheckbox.testsElements = [];

        label.appendChild(document.createTextNode(" " + suite.name));
        return suiteElement;
    },

    _createTestElement: function(listElement, test, suiteCheckbox)
    {
        var testElement = DocumentExtension.createElement("li", {}, listElement);
        var span = DocumentExtension.createElement("label", { class: "tree-label" }, testElement);

        var testCheckbox = DocumentExtension.createElement("input", { type: "checkbox" }, span);
        testCheckbox.test = test;
        testCheckbox.onchange = this._onChangeTestCheckbox.bind(this);
        testCheckbox.suiteCheckbox = suiteCheckbox;

        suiteCheckbox.testsElements.push(testElement);
        span.appendChild(document.createTextNode(" " + test.name));
        DocumentExtension.createElement("input", { type: "number" }, testElement);
        return testElement;
    },

    createElements: function()
    {
        var treeElement = this._treeElement();

        Suites.forEach(function(suite, index) {
            var suiteElement = this._createSuiteElement(treeElement, suite, "suite-" + index);
            var listElement = DocumentExtension.createElement("ul", {}, suiteElement);
            var suiteCheckbox = this._checkboxElement(suiteElement);

            suite.tests.forEach(function(test) {
                var testElement = this._createTestElement(listElement, test, suiteCheckbox);
            }, this);
        }, this);
    },

    updateEditsElementsState: function()
    {
        var editsElements = this._editsElements();
        var showComplexityInputs = optionsManager.valueForOption("adjustment") == "fixed";

        for (var i = 0; i < editsElements.length; ++i) {
            var editElement = editsElements[i];
            if (showComplexityInputs)
                editElement.classList.add("selected");
            else
                editElement.classList.remove("selected");
        }
    },

    updateDisplay: function()
    {
        document.body.className = "display-" + optionsManager.valueForOption("display");
    },

    updateUIFromLocalStorage: function()
    {
        var suitesElements = this._suitesElements();

        for (var i = 0; i < suitesElements.length; ++i) {
            var suiteElement = suitesElements[i];
            var suiteCheckbox = this._checkboxElement(suiteElement);
            var suite = suiteCheckbox.suite;

            suiteCheckbox.testsElements.forEach(function(testElement) {
                var testCheckbox = this._checkboxElement(testElement);
                var testEdit = this._editElement(testElement);
                var test = testCheckbox.test;

                var str = localStorage.getItem(this._localStorageNameForTest(suite.name, test.name));
                if (str === null)
                    return;

                var value = JSON.parse(str);
                testCheckbox.checked = value.checked;
                testEdit.value = value.complexity;
            }, this);

            this._updateSuiteCheckboxState(suiteCheckbox);
        }

        this._updateStartButtonState();
    },

    updateLocalStorageFromUI: function()
    {
        var suitesElements = this._suitesElements();
        var suites = [];

        for (var i = 0; i < suitesElements.length; ++i) {
            var suiteElement = suitesElements[i];
            var suiteCheckbox = this._checkboxElement(suiteElement);
            var suite = suiteCheckbox.suite;

            var tests = [];
            suiteCheckbox.testsElements.forEach(function(testElement) {
                var testCheckbox = this._checkboxElement(testElement);
                var testEdit = this._editElement(testElement);
                var test = testCheckbox.test;

                if (testCheckbox.checked) {
                    test.complexity = testEdit.value;
                    tests.push(test);
                }

                var value = { checked: testCheckbox.checked, complexity: testEdit.value };
                localStorage.setItem(this._localStorageNameForTest(suite.name, test.name), JSON.stringify(value));
            }, this);

            if (tests.length)
                suites.push(new Suite(suiteCheckbox.suite.name, tests));
        }

        return suites;
    },

    updateLocalStorageFromJSON: function(iterationResults)
    {
        for (var suiteName in iterationResults[Strings.json.results.suites]) {
            var suiteResults = iterationResults[Strings.json.results.suites][suiteName];

            for (var testName in suiteResults[Strings.json.results.tests]) {
                var testResults = suiteResults[Strings.json.results.tests][testName];
                var data = testResults[Strings.json.experiments.complexity];
                var complexity = Math.round(data[Strings.json.measurements.average]);

                var value = { checked: true, complexity: complexity };
                localStorage.setItem(this._localStorageNameForTest(suiteName, testName), JSON.stringify(value));
            }
        }
    }
}

Utilities.extendObject(window.benchmarkController, {
    initialize: function()
    {
        document.forms["benchmark-options"].addEventListener("change", benchmarkController.onBenchmarkOptionsChanged, true);
        document.forms["graph-options"].addEventListener("change", benchmarkController.onGraphOptionsChanged, true);
        optionsManager.updateUIFromLocalStorage();
        suitesManager.createElements();
        suitesManager.updateUIFromLocalStorage();
        suitesManager.updateDisplay();
        suitesManager.updateEditsElementsState();
    },

    onBenchmarkOptionsChanged: function(event)
    {
        if (event.target.name == "adjustment") {
            suitesManager.updateEditsElementsState();
            return;
        }
        if (event.target.name == "display") {
            suitesManager.updateDisplay();
        }
    },

    startBenchmark: function()
    {
        var options = optionsManager.updateLocalStorageFromUI();
        var suites = suitesManager.updateLocalStorageFromUI();
        this._startBenchmark(suites, options, "running-test");
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
        sectionsManager.populateTable("results-data", Headers.details, data);
        sectionsManager.showSection("results", true);

        suitesManager.updateLocalStorageFromJSON(data[0]);
    },

    showJSONResults: function()
    {
        document.querySelector("#results-json textarea").textContent = JSON.stringify(benchmarkRunnerClient.results.data, function(key, value) {
            if (typeof value == "number")
                return value.toFixed(2);
            return value;
        });
        document.querySelector("#results-json button").remove();
        document.querySelector("#results-json div").classList.remove("hidden");
    },

    showTestGraph: function(testName, score, mean, graphData)
    {
        sectionsManager.setSectionHeader("test-graph", testName);
        sectionsManager.setSectionScore("test-graph", score, mean);
        sectionsManager.showSection("test-graph", true);
        this.updateGraphData(graphData);
    }
});

window.addEventListener("load", benchmarkController.initialize);
