Controller = Utilities.createClass(
    function(benchmark, options)
    {
        // Initialize timestamps relative to the start of the benchmark
        // In start() the timestamps are offset by the start timestamp
        this._startTimestamp = 0;
        this._endTimestamp = options["test-interval"];
        // Default data series: timestamp, complexity, estimatedFrameLength
        this._sampler = new Sampler(options["series-count"] || 3, (60 * options["test-interval"] / 1000), this);
        this._marks = {};

        this._frameLengthEstimator = new SimpleKalmanEstimator(options["kalman-process-error"], options["kalman-measurement-error"]);
        this._isFrameLengthEstimatorEnabled = true;

        // Length of subsequent intervals; a value of 0 means use no intervals
        this._intervalLength = options["interval-length"] || 100;

        this.initialComplexity = 0;
    }, {

    set isFrameLengthEstimatorEnabled(enabled) {
        this._isFrameLengthEstimatorEnabled = enabled;
    },

    start: function(startTimestamp, stage)
    {
        this._startTimestamp = startTimestamp;
        this._endTimestamp += startTimestamp;
        this._measureAndResetInterval(startTimestamp);
        this.recordFirstSample(startTimestamp, stage);
    },

    recordFirstSample: function(startTimestamp, stage)
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

    _measureAndResetInterval: function(currentTimestamp)
    {
        var sampleCount = this._sampler.sampleCount;
        var averageFrameLength = 0;

        if (this._intervalEndTimestamp) {
            var intervalStartTimestamp = this._sampler.samples[0][this._intervalStartIndex];
            averageFrameLength = (currentTimestamp - intervalStartTimestamp) / (sampleCount - this._intervalStartIndex);
        }

        this._intervalStartIndex = sampleCount;
        this._intervalEndTimestamp = currentTimestamp + this._intervalLength;

        return averageFrameLength;
    },

    update: function(timestamp, stage)
    {
        var frameLengthEstimate = -1;
        var didFinishInterval = false;
        if (!this._intervalLength) {
            if (this._isFrameLengthEstimatorEnabled) {
                this._frameLengthEstimator.sample(timestamp - this._sampler.samples[0][this._sampler.sampleCount - 1]);
                frameLengthEstimate = this._frameLengthEstimator.estimate;
            }
        } else if (timestamp >= this._intervalEndTimestamp) {
            var intervalStartTimestamp = this._sampler.samples[0][this._intervalStartIndex];
            intervalAverageFrameLength = this._measureAndResetInterval(timestamp);
            if (this._isFrameLengthEstimatorEnabled) {
                this._frameLengthEstimator.sample(intervalAverageFrameLength);
                frameLengthEstimate = this._frameLengthEstimator.estimate;
            }
            didFinishInterval = true;
            this.didFinishInterval(timestamp, stage, intervalAverageFrameLength);
        }

        this._sampler.record(timestamp, stage.complexity(), frameLengthEstimate);
        this.tune(timestamp, stage, didFinishInterval);
    },

    didFinishInterval: function(timestamp, stage, intervalAverageFrameLength)
    {
    },

    tune: function(timestamp, stage, didFinishInterval)
    {
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

StepController = Utilities.createSubclass(Controller,
    function(benchmark, options)
    {
        options["interval-length"] = 0;
        Controller.call(this, benchmark, options);
        this.initialComplexity = options["complexity"];
        this._stepped = false;
        this._stepTime = options["test-interval"] / 2;
    }, {

    start: function(startTimestamp, stage)
    {
        Controller.prototype.start.call(this, startTimestamp, stage);
        this._stepTime += startTimestamp;
    },

    tune: function(timestamp, stage)
    {
        if (this._stepped || timestamp < this._stepTime)
            return;

        this.mark(Strings.json.samplingEndTimeOffset, timestamp);
        this._stepped = true;
        stage.tune(stage.complexity() * 3);
    }
});

AdaptiveController = Utilities.createSubclass(Controller,
    function(benchmark, options)
    {
        // Data series: timestamp, complexity, estimatedIntervalFrameLength
        Controller.call(this, benchmark, options);

        // All tests start at 0, so we expect to see 60 fps quickly.
        this._samplingTimestamp = options["test-interval"] / 2;
        this._startedSampling = false;
        this._targetFrameRate = options["frame-rate"];
        this._pid = new PIDController(this._targetFrameRate);

        this._intervalFrameCount = 0;
        this._numberOfFramesToMeasurePerInterval = 4;
    }, {

    start: function(startTimestamp, stage)
    {
        Controller.prototype.start.call(this, startTimestamp, stage);

        this._samplingTimestamp += startTimestamp;
        this._intervalTimestamp = startTimestamp;
    },

    recordFirstSample: function(startTimestamp, stage)
    {
        this._sampler.record(startTimestamp, stage.complexity(), -1);
    },

    update: function(timestamp, stage)
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
        this._frameLengthEstimator.sample(intervalLength / this._numberOfFramesToMeasurePerInterval);
        var intervalEstimatedFrameRate = 1000 / this._frameLengthEstimator.estimate;
        var tuneValue = -this._pid.tune(timestamp - this._startTimestamp, intervalLength, intervalEstimatedFrameRate);
        tuneValue = tuneValue > 0 ? Math.floor(tuneValue) : Math.ceil(tuneValue);
        stage.tune(tuneValue);

        this._sampler.record(timestamp, stage.complexity(), this._frameLengthEstimator.estimate);

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
        return Math.floor(this.random(min, max + 1));
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

    rotatingColor: function(cycleLengthMs, saturation, lightness)
    {
        return "hsl("
            + Stage.dateFractionalValue(cycleLengthMs) * 360 + ", "
            + ((saturation || .8) * 100).toFixed(0) + "%, "
            + ((lightness || .35) * 100).toFixed(0) + "%)";
    },

    // Returns a fractional value that wraps around within [0,1]
    dateFractionalValue: function(cycleLengthMs)
    {
        return (Date.now() / (cycleLengthMs || 2000)) % 1;
    },

    // Returns an increasing value slowed down by factor
    dateCounterValue: function(factor)
    {
        return Date.now() / factor;
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

        options["test-interval"] *= 1000;
        switch (options["adjustment"])
        {
        case "step":
            this._controller = new StepController(this, options);
            break;
        case "adaptive":
        default:
            this._controller = new AdaptiveController(this, options);
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
                this._controller.start(this._currentTimestamp, this._stage);
                this._previousTimestamp = this._currentTimestamp;
            }

            this._stage.animate(0);
            requestAnimationFrame(this._animateLoop);
            return;
        }

        this._controller.update(this._currentTimestamp, this._stage);
        if (this._controller.shouldStop(this._currentTimestamp)) {
            this._finishPromise.resolve(this._controller.results());
            return;
        }

        this._stage.animate(this._currentTimestamp - this._previousTimestamp);
        this._previousTimestamp = this._currentTimestamp;
        requestAnimationFrame(this._animateLoop);
    }
});
