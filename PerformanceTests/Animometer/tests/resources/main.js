function Rotater(rotateInterval)
{
    this._timeDelta = 0;
    this._rotateInterval = rotateInterval;
}

Rotater.prototype =
{
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
};

function BenchmarkState(testInterval)
{
    this._currentTimeOffset = 0;
    this._stageInterval = testInterval / BenchmarkState.stages.FINISHED;
}

// The enum values and the messages should be in the same order
BenchmarkState.stages = {
    WARMING: 0,
    SAMPLING: 1,
    FINISHED: 2,
}

BenchmarkState.prototype =
{
    _timeOffset: function(stage)
    {
        return stage * this._stageInterval;
    },

    _message: function(stage, timeOffset)
    {
        if (stage == BenchmarkState.stages.FINISHED)
            return BenchmarkState.stages.messages[stage];

        return BenchmarkState.stages.messages[stage] + "... ("
            + Math.floor((timeOffset - this._timeOffset(stage)) / 1000) + "/"
            + Math.floor((this._timeOffset(stage + 1) - this._timeOffset(stage)) / 1000) + ")";
    },

    update: function(currentTimeOffset)
    {
        this._currentTimeOffset = currentTimeOffset;
    },

    samplingTimeOffset: function()
    {
        return this._timeOffset(BenchmarkState.stages.SAMPLING);
    },

    currentStage: function()
    {
        for (var stage = BenchmarkState.stages.WARMING; stage < BenchmarkState.stages.FINISHED; ++stage) {
            if (this._currentTimeOffset < this._timeOffset(stage + 1))
                return stage;
        }
        return BenchmarkState.stages.FINISHED;
    }
}

function Stage() {}

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

Stage.prototype =
{
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
};

function Animator()
{
    this._intervalFrameCount = 0;
    this._numberOfFramesToMeasurePerInterval = 3;
    this._referenceTime = 0;
    this._currentTimeOffset = 0;
}

Animator.prototype =
{
    initialize: function(benchmark)
    {
        this._benchmark = benchmark;

        // Use Kalman filter to get a more non-fluctuating frame rate.
        if (benchmark.options["estimated-frame-rate"])
            this._estimator = new KalmanEstimator(60);
        else
            this._estimator = new IdentityEstimator;
    },

    get benchmark()
    {
        return this._benchmark;
    },

    _intervalTimeDelta: function()
    {
        return this._currentTimeOffset - this._startTimeOffset;
    },

    _shouldRequestAnotherFrame: function()
    {
        // Cadence is number of frames to measure, then one more frame to adjust the scene, and drop
        var currentTime = performance.now();

        if (!this._referenceTime)
            this._referenceTime = currentTime;

        this._currentTimeOffset = currentTime - this._referenceTime;

        if (!this._intervalFrameCount)
            this._startTimeOffset = this._currentTimeOffset;

        // Start the work for the next frame.
        ++this._intervalFrameCount;

        // Drop _dropFrameCount frames and measure the average of _measureFrameCount frames.
        if (this._intervalFrameCount <= this._numberOfFramesToMeasurePerInterval) {
            this._benchmark.record(this._currentTimeOffset, -1, -1);
            return true;
        }

        // Get the average FPS of _measureFrameCount frames over intervalTimeDelta.
        var intervalTimeDelta = this._intervalTimeDelta();
        var intervalFrameRate = 1000 / (intervalTimeDelta / this._numberOfFramesToMeasurePerInterval);
        var estimatedIntervalFrameRate = this._estimator.estimate(intervalFrameRate);
        // Record the complexity of the frame we just rendered. The next frame's time respresents the adjusted
        // complexity
        this._benchmark.record(this._currentTimeOffset, estimatedIntervalFrameRate, intervalFrameRate);

        // Adjust the test to reach the desired FPS.
        var shouldContinueRunning = this._benchmark.update(this._currentTimeOffset, intervalTimeDelta, estimatedIntervalFrameRate);

        // Start the next drop/measure cycle.
        this._intervalFrameCount = 0;

        // If result is false, no more requestAnimationFrame() will be invoked.
        return shouldContinueRunning;
    },

    animateLoop: function()
    {
        if (this._shouldRequestAnotherFrame()) {
            this._benchmark.stage.animate(this._intervalTimeDelta());
            requestAnimationFrame(this.animateLoop.bind(this));
        }
    }
}

function Benchmark(stage, options)
{
    this._options = options;

    this._stage = stage;
    this._stage.initialize(this);
    this._animator = new Animator();
    this._animator.initialize(this);

    this._recordInterval = 200;
    this._isSampling = false;
    this._controller = new PIDController(this._options["frame-rate"]);
    this._sampler = new Sampler(4, 60 * this._options["test-interval"], this);
    this._state = new BenchmarkState(this._options["test-interval"] * 1000);
}

Benchmark.prototype =
{
    get options()
    {
        return this._options;
    },

    get stage()
    {
        return this._stage;
    },

    get animator()
    {
        return this._animator;
    },

    // Called from the load event listener or from this.run().
    start: function()
    {
        this._animator.animateLoop();
    },

    // Called from the animator to adjust the complexity of the test.
    update: function(currentTimeOffset, intervalTimeDelta, estimatedIntervalFrameRate)
    {
        this._state.update(currentTimeOffset);

        var stage = this._state.currentStage();
        if (stage == BenchmarkState.stages.FINISHED) {
            this._stage.clear();
            return false;
        }

        if (stage == BenchmarkState.stages.SAMPLING && !this._isSampling) {
            this._sampler.mark(Strings.json.samplingTimeOffset, {
                time: this._state.samplingTimeOffset() / 1000
            });
            this._isSampling = true;
        }

        var tuneValue = 0;
        if (this._options["adjustment"] == "fixed") {
            if (this._options["complexity"]) {
                // this._stage.tune(0) returns the current complexity of the test.
                tuneValue = this._options["complexity"] - this._stage.tune(0);
            }
        }
        else if (!(this._isSampling && this._options["adjustment"] == "fixed-after-warmup")) {
            // The relationship between frameRate and test complexity is inverse-proportional so we
            // need to use the negative of PIDController.tune() to change the complexity of the test.
            tuneValue = -this._controller.tune(currentTimeOffset, intervalTimeDelta, estimatedIntervalFrameRate);
            tuneValue = tuneValue > 0 ? Math.floor(tuneValue) : Math.ceil(tuneValue);
        }

        this._stage.tune(tuneValue);

        if (typeof this._recordTimeOffset == "undefined")
            this._recordTimeOffset = currentTimeOffset;

        var stage = this._state.currentStage();
        if (stage != BenchmarkState.stages.FINISHED && currentTimeOffset < this._recordTimeOffset + this._recordInterval)
            return true;

        this._recordTimeOffset = currentTimeOffset;
        return true;
    },

    record: function(currentTimeOffset, estimatedFrameRate, intervalFrameRate)
    {
        // If the frame rate is -1 it means we are still recording for this sample
        this._sampler.record(currentTimeOffset, this.stage.complexity(), estimatedFrameRate, intervalFrameRate);
    },

    run: function()
    {
        return this.waitUntilReady().then(function() {
            this.start();
            var promise = new SimplePromise;
            var resolveWhenFinished = function() {
                if (typeof this._state != "undefined" && (this._state.currentStage() == BenchmarkState.stages.FINISHED))
                    return promise.resolve(this._sampler);
                setTimeout(resolveWhenFinished, 50);
            }.bind(this);

            resolveWhenFinished();
            return promise;
        }.bind(this));
    },

    waitUntilReady: function()
    {
        var promise = new SimplePromise;
        promise.resolve();
        return promise;
    },

    processSamples: function(results)
    {
        var complexity = new Experiment;
        var smoothedFPS = new Experiment;
        var samplingIndex = 0;

        var samplingMark = this._sampler.marks[Strings.json.samplingTimeOffset];
        if (samplingMark) {
            samplingIndex = samplingMark.index;
            results[Strings.json.samplingTimeOffset] = samplingMark.time;
        }

        results[Strings.json.samples] = this._sampler.samples[0].map(function(d, i) {
            var result = {
                // time offsets represented as seconds
                time: d/1000,
                complexity: this._sampler.samples[1][i]
            };

            // time offsets represented as FPS
            if (i == 0)
                result.fps = 60;
            else
                result.fps = 1000 / (d - this._sampler.samples[0][i - 1]);

            var smoothedFPSresult = this._sampler.samples[2][i];
            if (smoothedFPSresult != -1) {
                result.smoothedFPS = smoothedFPSresult;
                result.intervalFPS = this._sampler.samples[3][i];
            }

            if (i >= samplingIndex) {
                complexity.sample(result.complexity);
                if (smoothedFPSresult != -1) {
                    smoothedFPS.sample(smoothedFPSresult);
                }
            }

            return result;
        }, this);

        results[Strings.json.score] = complexity.score(Experiment.defaults.CONCERN);
        [complexity, smoothedFPS].forEach(function(experiment, index) {
            var jsonExperiment = !index ? Strings.json.experiments.complexity : Strings.json.experiments.frameRate;
            results[jsonExperiment] = {};
            results[jsonExperiment][Strings.json.measurements.average] = experiment.mean();
            results[jsonExperiment][Strings.json.measurements.concern] = experiment.concern(Experiment.defaults.CONCERN);
            results[jsonExperiment][Strings.json.measurements.stdev] = experiment.standardDeviation();
            results[jsonExperiment][Strings.json.measurements.percent] = experiment.percentage();
        });
    }
};
