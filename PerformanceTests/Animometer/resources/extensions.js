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

function RecordTable(element)
{
    this.element = element;
    this.clear();
}

RecordTable.prototype =
{
    clear: function()
    {
        this.element.innerHTML = "";
    },
    
    _showTitles: function(row, queue, titles, message)
    {
        titles.forEach(function (title) {
            var th = document.createElement("th");
            th.textContent = title.text;
            if (typeof message != "undefined" && message.length) {
                th.appendChild(document.createElement('br'));
                th.appendChild(document.createTextNode('[' + message +']'));
                message = "";
            }
            if ("width" in title)
                th.width = title.width + "%";
            row.appendChild(th);
            queue.push({element: th, titles: title.children });
        });
    },
    
    _showHeader: function(suiteName, titles, message)
    {
        var row = document.createElement("tr");

        var queue = [];
        this._showTitles(row, queue, titles, message);
        this.element.appendChild(row);

        while (queue.length) {
            var row = null;
            var entries = [];

            for (var i = 0, len = queue.length; i < len; ++i) {
                var entry = queue.shift();

                if (!entry.titles.length) {
                    entries.push(entry.element);
                    continue;
                }

                if (!row)
                    var row = document.createElement("tr");

                this._showTitles(row, queue, entry.titles, "");
                entry.element.colSpan = entry.titles.length;
            }

            if (row) {
                this.element.appendChild(row);
                entries.forEach(function(entry) {
                    ++entry.rowSpan;
                });
            }
        }
    },
    
    _showEmpty: function(row, testName)
    {
        var td = document.createElement("td");
        row.appendChild(td);
    },
    
    _showValue: function(row, testName, value)
    {
        var td = document.createElement("td");
        td.textContent = value.toFixed(2);
        row.appendChild(td);
    },
    
    _showSamples: function(row, testName, axes, samples, samplingTimeOffset)
    {
        var td = document.createElement("td");
        var button = document.createElement("div");
        button.className = "small-button";
            
        button.addEventListener("click", function() {
            window.showGraph(testName, axes, samples, samplingTimeOffset);
        });
            
        button.textContent = "Graph...";
        td.appendChild(button);
        row.appendChild(td);
    },
    
    _showTest: function(testName, titles, sampler, finalResults)
    {
        var row = document.createElement("tr");
        var td = document.createElement("td");
        
        td.textContent = testName;
        row.appendChild(td);
        
        var axes = [];
        sampler.experiments.forEach(function(experiment, index) {
            this._showValue(row, testName, experiment.mean());
            this._showValue(row, testName, experiment.concern(Experiment.defaults.CONCERN));
            this._showValue(row, testName, experiment.standardDeviation());
            this._showValue(row, testName, experiment.percentage());
            axes.push(titles[index + 1].text);
            
        }, this);

        this._showValue(row, testName, sampler.experiments[0].score(Experiment.defaults.CONCERN));

        if (finalResults)
            this._showSamples(row, testName, axes, sampler.samples, sampler.samplingTimeOffset);
        else
            this._showEmpty(row, testName);
            
        this.element.appendChild(row);
    },
    
    _showSuite: function(suite, suiteSamplers)
    {
        var scores = [];        
        for (var testName in suiteSamplers) {
            var test = testFromName(suite, testName);
            var sampler = suiteSamplers[testName]; 
            this._showTest(testName, suite.titles, sampler, true);
            scores.push(sampler.experiments[0].score(Experiment.defaults.CONCERN));
        }
        return scores;
    },
    
    showRecord: function(suite, test, sampler, message)
    {
        this.clear();        
        this._showHeader("", suite.titles, message);
        this._showTest(test.name, suite.titles, sampler, false);        
    },
    
    showIterations: function(iterationsSamplers)
    {
        this.clear();

        var scores = [];
        var titles = null;
        iterationsSamplers.forEach(function(suitesSamplers) {
            for (var suiteName in suitesSamplers) {
                var suite = suiteFromName(suiteName);
                if (titles != suite.titles) {
                    titles = suite.titles;
                    this._showHeader(suiteName, titles, "");
                }

                var suiteScores = this._showSuite(suite, suitesSamplers[suiteName]);
                scores.push.apply(scores, suiteScores);
            }
        }, this);

        return Statistics.geometricMean(scores);
    }
}
