Sampler = Utilities.createClass(
    function(seriesCount, expectedSampleCount, processor)
    {
        this._processor = processor;

        this.samples = [];
        for (var i = 0; i < seriesCount; ++i) {
            var array = new Array(expectedSampleCount);
            array.fill(0);
            this.samples[i] = array;
        }
        this.sampleCount = 0;
    }, {

    record: function() {
        // Assume that arguments.length == this.samples.length
        for (var i = 0; i < arguments.length; i++) {
            this.samples[i][this.sampleCount] = arguments[i];
        }
        ++this.sampleCount;
    },

    processSamples: function()
    {
        var results = {};

        // Remove unused capacity
        this.samples = this.samples.map(function(array) {
            return array.slice(0, this.sampleCount);
        }, this);

        this._processor.processSamples(results);

        return results;
    }
});

Controller = Utilities.createClass(
    function(benchmark, options)
    {
        // Initialize timestamps relative to the start of the benchmark
        // In start() the timestamps are offset by the start timestamp
        this._startTimestamp = 0;
        this._endTimestamp = options["test-interval"];
        // Default data series: timestamp, complexity, estimatedFrameLength
        var sampleSize = options["sample-capacity"] || (60 * options["test-interval"] / 1000);
        this._sampler = new Sampler(options["series-count"] || 3, sampleSize, this);
        this._marks = {};

        this._frameLengthEstimator = new SimpleKalmanEstimator(options["kalman-process-error"], options["kalman-measurement-error"]);
        this._isFrameLengthEstimatorEnabled = true;

        // Length of subsequent intervals; a value of 0 means use no intervals
        this.intervalSamplingLength = 100;

        this.initialComplexity = 1;
    }, {

    set isFrameLengthEstimatorEnabled(enabled) {
        this._isFrameLengthEstimatorEnabled = enabled;
    },

    start: function(startTimestamp, stage)
    {
        this._startTimestamp = startTimestamp;
        this._endTimestamp += startTimestamp;
        this._previousTimestamp = startTimestamp;
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
        this._intervalEndTimestamp = currentTimestamp + this.intervalSamplingLength;

        return averageFrameLength;
    },

    update: function(timestamp, stage)
    {
        var lastFrameLength = timestamp - this._previousTimestamp;
        this._previousTimestamp = timestamp;

        var frameLengthEstimate = -1, intervalAverageFrameLength = -1;
        var didFinishInterval = false;
        if (!this.intervalSamplingLength) {
            if (this._isFrameLengthEstimatorEnabled) {
                this._frameLengthEstimator.sample(lastFrameLength);
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
        this.tune(timestamp, stage, lastFrameLength, didFinishInterval, intervalAverageFrameLength);
    },

    didFinishInterval: function(timestamp, stage, intervalAverageFrameLength)
    {
    },

    tune: function(timestamp, stage, lastFrameLength, didFinishInterval, intervalAverageFrameLength)
    {
    },

    shouldStop: function(timestamp)
    {
        return timestamp > this._endTimestamp;
    },

    results: function()
    {
        return this._sampler.processSamples();
    },

    _processComplexitySamples: function(complexitySamples, complexityAverageSamples)
    {
        complexityAverageSamples.addField(Strings.json.complexity, 0);
        complexityAverageSamples.addField(Strings.json.frameLength, 1);
        complexityAverageSamples.addField(Strings.json.measurements.stdev, 2);

        complexitySamples.sort(function(a, b) {
            return complexitySamples.getFieldInDatum(a, Strings.json.complexity) - complexitySamples.getFieldInDatum(b, Strings.json.complexity);
        });

        // Samples averaged based on complexity
        var currentComplexity = -1;
        var experimentAtComplexity;
        function addSample() {
            var mean = experimentAtComplexity.mean();
            var stdev = experimentAtComplexity.standardDeviation();

            var averageSample = complexityAverageSamples.createDatum();
            complexityAverageSamples.push(averageSample);
            complexityAverageSamples.setFieldInDatum(averageSample, Strings.json.complexity, currentComplexity);
            complexityAverageSamples.setFieldInDatum(averageSample, Strings.json.frameLength, mean);
            complexityAverageSamples.setFieldInDatum(averageSample, Strings.json.measurements.stdev, stdev);
        }
        complexitySamples.forEach(function(sample) {
            var sampleComplexity = complexitySamples.getFieldInDatum(sample, Strings.json.complexity);
            if (sampleComplexity != currentComplexity) {
                if (currentComplexity > -1)
                    addSample();

                currentComplexity = sampleComplexity;
                experimentAtComplexity = new Experiment;
            }
            experimentAtComplexity.sample(complexitySamples.getFieldInDatum(sample, Strings.json.frameLength));
        });
        // Finish off the last one
        addSample();
    },

    processSamples: function(results)
    {
        var complexityExperiment = new Experiment;
        var smoothedFrameLengthExperiment = new Experiment;

        var samples = this._sampler.samples;

        for (var markName in this._marks)
            this._marks[markName].time -= this._startTimestamp;
        results[Strings.json.marks] = this._marks;

        results[Strings.json.samples] = {};

        var controllerSamples = new SampleData;
        results[Strings.json.samples][Strings.json.controller] = controllerSamples;

        controllerSamples.addField(Strings.json.time, 0);
        controllerSamples.addField(Strings.json.complexity, 1);
        controllerSamples.addField(Strings.json.frameLength, 2);
        controllerSamples.addField(Strings.json.smoothedFrameLength, 3);

        var complexitySamples = new SampleData(controllerSamples.fieldMap);
        results[Strings.json.samples][Strings.json.complexity] = complexitySamples;

        samples[0].forEach(function(timestamp, i) {
            var sample = controllerSamples.createDatum();
            controllerSamples.push(sample);
            complexitySamples.push(sample);

            // Represent time in milliseconds
            controllerSamples.setFieldInDatum(sample, Strings.json.time, timestamp - this._startTimestamp);
            controllerSamples.setFieldInDatum(sample, Strings.json.complexity, samples[1][i]);

            if (i == 0)
                controllerSamples.setFieldInDatum(sample, Strings.json.frameLength, 1000/60);
            else
                controllerSamples.setFieldInDatum(sample, Strings.json.frameLength, timestamp - samples[0][i - 1]);

            if (samples[2][i] != -1)
                controllerSamples.setFieldInDatum(sample, Strings.json.smoothedFrameLength, samples[2][i]);
        }, this);

        var complexityAverageSamples = new SampleData;
        results[Strings.json.samples][Strings.json.complexityAverage] = complexityAverageSamples;
        this._processComplexitySamples(complexitySamples, complexityAverageSamples);
    }
});

FixedController = Utilities.createSubclass(Controller,
    function(benchmark, options)
    {
        Controller.call(this, benchmark, options);
        this.initialComplexity = options["complexity"];
        this.intervalSamplingLength = 0;
    }
);

StepController = Utilities.createSubclass(Controller,
    function(benchmark, options)
    {
        Controller.call(this, benchmark, options);
        this.initialComplexity = options["complexity"];
        this.intervalSamplingLength = 0;
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
        if (!this._startedSampling && timestamp >= this._samplingTimestamp) {
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
    }
});

RampController = Utilities.createSubclass(Controller,
    function(benchmark, options)
    {
        // The tier warmup takes at most 5 seconds
        options["sample-capacity"] = (options["test-interval"] / 1000 + 5) * 60;
        Controller.call(this, benchmark, options);

        // Initially start with a tier test to find the bounds
        // The number of objects in a tier test is 10^|_tier|
        this._tier = -.5;
        // The timestamp is first set after the first interval completes
        this._tierStartTimestamp = 0;
        this._minimumComplexity = 1;
        this._maximumComplexity = 1;

        // After the tier range is determined, figure out the number of ramp iterations
        var minimumRampLength = 3000;
        var totalRampIterations = Math.max(1, Math.floor(this._endTimestamp / minimumRampLength));
        // Give a little extra room to run since the ramps won't be exactly this length
        this._rampLength = Math.floor((this._endTimestamp - totalRampIterations * this.intervalSamplingLength) / totalRampIterations);
        this._rampDidWarmup = false;
        this._rampRegressions = [];

        this._finishedTierSampling = false;
        this._changePointEstimator = new Experiment;
        this._minimumComplexityEstimator = new Experiment;
        // Estimates all frames within an interval
        this._intervalFrameLengthEstimator = new Experiment;
    }, {

    // If the engine can handle the tier's complexity at the desired frame rate, test for a short
    // period, then move on to the next tier
    tierFastTestLength: 250,
    // If the engine is under stress, let the test run a little longer to let the measurement settle
    tierSlowTestLength: 750,

    rampWarmupLength: 200,

    // Used for regression calculations in the ramps
    frameLengthDesired: 1000/60,
    // Add some tolerance; frame lengths shorter than this are considered to be @ the desired frame length
    frameLengthDesiredThreshold: 1000/58,
    // During tier sampling get at least this slow to find the right complexity range
    frameLengthTierThreshold: 1000/30,
    // Try to make each ramp get this slow so that we can cross the break point
    frameLengthRampLowerThreshold: 1000/45,
    // Do not let the regression calculation at the maximum complexity of a ramp get slower than this threshold
    frameLengthRampUpperThreshold: 1000/20,

    start: function(startTimestamp, stage)
    {
        Controller.prototype.start.call(this, startTimestamp, stage);
        this._rampStartTimestamp = 0;
        this.intervalSamplingLength = 100;
    },

    didFinishInterval: function(timestamp, stage, intervalAverageFrameLength)
    {
        if (!this._finishedTierSampling) {
            if (this._tierStartTimestamp > 0 && timestamp < this._tierStartTimestamp + this.tierFastTestLength)
                return;

            var currentComplexity = stage.complexity();
            var currentFrameLength = this._frameLengthEstimator.estimate;
            if (currentFrameLength < this.frameLengthTierThreshold) {
                var isAnimatingAt60FPS = currentFrameLength < this.frameLengthDesiredThreshold;
                var hasFinishedSlowTierTest = timestamp > this._tierStartTimestamp + this.tierSlowTestLength;

                if (!isAnimatingAt60FPS && !hasFinishedSlowTierTest)
                    return;

                // We're measuring at 60 fps, so quickly move on to the next tier, or
                // we've slower than 60 fps, but we've let this tier run long enough to
                // get an estimate
                this._lastTierComplexity = currentComplexity;
                this._lastTierFrameLength = currentFrameLength;

                this._tier += .5;
                var nextTierComplexity = Math.round(Math.pow(10, this._tier));
                stage.tune(nextTierComplexity - currentComplexity);

                // Some tests may be unable to go beyond a certain capacity. If so, don't keep moving up tiers
                if (stage.complexity() - currentComplexity > 0 || nextTierComplexity == 1) {
                    this._tierStartTimestamp = timestamp;
                    this.mark("Complexity: " + nextTierComplexity, timestamp);
                    return;
                }
            } else if (timestamp < this._tierStartTimestamp + this.tierSlowTestLength)
                return;

            this._finishedTierSampling = true;
            this.isFrameLengthEstimatorEnabled = false;
            this.intervalSamplingLength = 120;

            // Extend the test length so that the full test length is made of the ramps
            this._endTimestamp += timestamp;
            this.mark(Strings.json.samplingStartTimeOffset, timestamp);

            this._minimumComplexity = 1;
            this._possibleMinimumComplexity = this._minimumComplexity;
            this._minimumComplexityEstimator.sample(this._minimumComplexity);

            // Sometimes this last tier will drop the frame length well below the threshold.
            // Avoid going down that far since it means fewer measurements are taken in the 60 fps area.
            // Interpolate a maximum complexity that gets us around the lowest threshold.
            // Avoid doing this calculation if we never get out of the first tier (where this._lastTierComplexity is undefined).
            if (this._lastTierComplexity && this._lastTierComplexity != currentComplexity)
                this._maximumComplexity = Math.floor(Utilities.lerp(Utilities.progressValue(this.frameLengthTierThreshold, this._lastTierFrameLength, currentFrameLength), this._lastTierComplexity, currentComplexity));
            else {
                // If the browser is capable of handling the most complex version of the test, use that
                this._maximumComplexity = currentComplexity;
            }
            this._possibleMaximumComplexity = this._maximumComplexity;

            // If we get ourselves onto a ramp where the maximum complexity does not yield slow enough FPS,
            // We'll use this as a boundary to find a higher maximum complexity for the next ramp
            this._lastTierComplexity = currentComplexity;
            this._lastTierFrameLength = currentFrameLength;

            // First ramp
            stage.tune(this._maximumComplexity - currentComplexity);
            this._rampDidWarmup = false;
            // Start timestamp represents start of ramp iteration and warm up
            this._rampStartTimestamp = timestamp;
            return;
        }

        if ((timestamp - this._rampStartTimestamp) < this.rampWarmupLength)
            return;

        if (this._rampDidWarmup)
            return;

        this._rampDidWarmup = true;
        this._currentRampLength = this._rampStartTimestamp + this._rampLength - timestamp;
        // Start timestamp represents start of ramp down, after warm up
        this._rampStartTimestamp = timestamp;
        this._rampStartIndex = this._sampler.sampleCount;
    },

    tune: function(timestamp, stage, lastFrameLength, didFinishInterval, intervalAverageFrameLength)
    {
        if (!this._rampDidWarmup)
            return;

        this._intervalFrameLengthEstimator.sample(lastFrameLength);
        if (!didFinishInterval)
            return;

        var currentComplexity = stage.complexity();
        var intervalFrameLengthMean = this._intervalFrameLengthEstimator.mean();
        var intervalFrameLengthStandardDeviation = this._intervalFrameLengthEstimator.standardDeviation();

        if (intervalFrameLengthMean < this.frameLengthDesiredThreshold && this._intervalFrameLengthEstimator.cdf(this.frameLengthDesiredThreshold) > .9) {
            this._possibleMinimumComplexity = Math.max(this._possibleMinimumComplexity, currentComplexity);
        } else if (intervalFrameLengthStandardDeviation > 2) {
            // In the case where we might have found a previous interval where 60fps was reached. We hit a significant blip,
            // so we should resample this area in the next ramp.
            this._possibleMinimumComplexity = 1;
        }
        if (intervalFrameLengthMean - intervalFrameLengthStandardDeviation > this.frameLengthRampLowerThreshold)
            this._possibleMaximumComplexity = Math.min(this._possibleMaximumComplexity, currentComplexity);
        this._intervalFrameLengthEstimator.reset();

        var progress = (timestamp - this._rampStartTimestamp) / this._currentRampLength;

        if (progress < 1) {
            // Reframe progress percentage so that the last interval of the ramp can sample at minimum complexity
            progress = (timestamp - this._rampStartTimestamp) / (this._currentRampLength - this.intervalSamplingLength);
            stage.tune(Math.max(this._minimumComplexity, Math.floor(Utilities.lerp(progress, this._maximumComplexity, this._minimumComplexity))) - currentComplexity);
            return;
        }

        var regression = new Regression(this._sampler.samples, this._getComplexity, this._getFrameLength,
            this._sampler.sampleCount - 1, this._rampStartIndex, { desiredFrameLength: this.frameLengthDesired });
        this._rampRegressions.push(regression);

        var frameLengthAtMaxComplexity = regression.valueAt(this._maximumComplexity);
        if (frameLengthAtMaxComplexity < this.frameLengthRampLowerThreshold)
            this._possibleMaximumComplexity = Math.floor(Utilities.lerp(Utilities.progressValue(this.frameLengthRampLowerThreshold, frameLengthAtMaxComplexity, this._lastTierFrameLength), this._maximumComplexity, this._lastTierComplexity));
        // If the regression doesn't fit the first segment at all, keep the minimum bound at 1
        if ((timestamp - this._sampler.samples[0][this._sampler.sampleCount - regression.n1]) / this._currentRampLength < .25)
            this._possibleMinimumComplexity = 1;

        this._minimumComplexityEstimator.sample(this._possibleMinimumComplexity);
        this._minimumComplexity = Math.round(this._minimumComplexityEstimator.mean());

        if (frameLengthAtMaxComplexity < this.frameLengthRampUpperThreshold) {
            this._changePointEstimator.sample(regression.complexity);
            // Ideally we'll target the change point in the middle of the ramp. If the range of the ramp is too small, there isn't enough
            // range along the complexity (x) axis for a good regression calculation to be made, so force at least a range of 5
            // particles. Make it possible to increase the maximum complexity in case unexpected noise caps the regression too low.
            this._maximumComplexity = Math.round(this._minimumComplexity +
                Math.max(5,
                    this._possibleMaximumComplexity - this._minimumComplexity,
                    (this._changePointEstimator.mean() - this._minimumComplexity) * 2));
        } else {
            // The slowest samples weighed the regression too heavily
            this._maximumComplexity = Math.max(Math.round(.8 * this._maximumComplexity), this._minimumComplexity + 5);
        }

        // Next ramp
        stage.tune(this._maximumComplexity - stage.complexity());
        this._rampDidWarmup = false;
        // Start timestamp represents start of ramp iteration and warm up
        this._rampStartTimestamp = timestamp;
        this._possibleMinimumComplexity = 1;
        this._possibleMaximumComplexity = this._maximumComplexity;
    },

    _getComplexity: function(samples, i) {
        return samples[1][i];
    },

    _getFrameLength: function(samples, i) {
        return samples[0][i] - samples[0][i - 1];
    },

    processSamples: function(results)
    {
        Controller.prototype.processSamples.call(this, results);

        // Have samplingTimeOffset represent time 0
        var startTimestamp = this._marks[Strings.json.samplingStartTimeOffset].time;

        for (var markName in results[Strings.json.marks]) {
            results[Strings.json.marks][markName].time -= startTimestamp;
        }

        var controllerSamples = results[Strings.json.samples][Strings.json.controller];
        controllerSamples.forEach(function(timeSample) {
            controllerSamples.setFieldInDatum(timeSample, Strings.json.time, controllerSamples.getFieldInDatum(timeSample, Strings.json.time) - startTimestamp);
        });

        // Aggregate all of the ramps into one big complexity-frameLength dataset
        var complexitySamples = new SampleData(controllerSamples.fieldMap);
        results[Strings.json.samples][Strings.json.complexity] = complexitySamples;

        results[Strings.json.controller] = [];
        this._rampRegressions.forEach(function(ramp) {
            var startIndex = ramp.startIndex, endIndex = ramp.endIndex;
            var startTime = controllerSamples.getFieldInDatum(startIndex, Strings.json.time);
            var endTime = controllerSamples.getFieldInDatum(endIndex, Strings.json.time);
            var startComplexity = controllerSamples.getFieldInDatum(startIndex, Strings.json.complexity);
            var endComplexity = controllerSamples.getFieldInDatum(endIndex, Strings.json.complexity);

            var regression = {};
            results[Strings.json.controller].push(regression);

            var percentage = (ramp.complexity - startComplexity) / (endComplexity - startComplexity);
            var inflectionTime = startTime + percentage * (endTime - startTime);

            regression[Strings.json.regressions.segment1] = [
                [startTime, ramp.s2 + ramp.t2 * startComplexity],
                [inflectionTime, ramp.s2 + ramp.t2 * ramp.complexity]
            ];
            regression[Strings.json.regressions.segment2] = [
                [inflectionTime, ramp.s1 + ramp.t1 * ramp.complexity],
                [endTime, ramp.s1 + ramp.t1 * endComplexity]
            ];
            regression[Strings.json.complexity] = ramp.complexity;
            regression[Strings.json.regressions.startIndex] = startIndex;
            regression[Strings.json.regressions.endIndex] = endIndex;
            regression[Strings.json.regressions.profile] = ramp.profile;

            for (var j = startIndex; j <= endIndex; ++j)
                complexitySamples.push(controllerSamples.at(j));
        });

        var complexityAverageSamples = new SampleData;
        results[Strings.json.samples][Strings.json.complexityAverage] = complexityAverageSamples;
        this._processComplexitySamples(complexitySamples, complexityAverageSamples);
    }
});

Ramp30Controller = Utilities.createSubclass(RampController,
    function(benchmark, options)
    {
        RampController.call(this, benchmark, options);
    }, {

    frameLengthDesired: 1000/30,
    frameLengthDesiredThreshold: 1000/29,
    frameLengthTierThreshold: 1000/20,
    frameLengthRampLowerThreshold: 1000/20,
    frameLengthRampUpperThreshold: 1000/12
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
        return (Pseudo.random() * (max - min)) + min;
    },

    randomBool: function()
    {
        return !!Math.round(Pseudo.random());
    },

    randomSign: function()
    {
        return Pseudo.random() >= .5 ? 1 : -1;
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

    randomStyleMixBlendMode: function()
    {
        var mixBlendModeList = [
          'normal',
          'multiply',
          'screen',
          'overlay',
          'darken',
          'lighten',
          'color-dodge',
          'color-burn',
          'hard-light',
          'soft-light',
          'difference',
          'exclusion',
          'hue',
          'saturation',
          'color',
          'luminosity'
        ];

        return mixBlendModeList[this.randomInt(0, mixBlendModeList.length)];
    },

    randomStyleFilter: function()
    {
        var filterList = [
            'grayscale(50%)',
            'sepia(50%)',
            'saturate(50%)',
            'hue-rotate(180)',
            'invert(50%)',
            'opacity(50%)',
            'brightness(50%)',
            'contrast(50%)',
            'blur(10px)',
            'drop-shadow(10px 10px 10px gray)'
        ];

        return filterList[this.randomInt(0, filterList.length)];
    },

    randomElementInArray: function(array)
    {
        return array[Stage.randomInt(0, array.length - 1)];
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
            if (window.performance && window.performance.now)
                this._getTimestamp = performance.now.bind(performance);
            else
                this._getTimestamp = null;
            break;
        case "raf":
            this._getTimestamp = null;
            break;
        case "date":
            this._getTimestamp = Date.now;
            break;
        }

        options["test-interval"] *= 1000;
        switch (options["controller"])
        {
        case "fixed":
            this._controller = new FixedController(this, options);
            break;
        case "step":
            this._controller = new StepController(this, options);
            break;
        case "adaptive":
            this._controller = new AdaptiveController(this, options);
            break;
        case "ramp":
            this._controller = new RampController(this, options);
            break;
        case "ramp30":
            this._controller = new Ramp30Controller(this, options);
        }
    }, {

    get stage()
    {
        return this._stage;
    },

    get timestamp()
    {
        return this._currentTimestamp - this._startTimestamp;
    },

    backgroundColor: function()
    {
        var stage = window.getComputedStyle(document.getElementById("stage"));
        return stage["background-color"];
    },

    run: function()
    {
        return this.waitUntilReady().then(function() {
            this._finishPromise = new SimplePromise;
            this._previousTimestamp = undefined;
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

    _animateLoop: function(timestamp)
    {
        timestamp = (this._getTimestamp && this._getTimestamp()) || timestamp;
        this._currentTimestamp = timestamp;

        if (this._controller.shouldStop(timestamp)) {
            this._finishPromise.resolve(this._controller.results());
            return;
        }

        if (!this._didWarmUp) {
            if (!this._previousTimestamp)
                this._previousTimestamp = timestamp;
            else if (timestamp - this._previousTimestamp >= 100) {
                this._didWarmUp = true;
                this._startTimestamp = timestamp;
                this._controller.start(timestamp, this._stage);
                this._previousTimestamp = timestamp;
            }

            this._stage.animate(0);
            requestAnimationFrame(this._animateLoop);
            return;
        }

        this._controller.update(timestamp, this._stage);
        this._stage.animate(timestamp - this._previousTimestamp);
        this._previousTimestamp = timestamp;
        requestAnimationFrame(this._animateLoop);
    }
});
