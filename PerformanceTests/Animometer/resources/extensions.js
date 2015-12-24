Array.prototype.swap = function(i, j)
{
    var t = this[i];
    this[i] = this[j];
    this[j] = t;
    return this;
}

if (!Array.prototype.fill) {
    Array.prototype.fill = function(value) {
        if (this == null)
            throw new TypeError('Array.prototype.fill called on null or undefined');

        var object = Object(this);
        var len = parseInt(object.length, 10);
        var start = arguments[1];
        var relativeStart = parseInt(start, 10) || 0;
        var k = relativeStart < 0 ? Math.max(len + relativeStart, 0) : Math.min(relativeStart, len);
        var end = arguments[2];
        var relativeEnd = end === undefined ? len : (parseInt(end) || 0) ;
        var final = relativeEnd < 0 ? Math.max(len + relativeEnd, 0) : Math.min(relativeEnd, len);

        for (; k < final; k++)
            object[k] = value;

        return object;
    };
}

if (!Array.prototype.find) {
    Array.prototype.find = function(predicate) {
        if (this == null)
            throw new TypeError('Array.prototype.find called on null or undefined');
        if (typeof predicate !== 'function')
            throw new TypeError('predicate must be a function');

        var list = Object(this);
        var length = list.length >>> 0;
        var thisArg = arguments[1];
        var value;

        for (var i = 0; i < length; i++) {
            value = list[i];
            if (predicate.call(thisArg, value, i, list))
                return value;
        }
        return undefined;
    };
}

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
    createElement: function(name, attrs, parentElement)
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
    this._processedData = undefined;
}

ResultsDashboard.prototype =
{
    push: function(suitesSamplers)
    {
        this._iterationsSamplers.push(suitesSamplers);        
    },
    
    _processData: function(statistics, graph)
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
        this._processData(true, true);
        return this._processedData;
    },

    get score()
    {
        return this.data[Strings.json.score];
    }
}

function ResultsTable(element, headers)
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
}

ResultsTable.prototype =
{
    clear: function()
    {
        this.element.innerHTML = "";
    },

    _addHeader: function()
    {
        var thead = DocumentExtension.createElement("thead", {}, this.element);
        var row = DocumentExtension.createElement("tr", {}, thead);

        this._headers.forEach(function (header) {
            var th = DocumentExtension.createElement("th", {}, row);
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
        
        var button = DocumentExtension.createElement("button", { class: "small-button" }, td);

        button.addEventListener("click", function() {
            var samples = data[Strings.json.graph.points];
            var samplingTimeOffset = data[Strings.json.graph.samplingTimeOffset];
            var axes = [Strings.text.experiments.complexity, Strings.text.experiments.frameRate];
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
            benchmarkController.showTestGraph(testName, score, mean, axes, samples, samplingTimeOffset);
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
        var row = DocumentExtension.createElement("tr", {}, this.element);
        this._flattenedHeaders.forEach(function (header) {
            return DocumentExtension.createElement("td", { class: "suites-separator" }, row);
        });
    },

    _addTest: function(testName, testResults, options)
    {
        var row = DocumentExtension.createElement("tr", {}, this.element);

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
                var td = DocumentExtension.createElement("td", { class: titleClassName }, row);
                td.textContent = testName;
                return;
            }

            var td = DocumentExtension.createElement("td", { class: className }, row);
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
}

window.Utilities =
{
    _parse: function(str, sep)
    {
        var output = {};
        str.split(sep).forEach(function(part) {
            var item = part.split("=");
            var value = decodeURIComponent(item[1]);
            if (value[0] == "'" )
                output[item[0]] = value.substr(1, value.length - 2);
            else
                output[item[0]] = value;
          });
        return output;
    },

    parseParameters: function()
    {
        return this._parse(window.location.search.substr(1), "&");
    },

    parseArguments: function(str)
    {
        return this._parse(str, " ");
    },

    extendObject: function(obj1, obj2)
    {
        for (var attrname in obj2)
            obj1[attrname] = obj2[attrname];
        return obj1;
    },

    copyObject: function(obj)
    {
        return this.extendObject({}, obj);
    },

    mergeObjects: function(obj1, obj2)
    {
        return this.extendObject(this.copyObject(obj1), obj2);
    }
}
