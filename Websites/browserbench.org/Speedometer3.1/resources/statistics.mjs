/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

export function max(values) {
    return Math.max.apply(Math, values);
}

export function min(values) {
    return Math.min.apply(Math, values);
}

export function sum(values) {
    return values.reduce((a, b) => a + b, 0);
}

export function product(values) {
    return values.reduce((a, b) => a * b, 1);
}

export function squareSum(values) {
    return values.reduce((sum, value) => sum + value * value, 0);
}

// With sum and sum of squares, we can compute the sample standard deviation in O(1).
// See https://rniwa.com/2012-11-10/sample-standard-deviation-in-terms-of-sum-and-square-sum-of-samples/
export function sampleStandardDeviation(numberOfSamples, sum, squareSum) {
    if (numberOfSamples < 2)
        return 0;
    return Math.sqrt(squareSum / (numberOfSamples - 1) - (sum * sum) / (numberOfSamples - 1) / numberOfSamples);
}

export function supportedConfidenceLevels() {
    const supportedLevels = [];
    for (let quantile in tDistributionInverseCDF)
        supportedLevels.push((1 - (1 - quantile) * 2).toFixed(2));
    return supportedLevels;
}

// Computes the delta d s.t. (mean - d, mean + d) is the confidence interval with the specified confidence level in O(1).
export function confidenceIntervalDelta(confidenceLevel, numberOfSamples, sum, squareSum) {
    const probability = 1 - (1 - confidenceLevel) / 2;
    if (!(probability in tDistributionInverseCDF)) {
        const supportedIntervals = supportedConfidenceLevels().map((level) => `${level * 100}%`);
        throw `We only support ${supportedIntervals.join(", ")} confidence intervals.`;
    }
    if (numberOfSamples - 2 < 0)
        return NaN;

    const cdfForProbability = tDistributionInverseCDF[probability];
    let degreesOfFreedom = numberOfSamples - 1;
    if (degreesOfFreedom > cdfForProbability.length)
        degreesOfFreedom = cdfForProbability.length - 1;

    // tDistributionQuantile(degreesOfFreedom, confidenceLevel) * sampleStandardDeviation / sqrt(numberOfSamples) * S/sqrt(numberOfSamples)
    const quantile = cdfForProbability[degreesOfFreedom - 1]; // The first entry is for the one degree of freedom.
    return (quantile * sampleStandardDeviation(numberOfSamples, sum, squareSum)) / Math.sqrt(numberOfSamples);
}

export function confidenceInterval(values, probability) {
    const sumValue = sum(values);
    const mean = sumValue / values.length;
    const delta = confidenceIntervalDelta(probability || 0.95, values.length, sumValue, squareSum(values));
    return [mean - delta, mean + delta];
}

// See http://en.wikipedia.org/wiki/Student's_t-distribution#Table_of_selected_values
// This table contains one sided (a.k.a. tail) values.
var tDistributionInverseCDF = {
    0.9: [
        3.077684, 1.885618, 1.637744, 1.533206, 1.475884, 1.439756, 1.414924, 1.396815, 1.383029, 1.372184, 1.36343, 1.356217, 1.350171, 1.34503, 1.340606, 1.336757, 1.333379, 1.330391, 1.327728, 1.325341, 1.323188, 1.321237, 1.31946, 1.317836,
        1.316345, 1.314972, 1.313703, 1.312527, 1.311434, 1.310415, 1.309464, 1.308573, 1.307737, 1.306952, 1.306212, 1.305514, 1.304854, 1.30423, 1.303639, 1.303077, 1.302543, 1.302035, 1.301552, 1.30109, 1.300649, 1.300228, 1.299825, 1.299439,
        1.299069, 1.298714,

        1.298373, 1.298045, 1.29773, 1.297426, 1.297134, 1.296853, 1.296581, 1.296319, 1.296066, 1.295821, 1.295585, 1.295356, 1.295134, 1.29492, 1.294712, 1.294511, 1.294315, 1.294126, 1.293942, 1.293763, 1.293589, 1.293421, 1.293256, 1.293097,
        1.292941, 1.29279, 1.292643, 1.2925, 1.29236, 1.292224, 1.292091, 1.291961, 1.291835, 1.291711, 1.291591, 1.291473, 1.291358, 1.291246, 1.291136, 1.291029, 1.290924, 1.290821, 1.290721, 1.290623, 1.290527, 1.290432, 1.29034, 1.29025,
        1.290161, 1.290075,
    ],
    0.95: [
        6.313752, 2.919986, 2.353363, 2.131847, 2.015048, 1.94318, 1.894579, 1.859548, 1.833113, 1.812461, 1.795885, 1.782288, 1.770933, 1.76131, 1.75305, 1.745884, 1.739607, 1.734064, 1.729133, 1.724718, 1.720743, 1.717144, 1.713872, 1.710882,
        1.708141, 1.705618, 1.703288, 1.701131, 1.699127, 1.697261, 1.695519, 1.693889, 1.69236, 1.690924, 1.689572, 1.688298, 1.687094, 1.685954, 1.684875, 1.683851, 1.682878, 1.681952, 1.681071, 1.68023, 1.679427, 1.67866, 1.677927, 1.677224,
        1.676551, 1.675905,

        1.675285, 1.674689, 1.674116, 1.673565, 1.673034, 1.672522, 1.672029, 1.671553, 1.671093, 1.670649, 1.670219, 1.669804, 1.669402, 1.669013, 1.668636, 1.668271, 1.667916, 1.667572, 1.667239, 1.666914, 1.6666, 1.666294, 1.665996, 1.665707,
        1.665425, 1.665151, 1.664885, 1.664625, 1.664371, 1.664125, 1.663884, 1.663649, 1.66342, 1.663197, 1.662978, 1.662765, 1.662557, 1.662354, 1.662155, 1.661961, 1.661771, 1.661585, 1.661404, 1.661226, 1.661052, 1.660881, 1.660715, 1.660551,
        1.660391, 1.660234,
    ],
    0.975: [
        12.706205, 4.302653, 3.182446, 2.776445, 2.570582, 2.446912, 2.364624, 2.306004, 2.262157, 2.228139, 2.200985, 2.178813, 2.160369, 2.144787, 2.13145, 2.119905, 2.109816, 2.100922, 2.093024, 2.085963, 2.079614, 2.073873, 2.068658, 2.063899,
        2.059539, 2.055529, 2.051831, 2.048407, 2.04523, 2.042272, 2.039513, 2.036933, 2.034515, 2.032245, 2.030108, 2.028094, 2.026192, 2.024394, 2.022691, 2.021075, 2.019541, 2.018082, 2.016692, 2.015368, 2.014103, 2.012896, 2.011741, 2.010635,
        2.009575, 2.008559,

        2.007584, 2.006647, 2.005746, 2.004879, 2.004045, 2.003241, 2.002465, 2.001717, 2.000995, 2.000298, 1.999624, 1.998972, 1.998341, 1.99773, 1.997138, 1.996564, 1.996008, 1.995469, 1.994945, 1.994437, 1.993943, 1.993464, 1.992997, 1.992543,
        1.992102, 1.991673, 1.991254, 1.990847, 1.99045, 1.990063, 1.989686, 1.989319, 1.98896, 1.98861, 1.988268, 1.987934, 1.987608, 1.98729, 1.986979, 1.986675, 1.986377, 1.986086, 1.985802, 1.985523, 1.985251, 1.984984, 1.984723, 1.984467,
        1.984217, 1.983972,
    ],
    0.99: [
        31.820516, 6.964557, 4.540703, 3.746947, 3.36493, 3.142668, 2.997952, 2.896459, 2.821438, 2.763769, 2.718079, 2.680998, 2.650309, 2.624494, 2.60248, 2.583487, 2.566934, 2.55238, 2.539483, 2.527977, 2.517648, 2.508325, 2.499867, 2.492159,
        2.485107, 2.47863, 2.47266, 2.46714, 2.462021, 2.457262, 2.452824, 2.448678, 2.444794, 2.44115, 2.437723, 2.434494, 2.431447, 2.428568, 2.425841, 2.423257, 2.420803, 2.41847, 2.41625, 2.414134, 2.412116, 2.410188, 2.408345, 2.406581,
        2.404892, 2.403272,

        2.401718, 2.400225, 2.39879, 2.39741, 2.396081, 2.394801, 2.393568, 2.392377, 2.391229, 2.390119, 2.389047, 2.388011, 2.387008, 2.386037, 2.385097, 2.384186, 2.383302, 2.382446, 2.381615, 2.380807, 2.380024, 2.379262, 2.378522, 2.377802,
        2.377102, 2.37642, 2.375757, 2.375111, 2.374482, 2.373868, 2.37327, 2.372687, 2.372119, 2.371564, 2.371022, 2.370493, 2.369977, 2.369472, 2.368979, 2.368497, 2.368026, 2.367566, 2.367115, 2.366674, 2.366243, 2.365821, 2.365407, 2.365002,
        2.364606, 2.364217,
    ],
};
