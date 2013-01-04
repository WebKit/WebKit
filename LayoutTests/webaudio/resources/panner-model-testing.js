var sampleRate = 44100.0;

var numberOfChannels = 1;

// Time step when each panner node starts.
var timeStep = 0.001;

// Length of the impulse signal.
var pulseLengthFrames = Math.round(timeStep * sampleRate);

// How many panner nodes to create for the test
var nodesToCreate = 100;

// Be sure we render long enough for all of our nodes.
var renderLengthSeconds = timeStep * (nodesToCreate + 1);

// These are global mostly for debugging.
var context;
var impulse;
var bufferSource;
var panner;
var position;
var time;
      
var renderedBuffer;
var renderedLeft;
var renderedRight;

function createGraph(context, nodeCount) {
    bufferSource = new Array(nodeCount);
    panner = new Array(nodeCount);
    position = new Array(nodeCount);
    time = new Array(nodeCount);
    // Angle between panner locations.  (nodeCount - 1 because we want
    // to include both 0 and 180 deg.
    var angleStep = Math.PI / (nodeCount - 1);

    if (numberOfChannels == 2) {
        impulse = createStereoImpulseBuffer(context, pulseLengthFrames);
    }
    else
        impulse = createImpulseBuffer(context, pulseLengthFrames);

    for (var k = 0; k < nodeCount; ++k) {
        bufferSource[k] = context.createBufferSource();
        bufferSource[k].buffer = impulse;

        panner[k] = context.createPanner();
        panner[k].panningModel = "equalpower";
        panner[k].distanceModel = "linear";

        var angle = angleStep * k;
        position[k] = {angle : angle, x : Math.cos(angle), z : Math.sin(angle)};
        panner[k].setPosition(position[k].x, 0, position[k].z);

        bufferSource[k].connect(panner[k]);
        panner[k].connect(context.destination);

        // Start the source
        time[k] = k * timeStep;
        bufferSource[k].noteOn(time[k]);
    }
}

function createTestAndRun(context, nodeCount, numberOfSourceChannels) {
    numberOfChannels = numberOfSourceChannels;

    createGraph(context, nodeCount);

    context.oncomplete = checkResult;
    context.startRendering();
}

// Map our position angle to the azimuth angle (in degrees).
//
// An angle of 0 corresponds to an azimuth of 90 deg; pi, to -90 deg.
function angleToAzimuth(angle) {
    return 90 - angle * 180 / Math.PI;
}

// The gain caused by the EQUALPOWER panning model
function equalPowerGain(angle) {
    var azimuth = angleToAzimuth(angle);

    if (numberOfChannels == 1) {
        var panPosition = (azimuth + 90) / 180;

        var gainL = Math.cos(0.5 * Math.PI * panPosition);
        var gainR = Math.sin(0.5 * Math.PI * panPosition);

        return { left : gainL, right : gainR };
    } else {
        if (azimuth <= 0) {
            var panPosition = (azimuth + 90) / 90;
    
            var gainL = 1 + Math.cos(0.5 * Math.PI * panPosition);
            var gainR = Math.sin(0.5 * Math.PI * panPosition);
    
            return { left : gainL, right : gainR };
        } else {
            var panPosition = azimuth / 90;
    
            var gainL = Math.cos(0.5 * Math.PI * panPosition);
            var gainR = 1 + Math.sin(0.5 * Math.PI * panPosition);
    
            return { left : gainL, right : gainR };
        }
    }
}

function checkResult(event) {
    renderedBuffer = event.renderedBuffer;
    renderedLeft = renderedBuffer.getChannelData(0);
    renderedRight = renderedBuffer.getChannelData(1);

    // The max error we allow between the rendered impulse and the
    // expected value.  This value is experimentally determined.  Set
    // to 0 to make the test fail to see what the actual error is.
    var maxAllowedError = 1.3e-6;
  
    var success = true;

    // Number of impulses found in the rendered result.
    var impulseCount = 0;

    // Max (relative) error and the index of the maxima for the left
    // and right channels.
    var maxErrorL = 0;
    var maxErrorIndexL = 0;
    var maxErrorR = 0;
    var maxErrorIndexR = 0;

    // Number of impulses that don't match our expected locations.
    var timeCount = 0;

    // Locations of where the impulses aren't at the expected locations.
    var timeErrors = new Array();

    for (var k = 0; k < renderedLeft.length; ++k) {
        // We assume that the left and right channels start at the same instant.
        if (renderedLeft[k] != 0 || renderedRight[k] != 0) {
            // The expected gain for the left and right channels.
            var pannerGain = equalPowerGain(position[impulseCount].angle);
            var expectedL = pannerGain.left;
            var expectedR = pannerGain.right;

            // Absolute error in the gain.
            var errorL = Math.abs(renderedLeft[k] - expectedL);
            var errorR = Math.abs(renderedRight[k] - expectedR);

            if (Math.abs(errorL) > maxErrorL) {
                maxErrorL = Math.abs(errorL);
                maxErrorIndexL = impulseCount;
            }
            if (Math.abs(errorR) > maxErrorR) {
                maxErrorR = Math.abs(errorR);
                maxErrorIndexR = impulseCount;
            }

            // Keep track of the impulses that didn't show up where we
            // expected them to be.
            var expectedOffset = timeToSampleFrame(time[impulseCount], sampleRate);
            if (k != expectedOffset) {
                timeErrors[timeCount] = { actual : k, expected : expectedOffset};
                ++timeCount;
            }
            ++impulseCount;
        }
    }

    if (impulseCount == nodesToCreate) {
        testPassed("Number of impulses matches the number of panner nodes.");
    } else {
        testFailed("Number of impulses is incorrect.  (Found " + impulseCount + " but expected " + nodesToCreate + ")");
        success = false;
    }

    if (timeErrors.length > 0) {
        success = false;
        testFailed(timeErrors.length + " timing errors found in " + nodesToCreate + " panner nodes.");
        for (var k = 0; k < timeErrors.length; ++k) {
            testFailed("Impulse at sample " + timeErrors[k].actual + " but expected " + timeErrors[k].expected);
        }
    } else {
        testPassed("All impulses at expected offsets.");
    }

    if (maxErrorL <= maxAllowedError) {
        testPassed("Left channel gain values are correct.");
    } else {
        testFailed("Left channel gain values are incorrect.  Max error = " + maxErrorL + " at time " + time[maxErrorIndexL] + " (threshold = " + maxAllowedError + ")");
        success = false;
    }
    
    if (maxErrorR <= maxAllowedError) {
        testPassed("Right channel gain values are correct.");
    } else {
        testFailed("Right channel gain values are incorrect.  Max error = " + maxErrorR + " at time " + time[maxErrorIndexR] + " (threshold = " + maxAllowedError + ")");
        success = false;
    }

    if (success) {
        testPassed("EqualPower panner test passed");
    } else {
        testFailed("EqualPower panner test failed");
    }

    finishJSTest();
}
