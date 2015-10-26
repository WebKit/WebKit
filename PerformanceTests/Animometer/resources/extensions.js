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

Insets.prototype =
{
    get width() {
        return this.left + this.right;
    },

    get height() {
        return this.top + this.bottom;
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

function Options(testInterval, frameRate)
{
    this.testInterval = testInterval;
    this.frameRate = frameRate;
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
                    testsScores.push(testsResults[testName][Strings["JSON_SCORE"]]);
                }

                suitesResults[suiteName] =  {};
                suitesResults[suiteName][Strings["JSON_SCORE"]] = Statistics.geometricMean(testsScores);
                suitesResults[suiteName][Strings["JSON_RESULTS"][2]] = testsResults;
                suitesScores.push(suitesResults[suiteName][Strings["JSON_SCORE"]]);
            }
            
            iterationsResults[index] = {};
            iterationsResults[index][Strings["JSON_SCORE"]] = Statistics.geometricMean(suitesScores);
            iterationsResults[index][Strings["JSON_RESULTS"][1]] = suitesResults;
            iterationsScores.push(iterationsResults[index][Strings["JSON_SCORE"]]);
        });

        var json = {};
        json[Strings["JSON_SCORE"]] = Statistics.sampleMean(iterationsScores.length, iterationsScores.reduce(function(a, b) { return a * b; }));
        json[Strings["JSON_RESULTS"][0]] = iterationsResults;
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
            var th = document.createElement("th");
            th.textContent = header.text;
            if (typeof message != "undefined" && message.length) {
                th.appendChild(document.createElement('br'));
                th.appendChild(document.createTextNode('[' + message +']'));
                message = "";
            }
            if ("width" in header)
                th.width = header.width + "%";
            row.appendChild(th);
            queue.push({element: th, headers: header.children });
        });
    },

    _showHeader: function(message)
    {
        var row = document.createElement("tr");

        var queue = [];
        this._showHeaderRow(row, queue, this._headers, message);
        this.element.appendChild(row);

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
                    var row = document.createElement("tr");

                this._showHeaderRow(row, queue, entry.headers, "");
                entry.element.colSpan = entry.headers.length;
            }

            if (row) {
                this.element.appendChild(row);
                entries.forEach(function(entry) {
                    ++entry.rowSpan;
                });
            }
        }
    },
    
    _showEmpty: function(row)
    {
        var td = document.createElement("td");
        row.appendChild(td);
    },

    _showText: function(row, text)
    {
        var td = document.createElement("td");
        td.textContent = text;
        row.appendChild(td);
    },

    _showFixedNumber: function(row, value, digits)
    {
        var td = document.createElement("td");
        td.textContent = value.toFixed(digits || 2);
        row.appendChild(td);
    },
    
    _showGraph: function(row, testName, testResults)
    {
        var data = testResults[Strings["JSON_SAMPLES"][0]];
        if (!data) {
            this._showEmpty(row);
            return;
        }
        
        var td = document.createElement("td");
        var button = document.createElement("div");
        button.className = "small-button";

        button.addEventListener("click", function() {
            var samples = data[Strings["JSON_GRAPH"][0]];
            var samplingTimeOffset = data[Strings["JSON_GRAPH"][1]];
            var axes = Strings["TEXT_EXPERIMENTS"];
            window.showTestGraph(testName, axes, samples, samplingTimeOffset);
        });
            
        button.textContent = Strings["TEXT_RESULTS"][1] + "...";
        td.appendChild(button);
        row.appendChild(td);
    },

    _showJSON: function(row, testName, testResults)
    {
        var data = testResults[Strings["JSON_SAMPLES"][0]];
        if (!data) {
            this._showEmpty(row);
            return;
        }
        
        var td = document.createElement("td");
        var button = document.createElement("div");
        button.className = "small-button";

        button.addEventListener("click", function() {
            window.showTestJSON(testName, testResults);
        });
            
        button.textContent = Strings["TEXT_RESULTS"][2] + "...";
        td.appendChild(button);
        row.appendChild(td);
    },

    _showTest: function(testName, testResults)
    {
        var row = document.createElement("tr");
        
        for (var index = 0; index < this._headers.length; ++index) {

            switch (index) {
            case 0:
                this._showText(row, testName);
                break;

            case 1:
                var data = testResults[Strings["JSON_SCORE"][0]];
                this._showFixedNumber(row, data, 2);
                break;

            case 2:
            case 3:
                var data = testResults[Strings["JSON_EXPERIMENTS"][index - 2]];
                for (var measurement in data)
                    this._showFixedNumber(row, data[measurement], 2);
                break;
                
            case 4:
                this._showGraph(row, testName, testResults);
                this._showJSON(row, testName, testResults);
                break;
            }
        }
        
        this.element.appendChild(row);
    },

    _showSuite: function(suiteName, suiteResults)
    {
        for (var testName in suiteResults[Strings["JSON_RESULTS"][2]]) {
            this._showTest(testName, suiteResults[Strings["JSON_RESULTS"][2]][testName]);
        }
    },
    
    _showIteration : function(iterationResults)
    {
        for (var suiteName in iterationResults[Strings["JSON_RESULTS"][1]]) {
            this._showSuite(suiteName, iterationResults[Strings["JSON_RESULTS"][1]][suiteName]);
        }
    },
    
    showRecord: function(testName, message, testResults)
    {
        this.clear();
        this._showHeader(message);
        this._showTest(testName, testResults);
    },

    showIterations: function(iterationsResults)
    {
        this.clear();
        this._showHeader("");
        
        iterationsResults.forEach(function(iterationResults) {
            this._showIteration(iterationResults);
        }, this);
    }
}
