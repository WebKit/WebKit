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
    this._frameCount = 0;
    this._dropFrameCount = 1;
    this._measureFrameCount = 3;
    this._referenceTime = 0;
    this._currentTimeOffset = 0;
    this._estimator = new KalmanEstimator(60);
}

Animator.prototype =
{
    initialize: function(benchmark)
    {
        this._benchmark = benchmark;
        this._estimateFrameRate = benchmark.options["estimated-frame-rate"];
    },

    get benchmark()
    {
        return this._benchmark;
    },

    timeDelta: function()
    {
        return this._currentTimeOffset - this._startTimeOffset;
    },

    _shouldRequestAnotherFrame: function()
    {
        var currentTime = performance.now();
        
        if (!this._referenceTime)
            this._referenceTime = currentTime;
        else
            this._currentTimeOffset = currentTime - this._referenceTime;

        if (!this._frameCount)
            this._startTimeOffset = this._currentTimeOffset;

        ++this._frameCount;

        // Start measuring after dropping _dropFrameCount frames.
        if (this._frameCount == this._dropFrameCount)
            this._measureTimeOffset = this._currentTimeOffset;

        // Drop _dropFrameCount frames and measure the average of _measureFrameCount frames.
        if (this._frameCount < this._dropFrameCount + this._measureFrameCount)
            return true;

        // Get the average FPS of _measureFrameCount frames over measureTimeDelta.
        var measureTimeDelta = this._currentTimeOffset - this._measureTimeOffset;
        var currentFrameRate = Math.floor(1000 / (measureTimeDelta / this._measureFrameCount));

        // Use Kalman filter to get a more non-fluctuating frame rate.
        if (this._estimateFrameRate)
            currentFrameRate = this._estimator.estimate(currentFrameRate);

        // Adjust the test to reach the desired FPS.
        var result = this._benchmark.update(this._currentTimeOffset, this.timeDelta(), currentFrameRate);

        // Start the next drop/measure cycle.
        this._frameCount = 0;

        // If result == 0, no more requestAnimationFrame() will be invoked.
        return result;
    },

    animateLoop: function()
    {
        if (this._shouldRequestAnotherFrame()) {
            this._benchmark.stage.animate(this.timeDelta());
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
    this._sampler = new Sampler(2);
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
    update: function(currentTimeOffset, timeDelta, currentFrameRate)
    {
        this._state.update(currentTimeOffset);

        var stage = this._state.currentStage();
        if (stage == BenchmarkState.stages.FINISHED) {
            this._stage.clear();
            return false;
        }

        if (stage == BenchmarkState.stages.SAMPLING && !this._isSampling) {
            this._sampler.startSampling(this._state.samplingTimeOffset());
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
            tuneValue = -this._controller.tune(currentTimeOffset, timeDelta, currentFrameRate);
            tuneValue = tuneValue > 0 ? Math.floor(tuneValue) : Math.ceil(tuneValue);
        }

        var currentComplexity = this._stage.tune(tuneValue);
        this.record(currentTimeOffset, currentComplexity, currentFrameRate);
        return true;
    },

    record: function(currentTimeOffset, currentComplexity, currentFrameRate)
    {
        this._sampler.sample(currentTimeOffset, [currentComplexity, currentFrameRate]);

        if (typeof this._recordTimeOffset == "undefined")
            this._recordTimeOffset = currentTimeOffset;

        var stage = this._state.currentStage();
        if (stage != BenchmarkState.stages.FINISHED && currentTimeOffset < this._recordTimeOffset + this._recordInterval)
            return;

        this._recordTimeOffset = currentTimeOffset;
    },

    run: function()
    {
        this.start();

        var promise = new SimplePromise;
        var self = this;
        function resolveWhenFinished() {
            if (typeof self._state != "undefined" && (self._state.currentStage() == BenchmarkState.stages.FINISHED))
                return promise.resolve(self._sampler);
            setTimeout(resolveWhenFinished.bind(self), 50);
        }

        resolveWhenFinished();
        return promise;
    }
};
