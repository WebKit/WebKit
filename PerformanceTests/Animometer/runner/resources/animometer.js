window.benchmarkRunnerClient = {
    iterationCount: 1,
    testsCount: null,
    progressBar: null,
    recordTable: null,
    options: null,
    score: 0,
    _resultsDashboard: null,
    _resultsTable: null,
    
    initialize: function(suites, options)
    {
        this.testsCount = this.iterationCount * suites.reduce(function (count, suite) { return count + suite.tests.length; }, 0);
        this.options = options;
    },

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
        this._resultsTable = new ResultsTable(document.querySelector("section#results > data > table"), Headers);
        
        this.progressBar = new ProgressBar(document.getElementById("progress-completed"), this.testsCount);
        this.recordTable = new ResultsTable(document.querySelector("section#running > #record > table"), Headers);
    },
    
    didRunSuites: function (suitesSamplers)
    {
        this._resultsDashboard.push(suitesSamplers);
    },
    
    didFinishLastIteration: function ()
    {
        var json = this._resultsDashboard.toJSON(true, true);
        this.score = json[Strings["JSON_SCORE"]];
        this._resultsTable.showIterations(json[Strings["JSON_RESULTS"][0]]);
        sectionsManager.showJSON("json", json[Strings["JSON_RESULTS"][0]][0]);
        suitesManager.updateLocalStorageFromJSON(json[Strings["JSON_RESULTS"][0]][0]);
        benchmarkController.showResults();
    }
}

window.sectionsManager =
{
    _sectionHeaderH1Element: function(sectionIdentifier)
    {
        return document.querySelector("#" + sectionIdentifier + " > header > h1");
    },
    
    _sectionDataDivElement: function(sectionIdentifier)
    {
        return document.querySelector("#" + sectionIdentifier + " >  data > div");
    },
    
    showScore: function(sectionIdentifier, title)
    {
        var element = this._sectionHeaderH1Element(sectionIdentifier);
        element.textContent = title + ":";

        var score = benchmarkRunnerClient.score.toFixed(2);
        element.textContent += " [Score = " + score + "]";
    },
    
    showTestName: function(sectionIdentifier, title, testName)
    {
        var element = this._sectionHeaderH1Element(sectionIdentifier);
        element.textContent = title + ":";

        if (!testName.length)
            return;
            
        element.textContent += " [test = " + testName + "]";
    },
    
    showJSON: function(sectionIdentifier, json)
    {
        var element = this._sectionDataDivElement(sectionIdentifier);
        element.textContent = JSON.stringify(json, function(key, value) { 
            if (typeof value == "number")
                return value.toFixed(2);
            return value;
        }, 4);
    },
    
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

    setupSectionStyle: function()
    {
        if (screen.width >= 1800 && screen.height >= 1000)
            DocumentExtension.insertCssRuleAfter(" section { width: 1600px; height: 800px; }", "section");
        else
            DocumentExtension.insertCssRuleAfter(" section { width: 800px; height: 600px; }", "section");
    },
    
    setupRunningSectionStyle: function(options)
    {
        if (!options["show-running-results"])
            document.getElementById("record").style.display = "none";

        if (options["normalize-for-device-scale-factor"] && window.devicePixelRatio != 1) {
            var percentage = window.devicePixelRatio * 100;
            var rule = "section#running > #running-test > iframe";
            var newRule = rule;
            newRule += " { ";
            newRule += "width: " + percentage + "%; ";
            newRule += "height: " + percentage + "%; ";
            newRule += "transform: scale(" + 100 / percentage + ") translate(" + (100 - percentage) / 2 + "%," + (100 - percentage) / 2 + "%);";
            newRule += " }";
            DocumentExtension.insertCssRuleAfter(newRule, rule);
        }
    }
}

window.optionsManager =
{
    _optionsElements : function()
    {
        return document.querySelectorAll("section#home > options input");;
    },
    
    _adaptiveTestElement: function()
    {
        return document.querySelector("section#home > options #adaptive-test");;
    },
    
    updateUIFromLocalStorage: function()
    {
        var optionsElements = this._optionsElements();

        for (var i = 0; i < optionsElements.length; ++i) {
            var optionElement = optionsElements[i];
            
            var value = localStorage.getItem(optionElement.id);
            if (value === null)
                continue;

            if (optionElement.getAttribute("type") == "number")
                optionElement.value = +value;
            else if (optionElement.getAttribute("type") == "checkbox")
                optionElement.checked = value == "true";
        }
    },

    updateLocalStorageFromUI: function()
    {
        var optionsElements = this._optionsElements();
        var options = {};        

        for (var i = 0; i < optionsElements.length; ++i) {
            var optionElement = optionsElements[i];
        
            if (optionElement.getAttribute("type") == "number")
                options[optionElement.id] = optionElement.value;
            else if (optionElement.getAttribute("type") == "checkbox")
                options[optionElement.id] = optionElement.checked;
    
            localStorage.setItem(optionElement.id, options[optionElement.id]);
        }
        
        return options;
    }
}

window.suitesManager =
{
    _treeElement : function()
    {
        return document.querySelector("suites > .tree");
    },
    
    _suitesElements : function()
    {
        return document.querySelectorAll("#home > suites > ul > li");
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
        return document.querySelectorAll("section#home > suites input[type='number']");
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
        var startButton = document.querySelector("#home > footer > button");
        
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
        var span = DocumentExtension.createElement("span", { class: "tree-label" }, testElement);

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
        var show = !optionsManager._adaptiveTestElement().checked;

        for (var i = 0; i < editsElements.length; ++i) {
            var editElement = editsElements[i];
            if (show)
                editElement.classList.add("selected");
            else
                editElement.classList.remove("selected");
        }
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
        for (var suiteName in iterationResults[Strings["JSON_RESULTS"][1]]) {
            var suiteResults = iterationResults[Strings["JSON_RESULTS"][1]][suiteName];

            for (var testName in suiteResults[Strings["JSON_RESULTS"][2]]) {
                var testResults = suiteResults[Strings["JSON_RESULTS"][2]][testName];
                var data = testResults[Strings["JSON_EXPERIMENTS"][0]]
                var complexity = Math.round(data[Strings["JSON_MEASUREMENTS"][0]]);

                var value = { checked: true, complexity: complexity };
                localStorage.setItem(this._localStorageNameForTest(suiteName, testName), JSON.stringify(value));
            }
        }
    }
}

window.benchmarkController =
{
    initialize: function()
    {
        sectionsManager.setupSectionStyle();
        optionsManager.updateUIFromLocalStorage();
        suitesManager.createElements();
        suitesManager.updateUIFromLocalStorage();
        suitesManager.updateEditsElementsState();
    },

    onChangeAdaptiveTestCheckbox: function()
    {
        suitesManager.updateEditsElementsState();
    },
    
    _runBenchmark: function(suites, options)
    {
        benchmarkRunnerClient.initialize(suites, options);
        var frameContainer = document.querySelector("#running > #running-test");
        var runner = new BenchmarkRunner(suites, frameContainer || document.body, benchmarkRunnerClient);
        runner.runMultipleIterations();
    },

    startTest: function()
    {
        var options = optionsManager.updateLocalStorageFromUI();
        var suites = suitesManager.updateLocalStorageFromUI();
        sectionsManager.setupRunningSectionStyle(options);
        this._runBenchmark(suites, options);
        sectionsManager.showSection("running");
    },
    
    showResults: function()
    {
        sectionsManager.showScore("results", Strings["TEXT_RESULTS"][0]);
        sectionsManager.showSection("results", true);
    },
    
    showJson: function()
    {
        sectionsManager.showScore("json", Strings["TEXT_RESULTS"][0]);
        sectionsManager.showSection("json", true);
    },
    
    showTestGraph: function(testName, axes, samples, samplingTimeOffset)
    {
        sectionsManager.showTestName("test-graph", Strings["TEXT_RESULTS"][1], testName);
        sectionsManager.showSection("test-graph", true);
        graph("section#test-graph > data", new Insets(20, 50, 20, 50), axes, samples, samplingTimeOffset);
    },

    showTestJSON: function(testName, json)
    {
        sectionsManager.showTestName("test-json", Strings["TEXT_RESULTS"][1], testName);
        sectionsManager.showJSON("test-json", json);
        sectionsManager.showSection("test-json", true);
    }
}

document.addEventListener("DOMContentLoaded", benchmarkController.initialize());
