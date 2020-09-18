var sampleRate = 44100;

// Information about the starting/ending times and starting/ending values for each time interval.
var timeValueInfo;

// The difference between starting values between each time interval.
var startingValueDelta;
      
// For any automation function that has an end or target value, the end value is based the starting
// value of the time interval.  The starting value will be increased or decreased by
// |startEndValueChange|. We choose half of |startingValueDelta| so that the ending value will be
// distinct from the starting value for next time interval.  This allows us to detect where the ramp
// begins and ends.
var startEndValueChange;

// Default threshold to use for detecting discontinuities that should appear at each time interval.
var discontinuityThreshold;

// Time interval between value changes.  It is best if 1 / numberOfTests is not close to timeInterval.
var timeInterval = .03;

// Some suitable time constant so that we can see a significant change over a timeInterval.  This is
// only needed by setTargetAtTime() which needs a time constant.
var timeConstant = timeInterval / 3;

var gainNode;

var context;

// Make sure we render long enough to capture all of our test data.
function renderLength(numberOfTests)
{
    return timeToSampleFrame((numberOfTests + 1) * timeInterval, sampleRate);
}

// Create a buffer containing the same constant value.
function createConstantBuffer(context, constant, length) {
    var buffer = context.createBuffer(1, length, context.sampleRate);
    var n = buffer.length;
    var data = buffer.getChannelData(0);

    for (var k = 0; k < n; ++k) {
        data[k] = constant;
    }

    return buffer;
}

// Create a constant reference signal with the given |value|.  Basically the same as
// |createConstantBuffer|, but with the parameters to match the other create functions.  The
// |endValue| is ignored.
function createConstantArray(startTime, endTime, value, endValue, sampleRate)
{
    var startFrame = timeToSampleFrame(startTime, sampleRate);
    var endFrame = timeToSampleFrame(endTime, sampleRate);
    var length = endFrame - startFrame;

    var buffer = createConstantBuffer(context, value, length);

    return buffer.getChannelData(0);
}

// Create a linear ramp starting at |startValue| and ending at |endValue|.  The ramp starts at time
// |startTime| and ends at |endTime|.  (The start and end times are only used to compute how many
// samples to return.)
function createLinearRampArray(startTime, endTime, startValue, endValue, sampleRate)
{
    var startFrame = timeToSampleFrame(startTime, sampleRate);
    var endFrame = timeToSampleFrame(endTime, sampleRate);
    var length = endFrame - startFrame;
    var array = new Array(length);

    var step = (endValue - startValue) / length;

    for (k = 0; k < length; ++k) {
        array[k] = startValue + k * step;
    }

    return array;
}

// Create an exponential ramp starting at |startValue| and ending at |endValue|.  The ramp starts at
// time |startTime| and ends at |endTime|.  (The start and end times are only used to compute how
// many samples to return.)
function createExponentialRampArray(startTime, endTime, startValue, endValue, sampleRate)
{
    var startFrame = timeToSampleFrame(startTime, sampleRate);
    var endFrame = timeToSampleFrame(endTime, sampleRate);
    var length = endFrame - startFrame;
    var array = new Array(length);

    var multiplier = Math.pow(endValue / startValue, 1 / length);
    
    for (var k = 0; k < length; ++k) {
        array[k] = startValue * Math.pow(multiplier, k);
    }

    return array;
}

function discreteTimeConstantForSampleRate(timeConstant, sampleRate)
{
    return 1 - Math.exp(-1 / (sampleRate * timeConstant));
}

// Create a signal that starts at |startValue| and exponentially approaches the target value of
// |targetValue|, using a time constant of |timeConstant|.  The ramp starts at time |startTime| and
// ends at |endTime|.  (The start and end times are only used to compute how many samples to
// return.)
function createExponentialApproachArray(startTime, endTime, startValue, targetValue, sampleRate, timeConstant)
{
    var startFrame = timeToSampleFrame(startTime, sampleRate);
    var endFrame = timeToSampleFrame(endTime, sampleRate);
    var length = endFrame - startFrame;
    var array = new Array(length);
    var c = discreteTimeConstantForSampleRate(timeConstant, sampleRate);

    var value = startValue;
    
    for (var k = 0; k < length; ++k) {
        array[k] = value;
        value += (targetValue - value) * c;
    }

    return array;
}

// Create a sine wave of the given frequency and amplitude.  The sine wave is offset by half the
// amplitude so that result is always positive.
function createSineWaveArray(durationSeconds, freqHz, amplitude, sampleRate)
{
    var length = timeToSampleFrame(durationSeconds, sampleRate);
    var signal = new Float32Array(length);
    var omega = 2 * Math.PI * freqHz / sampleRate;
    var halfAmplitude = amplitude / 2;
    
    for (var k = 0; k < length; ++k) {
        signal[k] = halfAmplitude + halfAmplitude * Math.sin(omega * k);
    }

    return signal;
}

// Return the difference between the starting value and the ending value for time interval
// |timeIntervalIndex|.  We alternate between an end value that is above or below the starting
// value.
function endValueDelta(timeIntervalIndex)
{
    if (timeIntervalIndex & 1) {
        return -startEndValueChange;
    } else {
        return startEndValueChange;
    }
}

// Return the difference between the starting value at |timeIntervalIndex| and the starting value at
// the next time interval.  Since we started at a large initial value, we decrease the value at each
// time interval.
function valueUpdate(timeIntervalIndex)
{
    return -startingValueDelta;
}

// Compare a section of the rendered data against our expected signal.
function comparePartialSignals(rendered, expectedFunction, startTime, endTime, valueInfo, sampleRate)
{
    var startSample = timeToSampleFrame(startTime, sampleRate);
    var expected = expectedFunction(startTime, endTime, valueInfo.startValue, valueInfo.endValue, sampleRate, timeConstant);

    var n = expected.length;
    var maxError = -1;
    var maxErrorIndex = -1;
    
    for (var k = 0; k < n; ++k) {
        // Make sure we don't pass these tests because a NaN has been generated in either the
        // rendered data or the reference data.
        if (!isValidNumber(rendered[startSample + k])) {
            maxError = Infinity;
            maxErrorIndex = startSample + k;
            testFailed("NaN or infinity for rendered data at " + maxErrorIndex);
            break;
        }
        if (!isValidNumber(expected[k])) {
            maxError = Infinity;
            maxErrorIndex = startSample + k;
            testFailed("Nan or infinity for reference data at " + maxErrorIndex);
            break;
        }
        var error = Math.abs(rendered[startSample + k] - expected[k]);
        if (error > maxError) {
            maxError = error;
            maxErrorIndex = k;
        }
    }

    return {maxError : maxError, index : maxErrorIndex};
}

// Find the discontinuities in the data and compare the locations of the discontinuities with the
// times that define the time intervals. There is a discontinuity if the difference between
// successive samples exceeds the threshold.
function verifyDiscontinuities(values, times, threshold)
{
    var n = values.length;
    var success = true;
    var badLocations = 0;
    var breaks = [];

    // Find discontinuities.
    for (var k = 1; k < n; ++k) {
        if (Math.abs(values[k] - values[k - 1]) > threshold) {
            breaks.push(k);
        }
    }

    var testCount;

    // If there are numberOfTests intervals, there are only numberOfTests - 1 internal interval
    // boundaries. Hence the maximum number of discontinuties we expect to find is numberOfTests -
    // 1. If we find more than that, we have no reference to compare against. We also assume that
    // the actual discontinuities are close to the expected ones.
    //
    // This is just a sanity check when something goes really wrong.  For example, if the threshold
    // is too low, every sample frame looks like a discontinuity.
    if (breaks.length >= numberOfTests) {
        testCount = numberOfTests - 1;
        testFailed("Found more discontinuities (" + breaks.length + ") than expected.  Only comparing first " + testCount + "discontinuities.");
        success = false;
    } else {
        testCount = breaks.length;
    }
    
    // Compare the location of each discontinuity with the end time of each interval. (There is no
    // discontinuity at the start of the signal.)
    for (var k = 0; k < testCount; ++k) {
        var expectedSampleFrame = timeToSampleFrame(times[k + 1], sampleRate);
        if (breaks[k] != expectedSampleFrame) {
            success = false;
            ++badLocations;
            testFailed("Expected discontinuity at " + expectedSampleFrame + " but got " + breaks[k]);
        }
    }

    if (badLocations) {
        testFailed(badLocations + " discontinuities at incorrect locations");
        success = false;
    } else {
        if (breaks.length == numberOfTests - 1) {
            testPassed("All " + numberOfTests + " tests started and ended at the correct time.");
        } else {
            testFailed("Found " + breaks.length + " discontinuities but expected " + (numberOfTests - 1));
            success = false;
        }
    }
    
    return success;
}

// Compare the rendered data with the expected data.
//
// testName - string describing the test
//
// maxError - maximum allowed difference between the rendered data and the expected data
//
// rendererdData - array containing the rendered (actual) data
//
// expectedFunction - function to compute the expected data
//
// timeValueInfo - array containing information about the start and end times and the start and end
// values of each interval.
//
// breakThreshold - threshold to use for determining discontinuities.
function compareSignals(testName, maxError, renderedData, expectedFunction, timeValueInfo, breakThreshold)
{
    var success = true;
    var failedTestCount = 0;
    var times = timeValueInfo.times;
    var values = timeValueInfo.values;
    var n = values.length;

    success = verifyDiscontinuities(renderedData, times, breakThreshold);

    for (var k = 0; k < n; ++k) {
        var result = comparePartialSignals(renderedData, expectedFunction, times[k], times[k + 1], values[k], sampleRate);

        if (result.maxError > maxError) {
            testFailed("Incorrect value for test " + k + ". Max error = " + result.maxError + " at offset " + (result.index + timeToSampleFrame(times[k], sampleRate)));
            ++failedTestCount;
        }
    }

    if (failedTestCount) {
        testFailed(failedTestCount + " tests failed out of " + n);
        success = false;
    } else {
        testPassed("All " + n + " tests passed within an acceptable tolerance.");
    }
      
    if (success) {
        testPassed("AudioParam " + testName + " test passed.");
    } else {
        testFailed("AudioParam " + testName + " test failed.");
    }
}

// Create a function to test the rendered data with the reference data.
//
// testName - string describing the test
//
// error - max allowed error between rendered data and the reference data.
//
// referenceFunction - function that generates the reference data to be compared with the rendered
// data.
//
// jumpThreshold - optional parameter that specifies the threshold to use for detecting
// discontinuities.  If not specified, defaults to discontinuityThreshold.
//
function checkResultFunction(testName, error, referenceFunction, jumpThreshold)
{
    return function(event) {
        var buffer = event.renderedBuffer;
        renderedData = buffer.getChannelData(0);

        var threshold;

        if (!jumpThreshold) {
            threshold = discontinuityThreshold;
        } else {
            threshold = jumpThreshold;
        }
        
        compareSignals(testName, error, renderedData, referenceFunction, timeValueInfo, threshold);

        finishJSTest();
    }
}

// Run all the automation tests.
//
// numberOfTests - number of tests (time intervals) to run.
//
// initialValue - The initial value of the first time interval.
//
// setValueFunction - function that sets the specified value at the start of a time interval.
//
// automationFunction - function that sets the end value for the time interval.  It specifies how
// the value approaches the end value.
//
// An object is returned containing an array of start times for each time interval, and an array
// giving the start and end values for the interval.
function doAutomation(numberOfTests, initialValue, setValueFunction, automationFunction)
{
    var timeInfo = [0];
    var valueInfo = [];
    var value = initialValue;
    
    for (var k = 0; k < numberOfTests; ++k) {
        var startTime = k * timeInterval;
        var endTime = (k + 1) * timeInterval;
        var endValue = value + endValueDelta(k);

        // Set the value at the start of the time interval.
        setValueFunction(value, startTime);

        // Specify the end or target value, and how we should approach it.
        automationFunction(endValue, startTime, endTime);

        // Keep track of the start times, and the start and end values for each time interval.
        timeInfo.push(endTime);
        valueInfo.push({startValue: value, endValue : endValue});

        value += valueUpdate(k);
    }

    return {times : timeInfo, values : valueInfo};
}

// Create the audio graph for the test and then run the test.
//
// numberOfTests - number of time intervals (tests) to run.
//
// initialValue - the initial value of the gain at time 0.
//
// setValueFunction - function to set the value at the beginning of each time interval.
//
// automationFunction - the AudioParamTimeline automation function
//
// testName - string indicating the test that is being run.
//
// maxError - maximum allowed error between the rendered data and the reference data
//
// referenceFunction - function that generates the reference data to be compared against the
// rendered data.
//
// jumpThreshold - optional parameter that specifies the threshold to use for detecting
// discontinuities.  If not specified, defaults to discontinuityThreshold.
//
function createAudioGraphAndTest(numberOfTests, initialValue, setValueFunction, automationFunction, testName, maxError, referenceFunction, jumpThreshold)
{
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
    }

    window.jsTestIsAsync = true;

    // Create offline audio context.
    context = new webkitOfflineAudioContext(2, renderLength(numberOfTests), sampleRate);
    var constantBuffer = createConstantBuffer(context, 1, renderLength(numberOfTests));

    // We use an AudioGainNode here simply as a convenient way to test the AudioParam
    // automation, since it's easy to pass a constant value through the node, automate the
    // .gain attribute and observe the resulting values.

    gainNode = context.createGain();

    var bufferSource = context.createBufferSource();
    bufferSource.buffer = constantBuffer;
    bufferSource.connect(gainNode);
    gainNode.connect(context.destination);

    // Set up default values for the parameters that control how the automation test values progress
    // for each time interval.
    startingValueDelta = initialValue / numberOfTests;
    startEndValueChange = startingValueDelta / 2;
    discontinuityThreshold = startEndValueChange / 2;

    // Run the automation tests.
    timeValueInfo = doAutomation(numberOfTests,
                                 initialValue,
                                 setValueFunction,
                                 automationFunction);
    bufferSource.start(0);
      
    context.oncomplete = checkResultFunction(testName,
                                             maxError,
                                             referenceFunction,
                                             jumpThreshold);
    context.startRendering();
}
