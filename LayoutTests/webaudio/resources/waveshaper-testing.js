var context;
var lengthInSeconds = 2;

// Skip this many frames before comparing against reference to allow
// a steady-state to be reached in the up-sampling filters.
var filterStabilizeSkipFrames = 2048;

var numberOfCurveFrames = 65536;
var waveShapingCurve;

var waveshaper;

// FIXME: test at more frequencies.
// When using the up-sampling filters (2x, 4x) any significant aliasing components
// should be at very high frequencies near Nyquist.  These tests could be improved
// to allow for a higher acceptable amount of aliasing near Nyquist, but then
// become more stringent for lower frequencies.

// These test parameters are set in runWaveShaperOversamplingTest().
var sampleRate;
var nyquist;
var oversample;
var fundamentalFrequency;
var acceptableAliasingThresholdDecibels;

var kScale = 0.25;

// Chebyshev Polynomials.
// Given an input sinusoid, returns an output sinusoid of the given frequency multiple.
function T0(x) { return 1; }
function T1(x) { return x; }
function T2(x) { return 2*x*x - 1; }
function T3(x) { return 4*x*x*x - 3*x; }
function T4(x) { return 8*x*x*x*x - 8*x*x + 1; }

function generateWaveShapingCurve() {
    var n = 65536;
    var n2 = n / 2;
    var curve = new Float32Array(n);

    // The shaping curve uses Chebyshev Polynomial such that an input sinusoid
    // at frequency f will generate an output of four sinusoids of frequencies:
    // f, 2*f, 3*f, 4*f
    // each of which is scaled.
    for (var i = 0; i < n; ++i) {
        var x = (i - n2) / n2;
        var y = kScale * (T1(x) + T2(x) + T3(x) + T4(x));
        curve[i] = y;
    }

    return curve;
}

function checkShapedCurve(event) {
    var buffer = event.renderedBuffer;

    var outputData = buffer.getChannelData(0);
    var n = buffer.length;

    // The WaveShaperNode will have a processing latency if oversampling is used,
    // so we should account for it.

    // FIXME: .latency should be exposed as an attribute of the node
    // var waveShaperLatencyFrames = waveshaper.latency * sampleRate;
    // But for now we'll use the hard-coded values corresponding to the actual latencies:
    var waveShaperLatencyFrames = 0;
    if (oversample == "2x")
        waveShaperLatencyFrames = 128;
    else if (oversample == "4x")
        waveShaperLatencyFrames = 192;

    var worstDeltaInDecibels = -1000;

    for (var i = waveShaperLatencyFrames; i < n; ++i) {
        var actual = outputData[i];

        // Account for the expected processing latency.
        var j = i - waveShaperLatencyFrames;

        // Compute reference sinusoids.
        var phaseInc = 2 * Math.PI * fundamentalFrequency / sampleRate;

        // Generate an idealized reference based on the four generated frequencies truncated
        // to the Nyquist rate.  Ideally, we'd like the waveshaper's oversampling to perfectly
        // remove all frequencies above Nyquist to avoid aliasing.  In reality the oversampling filters are not
        // quite perfect, so there will be a (hopefully small) amount of aliasing.  We should
        // be close to the ideal.
        var reference = 0;

        // Sum in fundamental frequency.
        if (fundamentalFrequency < nyquist)
            reference += Math.sin(phaseInc * j);

        // Note that the phase of each of the expected generated harmonics is different.
        if (fundamentalFrequency * 2 < nyquist)
            reference += -Math.cos(phaseInc * j * 2);
        if (fundamentalFrequency * 3 < nyquist)
            reference += -Math.sin(phaseInc * j * 3);
        if (fundamentalFrequency * 4 < nyquist)
            reference += Math.cos(phaseInc * j * 4);

        // Scale the reference the same as the waveshaping curve itself.
        reference *= kScale;

        var delta = Math.abs(actual - reference);
        var deltaInDecibels = delta > 0 ? 20 * Math.log(delta)/Math.log(10) : -200;

        if (j >= filterStabilizeSkipFrames) {
            if (deltaInDecibels > worstDeltaInDecibels) {
                worstDeltaInDecibels = deltaInDecibels;
            }
        }
    }

    // console.log("worstDeltaInDecibels: " + worstDeltaInDecibels);

    var success = worstDeltaInDecibels < acceptableAliasingThresholdDecibels;

    if (success) {
        testPassed(oversample + " WaveShaperNode oversampling within acceptable tolerance.");
    } else {
        testFailed(oversample + " WaveShaperNode oversampling not within acceptable tolerance.  Error = " + worstDeltaInDecibels + " dBFS");
    }

    finishJSTest();
}

function createImpulseBuffer(context, sampleFrameLength) {
    var audioBuffer = context.createBuffer(1, sampleFrameLength, context.sampleRate);
    var n = audioBuffer.length;
    var dataL = audioBuffer.getChannelData(0);

    for (var k = 0; k < n; ++k)
        dataL[k] = 0;

    dataL[0] = 1;

    return audioBuffer;
}

window.OfflineAudioContext = window.OfflineAudioContext || window.webkitOfflineAudioContext;

function runWaveShaperOversamplingTest(testParams) {
    sampleRate = testParams.sampleRate;
    nyquist = 0.5 * sampleRate;
    oversample = testParams.oversample;
    fundamentalFrequency = testParams.fundamentalFrequency;
    acceptableAliasingThresholdDecibels = testParams.acceptableAliasingThresholdDecibels;

    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
    }

    window.jsTestIsAsync = true;

    // Create offline audio context.
    var numberOfRenderFrames = sampleRate * lengthInSeconds;
    context = new OfflineAudioContext(1, numberOfRenderFrames, sampleRate);

    // source -> waveshaper -> destination
    var source = context.createBufferSource();
    source.buffer = createToneBuffer(context, fundamentalFrequency, lengthInSeconds, 1);

    // Apply a non-linear distortion curve.
    waveshaper = context.createWaveShaper();
    waveshaper.curve = generateWaveShapingCurve();
    waveshaper.oversample = oversample;

    source.connect(waveshaper);
    waveshaper.connect(context.destination);

    source.start(0);

    context.oncomplete = checkShapedCurve;
    context.startRendering();
}
