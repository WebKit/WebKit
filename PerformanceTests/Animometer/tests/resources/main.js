Controller = Utilities.createClass(
    function(testLength, benchmark, options)
    {
        // Initialize timestamps relative to the start of the benchmark
        // In start() the timestamps are offset by the start timestamp
        this._startTimestamp = 0;
        this._endTimestamp = testLength;
        // Default data series: timestamp, complexity, estimatedFrameLength
        this._sampler = new Sampler(options["series-count"] || 3, 60 * testLength, this);
        this._estimator = new SimpleKalmanEstimator(options["kalman-process-error"], options["kalman-measurement-error"]);
        this._marks = {};

        this.initialComplexity = 0;
    }, {

    start: function(stage, startTimestamp)
    {
        this._startTimestamp = startTimestamp;
        this._endTimestamp += startTimestamp;
        this.recordFirstSample(stage, startTimestamp);
    },

    recordFirstSample: function(stage, startTimestamp)
    {
        this._sampler.record(startTimestamp, stage.complexity(), -1);
        this.mark(Strings.json.samplingStartTimeOffset, startTimestamp);
    },

    mark: function(comment, timestamp, data) {
        data = data || {};
        data.time = timestamp;
        data.index = this._sampler.sampleCount;
        this._marks[comment] = data;
    },

    containsMark: function(comment) {
        return comment in this._marks;
    },

    update: function(stage, timestamp)
    {
        this._estimator.sample(timestamp - this._sampler.samples[0][this._sampler.sampleCount - 1]);
        this._sampler.record(timestamp, stage.complexity(), this._estimator.estimate);
    },

    shouldStop: function(timestamp)
    {
        return timestamp > this._endTimestamp;
    },

    results: function()
    {
        return this._sampler.process();
    },

    processSamples: function(results)
    {
        var complexityExperiment = new Experiment;
        var smoothedFrameLengthExperiment = new Experiment;

        var samples = this._sampler.samples;

        var samplingStartIndex = 0, samplingEndIndex = -1;
        if (Strings.json.samplingStartTimeOffset in this._marks)
            samplingStartIndex = this._marks[Strings.json.samplingStartTimeOffset].index;
        if (Strings.json.samplingEndTimeOffset in this._marks)
            samplingEndIndex = this._marks[Strings.json.samplingEndTimeOffset].index;

        for (var markName in this._marks)
            this._marks[markName].time -= this._startTimestamp;
        results[Strings.json.marks] = this._marks;

        results[Strings.json.samples] = samples[0].map(function(timestamp, i) {
            var result = {
                // Represent time in milliseconds
                time: timestamp - this._startTimestamp,
                complexity: samples[1][i]
            };

            if (i == 0)
                result.frameLength = 1000/60;
            else
                result.frameLength = timestamp - samples[0][i - 1];

            if (samples[2][i] != -1)
                result.smoothedFrameLength = samples[2][i];

            // Don't start adding data to the experiments until we reach the sampling timestamp
            if (i >= samplingStartIndex && (samplingEndIndex == -1 || i < samplingEndIndex)) {
                complexityExperiment.sample(result.complexity);
                if (result.smoothedFrameLength && result.smoothedFrameLength != -1)
                    smoothedFrameLengthExperiment.sample(result.smoothedFrameLength);
            }

            return result;
        }, this);

        results[Strings.json.score] = complexityExperiment.score(Experiment.defaults.CONCERN);

        var complexityResults = {};
        results[Strings.json.experiments.complexity] = complexityResults;
        complexityResults[Strings.json.measurements.average] = complexityExperiment.mean();
        complexityResults[Strings.json.measurements.concern] = complexityExperiment.concern(Experiment.defaults.CONCERN);
        complexityResults[Strings.json.measurements.stdev] = complexityExperiment.standardDeviation();
        complexityResults[Strings.json.measurements.percent] = complexityExperiment.percentage();

        var smoothedFrameLengthResults = {};
        results[Strings.json.experiments.frameRate] = smoothedFrameLengthResults;
        smoothedFrameLengthResults[Strings.json.measurements.average] = 1000 / smoothedFrameLengthExperiment.mean();
        smoothedFrameLengthResults[Strings.json.measurements.concern] = smoothedFrameLengthExperiment.concern(Experiment.defaults.CONCERN);
        smoothedFrameLengthResults[Strings.json.measurements.stdev] = smoothedFrameLengthExperiment.standardDeviation();
        smoothedFrameLengthResults[Strings.json.measurements.percent] = smoothedFrameLengthExperiment.percentage();
    }
});

FixedComplexityController = Utilities.createSubclass(Controller,
    function(testInterval, benchmark, options)
    {
        Controller.call(this, testInterval, benchmark, options);
        this.initialComplexity = options["complexity"];
    }
);

AdaptiveController = Utilities.createSubclass(Controller,
    function(testInterval, benchmark, options)
    {
        // Data series: timestamp, complexity, estimatedIntervalFrameLength
        Controller.call(this, testInterval, benchmark, options);

        // All tests start at 0, so we expect to see 60 fps quickly.
        this._samplingTimestamp = testInterval / 2;
        this._startedSampling = false;
        this._targetFrameRate = options["frame-rate"];
        this._pid = new PIDController(this._targetFrameRate);

        this._intervalFrameCount = 0;
        this._numberOfFramesToMeasurePerInterval = 4;
    }, {

    start: function(stage, startTimestamp)
    {
        Controller.prototype.start.call(this, stage, startTimestamp);

        this._samplingTimestamp += startTimestamp;
        this._intervalTimestamp = startTimestamp;
    },

    recordFirstSample: function(stage, startTimestamp)
    {
        this._sampler.record(startTimestamp, stage.complexity(), -1);
    },

    update: function(stage, timestamp)
    {
        if (!this._startedSampling && timestamp > this._samplingTimestamp) {
            this._startedSampling = true;
            this.mark(Strings.json.samplingStartTimeOffset, this._samplingTimestamp);
        }

        // Start the work for the next frame.
        ++this._intervalFrameCount;

        if (this._intervalFrameCount < this._numberOfFramesToMeasurePerInterval) {
            this._sampler.record(timestamp, stage.complexity(), -1);
            return;
        }

        // Adjust the test to reach the desired FPS.
        var intervalLength = timestamp - this._intervalTimestamp;
        this._estimator.sample(intervalLength / this._numberOfFramesToMeasurePerInterval);
        var intervalEstimatedFrameRate = 1000 / this._estimator.estimate;
        var tuneValue = -this._pid.tune(timestamp - this._startTimestamp, intervalLength, intervalEstimatedFrameRate);
        tuneValue = tuneValue > 0 ? Math.floor(tuneValue) : Math.ceil(tuneValue);
        stage.tune(tuneValue);

        this._sampler.record(timestamp, stage.complexity(), this._estimator.estimate);

        // Start the next interval.
        this._intervalFrameCount = 0;
        this._intervalTimestamp = timestamp;
    },

    processSamples: function(results)
    {
        Controller.prototype.processSamples.call(this, results);
        results[Strings.json.targetFrameLength] = 1000 / this._targetFrameRate;
    }
});

Stage = Utilities.createClass(
    function()
    {
    }, {

    initialize: function(benchmark)
    {
        this._benchmark = benchmark;
        this._element = document.getElementById("stage");
        this._element.setAttribute("width", document.body.offsetWidth);
        this._element.setAttribute("height", document.body.offsetHeight);
        this._size = Point.elementClientSize(this._element).subtract(Insets.elementPadding(this._element).size);
    },

    get element()
    {
        return this._element;
    },

    get size()
    {
        return this._size;
    },

    complexity: function()
    {
        return 0;
    },

    tune: function()
    {
        throw "Not implemented";
    },

    animate: function()
    {
        throw "Not implemented";
    },

    clear: function()
    {
        return this.tune(-this.tune(0));
    }
});

Utilities.extendObject(Stage, {
    random: function(min, max)
    {
        return (Math.random() * (max - min)) + min;
    },

    randomBool: function()
    {
        return !!Math.round(this.random(0, 1));
    },

    randomInt: function(min, max)
    {
        return Math.round(this.random(min, max));
    },

    randomPosition: function(maxPosition)
    {
        return new Point(this.randomInt(0, maxPosition.x), this.randomInt(0, maxPosition.y));
    },

    randomSquareSize: function(min, max)
    {
        var side = this.random(min, max);
        return new Point(side, side);
    },

    randomVelocity: function(maxVelocity)
    {
        return this.random(maxVelocity / 8, maxVelocity);
    },

    randomAngle: function()
    {
        return this.random(0, Math.PI * 2);
    },

    randomColor: function()
    {
        var min = 32;
        var max = 256 - 32;
        return "#"
            + this.randomInt(min, max).toString(16)
            + this.randomInt(min, max).toString(16)
            + this.randomInt(min, max).toString(16);
    },

    randomRotater: function()
    {
        return new Rotater(this.random(1000, 10000));
    }
});

Rotater = Utilities.createClass(
    function(rotateInterval)
    {
        this._timeDelta = 0;
        this._rotateInterval = rotateInterval;
        this._isSampling = false;
    }, {

    get interval()
    {
        return this._rotateInterval;
    },

    next: function(timeDelta)
    {
        this._timeDelta = (this._timeDelta + timeDelta) % this._rotateInterval;
    },

    degree: function()
    {
        return (360 * this._timeDelta) / this._rotateInterval;
    },

    rotateZ: function()
    {
        return "rotateZ(" + Math.floor(this.degree()) + "deg)";
    },

    rotate: function(center)
    {
        return "rotate(" + Math.floor(this.degree()) + ", " + center.x + "," + center.y + ")";
    }
});

Benchmark = Utilities.createClass(
    function(stage, options)
    {
        this._animateLoop = this._animateLoop.bind(this);

        this._stage = stage;
        this._stage.initialize(this, options);

        switch (options["time-measurement"])
        {
        case "performance":
            this._getTimestamp = performance.now.bind(performance);
            break;
        case "date":
            this._getTimestamp = Date.now;
            break;
        }

        var testIntervalMilliseconds = options["test-interval"] * 1000;
        switch (options["adjustment"])
        {
        case "fixed":
            this._controller = new FixedComplexityController(testIntervalMilliseconds, this, options);
            break;
        case "adaptive":
        default:
            this._controller = new AdaptiveController(testIntervalMilliseconds, this, options);
            break;
        }
    }, {

    get stage()
    {
        return this._stage;
    },

    run: function()
    {
        return this.waitUntilReady().then(function() {
            this._finishPromise = new SimplePromise;
            this._previousTimestamp = this._getTimestamp();
            this._didWarmUp = false;
            this._stage.tune(this._controller.initialComplexity - this._stage.complexity());
            this._animateLoop();
            return this._finishPromise;
        }.bind(this));
    },

    // Subclasses should override this if they have setup to do prior to commencing.
    waitUntilReady: function()
    {
        var promise = new SimplePromise;
        promise.resolve();
        return promise;
    },

    _animateLoop: function()
    {
        this._currentTimestamp = this._getTimestamp();

        if (!this._didWarmUp) {
            if (this._currentTimestamp - this._previousTimestamp >= 100) {
                this._didWarmUp = true;
                this._controller.start(this._stage, this._currentTimestamp);
                this._previousTimestamp = this._currentTimestamp;
            }

            this._stage.animate(0);
            requestAnimationFrame(this._animateLoop);
            return;
        }

        this._controller.update(this._stage, this._currentTimestamp);
        if (this._controller.shouldStop(this._currentTimestamp)) {
            this._finishPromise.resolve(this._controller.results());
            return;
        }

        this._stage.animate(this._currentTimestamp - this._previousTimestamp);
        this._previousTimestamp = this._currentTimestamp;
        requestAnimationFrame(this._animateLoop);
    }
});
