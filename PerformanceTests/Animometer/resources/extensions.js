function Point(x, y)
{
    this.x = x;
    this.y = y;
}

Point.pointOnCircle = function(angle, radius)
{
    return new Point(radius * Math.cos(angle), radius * Math.sin(angle));
}

Point.pointOnEllipse = function(angle, radiuses)
{
    return new Point(radiuses.x * Math.cos(angle), radiuses.y * Math.sin(angle));
}

Point.elementClientSize = function(element)
{
    return new Point(element.clientWidth, element.clientHeight);
}

Point.prototype =
{
    // Used when the point object is used as a size object.
    get width()
    {
        return this.x;
    },
    
    // Used when the point object is used as a size object.
    get height()
    {
        return this.y;
    },
    
    // Used when the point object is used as a size object.
    get center()
    {
        return new Point(this.x / 2, this.y / 2);
    },
    
    add: function(other)
    {
        return new Point(this.x + other.x, this.y + other.y);
    },
    
    subtract: function(other)
    {
        return new Point(this.x - other.x, this.y - other.y);
    },
    
    multiply: function(other)
    {
        return new Point(this.x * other.x, this.y * other.y);
    },
    
    move: function(angle, velocity, timeDelta)
    {
        return this.add(Point.pointOnCircle(angle, velocity * (timeDelta / 1000)));
    }
}

function Insets(top, right, bottom, left)
{
    this.top = top;
    this.right = right;
    this.bottom = bottom;
    this.left = left;
}

Insets.elementPadding = function(element)
{
    var styles = window.getComputedStyle(element);
    return new Insets(
        parseFloat(styles.paddingTop),
        parseFloat(styles.paddingRight),
        parseFloat(styles.paddingBottom),
        parseFloat(styles.paddingTop));
}

Insets.prototype =
{
    get width()
    {
        return this.left + this.right;
    },

    get height()
    {
        return this.top + this.bottom;
    },
    
    get size()
    {
        return new Point(this.width, this.height);
    }
}

function SimplePromise()
{
    this._chainedPromise = null;
    this._callback = null;
}

SimplePromise.prototype.then = function (callback)
{
    if (this._callback)
        throw "SimplePromise doesn't support multiple calls to then";
        
    this._callback = callback;
    this._chainedPromise = new SimplePromise;
    
    if (this._resolved)
        this.resolve(this._resolvedValue);

    return this._chainedPromise;
}

SimplePromise.prototype.resolve = function (value)
{
    if (!this._callback) {
        this._resolved = true;
        this._resolvedValue = value;
        return;
    }

    var result = this._callback(value);
    if (result instanceof SimplePromise) {
        var chainedPromise = this._chainedPromise;
        result.then(function (result) { chainedPromise.resolve(result); });
    } else
        this._chainedPromise.resolve(result);
}

window.DocumentExtension =
{
    createElement : function(name, attrs, parentElement)
    {
        var element = document.createElement(name);

        for (var key in attrs)
            element.setAttribute(key, attrs[key]);

        parentElement.appendChild(element);
        return element;
    },

    createSvgElement: function(name, attrs, xlinkAttrs, parentElement)
    {
        const svgNamespace = "http://www.w3.org/2000/svg";
        const xlinkNamespace = "http://www.w3.org/1999/xlink";

        var element = document.createElementNS(svgNamespace, name);
        
        for (var key in attrs)
            element.setAttribute(key, attrs[key]);
            
        for (var key in xlinkAttrs)
            element.setAttributeNS(xlinkNamespace, key, xlinkAttrs[key]);
            
        parentElement.appendChild(element);
        return element;
    }
}

function ProgressBar(element, ranges)
{
    this.element = element;
    this.ranges = ranges;
    this.currentRange = 0;
}

ProgressBar.prototype =
{
    _progressToPercent: function(progress)
    {
        return progress * (100 / this.ranges);
    },
    
    incRange: function()
    {
        ++this.currentRange;
    },
    
    setPos: function(progress)
    {
        this.element.style.width = this._progressToPercent(this.currentRange + progress) + "%";
    }
}

function ResultsDashboard()
{
    this._iterationsSamplers = [];
}

ResultsDashboard.prototype =
{
    push: function(suitesSamplers)
    {
        this._iterationsSamplers.push(suitesSamplers);        
    },
    
    toJSON: function(statistics, graph)
    {
        var iterationsResults = [];
        var iterationsScores = [];
        
        this._iterationsSamplers.forEach(function(iterationSamplers, index) {
            var suitesResults = {};
            var suitesScores = [];
        
            for (var suiteName in iterationSamplers) {
                var suite = suiteFromName(suiteName);
                var suiteSamplers = iterationSamplers[suiteName];

                var testsResults = {};
                var testsScores = [];
                
                for (var testName in suiteSamplers) {
                    var sampler = suiteSamplers[testName];
                    testsResults[testName] = sampler.toJSON(statistics, graph);
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

        var json = {};
        json[Strings.json.score] = Statistics.sampleMean(iterationsScores.length, iterationsScores.reduce(function(a, b) { return a * b; }));
        json[Strings.json.results.iterations] = iterationsResults;
        return json;
    }
}

function ResultsTable(element, headers)
{
    this.element = element;
    this._headers = headers;
    this.clear();
}

ResultsTable.prototype =
{
    clear: function()
    {
        this.element.innerHTML = "";
    },

    _showHeaderRow: function(row, queue, headers, message)
    {
        headers.forEach(function (header) {
            var th = DocumentExtension.createElement("th", {}, row);
            th.textContent = header.text;
            if (typeof message != "undefined" && message.length) {
                th.innerHTML += "<br>" + '[' + message +']';
                message = "";
            }
            if ("width" in header)
                th.width = header.width + "%";
            queue.push({element: th, headers: header.children });
        });
    },

    _showHeader: function(message)
    {
        var thead = DocumentExtension.createElement("thead", {}, this.element);
        var row = DocumentExtension.createElement("tr", {}, thead);

        var queue = [];
        this._showHeaderRow(row, queue, this._headers, message);

        while (queue.length) {
            var row = null;
            var entries = [];

            for (var i = 0, len = queue.length; i < len; ++i) {
                var entry = queue.shift();

                if (!entry.headers.length) {
                    entries.push(entry.element);
                    continue;
                }

                if (!row)
                    row = DocumentExtension.createElement("tr", {}, thead);

                this._showHeaderRow(row, queue, entry.headers, "");
                entry.element.colSpan = entry.headers.length;
            }

            if (row) {
                entries.forEach(function(entry) {
                    ++entry.rowSpan;
                });
            }
        }
    },
    
    _showEmptyCell: function(row, className)
    {
        return DocumentExtension.createElement("td", { class: className }, row);
    },

    _showText: function(row, text, className)
    {
        var td = DocumentExtension.createElement("td", { class: className }, row);
        td.textContent = text;
    },

    _showFixedNumber: function(row, value, digits, className)
    {
        var td = DocumentExtension.createElement("td", { class: className }, row);
        td.textContent = value.toFixed(digits || 2);
    },
    
    _showGraph: function(row, testName, testResults)
    {
        var data = testResults[Strings.json.samples];
        if (!data) {
            this._showEmptyCell(row, "");
            return;
        }
        
        var td = DocumentExtension.createElement("td", {}, row);
        var button = DocumentExtension.createElement("button", { class: "small-button" }, td);

        button.addEventListener("click", function() {
            var samples = data[Strings.json.graph.points];
            var samplingTimeOffset = data[Strings.json.graph.samplingTimeOffset];
            var axes = [Strings.text.experiments.complexity, Strings.text.experiments.frameRate];
            benchmarkController.showTestGraph(testName, axes, samples, samplingTimeOffset);
        });
            
        button.textContent = Strings.text.results.graph + "...";
    },

    _showJSON: function(row, testName, testResults)
    {
        var data = testResults[Strings.json.samples];
        if (!data) {
            this._showEmptyCell(row, "");
            return;
        }

        var td = DocumentExtension.createElement("td", {}, row);
        var button = DocumentExtension.createElement("button", { class: "small-button" }, td);

        button.addEventListener("click", function() {
            benchmarkController.showTestJSON(testName, testResults);
        });
            
        button.textContent = Strings.text.results.json + "...";
    },
    
    _isNoisyMeasurement: function(experiment, data, measurement, options)
    {
        const percentThreshold = 10;
        const averageThreshold = 2;
         
        if (measurement == Strings.json.measurements.percent)
            return data[Strings.json.measurements.percent] >= percentThreshold;
            
        if (experiment == Strings.json.experiments.frameRate && measurement == Strings.json.measurements.average)
            return Math.abs(data[Strings.json.measurements.average] - options["frame-rate"]) >= averageThreshold;

        return false;
    },

    _isNoisyTest: function(testResults, options)
    {
        for (var experiment in testResults) {
            var data = testResults[experiment];
            for (var measurement in data) {
                if (this._isNoisyMeasurement(experiment, data, measurement, options))
                    return true;
            }
        }
        return false;
    },

    _showEmptyCells: function(row, headers)
    {
        for (var index = 0; index < headers.length; ++index) {
            if (!headers[index].children.length)
                this._showEmptyCell(row, "suites-separator");
            else
                this._showEmptyCells(row, headers[index].children);
        }
    },

    _showEmptyRow: function()
    {
        var row = DocumentExtension.createElement("tr", {}, this.element);
        this._showEmptyCells(row, this._headers);
    },

    _showTest: function(testName, testResults, options)
    {
        var row = DocumentExtension.createElement("tr", {}, this.element);
        var className = this._isNoisyTest(testResults, options) ? "noisy-results" : "";
        
        for (var index = 0; index < this._headers.length; ++index) {

            switch (index) {
            case 0:
                this._showText(row, testName, className);
                break;

            case 1:
                var data = testResults[Strings.json.score];
                this._showFixedNumber(row, data, 2);
                break;

            case 2:
            case 3:
                var data = testResults[index == 2 ? Strings.json.experiments.complexity : Strings.json.experiments.frameRate];
                for (var measurement in data)
                    this._showFixedNumber(row, data[measurement], 2, this._isNoisyMeasurement(index - 2, data, measurement, options) ? className : "");
                break;
                
            case 4:
                this._showGraph(row, testName, testResults);
                this._showJSON(row, testName, testResults);
                break;
            }
        }
    },

    _showSuite: function(suiteName, suiteResults, options)
    {
        for (var testName in suiteResults[Strings.json.results.tests]) {
            this._showTest(testName, suiteResults[Strings.json.results.tests][testName], options);
        }
    },
    
    _showIteration : function(iterationResults, options)
    {
        for (var suiteName in iterationResults[Strings.json.results.suites]) {
            if (suiteName != Object.keys(iterationResults[Strings.json.results.suites])[0])
                this._showEmptyRow();
            this._showSuite(suiteName, iterationResults[Strings.json.results.suites][suiteName], options);
        }
    },
    
    showRecord: function(testName, message, testResults, options)
    {
        this.clear();
        this._showHeader(message);
        this._showTest(testName, testResults, options);
    },

    showIterations: function(iterationsResults, options)
    {
        this.clear();
        this._showHeader("");
        
        iterationsResults.forEach(function(iterationResults) {
            this._showIteration(iterationResults, options);
        }, this);
    }
}

