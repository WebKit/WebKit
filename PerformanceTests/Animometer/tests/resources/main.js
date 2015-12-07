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
    messages: [ 
        Strings.text.runningState.warming,
        Strings.text.runningState.sampling,
        Strings.text.runningState.finished
    ]
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
    },
    
    currentMessage: function()
    {
        return this._message(this.currentStage(), this._currentTimeOffset);
    },
    
    currentProgress: function()
    {
        return this._currentTimeOffset / this._timeOffset(BenchmarkState.stages.FINISHED);
    }
}

function Animator(benchmark, options)
{
    this._benchmark = benchmark;
    this._options = options;
    
    this._frameCount = 0;
    this._dropFrameCount = 1;
    this._measureFrameCount = 3; 
    this._referenceTime = 0;
    this._currentTimeOffset = 0;
    this._estimator = new KalmanEstimator(60);
}

Animator.prototype =
{
    timeDelta: function()
    {
        return this._currentTimeOffset - this._startTimeOffset;
    },
    
    animate: function()
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
        if (this._options["estimated-frame-rate"])
            currentFrameRate = this._estimator.estimate(currentFrameRate);
        
        // Adjust the test to reach the desired FPS.
        var result = this._benchmark.update(this._currentTimeOffset, this.timeDelta(), currentFrameRate);
        
        // Start the next drop/measure cycle.
        this._frameCount = 0;
        
        // If result == 0, no more requestAnimationFrame() will be invoked.
        return result;
    },
    
    animateLoop: function(timestamp)
    {
        if (this.animate())
            requestAnimationFrame(this.animateLoop.bind(this));
    }
}

function Benchmark(options)
{
    this._options = options;
    this._recordInterval = 200;    
    this._isSampling = false;

    this._controller = new PIDController(this._options["frame-rate"]);
    this._sampler = new Sampler(2);
    this._state = new BenchmarkState(this._options["test-interval"] * 1000);
}

Benchmark.prototype =
{
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
            this.clear();
            return false;
        }

        if (stage == BenchmarkState.stages.SAMPLING && !this._isSampling) {
            this._sampler.startSampling(this._state.samplingTimeOffset());
            this._isSampling = true;
        }

        var tuneValue = 0;
        if (this._options["adjustment"] == "fixed") {
            if (this._options["complexity"]) {
                // this.tune(0) returns the current complexity of the test.
                tuneValue = this._options["complexity"] - this.tune(0);
            }
        }
        else if (!(this._isSampling && this._options["adjustment"] == "fixed-after-warmup")) {
            // The relationship between frameRate and test complexity is inverse-proportional so we
            // need to use the negative of PIDController.tune() to change the complexity of the test.
            tuneValue = -this._controller.tune(currentTimeOffset, timeDelta, currentFrameRate);
            tuneValue = tuneValue > 0 ? Math.floor(tuneValue) : Math.ceil(tuneValue);
        }

        var currentComplexity = this.tune(tuneValue);
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

        this.showResults(this._state.currentMessage(), this._state.currentProgress());
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
}

window.benchmarkClient = {};

// This event listener runs the test if it is loaded outside the benchmark runner.
window.addEventListener("load", function()
{
    if (window.self !== window.top)
        return;
    window.benchmark = window.benchmarkClient.create(null, null, 30000, 50, null, null);
    window.benchmark.start();
});

// This function is called from the suite controller run-callback when running the benchmark runner.
window.runBenchmark = function(suite, test, options, recordTable, progressBar)
{
    var benchmarkOptions = { complexity: test.complexity };
    benchmarkOptions = Utilities.mergeObjects(benchmarkOptions, options);
    benchmarkOptions = Utilities.mergeObjects(benchmarkOptions, Utilities.parseParameters());
    window.benchmark = window.benchmarkClient.create(suite, test, benchmarkOptions, recordTable, progressBar);
    return window.benchmark.run();
}
