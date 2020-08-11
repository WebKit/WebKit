// Globals, to make testing and debugging easier.
var context;
var filter;
var signal;
var renderedBuffer;
var renderedData;

var sampleRate = 44100.0;
var pulseLengthFrames = .1 * sampleRate;

// Maximum allowed error for the test to succeed.  Experimentally determined. 
var maxAllowedError = 5.9e-8;

// This must be large enough so that the filtered result is
// essentially zero.  See comments for createTestAndRun.
var timeStep = .1;

// Maximum number of filters we can process (mostly for setting the
// render length correctly.)
var maxFilters = 5;

// How long to render.  Must be long enough for all of the filters we
// want to test.
var renderLengthSeconds = timeStep * (maxFilters + 1) ;

var renderLengthSamples = Math.round(renderLengthSeconds * sampleRate);

// Number of filters that will be processed.
var nFilters;

// A biquad filter has a z-transform of
// H(z) = (b0 + b1 / z + b2 / z^2) / (1 + a1 / z + a2 / z^2)
//
// The formulas for the various filters were taken from
// http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt.


// Lowpass filter.
function createLowpassFilter(freq, q, gain) {
    var b0;
    var b1;
    var b2;
    var a1;
    var a2;

    if (freq == 1) {
        // The formula below works, except for roundoff.  When freq = 1,
        // the filter is just a wire, so hardwire the coefficients.
        return {b0: 1, b1: 0, b2: 0, a1: 0, a2: 0};
    } else {
        var resonance = Math.pow(10, q / 20);
        var theta = Math.PI * freq;
        var alpha = Math.sin(theta) / (2 * resonance);
        var cosw = Math.cos(theta);
        var beta = (1 - cosw) / 2;

        b0 = beta;
        b1 = 2 * beta;
        b2 = beta;

        a0 = 1 + alpha;
        a1 = -2 * cosw;
        a2 = 1 - alpha;

        return normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2);
    }
}

function createHighpassFilter(freq, q, gain) {
    var b0;
    var b1;
    var b2;
    var a1;
    var a2;

    if (freq == 1) {
        // The filter is 0
        b0 = 0;
        b1 = 0;
        b2 = 0;
        a1 = 0;
        a2 = 0;
    } else if (freq == 0) {
        // The filter is 1.  Computation of coefficients below is ok, but
        // there's a pole at 1 and a zero at 1, so round-off could make
        // the filter unstable.
        b0 = 1;
        b1 = 0;
        b2 = 0;
        a1 = 0;
        a2 = 0;
    } else {
        var g = Math.pow(10, q / 20);
        var d = Math.sqrt((4 - Math.sqrt(16 - 16 / (g * g))) / 2);
        var theta = Math.PI * freq;
        var sn = d * Math.sin(theta) / 2;
        var beta = 0.5 * (1 - sn) / (1 + sn);
        var gamma = (0.5 + beta) * Math.cos(theta);
        var alpha = 0.25 * (0.5 + beta + gamma);

        b0 = 2 * alpha;
        b1 = -4 * alpha;
        b2 = 2 * alpha;
        a1 = 2 * (-gamma);
        a2 = 2 * beta;
    }

    return {b0 : b0, b1 : b1, b2 : b2, a1 : a1, a2 : a2};
}

function normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2) {
    var scale = 1 / a0;

    return {b0 : b0 * scale,
            b1 : b1 * scale,
            b2 : b2 * scale,
            a1 : a1 * scale,
            a2 : a2 * scale};
}

function createBandpassFilter(freq, q, gain) {
    var b0;
    var b1;
    var b2;
    var a0;
    var a1;
    var a2;
    var coef;

    if (freq > 0 && freq < 1) {
        var w0 = Math.PI * freq;
        if (q > 0) {
            var alpha = Math.sin(w0) / (2 * q);
            var k = Math.cos(w0);

            b0 = alpha;
            b1 = 0;
            b2 = -alpha;
            a0 = 1 + alpha;
            a1 = -2 * k;
            a2 = 1 - alpha;

            coef = normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2);
        } else {
            // q = 0, and frequency is not 0 or 1.  The above formula has a
            // divide by zero problem.  The limit of the z-transform as q
            // approaches 0 is 1, so set the filter that way.
            coef = {b0 : 1, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
        }
    } else {
        // When freq = 0 or 1, the z-transform is identically 0,
        // independent of q.
        coef = {b0 : 0, b1 : 0, b2 : 0, a1 : 0, a2 : 0}
    }
  
    return coef;
}

function createLowShelfFilter(freq, q, gain) {
    // q not used
    var b0;
    var b1;
    var b2;
    var a0;
    var a1;
    var a2;
    var coef;
  
    var S = 1;
    var A = Math.pow(10, gain / 40);

    if (freq == 1) {
        // The filter is just a constant gain
        coef = {b0 : A * A, b1 : 0, b2 : 0, a1 : 0, a2 : 0};  
    } else if (freq == 0) {
        // The filter is 1
        coef = {b0 : 1, b1 : 0, b2 : 0, a1 : 0, a2 : 0};  
    } else {
        var w0 = Math.PI * freq;
        var alpha = 1 / 2 * Math.sin(w0) * Math.sqrt((A + 1 / A) * (1 / S - 1) + 2);
        var k = Math.cos(w0);
        var k2 = 2 * Math.sqrt(A) * alpha;
        var Ap1 = A + 1;
        var Am1 = A - 1;

        b0 = A * (Ap1 - Am1 * k + k2);
        b1 = 2 * A * (Am1 - Ap1 * k);
        b2 = A * (Ap1 - Am1 * k - k2);
        a0 = Ap1 + Am1 * k + k2;
        a1 = -2 * (Am1 + Ap1 * k);
        a2 = Ap1 + Am1 * k - k2;
        coef = normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2);
    }

    return coef;
}

function createHighShelfFilter(freq, q, gain) {
    // q not used
    var b0;
    var b1;
    var b2;
    var a0;
    var a1;
    var a2;
    var coef;

    var A = Math.pow(10, gain / 40);

    if (freq == 1) {
        // When freq = 1, the z-transform is 1
        coef = {b0 : 1, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
    } else if (freq > 0) {
        var w0 = Math.PI * freq;
        var S = 1;
        var alpha = 0.5 * Math.sin(w0) * Math.sqrt((A + 1 / A) * (1 / S - 1) + 2);
        var k = Math.cos(w0);
        var k2 = 2 * Math.sqrt(A) * alpha;
        var Ap1 = A + 1;
        var Am1 = A - 1;

        b0 = A * (Ap1 + Am1 * k + k2);
        b1 = -2 * A * (Am1 + Ap1 * k);
        b2 = A * (Ap1 + Am1 * k - k2);
        a0 = Ap1 - Am1 * k + k2;
        a1 = 2 * (Am1 - Ap1*k);
        a2 = Ap1 - Am1 * k-k2;

        coef = normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2);
    } else {
        // When freq = 0, the filter is just a gain
        coef = {b0 : A * A, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
    }

    return coef;
}

function createPeakingFilter(freq, q, gain) {
    var b0;
    var b1;
    var b2;
    var a0;
    var a1;
    var a2;
    var coef;

    var A = Math.pow(10, gain / 40);

    if (freq > 0 && freq < 1) {
        if (q > 0) {
            var w0 = Math.PI * freq;
            var alpha = Math.sin(w0) / (2 * q);
            var k = Math.cos(w0);

            b0 = 1 + alpha * A;
            b1 = -2 * k;
            b2 = 1 - alpha * A;
            a0 = 1 + alpha / A;
            a1 = -2 * k;
            a2 = 1 - alpha / A;
  
            coef = normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2);
        } else {
            // q = 0, we have a divide by zero problem in the formulas
            // above.  But if we look at the z-transform, we see that the
            // limit as q approaches 0 is A^2.
            coef = {b0 : A * A, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
        }
    } else {
        // freq = 0 or 1, the z-transform is 1
        coef = {b0 : 1, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
    }

    return coef;
}

function createNotchFilter(freq, q, gain) {
    var b0;
    var b1;
    var b2;
    var a0;
    var a1;
    var a2;
    var coef;

    if (freq > 0 && freq < 1) {
        if (q > 0) {
            var w0 = Math.PI * freq;
            var alpha = Math.sin(w0) / (2 * q);
            var k = Math.cos(w0);

            b0 = 1;
            b1 = -2 * k;
            b2 = 1;
            a0 = 1 + alpha;
            a1 = -2 * k;
            a2 = 1 - alpha;
            coef = normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2);
        } else {
            // When q = 0, we get a divide by zero above.  The limit of the
            // z-transform as q approaches 0 is 0, so set the coefficients
            // appropriately.
            coef = {b0 : 0, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
        }
    } else {
        // When freq = 0 or 1, the z-transform is 1
        coef = {b0 : 1, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
    }

    return coef;
}

function createAllpassFilter(freq, q, gain) {
    var b0;
    var b1;
    var b2;
    var a0;
    var a1;
    var a2;
    var coef;

    if (freq > 0 && freq < 1) {
        if (q > 0) {
            var w0 = Math.PI * freq;
            var alpha = Math.sin(w0) / (2 * q);
            var k = Math.cos(w0);

            b0 = 1 - alpha;
            b1 = -2 * k;
            b2 = 1 + alpha;
            a0 = 1 + alpha;
            a1 = -2 * k;
            a2 = 1 - alpha;
            coef = normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2);
        } else {
            // q = 0
            coef = {b0 : -1, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
        }
    } else {
        coef = {b0 : 1, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
    }

    return coef;
}

// Array of functions to compute the filter coefficients.  This must
// be arraned in the same order as the filter types in the idl file.
var filterCreatorFunction = {"lowpass": createLowpassFilter,
                             "highpass": createHighpassFilter,
                             "bandpass": createBandpassFilter,
                             "lowshelf": createLowShelfFilter,
                             "highshelf": createHighShelfFilter,
                             "peaking": createPeakingFilter,
                             "notch": createNotchFilter,
                             "allpass": createAllpassFilter};

var filterTypeName = {"lowpass": "Lowpass filter",
                      "highpass": "Highpass filter",
                      "bandpass": "Bandpass filter",
                      "lowshelf": "Lowshelf filter",
                      "highshelf": "Highshelf filter",
                      "peaking": "Peaking filter",
                      "notch": "Notch filter",
                      "allpass": "Allpass filter"};

function createFilter(filterType, freq, q, gain) {
    return filterCreatorFunction[filterType](freq, q, gain);
}

function filterData(filterCoef, signal, len) {
    var y = new Array(len);
    var b0 = filterCoef.b0;
    var b1 = filterCoef.b1;
    var b2 = filterCoef.b2;
    var a1 = filterCoef.a1;
    var a2 = filterCoef.a2;

    // Prime the pump. (Assumes the signal has length >= 2!)
    y[0] = b0 * signal[0];
    y[1] = b0 * signal[1] + b1 * signal[0] - a1 * y[0];

    // Filter all of the signal that we have.
    for (var k = 2; k < Math.min(signal.length, len); ++k) {
        y[k] = b0 * signal[k] + b1 * signal[k-1] + b2 * signal[k-2] - a1 * y[k-1] - a2 * y[k-2];
    }

    // If we need to filter more, but don't have any signal left,
    // assume the signal is zero.
    for (var k = signal.length; k < len; ++k) {
        y[k] = - a1 * y[k-1] - a2 * y[k-2];
    }

    return y;
}

function createImpulseBuffer(context, length) {
    var impulse = context.createBuffer(1, length, context.sampleRate);
    var data = impulse.getChannelData(0);
    for (var k = 1; k < data.length; ++k) {
        data[k] = 0;
    }
    data[0] = 1;

    return impulse;
}


function createTestAndRun(context, filterType, filterParameters) {
    // To test the filters, we apply a signal (an impulse) to each of
    // the specified filters, with each signal starting at a different
    // time.  The output of the filters is summed together at the
    // output.  Thus for filter k, the signal input to the filter
    // starts at time k * timeStep.  For this to work well, timeStep
    // must be large enough for the output of each filter to have
    // decayed to zero with timeStep seconds.  That way the filter
    // outputs don't interfere with each other.
    
    nFilters = Math.min(filterParameters.length, maxFilters);

    signal = new Array(nFilters);
    filter = new Array(nFilters);

    impulse = createImpulseBuffer(context, pulseLengthFrames);

    // Create all of the signal sources and filters that we need.
    for (var k = 0; k < nFilters; ++k) {
        signal[k] = context.createBufferSource();
        signal[k].buffer = impulse;

        filter[k] = context.createBiquadFilter();
        filter[k].type = filterType;
        filter[k].frequency.value = context.sampleRate / 2 * filterParameters[k].cutoff;
        filter[k].detune.value = (filterParameters[k].detune === undefined) ? 0 : filterParameters[k].detune;
        filter[k].Q.value = filterParameters[k].q;
        filter[k].gain.value = filterParameters[k].gain;

        signal[k].connect(filter[k]);
        filter[k].connect(context.destination);

        signal[k].start(timeStep * k);
    }

    context.oncomplete = checkFilterResponse(filterType, filterParameters);
    context.startRendering();
}

function addSignal(dest, src, destOffset) {
    // Add src to dest at the given dest offset.
    for (var k = destOffset, j = 0; k < dest.length, j < src.length; ++k, ++j) {
        dest[k] += src[j];
    }
}

function generateReference(filterType, filterParameters) {
    var result = new Array(renderLengthSamples);
    var data = new Array(renderLengthSamples);
    // Initialize the result array and data.
    for (var k = 0; k < result.length; ++k) {
        result[k] = 0;
        data[k] = 0;
    }
    // Make data an impulse.
    data[0] = 1;
    
    for (var k = 0; k < nFilters; ++k) {
        // Filter an impulse
        var detune = (filterParameters[k].detune === undefined) ? 0 : filterParameters[k].detune;
        var frequency = filterParameters[k].cutoff * Math.pow(2, detune / 1200); // Apply detune, converting from Cents.
        
        var filterCoef = createFilter(filterType,
                                      frequency,
                                      filterParameters[k].q,
                                      filterParameters[k].gain);
        var y = filterData(filterCoef, data, renderLengthSamples);

        // Accumulate this filtered data into the final output at the desired offset.
        addSignal(result, y, timeToSampleFrame(timeStep * k, sampleRate));
    }

    return result;
}

function checkFilterResponse(filterType, filterParameters) {
    return function(event) {
        renderedBuffer = event.renderedBuffer;
        renderedData = renderedBuffer.getChannelData(0);

        reference = generateReference(filterType, filterParameters);
        
        var len = Math.min(renderedData.length, reference.length);

        var success = true;

        // Maximum error between rendered data and expected data
        var maxError = 0;

        // Sample offset where the maximum error occurred.
        var maxPosition = 0;

        // Number of infinities or NaNs that occurred in the rendered data.
        var invalidNumberCount = 0;

        if (nFilters != filterParameters.length) {
            testFailed("Test wanted " + filterParameters.length + " filters but only " + maxFilters + " allowed.");
            success = false;
        }

        // Compare the rendered signal with our reference, keeping
        // track of the maximum difference (and the offset of the max
        // difference.)  Check for bad numbers in the rendered output
        // too.  There shouldn't be any.
        for (var k = 0; k < len; ++k) {
            var err = Math.abs(renderedData[k] - reference[k]);
            if (err > maxError) {
                maxError = err;
                maxPosition = k;
            }
            if (!isValidNumber(renderedData[k])) {
                ++invalidNumberCount;
            }
        }

        if (invalidNumberCount > 0) {
            testFailed("Rendered output has " + invalidNumberCount + " infinities or NaNs.");
            success = false;
        } else {
            testPassed("Rendered output did not have infinities or NaNs.");
        }
        
        if (maxError <= maxAllowedError) {
            testPassed(filterTypeName[filterType] + " response is correct.");
        } else {
            testFailed(filterTypeName[filterType] + " response is incorrect.  Max err = " + maxError + " at " + maxPosition + ".  Threshold = " + maxAllowedError);
            success = false;
        }
        
        if (success) {
            testPassed("Test signal was correctly filtered.");
        } else {
            testFailed("Test signal was not correctly filtered.");
        }
        finishJSTest();
    }
}
