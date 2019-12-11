function createTestBuffer(context, sampleFrameLength) {
    var audioBuffer = context.createBuffer(1, sampleFrameLength, context.sampleRate);
    var channelData = audioBuffer.getChannelData(0);

    // Create a simple linear ramp starting at zero, with each value in the buffer equal to its index position.
    for (var i = 0; i < sampleFrameLength; ++i)
        channelData[i] = i;

    return audioBuffer;
}

function createRamp(context, startValue, endValue, numberOfSamples) {
    var audioBuffer = context.createBuffer(1, numberOfSamples, context.sampleRate);
    var channelData = audioBuffer.getChannelData(0);

    var delta = (endValue - startValue) / (numberOfSamples - 1);
    var nextValue = startValue;

    for (var i = 0; i < numberOfSamples; ++i) {
        channelData[i] = nextValue;
        nextValue += delta;
    }

    return audioBuffer;
}

function checkSingleTest(renderedBuffer, i) {
    var renderedData = renderedBuffer.getChannelData(0);
    var offsetFrame = i * testSpacingFrames;

    var test = tests[i];
    var description = test.description;
    var expected = test.expected;

    var success = true;

    for (var j = 0; j < test.renderFrames; ++j) {
        if (expected[j] != renderedData[offsetFrame + j]) {
            // Copy from Float32Array to regular JavaScript array for error message.
            var renderedArray = new Array();
            for (var j = 0; j < test.renderFrames; ++j)
                renderedArray[j] = renderedData[offsetFrame + j];

            var s = description + ": expected: " + expected + " actual: " + renderedArray;
            testFailed(s);
            success = false;
            break;
        }
    }

    if (success)
        testPassed(description);

    return success;
}

function checkAllTests(event) {
    var renderedBuffer = event.renderedBuffer;
    for (var i = 0; i < tests.length; ++i)
        checkSingleTest(renderedBuffer, i);

    finishJSTest();
}
