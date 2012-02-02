var sampleRate = 44100.0;

// How many panner nodes to create for the test.
var nodesToCreate = 100;

// Time step when each panner node starts.
var timeStep = 0.001;

// Make sure we render long enough to get all of our nodes.
var renderLengthSeconds = timeStep * (nodesToCreate + 1);

// Length of an impulse signal.
var pulseLengthFrames = Math.round(timeStep * sampleRate);

// Globals to make debugging a little easier.
var context;
var impulse;
var bufferSource;
var panner;
var position;
var time;
      
// For the record, these distance formulas were taken from the OpenAL
// spec
// (http://connect.creativelabs.com/openal/Documentation/OpenAL%201.1%20Specification.pdf),
// not the code.  The Web Audio spec follows the OpenAL formulas.

function linearDistance(panner, x, y, z) {
    var distance = Math.sqrt(x * x + y * y + z * z);
    distance = Math.min(distance, panner.maxDistance);
    var rolloff = panner.rolloffFactor;
    var gain = (1 - rolloff * (distance - panner.refDistance) / (panner.maxDistance - panner.refDistance));

    return gain;
}

function inverseDistance(panner, x, y, z) {
    var distance = Math.sqrt(x * x + y * y + z * z);
    distance = Math.min(distance, panner.maxDistance);
    var rolloff = panner.rolloffFactor;
    var gain = panner.refDistance / (panner.refDistance + rolloff * (distance - panner.refDistance));

    return gain;
}

function exponentialDistance(panner, x, y, z) {
    var distance = Math.sqrt(x * x + y * y + z * z);
    distance = Math.min(distance, panner.maxDistance);
    var rolloff = panner.rolloffFactor;
    var gain = Math.pow(distance / panner.refDistance, -rolloff);

    return gain;
}

// This array must be arranged in the numeric order of the distance
// model values.
var distanceModelFunction = [linearDistance, inverseDistance, exponentialDistance];

function createGraph(context, distanceModel, nodeCount) {
    bufferSource = new Array(nodeCount);
    panner = new Array(nodeCount);
    position = new Array(nodeCount);
    time = new Array(nodesToCreate);

    impulse = createImpulseBuffer(context, pulseLengthFrames);

    // Create all the sources and panners.
    //
    // We MUST use the EQUALPOWER panning model so that we can easily
    // figure out the gain introduced by the panner.
    //
    // We want to stay in the middle of the panning range, which means
    // we want to stay on the z-axis.  If we don't, then the effect of
    // panning model will be much more complicated.  We're not testing
    // the panner, but the distance model, so we want the panner effect
    // to be simple.
    //
    // The panners are placed at a uniform intervals between the panner
    // reference distance and the panner max distance.  The source is
    // also started at regular intervals.
    for (var k = 0; k < nodeCount; ++k) {
        bufferSource[k] = context.createBufferSource();
        bufferSource[k].buffer = impulse;

        panner[k] = context.createPanner();
        panner[k].panningModel = panner.EQUALPOWER;
        panner[k].distanceModel = distanceModel;

        var distanceStep = (panner[k].maxDistance - panner[k].refDistance) / nodeCount;
        position[k] = distanceStep * k + panner[k].refDistance;
        panner[k].setPosition(0, 0, position[k]);

        bufferSource[k].connect(panner[k]);
        panner[k].connect(context.destination);

        time[k] = k * timeStep;
        bufferSource[k].noteOn(time[k]);
    }
}

// distanceModel should be the distance model constant like
// LINEAR_DISTANCE, INVERSE_DISTANCE, and EXPONENTIAL_DISTANCE.  The
// expectedModel is the expected actual numeric value of the constant.
function createTestAndRun(context, distanceModel, expectedModel) {
    // To test the distance models, we create a number of panners at
    // uniformly spaced intervals on the z-axis.  Each of these are
    // started at equally spaced time intervals.  After rendering the
    // signals, we examine where each impulse is located and the
    // attenuation of the impulse.  The attenuation is compared
    // against our expected attenuation.

    createGraph(context, distanceModel, nodesToCreate);

    context.oncomplete = checkDistanceResult(distanceModel, expectedModel);
    context.startRendering();
}

// The gain caused by the EQUALPOWER panning model, if we stay on the
// z axis, with the default orientations.
function equalPowerGain() {
    return Math.SQRT1_2;
}

function checkDistanceResult(model, expectedModel) {
    return function(event) {
        renderedBuffer = event.renderedBuffer;
        renderedData = renderedBuffer.getChannelData(0);

        // The max allowed error between the actual gain and the expected
        // value.  This is determined experimentally.  Set to 0 to see what
        // the actual errors are.
        var maxAllowedError = 2.3e-6;
   
        var success = true;

        // Number of impulses we found in the rendered result.
        var impulseCount = 0;

        // Maximum relative error in the gain of the impulses.
        var maxError = 0;

        // Array of locations of the impulses that were not at the
        // expected location.  (Contains the actual and expected frame
        // of the impulse.)
        var impulsePositionErrors = new Array();

        // Step through the rendered data to find all the non-zero points
        // so we can find where our distance-attenuated impulses are.
        // These are tested against the expected attenuations at that
        // distance.
        for (var k = 0; k < renderedData.length; ++k) {
            if (renderedData[k] != 0) {
                var distanceFunction = distanceModelFunction[panner[impulseCount].distanceModel];
                var expected = distanceFunction(panner[impulseCount], 0, 0, position[impulseCount]);

                // Adjust for the center-panning of the EQUALPOWER panning
                // model that we're using.
                expected *= equalPowerGain();

                var error = Math.abs(renderedData[k] - expected) / Math.abs(expected);

                maxError = Math.max(maxError, Math.abs(error));

                // Keep track of any impulses that aren't where we expect them
                // to be.
                var expectedOffset = timeToSampleFrame(time[impulseCount], sampleRate);
                if (k != expectedOffset) {
                    impulsePositionErrors.push({ actual : k, expected : expectedOffset});
                }
                ++impulseCount;
            }
        }

        if (model == expectedModel) {
            testPassed("Distance model value matched expected value.");
        } else {
            testFailed("Distance model value does not match expected value.");
            success = false;
        }    

        if (impulseCount == nodesToCreate) {
            testPassed("Number of impulses found matches number of panner nodes.");
        } else {
            testFailed("Number of impulses is incorrect.  Found " + impulseCount + " but expected " + nodesToCreate + ".");
            success = false;
        }

        if (maxError <= maxAllowedError) {
            testPassed("Distance gains are correct.");
        } else {
            testFailed("Distance gains are incorrect.  Max rel error = " + maxError + " (maxAllowedError = " + maxAllowedError + ")");
            success = false;
        }

        // Display any timing errors that we found.
        if (impulsePositionErrors.length > 0) {
            success = false;
            testFailed(impulsePositionErrors.length + " timing errors found");
            for (var k = 0; k < impulsePositionErrors.length; ++k) {
                testFailed("Sample at frame " + impulsePositionErrors[k].actual + " but expected " + impulsePositionErrors[k].expected);
            }
        }

        if (success) {
            testPassed("Distance test passed for distance model " + model);
        } else {
            testFailed("Distance test failed for distance model " + model);
        }

        finishJSTest();
    }
}
