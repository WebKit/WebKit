// Notes about generated waveforms:
//
// QUESTION: Why does the wave shape not look like the exact shape (sharp edges)?
// ANSWER: Because a shape with sharp edges has infinitely high frequency content.
// Since a digital audio signal must be band-limited based on the nyquist frequency (half the sample-rate)
// in order to avoid aliasing, this creates more rounded edges and "ringing" in the
// appearance of the waveform.  See Nyquist-Shannon sampling theorem:
// http://en.wikipedia.org/wiki/Nyquist%E2%80%93Shannon_sampling_theorem
//
// QUESTION: Why does the very end of the generated signal appear to get slightly weaker?
// ANSWER: This is an artifact of the algorithm to avoid aliasing.

var sampleRate = 44100.0;
var nyquist = 0.5 * sampleRate;
var lengthInSeconds = 4;
var lowFrequency = 10;
var highFrequency = nyquist + 2000; // go slightly higher than nyquist to make sure we generate silence there
var context = 0;

function generateExponentialOscillatorSweep(oscillatorType) {
    // Create offline audio context.
    context = new webkitOfflineAudioContext(1, sampleRate * lengthInSeconds, sampleRate);

    var osc = context.createOscillator();
    if (oscillatorType == "custom") {
        // Create a simple waveform with three Fourier coefficients.
        // Note the first values are expected to be zero (DC for coeffA and Nyquist for coeffB).
        var coeffA = new Float32Array([0, 1, 0.5]);
        var coeffB = new Float32Array([0, 0, 0]);        
        var wave = context.createPeriodicWave(coeffA, coeffB);
        osc.setPeriodicWave(wave);
    } else {
        osc.type = oscillatorType;
    }

    // Scale by 1/2 to better visualize the waveform and to avoid clipping past full scale.
    var gainNode = context.createGain();
    gainNode.gain.value = 0.5;
    osc.connect(gainNode);
    gainNode.connect(context.destination);

    osc.start(0);

    var nyquist = 0.5 * sampleRate;
    osc.frequency.setValueAtTime(10, 0);
    osc.frequency.exponentialRampToValueAtTime(highFrequency, lengthInSeconds);

    context.oncomplete = finishAudioTest;
    context.startRendering();    

    testRunner.waitUntilDone();
}
