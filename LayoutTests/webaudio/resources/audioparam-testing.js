var sampleRate = 44100;

// Number of tests to run.
var numberOfTests = 100;

// Time interval between value changes.  It is best if 1 / numberOfTests is not close to
// timeInterval.
var timeInterval = .03;

// Make sure we render long enough to capture all of our test data.
var renderTime = (numberOfTests + 1) * timeInterval;
var renderLength = timeToSampleFrame(renderTime, sampleRate);

var context;

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

// Create an exponential ramp starting at |startValue| and ending at |endValue|.  The ramp starts at
// time |startTime| and ends at |endTime|.  (The start and end times are only used to compute how
// many samples to return.)
function createExponentialRampArray(startTime, endTime, startValue, endValue, sampleRate)
{
    var startFrame = timeToSampleFrame(startTime, sampleRate);
    var endFrame = timeToSampleFrame(endTime, sampleRate);
    var length = endFrame - startFrame;
    var buffer = new Array(length);

    var multiplier = Math.pow(endValue / startValue, 1 / length);
    
    for (var k = 0; k < length; ++k) {
        buffer[k] = startValue * Math.pow(multiplier, k);
    }

    return buffer;
}

// Compare a section of the rendered data against our expected signal.
function comparePartialSignals(rendered, expectedFunction, startTime, endTime, valueInfo, sampleRate)
{
    var startSample = timeToSampleFrame(startTime, sampleRate);
    var expected = expectedFunction(startTime, endTime, valueInfo.startValue, valueInfo.endValue, sampleRate);

    var n = expected.length;
    var maxError = -1;
    var maxErrorIndex = -1;
    
    for (var k = 0; k < n; ++k) {
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

    // Compare the location of each discontinuity with the end time of each interval. (There is no
    // discontinuity at the start of the signal.)
    for (var k = 0; k < breaks.length; ++k) {
        var expectedSampleFrame = timeToSampleFrame(times[k + 1], sampleRate);
        if (breaks[k] != expectedSampleFrame) {
            success = false;
            ++badLocations;
            testFailed("Expected discontinuity at " + expectedSampleFrame + " but got " + breaks[k]);
        }
    }

    if (badLocations) {
        testFailed(badLocations + " discontinuities at incorrect locations");
    } else {
        testPassed("All " + (breaks.length + 1) + " tests started and ended at the correct time.");
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
            testFailed("Incorrect gain for test " + k + ". Max error = " + result.maxError + " at offset " + (result.index + timeToSampleFrame(times[k], sampleRate)));
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

// Run all the automation tests.
//
// initialValue - The initial value of the first time interval.
//
// endValueDeltaFunction - How much the end value differs from the value at the start of the
// interval.
//
// setValueFunction - function that sets the specified value at the start of a time interval.
//
// automationFunction - function that sets the end value for the time interval.  It specifies how
// the value approaches the end value.
//
// valueUpdateFunction - The starting value at time interval k is adjusted by valueUpdateFunction(k) to give the new starting value at time interval k + 1.
//
// An object is returned containing an array of start times for each time interval, and an array
// giving the start and end values for the interval.
function doAutomation(initialValue, endValueDeltaFunction, setValueFunction, automationFunction, valueUpdateFunction)
{
    var timeInfo = [0];
    var valueInfo = [];
    var value = initialValue;
    
    for (var k = 0; k < numberOfTests; ++k) {
        var startTime = k * timeInterval;
        var endTime = (k + 1) * timeInterval;
        var endValue = value + endValueDeltaFunction(k);

        // Set the value at the start of the time interval.
        setValueFunction(value, startTime);

        // Specify the target value, and how we should approach the target value.
        automationFunction(endValue, startTime, endTime);

        // Keep track of the start times, and the start and end values for each time interval.
        timeInfo.push(endTime);
        valueInfo.push({startValue: value, endValue : endValue});

        value += valueUpdateFunction(k);
    }

    return {times : timeInfo, values : valueInfo};
}
