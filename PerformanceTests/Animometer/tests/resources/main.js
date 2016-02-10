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
        return this._sampler.processSamples();
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
    },

    processSamples: function(results)
    {
        Controller.prototype.processSamples.call(this, results);
        results[Strings.json.targetFrameLength] = 1000 / this._targetFrameRate;
    }
});

Regression = Utilities.createClass(
    function(samples, getComplexity, getFrameLength, startIndex, endIndex, options)
    {
        var slope = this._calculateRegression(samples, getComplexity, getFrameLength, startIndex, endIndex, {
            shouldClip: true,
            s1: 1000/60,
            t1: 0
        });
        var flat = this._calculateRegression(samples, getComplexity, getFrameLength, startIndex, endIndex, {
            shouldClip: true,
            t1: 0,
            t2: 0
        });
        var desired;
        if (slope.error < flat.error)
            desired = slope;
        else
            desired = flat;

        this.startIndex = Math.min(startIndex, endIndex);
        this.endIndex = Math.max(startIndex, endIndex);

        this.complexity = desired.complexity;
        this.s1 = desired.s1;
        this.t1 = desired.t1;
        this.s2 = desired.s2;
        this.t2 = desired.t2;
        this.error = desired.error;
    }, {

    // A generic two-segment piecewise regression calculator. Based on Kundu/Ubhaya
    //
    // Minimize sum of (y - y')^2
    // where                        y = s1 + t1*x
    //                              y = s2 + t2*x
    //                y' = s1 + t1*x' = s2 + t2*x'   if x_0 <= x' <= x_n
    //
    // Allows for fixing s1, t1, s2, t2
    //
    // x is assumed to be complexity, y is frame length. Can be used for pure complexity-FPS
    // analysis or for ramp controllers since complexity monotonically decreases with time.
    _calculateRegression: function(samples, getComplexity, getFrameLength, startIndex, endIndex, options)
    {
        var iterationDirection = endIndex > startIndex ? 1 : -1;
        var lowComplexity = getComplexity(samples, startIndex);
        var highComplexity = getComplexity(samples, endIndex);
        var a1 = 0, b1 = 0, c1 = 0, d1 = 0, h1 = 0, k1 = 0;
        var a2 = 0, b2 = 0, c2 = 0, d2 = 0, h2 = 0, k2 = 0;

        // Iterate from low to high complexity
        for (var i = startIndex; iterationDirection * (endIndex - i) > -1; i += iterationDirection) {
            var x = getComplexity(samples, i);
            var y = getFrameLength(samples, i);
            a2 += 1;
            b2 += x;
            c2 += x * x;
            d2 += y;
            h2 += y * x;
            k2 += y * y;
        }

        var s1_best, t1_best, s2_best, t2_best, x_best, error_best, x_prime;

        function setBest(s1, t1, s2, t2, error, x_prime, x)
        {
            s1_best = s1;
            t1_best = t1;
            s2_best = s2;
            t2_best = t2;
            error_best = error;
            if (!options.shouldClip || (x_prime >= lowComplexity && x_prime <= highComplexity))
                x_best = x_prime;
            else {
                // Discontinuous piecewise regression
                x_best = x;
            }
        }

        // Iterate from startIndex to endIndex - 1, inclusive
        for (var i = startIndex; iterationDirection * (endIndex - i) > 0; i += iterationDirection) {
            var x = getComplexity(samples, i);
            var y = getFrameLength(samples, i);
            var xx = x * x;
            var yx = y * x;
            var yy = y * y;
            // a1, b1, etc. is sum from startIndex to i, inclusive
            a1 += 1;
            b1 += x;
            c1 += xx;
            d1 += y;
            h1 += yx;
            k1 += yy;
            // a2, b2, etc. is sum from i+1 to endIndex, inclusive
            a2 -= 1;
            b2 -= x;
            c2 -= xx;
            d2 -= y;
            h2 -= yx;
            k2 -= yy;

            var A = c1*d1 - b1*h1;
            var B = a1*h1 - b1*d1;
            var C = a1*c1 - b1*b1;
            var D = c2*d2 - b2*h2;
            var E = a2*h2 - b2*d2;
            var F = a2*c2 - b2*b2;
            var s1 = options.s1 !== undefined ? options.s1 : (A / C);
            var t1 = options.t1 !== undefined ? options.t1 : (B / C);
            var s2 = options.s2 !== undefined ? options.s2 : (D / F);
            var t2 = options.t2 !== undefined ? options.t2 : (E / F);
            // Assumes that the two segments meet
            var x_prime = (s1 - s2) / (t2 - t1);

            var error1 = (k1 + a1*s1*s1 + c1*t1*t1 - 2*d1*s1 - 2*h1*t1 + 2*b1*s1*t1) || 0;
            var error2 = (k2 + a2*s2*s2 + c2*t2*t2 - 2*d2*s2 - 2*h2*t2 + 2*b2*s2*t2) || 0;

            if (i == startIndex) {
                setBest(s1, t1, s2, t2, error1 + error2, x_prime, x);
                continue;
            }

            // Projected point is not between this and the next sample
            if (x_prime > getComplexity(samples, i + iterationDirection) || x_prime < x) {
                // Calculate lambda, which divides the weight of this sample between the two lines

                // These values remove the influence of this sample
                var I = c1 - 2*b1*x + a1*xx;
                var H = C - I;
                var G = A + B*x - C*y;

                var J = D + E*x - F*y;
                var K = c2 - 2*b2*x + a2*xx;

                var lambda = (G*F + G*K - H*J) / (I*J + G*K);
                if (lambda > 0 && lambda < 1) {
                    var lambda1 = 1 - lambda;
                    s1 = options.s1 !== undefined ? options.s1 : ((A - lambda1*(-h1*x + d1*xx + c1*y - b1*yx)) / (C - lambda1*I));
                    t1 = options.t1 !== undefined ? options.t1 : ((B - lambda1*(h1 - d1*x - b1*y + a1*yx)) / (C - lambda1*I));
                    s2 = options.s2 !== undefined ? options.s2 : ((D + lambda1*(-h2*x + d2*xx + c2*y - b2*yx)) / (F + lambda1*K));
                    t2 = options.t2 !== undefined ? options.t2 : ((E + lambda1*(h2 - d2*x - b2*y + a2*yx)) / (F + lambda1*K));
                    x_prime = (s1 - s2) / (t2 - t1);

                    error1 = ((k1 + a1*s1*s1 + c1*t1*t1 - 2*d1*s1 - 2*h1*t1 + 2*b1*s1*t1) - lambda1 * Math.pow(y - (s1 + t1*x), 2)) || 0;
                    error2 = ((k2 + a2*s2*s2 + c2*t2*t2 - 2*d2*s2 - 2*h2*t2 + 2*b2*s2*t2) + lambda1 * Math.pow(y - (s2 + t2*x), 2)) || 0;
                }
            }

            if (error1 + error2 < error_best)
                setBest(s1, t1, s2, t2, error1 + error2, x_prime, x);
        }

        return {
            complexity: x_best,
            s1: s1_best,
            t1: t1_best,
            s2: s2_best,
            t2: t2_best,
            error: error_best
        };
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
        this._tier = 0;
        // The timestamp is first set after the first interval completes
        this._tierStartTimestamp = 0;
        // If the engine can handle the tier's complexity at 60 FPS, test for a short
        // period, then move on to the next tier
        this._tierFastTestLength = 250;
        // If the engine is under stress, let the test run a little longer to let
        // the measurement settle
        this._tierSlowTestLength = 750;
        this._maximumComplexity = 0;
        this._minimumTier = 0;

        // After the tier range is determined, figure out the number of ramp iterations
        var minimumRampLength = 3000;
        var totalRampIterations = Math.max(1, Math.floor(this._endTimestamp / minimumRampLength));
        // Give a little extra room to run since the ramps won't be exactly this length
        this._rampLength = Math.floor((this._endTimestamp - totalRampIterations * this._intervalLength) / totalRampIterations);
        this._rampWarmupLength = 200;
        this._rampDidWarmup = false;
        this._rampRegressions = [];

        // Add some tolerance; frame lengths shorter than this are considered to be @ 60 fps
        this._fps60Threshold = 1000/58;
        // We are looking for the complexity that will get us at least as slow this threshold
        this._fpsLowestThreshold = 1000/30;

        this._finishedTierSampling = false;
        this._startedRamps = false;
        this._complexityPrime = new Experiment;
    }, {

    start: function(startTimestamp, stage)
    {
        Controller.prototype.start.call(this, startTimestamp, stage);
        this._rampStartTimestamp = 0;
    },

    didFinishInterval: function(timestamp, stage, intervalAverageFrameLength)
    {
        if (!this._finishedTierSampling) {
            if (this._tierStartTimestamp > 0 && timestamp < this._tierStartTimestamp + this._tierFastTestLength)
                return;

            var currentComplexity = stage.complexity();
            var currentFrameLength = this._frameLengthEstimator.estimate;
            if (currentFrameLength < this._fpsLowestThreshold) {
                var isAnimatingAt60FPS = currentFrameLength < this._fps60Threshold;
                var hasFinishedSlowTierTest = timestamp > this._tierStartTimestamp + this._tierSlowTestLength;

                // We're measuring at 60 fps, so quickly move on to the next tier, or
                // we've slower than 60 fps, but we've let this tier run long enough to
                // get an estimate
                if (currentFrameLength < this._fps60Threshold || timestamp > this._tierStartTimestamp + this._tierSlowTestLength) {
                    this._lastComplexity = currentComplexity;
                    this._lastFrameLength = currentFrameLength;

                    this._tierStartTimestamp = timestamp;
                    this._tier += .5;
                    var nextTierComplexity = Math.round(Math.pow(10, this._tier));
                    this.mark("Complexity: " + nextTierComplexity, timestamp);

                    stage.tune(nextTierComplexity - currentComplexity);
                }
                return;
            } else if (timestamp < this._tierStartTimestamp + this._tierSlowTestLength)
                return;

            this._finishedTierSampling = true;
            // Extend the test length so that the full test length is made of the ramps
            this._endTimestamp += timestamp;
            this.mark(Strings.json.samplingStartTimeOffset, timestamp);

            // Sometimes this last tier will drop the frame length well below the threshold
            // Avoid going down that far since it means fewer measurements are taken in the 60 fps area
            // Interpolate a maximum complexity that gets us around the lowest threshold
            this._maximumComplexity = Math.floor(this._lastComplexity + (this._fpsLowestThreshold - this._lastFrameLength) / (currentFrameLength - this._lastFrameLength) * (currentComplexity - this._lastComplexity));
            stage.tune(this._maximumComplexity - currentComplexity);
            this._rampStartTimestamp = timestamp;
            this._rampDidWarmup = false;
            this.isFrameLengthEstimatorEnabled = false;
            this._intervalCount = 0;
            return;
        }

        if ((timestamp - this._rampStartTimestamp) < this._rampWarmupLength)
            return;

        if (this._rampDidWarmup)
            return;

        this._rampDidWarmup = true;
        this._currentRampLength = this._rampStartTimestamp + this._rampLength - timestamp;
        // Start timestamp represents start of ramp down, after warm up
        this._rampStartTimestamp = timestamp;
        this._rampStartIndex = this._sampler.sampleCount;
    },

    tune: function(timestamp, stage, didFinishInterval)
    {
        if (!didFinishInterval || !this._rampDidWarmup)
            return;

        var progress = (timestamp - this._rampStartTimestamp) / this._currentRampLength;

        if (progress < 1) {
            stage.tune(Math.round((1 - progress) * this._maximumComplexity) - stage.complexity());
            return;
        }

        var regression = new Regression(this._sampler.samples, this._getComplexity, this._getFrameLength,
            this._sampler.sampleCount - 1, this._rampStartIndex);
        this._rampRegressions.push(regression);

        this._complexityPrime.sample(regression.complexity);
        this._maximumComplexity = Math.max(5, Math.round(this._complexityPrime.mean() * 2));

        // Next ramp
        this._rampDidWarmup = false;
        // Start timestamp represents start of ramp iteration and warm up
        this._rampStartTimestamp = timestamp;
        stage.tune(this._maximumComplexity - stage.complexity());
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
        results[Strings.json.samples].forEach(function(sample) {
            sample.time -= startTimestamp;
        });
        for (var markName in results[Strings.json.marks]) {
            results[Strings.json.marks][markName].time -= startTimestamp;
        }

        var samples = results[Strings.json.samples];
        results[Strings.json.regressions.timeRegressions] = [];
        var complexityRegressionSamples = [];
        var timeComplexityScore = new Experiment;
        this._rampRegressions.forEach(function(ramp) {
            var startIndex = ramp.startIndex, endIndex = ramp.endIndex;
            var startTime = samples[startIndex].time, endTime = samples[endIndex].time;
            var startComplexity = samples[startIndex].complexity, endComplexity = samples[endIndex].complexity;

            timeComplexityScore.sample(ramp.complexity);

            var regression = {};
            results[Strings.json.regressions.timeRegressions].push(regression);

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
            regression[Strings.json.regressions.complexity] = ramp.complexity;
            regression[Strings.json.regressions.maxComplexity] = Math.max(startComplexity, endComplexity);
            regression[Strings.json.regressions.startIndex] = startIndex;
            regression[Strings.json.regressions.endIndex] = endIndex;

            for (var j = startIndex; j <= endIndex; ++j)
                complexityRegressionSamples.push(samples[j]);
        });

        // Aggregate all of the ramps into one big dataset and calculate a regression from this
        complexityRegressionSamples.sort(function(a, b) {
            return a.complexity - b.complexity;
        });

        // Samples averaged based on complexity
        results[Strings.json.complexityAverageSamples] = [];
        var currentComplexity = -1;
        var experimentAtComplexity;
        function addSample() {
            results[Strings.json.complexityAverageSamples].push({
                complexity: currentComplexity,
                frameLength: experimentAtComplexity.mean(),
                stdev: experimentAtComplexity.standardDeviation(),
            });
        }
        complexityRegressionSamples.forEach(function(sample) {
            if (sample.complexity != currentComplexity) {
                if (currentComplexity > -1)
                    addSample();

                currentComplexity = sample.complexity;
                experimentAtComplexity = new Experiment;
            }
            experimentAtComplexity.sample(sample.frameLength);
        });
        // Finish off the last one
        addSample();

        function calculateRegression(samples, key) {
            var complexityRegression = new Regression(
                samples,
                function (samples, i) { return samples[i].complexity; },
                function (samples, i) { return samples[i].frameLength; },
                0, samples.length - 1
            );
            var minComplexity = samples[0].complexity;
            var maxComplexity = samples[samples.length - 1].complexity;
            var regression = {};
            results[key] = regression;
            regression[Strings.json.regressions.segment1] = [
                [minComplexity, complexityRegression.s1 + complexityRegression.t1 * minComplexity],
                [complexityRegression.complexity, complexityRegression.s1 + complexityRegression.t1 * complexityRegression.complexity]
            ];
            regression[Strings.json.regressions.segment2] = [
                [complexityRegression.complexity, complexityRegression.s2 + complexityRegression.t2 * complexityRegression.complexity],
                [maxComplexity, complexityRegression.s2 + complexityRegression.t2 * maxComplexity]
            ];
            regression[Strings.json.regressions.complexity] = complexityRegression.complexity;
            regression[Strings.json.measurements.stdev] = Math.sqrt(complexityRegression.error / samples.length);
        }

        calculateRegression(complexityRegressionSamples, Strings.json.regressions.complexityRegression);
        calculateRegression(results[Strings.json.complexityAverageSamples], Strings.json.regressions.complexityAverageRegression);

        // Frame rate experiment result is unneeded
        delete results[Strings.json.experiments.frameRate];

        results[Strings.json.score] = timeComplexityScore.mean();
        results[Strings.json.experiments.complexity] = {};
        results[Strings.json.experiments.complexity][Strings.json.measurements.average] = timeComplexityScore.mean();
        results[Strings.json.experiments.complexity][Strings.json.measurements.stdev] = timeComplexityScore.standardDeviation();
        results[Strings.json.experiments.complexity][Strings.json.measurements.percent] = timeComplexityScore.percentage();
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
            this._controller = new AdaptiveController(this, options);
            break;
        case "ramp":
            this._controller = new RampController(this, options);
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

        if (this._controller.shouldStop(this._currentTimestamp)) {
            this._finishPromise.resolve(this._controller.results());
            return;
        }

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
        this._stage.animate(this._currentTimestamp - this._previousTimestamp);
        this._previousTimestamp = this._currentTimestamp;
        requestAnimationFrame(this._animateLoop);
    }
});
