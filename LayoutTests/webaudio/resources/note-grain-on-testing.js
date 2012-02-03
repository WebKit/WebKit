var sampleRate = 44100.0;

// HRTF extra frames.  This is a magic constant currently in
// AudioBufferSourceNode::process that always extends the
// duration by this number of samples.  See bug 77224
// (https://bugs.webkit.org/show_bug.cgi?id=77224).
var extraFramesHRTF = 512;

// How many grains to play.
var numberOfTests = 100;

// Duration of each grain to be played
var duration = 0.01;

// Time step between the start of each grain.  We need to add a little
// bit of silence so we can detect grain boundaries and also account
// for the extra frames for HRTF.
var timeStep = duration + .005 + extraFramesHRTF / sampleRate;

// Time step between the start for each grain.
var grainOffsetStep = 0.001;

// How long to render to cover all of the grains.
var renderTime = (numberOfTests + 1) * timeStep;

var context;
var renderedData;

// Create a buffer containing the data that we want.  The function f
// returns the desired value at sample frame k.
function createSignalBuffer(context, f) {

    // Make sure the buffer has enough data for all of the possible
    // grain offsets and durations.  Need to include the extra frames
    // for HRTF.  The additional 1 is for any round-off errors.
    var signalLength = Math.floor(1 + extraFramesHRTF + sampleRate * (numberOfTests * grainOffsetStep + duration));

    var buffer = context.createBuffer(2, signalLength, sampleRate);
    var data = buffer.getChannelData(0);

    for (var k = 0; k < signalLength; ++k) {
        data[k] = f(k);
    }

    return buffer;
}

// From the data array, find the start and end sample frame for each
// grain.  This depends on the data having 0's between grain, and
// that the grain is always strictly non-zero.
function findStartAndEndSamples(data) {
    var nSamples = data.length;

    var startTime = [];
    var endTime = [];
    var lookForStart = true;
      
    // Look through the rendered data to find the start and stop
    // times of each grain.
    for (var k = 0; k < nSamples; ++k) {
        if (lookForStart) {
            // Find a non-zero point and record the start.  We're not
            // concerned with the value in this test, only that the
            // grain started here.
            if (renderedData[k]) {
                startTime.push(k);
                lookForStart = false;
            }
        } else {
            // Find a zero and record the end of the grain.
            if (!renderedData[k]) {
                endTime.push(k);
                lookForStart = true;
            }
        }
    }

    return {start : startTime, end : endTime};
}

function playGrain(context, source, time, offset, duration) {
    var bufferSource = context.createBufferSource();

    bufferSource.buffer = source;
    bufferSource.connect(context.destination);
    bufferSource.noteGrainOn(time, offset, duration);
}

// Play out all grains.  Returns a object containing two arrays, one
// for the start time and one for the grain offset time.
function playAllGrains(context, source, numberOfNotes) {
    var startTimes = new Array(numberOfNotes);
    var offsets = new Array(numberOfNotes);

    for (var k = 0; k < numberOfNotes; ++k) {
        var timeOffset = k * timeStep;
        var grainOffset = k * grainOffsetStep;

        playGrain(context, source, timeOffset, grainOffset, duration);
        startTimes[k] = timeOffset;
        offsets[k] = grainOffset;
    }

    return { startTimes : startTimes, grainOffsetTimes : offsets };
}

// Verify that the start and end frames for each grain match our
// expected start and end frames.
function verifyStartAndEndFrames(startEndFrames) {
    var startFrames = startEndFrames.start;
    var endFrames = startEndFrames.end;
      
    var success = true;

    // Count of how many grains started at the incorrect time.
    var errorCountStart = 0;

    // Count of how many grains ended at the incorrect time.
    var errorCountEnd = 0;

    if (startFrames.length != endFrames.length) {
        testFailed("Could not find the beginning or end of a grain.");
        success = false;
    }

    if (startFrames.length == numberOfTests && endFrames.length == numberOfTests) {
        testPassed("Found all " + numberOfTests + " grains.");
    } else {
        testFailed("Did not find all " + numberOfTests + " grains.");
    }

    // Examine the start and stop times to see if they match our
    // expectations.
    for (var k = 0; k < startFrames.length; ++k) {
        var expectedStart = timeToSampleFrame(k * timeStep, sampleRate);
        // The end point is the duration, plus the extra frames
        // for HRTF.
        var expectedEnd = extraFramesHRTF + expectedStart + grainLengthInSampleFrames(k * grainOffsetStep, duration, sampleRate);

        if (startFrames[k] != expectedStart) {
            testFailed("Pulse " + k + " started at " + startFrames[k] + " but expected at " + expectedStart);
            ++errorCountStart;
            success = false;
        }

        if (endFrames[k] != expectedEnd) {
            testFailed("Pulse " + k + " ended at " + endFrames[k] + " but expected at " + expectedEnd);
            ++errorCountEnd;
            success = false;
        }
    }

    // Check that all the grains started or ended at the correct time.
    if (!errorCountStart) {
        if (startFrames.length == numberOfTests) {
            testPassed("All " + numberOfTests + " grains started at the correct time.");
        } else {
            testFailed("All grains started at the correct time, but only " + startFrames.length + " grains found.");
            success = false;
        }
    } else {
        testFailed(errorCountStart + " out of " + numberOfTests + " grains started at the wrong time.");
        success = false;
    }

    if (!errorCountEnd) {
        if (endFrames.length == numberOfTests) {
            testPassed("All " + numberOfTests + " grains ended at the correct time.");
        } else {
            testFailed("All grains ended at the correct time, but only " + endFrames.length + " grains found.");
            success = false;
        }
    } else {
        testFailed(errorCountEnd + " out of " + numberOfTests + " grains ended at the wrong time.");
        success = false;
    }

    return success;
}
