// Notes about generated waveforms:
//
// QUESTION: Why does the wave shape not look like the exact shape (sharp
// edges)? ANSWER: Because a shape with sharp edges has infinitely high
// frequency content. Since a digital audio signal must be band-limited based on
// the nyquist frequency (half the sample-rate) in order to avoid aliasing, this
// creates more rounded edges and "ringing" in the appearance of the waveform.
// See Nyquist-Shannon sampling theorem:
// http://en.wikipedia.org/wiki/Nyquist%E2%80%93Shannon_sampling_theorem
//
// QUESTION: Why does the very end of the generated signal appear to get
// slightly weaker? ANSWER: This is an artifact of the algorithm to avoid
// aliasing.
//
// QUESTION: Since the tests compare the actual result with an expected
// reference file, how are the reference files created? ANSWER: Run the test in
// a browser.  When the test completes, a generated reference file with the name
// "<file>-actual.wav" is automatically downloaded.  Use this as the new
// reference, after carefully inspecting to see if this is correct.
//

OscillatorTestingUtils = (function() {

  let sampleRate = 44100.0;
  let nyquist = 0.5 * sampleRate;
  let lengthInSeconds = 4;
  let lowFrequency = 10;
  let highFrequency = nyquist + 2000;  // go slightly higher than nyquist to
                                       // make sure we generate silence there
  let context = 0;

  // Scaling factor for converting the 16-bit WAV data to float (and
  // vice-versa).
  let waveScaleFactor = 32768;

  // Thresholds for verifying the test passes.  The thresholds are
  // experimentally determined. The default values here will cause the test to
  // fail, which is useful for determining new thresholds, if needed.

  // SNR must be greater than this to pass the test.
  // Q: Why is the SNR threshold not infinity?
  // A: The reference result is a 16-bit WAV file, so it won't compare exactly
  // with the
  //    floating point result.
  let thresholdSNR = 10000;

  // Max diff must be less than this to pass the test.
  let thresholdDiff = 0;

  // Mostly for debugging

  // An AudioBuffer for the reference (expected) result.
  let reference = 0;

  // Signal power of the reference
  let signalPower = 0;

  // Noise power of the difference between the reference and actual result.
  let noisePower = 0;

  function generateExponentialOscillatorSweep(context, oscillatorType) {
    let osc = context.createOscillator();
    if (oscillatorType == 'custom') {
      // Create a simple waveform with three Fourier coefficients.
      // Note the first values are expected to be zero (DC for coeffA and
      // Nyquist for coeffB).
      let coeffA = new Float32Array([0, 1, 0.5]);
      let coeffB = new Float32Array([0, 0, 0]);
      let wave = context.createPeriodicWave(coeffA, coeffB);
      osc.setPeriodicWave(wave);
    } else {
      osc.type = oscillatorType;
    }

    // Scale by 1/2 to better visualize the waveform and to avoid clipping past
    // full scale.
    let gainNode = context.createGain();
    gainNode.gain.value = 0.5;
    osc.connect(gainNode);
    gainNode.connect(context.destination);

    osc.start(0);

    osc.frequency.setValueAtTime(10, 0);
    osc.frequency.exponentialRampToValueAtTime(highFrequency, lengthInSeconds);
  }

  function calculateSNR(sPower, nPower) {
    return 10 * Math.log10(sPower / nPower);
  }

  function loadReferenceAndRunTest(context, oscType, task, should) {
    Audit
        .loadFileFromUrl(
            '../Oscillator/oscillator-' + oscType + '-expected.wav')
        .then(response => {
          return context.decodeAudioData(response);
        })
        .then(audioBuffer => {
          reference = audioBuffer.getChannelData(0);
          generateExponentialOscillatorSweep(context, oscType);
          return context.startRendering();
        })
        .then(resultBuffer => {
          checkResult(resultBuffer, should, oscType);
        })
        .then(() => task.done());
  }

  function checkResult(renderedBuffer, should, oscType) {
    let renderedData = renderedBuffer.getChannelData(0);
    // Compute signal to noise ratio between the result and the reference. Also
    // keep track of the max difference (and position).

    let maxError = -1;
    let errorPosition = -1;
    let diffCount = 0;

    for (let k = 0; k < renderedData.length; ++k) {
      let diff = renderedData[k] - reference[k];
      noisePower += diff * diff;
      signalPower += reference[k] * reference[k];
      if (Math.abs(diff) > maxError) {
        maxError = Math.abs(diff);
        errorPosition = k;
      }
    }

    let snr = calculateSNR(signalPower, noisePower);
    should(snr, 'SNR').beGreaterThanOrEqualTo(thresholdSNR);
    should(maxError, 'Maximum difference').beLessThanOrEqualTo(thresholdDiff);

    let filename = 'oscillator-' + oscType + '-actual.wav';
    if (downloadAudioBuffer(renderedBuffer, filename, true))
      should(true, 'Saved reference file').message(filename, '');
  }

  function setThresholds(thresholds) {
    thresholdSNR = thresholds.snr;
    thresholdDiff = thresholds.maxDiff;
    thresholdDiffCount = thresholds.diffCount;
  }

  function runTest(context, oscType, description, task, should) {
    loadReferenceAndRunTest(context, oscType, task, should);
  }

  return {
    sampleRate: sampleRate,
    lengthInSeconds: lengthInSeconds,
    thresholdSNR: thresholdSNR,
    thresholdDiff: thresholdDiff,
    waveScaleFactor: waveScaleFactor,
    setThresholds: setThresholds,
    runTest: runTest,
  };

}());
