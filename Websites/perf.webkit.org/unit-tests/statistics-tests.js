"use strict";

var assert = require('assert');
var Statistics = require('../public/shared/statistics.js');
if (!assert.almostEqual)
    assert.almostEqual = require('./resources/almost-equal.js');


describe('assert.almostEqual', function () {
    it('should not throw when values are identical', function () {
        assert.doesNotThrow(function () { assert.almostEqual(1, 1); });
    });

    it('should not throw when values are close', function () {
        assert.doesNotThrow(function () { assert.almostEqual(1.10, 1.107, 2); });
        assert.doesNotThrow(function () { assert.almostEqual(1256.7, 1256.72, 4); });
    });

    it('should throw when values are not close', function () {
        assert.throws(function () { assert.almostEqual(1.10, 1.27, 2); });
        assert.throws(function () { assert.almostEqual(735.4, 735.6, 4); });
    });
});

describe('Statistics', function () {
    describe('min', function () {
        it('should find the mininum value', function () {
            assert.equal(Statistics.min([1, 2, 3, 4]), 1);
            assert.equal(Statistics.min([4, 3, 2, 1]), 1);
            assert.equal(Statistics.min([2000, 20, 200]), 20);
            assert.equal(Statistics.min([0.3, 0.06, 0.5]), 0.06);
            assert.equal(Statistics.min([-0.3, 0.06, 0.5]), -0.3);
            assert.equal(Statistics.min([-0.3, 0.06, 0.5, Infinity]), -0.3);
            assert.equal(Statistics.min([-0.3, 0.06, 0.5, -Infinity]), -Infinity);
            assert.equal(Statistics.min([]), Infinity);
        });
    });

    describe('max', function () {
        it('should find the mininum value', function () {
            assert.equal(Statistics.max([1, 2, 3, 4]), 4);
            assert.equal(Statistics.max([4, 3, 2, 1]), 4);
            assert.equal(Statistics.max([2000, 20, 200]), 2000);
            assert.equal(Statistics.max([0.3, 0.06, 0.5]), 0.5);
            assert.equal(Statistics.max([-0.3, 0.06, 0.5]), 0.5);
            assert.equal(Statistics.max([-0.3, 0.06, 0.5, Infinity]), Infinity);
            assert.equal(Statistics.max([-0.3, 0.06, 0.5, -Infinity]), 0.5);
            assert.equal(Statistics.max([]), -Infinity);
        });
    });

    describe('sum', function () {
        it('should find the sum of values', function () {
            assert.equal(Statistics.sum([1, 2, 3, 4]), 10);
            assert.equal(Statistics.sum([4, 3, 2, 1]), 10);
            assert.equal(Statistics.sum([2000, 20, 200]), 2220);
            assert.equal(Statistics.sum([0.3, 0.06, 0.5]), 0.86);
            assert.equal(Statistics.sum([-0.3, 0.06, 0.5]), 0.26);
            assert.equal(Statistics.sum([-0.3, 0.06, 0.5, Infinity]), Infinity);
            assert.equal(Statistics.sum([-0.3, 0.06, 0.5, -Infinity]), -Infinity);
            assert.equal(Statistics.sum([]), 0);
        });
    });

    describe('squareSum', function () {
        it('should find the square sum of values', function () {
            assert.equal(Statistics.squareSum([1, 2, 3, 4]), 30);
            assert.equal(Statistics.squareSum([4, 3, 2, 1]), 30);
            assert.equal(Statistics.squareSum([2000, 20, 200]), 2000 * 2000 + 20 * 20 + 200* 200);
            assert.equal(Statistics.squareSum([0.3, 0.06, 0.5]), 0.09 + 0.0036 + 0.25);
            assert.equal(Statistics.squareSum([-0.3, 0.06, 0.5]), 0.09 + 0.0036 + 0.25);
            assert.equal(Statistics.squareSum([-0.3, 0.06, 0.5, Infinity]), Infinity);
            assert.equal(Statistics.squareSum([-0.3, 0.06, 0.5, -Infinity]), Infinity);
            assert.equal(Statistics.squareSum([]), 0);
        });
    });

    describe('sampleStandardDeviation', function () {
        function stdev(values) {
            return Statistics.sampleStandardDeviation(values.length,
                Statistics.sum(values), Statistics.squareSum(values));
        }

        it('should find the standard deviation of values', function () {
            assert.almostEqual(stdev([1, 2, 3, 4]), 1.2909944);
            assert.almostEqual(stdev([4, 3, 2, 1]), 1.2909944);
            assert.almostEqual(stdev([2000, 20, 200]), 1094.89726);
            assert.almostEqual(stdev([0.3, 0.06, 0.5]), 0.220302822);
            assert.almostEqual(stdev([-0.3, 0.06, 0.5]), 0.40066611203);
            assert.almostEqual(stdev([-0.3, 0.06, 0.5, Infinity]), NaN);
            assert.almostEqual(stdev([-0.3, 0.06, 0.5, -Infinity]), NaN);
            assert.almostEqual(stdev([]), 0);
        });
    });

    describe('confidenceIntervalDelta', function () {
        it('should find the p-value of values using Student\'s t distribution', function () {
            function delta(values, probabilty) {
                return Statistics.confidenceIntervalDelta(probabilty, values.length,
                    Statistics.sum(values), Statistics.squareSum(values));
            }

            // https://onlinecourses.science.psu.edu/stat414/node/199
            var values = [118, 115, 125, 110, 112, 130, 117, 112, 115, 120, 113, 118, 119, 122, 123, 126];
            assert.almostEqual(delta(values, 0.95), 3.015, 3);

            // Following values are computed using Excel Online's STDEV and CONFIDENCE.T
            assert.almostEqual(delta([1, 2, 3, 4], 0.8), 1.057159, 4);
            assert.almostEqual(delta([1, 2, 3, 4], 0.9), 1.519090, 4);
            assert.almostEqual(delta([1, 2, 3, 4], 0.95), 2.054260, 4);

            assert.almostEqual(delta([0.3, 0.06, 0.5], 0.8), 0.2398353, 4);
            assert.almostEqual(delta([0.3, 0.06, 0.5], 0.9), 0.3713985, 4);
            assert.almostEqual(delta([0.3, 0.06, 0.5], 0.95), 0.5472625, 4);

            assert.almostEqual(delta([-0.3, 0.06, 0.5], 0.8), 0.4361900, 4);
            assert.almostEqual(delta([-0.3, 0.06, 0.5], 0.9), 0.6754647, 4);
            assert.almostEqual(delta([-0.3, 0.06, 0.5], 0.95), 0.9953098, 4);

            assert.almostEqual(delta([123, 107, 109, 104, 111], 0.8), 5.001167, 4);
            assert.almostEqual(delta([123, 107, 109, 104, 111], 0.9), 6.953874, 4);
            assert.almostEqual(delta([123, 107, 109, 104, 111], 0.95), 9.056490, 4);

            assert.almostEqual(delta([6785, 7812, 6904, 7503, 6943, 7207, 6812], 0.8), 212.6155, 4);
            assert.almostEqual(delta([6785, 7812, 6904, 7503, 6943, 7207, 6812], 0.9), 286.9585, 4);
            assert.almostEqual(delta([6785, 7812, 6904, 7503, 6943, 7207, 6812], 0.95), 361.3469, 4);

        });
    });

    // https://en.wikipedia.org/wiki/Welch%27s_t_test

    var example1 = {
        A1: [27.5, 21.0, 19.0, 23.6, 17.0, 17.9, 16.9, 20.1, 21.9, 22.6, 23.1, 19.6, 19.0, 21.7, 21.4],
        A2: [27.1, 22.0, 20.8, 23.4, 23.4, 23.5, 25.8, 22.0, 24.8, 20.2, 21.9, 22.1, 22.9, 20.5, 24.4],
        expectedT: 2.46,
        expectedDegreesOfFreedom: 25.0,
        expectedRange: [0.95, 0.98] // P = 0.021 so 1 - P = 0.979 is between 0.95 and 0.98
    };

    var example2 = {
        A1: [17.2, 20.9, 22.6, 18.1, 21.7, 21.4, 23.5, 24.2, 14.7, 21.8],
        A2: [21.5, 22.8, 21.0, 23.0, 21.6, 23.6, 22.5, 20.7, 23.4, 21.8, 20.7, 21.7, 21.5, 22.5, 23.6, 21.5, 22.5, 23.5, 21.5, 21.8],
        expectedT: 1.57,
        expectedDegreesOfFreedom: 9.9,
        expectedRange: [0.8, 0.9] // P = 0.149 so 1 - P = 0.851 is between 0.8 and 0.9
    };

    var example3 = {
        A1: [19.8, 20.4, 19.6, 17.8, 18.5, 18.9, 18.3, 18.9, 19.5, 22.0],
        A2: [28.2, 26.6, 20.1, 23.3, 25.2, 22.1, 17.7, 27.6, 20.6, 13.7, 23.2, 17.5, 20.6, 18.0, 23.9, 21.6, 24.3, 20.4, 24.0, 13.2],
        expectedT: 2.22,
        expectedDegreesOfFreedom: 24.5,
        expectedRange: [0.95, 0.98] // P = 0.036 so 1 - P = 0.964 is beteween 0.95 and 0.98
    };

    describe('computeWelchsT', function () {
        function computeWelchsT(values1, values2, probability) {
            return Statistics.computeWelchsT(values1, 0, values1.length, values2, 0, values2.length, probability);
        }

        it('should detect the statistically significant difference using Welch\'s t-test', function () {
            assert.equal(computeWelchsT(example1.A1, example1.A2, 0.8).significantlyDifferent, true);
            assert.equal(computeWelchsT(example1.A1, example1.A2, 0.9).significantlyDifferent, true);
            assert.equal(computeWelchsT(example1.A1, example1.A2, 0.95).significantlyDifferent, true);
            assert.equal(computeWelchsT(example1.A1, example1.A2, 0.98).significantlyDifferent, false);

            assert.equal(computeWelchsT(example2.A1, example2.A2, 0.8).significantlyDifferent, true);
            assert.equal(computeWelchsT(example2.A1, example2.A2, 0.9).significantlyDifferent, false);
            assert.equal(computeWelchsT(example2.A1, example2.A2, 0.95).significantlyDifferent, false);
            assert.equal(computeWelchsT(example2.A1, example2.A2, 0.98).significantlyDifferent, false);

            assert.equal(computeWelchsT(example3.A1, example3.A2, 0.8).significantlyDifferent, true);
            assert.equal(computeWelchsT(example3.A1, example3.A2, 0.9).significantlyDifferent, true);
            assert.equal(computeWelchsT(example3.A1, example3.A2, 0.95).significantlyDifferent, true);
            assert.equal(computeWelchsT(example3.A1, example3.A2, 0.98).significantlyDifferent, false);
        });

        it('should find the t-value of values using Welch\'s t-test', function () {
            assert.almostEqual(computeWelchsT(example1.A1, example1.A2).t, example1.expectedT, 2);
            assert.almostEqual(computeWelchsT(example2.A1, example2.A2).t, example2.expectedT, 2);
            assert.almostEqual(computeWelchsT(example3.A1, example3.A2).t, example3.expectedT, 2);
        });

        it('should find the degreees of freedom using Welch–Satterthwaite equation', function () {
            assert.almostEqual(computeWelchsT(example1.A1, example1.A2).degreesOfFreedom, example1.expectedDegreesOfFreedom, 2);
            assert.almostEqual(computeWelchsT(example2.A1, example2.A2).degreesOfFreedom, example2.expectedDegreesOfFreedom, 2);
            assert.almostEqual(computeWelchsT(example3.A1, example3.A2).degreesOfFreedom, example3.expectedDegreesOfFreedom, 2);
        });

        it('should respect the start and the end indices', function () {
            var A1 = example2.A1.slice();
            var A2 = example2.A2.slice();

            var expectedT = Statistics.computeWelchsT(A1, 0, A1.length, A2, 0, A2.length).t;

            A1.unshift(21);
            A1.push(15);
            A1.push(24);
            assert.almostEqual(Statistics.computeWelchsT(A1, 1, A1.length - 3, A2, 0, A2.length).t, expectedT);

            A2.unshift(24.3);
            A2.unshift(25.8);
            A2.push(23);
            A2.push(24);
            A2 = A2.reverse();
            assert.almostEqual(Statistics.computeWelchsT(A1, 1, A1.length - 3, A2, 2, A2.length - 4).t, expectedT);
        });
    });

    describe('probabilityRangeForWelchsT', function () {
        it('should find the t-value of values using Welch\'s t-test', function () {
            assert.almostEqual(Statistics.probabilityRangeForWelchsT(example1.A1, example1.A2).t, example1.expectedT, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsT(example2.A1, example2.A2).t, example2.expectedT, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsT(example3.A1, example3.A2).t, example3.expectedT, 2);
        });

        it('should find the degreees of freedom using Welch–Satterthwaite equation', function () {
            assert.almostEqual(Statistics.probabilityRangeForWelchsT(example1.A1, example1.A2).degreesOfFreedom,
                example1.expectedDegreesOfFreedom, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsT(example2.A1, example2.A2).degreesOfFreedom,
                example2.expectedDegreesOfFreedom, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsT(example3.A1, example3.A2).degreesOfFreedom,
                example3.expectedDegreesOfFreedom, 2);
        });

        it('should compute the range of probabilites using the p-value of Welch\'s t-test', function () {
            assert.almostEqual(Statistics.probabilityRangeForWelchsT(example1.A1, example1.A2).range[0], example1.expectedRange[0]);
            assert.almostEqual(Statistics.probabilityRangeForWelchsT(example1.A1, example1.A2).range[1], example1.expectedRange[1]);

            assert.almostEqual(Statistics.probabilityRangeForWelchsT(example2.A1, example2.A2).range[0], example2.expectedRange[0]);
            assert.almostEqual(Statistics.probabilityRangeForWelchsT(example2.A1, example2.A2).range[1], example2.expectedRange[1]);

            assert.almostEqual(Statistics.probabilityRangeForWelchsT(example3.A1, example3.A2).range[0], example3.expectedRange[0]);
            assert.almostEqual(Statistics.probabilityRangeForWelchsT(example3.A1, example3.A2).range[1], example3.expectedRange[1]);
        });
    });

    describe('minimumTForOneSidedProbability', () => {
        it('should not infinite loop when lookup t-value for any degrees of freedom', () => {
            for(const probability of [0.9, 0.95, 0.975, 0.99]) {
                for (let degreesOfFreedom = 1; degreesOfFreedom < 100000; degreesOfFreedom += 1)
                    Statistics.minimumTForOneSidedProbability(probability, degreesOfFreedom);
            }
        })
    });

    describe('probabilityRangeForWelchsTForMultipleSamples', () => {
        function splitSample(samples) {
            const mid = samples.length / 2;
            return splitSampleByIndices(samples, mid);
        }

        function splitSampleByIndices(samples, ...indices) {
            const sampleSize = samples.length;
            const splittedSamples = [];
            let previousIndex = 0;
            for (const index of indices) {
                if (index == previousIndex)
                    continue;
                console.assert(index > previousIndex);
                console.assert(index <= sampleSize);
                splittedSamples.push(samples.slice(previousIndex, index));
                previousIndex = index;
            }
            if (previousIndex < sampleSize)
                splittedSamples.push(samples.slice(previousIndex, sampleSize));
            return splittedSamples.map((values) => ({sum: Statistics.sum(values), squareSum: Statistics.squareSum(values), sampleSize: values.length}));
        }

        it('should find the t-value of values using Welch\'s t-test', () => {
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSample(example1.A1), splitSample(example1.A2)).t, example1.expectedT, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSample(example2.A1), splitSample(example2.A2)).t, example2.expectedT, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSample(example3.A1), splitSample(example3.A2)).t, example3.expectedT, 2);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example1.A1, 1), splitSampleByIndices(example1.A2, 1)).t, example1.expectedT, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example2.A1, 1), splitSampleByIndices(example2.A2, 1)).t, example2.expectedT, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example3.A1, 1), splitSampleByIndices(example3.A2, 1)).t, example3.expectedT, 2);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example1.A1, 0), splitSampleByIndices(example1.A2, 0)).t, example1.expectedT, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example2.A1, 0), splitSampleByIndices(example2.A2, 0)).t, example2.expectedT, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example3.A1, 0), splitSampleByIndices(example3.A2, 0)).t, example3.expectedT, 2);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example1.A1, 1, 4), splitSampleByIndices(example1.A2, 1, 4)).t, example1.expectedT, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example2.A1, 1, 4), splitSampleByIndices(example2.A2, 1, 4)).t, example2.expectedT, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example3.A1, 1, 4), splitSampleByIndices(example3.A2, 1, 4)).t, example3.expectedT, 2);
        });

        it('should find the degreees of freedom using Welch–Satterthwaite equation when split evenly', () => {
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSample(example1.A1), splitSample(example1.A2)).degreesOfFreedom,
                example1.expectedDegreesOfFreedom, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSample(example2.A1), splitSample(example2.A2)).degreesOfFreedom,
                example2.expectedDegreesOfFreedom, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSample(example3.A1), splitSample(example3.A2)).degreesOfFreedom,
                example3.expectedDegreesOfFreedom, 2);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example1.A1, 1), splitSampleByIndices(example1.A2, 1)).degreesOfFreedom,
                example1.expectedDegreesOfFreedom, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example2.A1, 1), splitSampleByIndices(example2.A2, 1)).degreesOfFreedom,
                example2.expectedDegreesOfFreedom, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example3.A1, 1), splitSampleByIndices(example3.A2, 1)).degreesOfFreedom,
                example3.expectedDegreesOfFreedom, 2);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example1.A1, 0), splitSampleByIndices(example1.A2, 0)).degreesOfFreedom,
                example1.expectedDegreesOfFreedom, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example2.A1, 0), splitSampleByIndices(example2.A2, 0)).degreesOfFreedom,
                example2.expectedDegreesOfFreedom, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example3.A1, 0), splitSampleByIndices(example3.A2, 0)).degreesOfFreedom,
                example3.expectedDegreesOfFreedom, 2);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example1.A1, 1, 4), splitSampleByIndices(example1.A2, 1, 4)).degreesOfFreedom,
                example1.expectedDegreesOfFreedom, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example2.A1, 1, 4), splitSampleByIndices(example2.A2, 1, 4)).degreesOfFreedom,
                example2.expectedDegreesOfFreedom, 2);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example3.A1, 1, 4), splitSampleByIndices(example3.A2, 1, 4)).degreesOfFreedom,
                example3.expectedDegreesOfFreedom, 2);
        });

        it('should compute the range of probabilites using the p-value of Welch\'s t-test when split evenly', function () {
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSample(example1.A1), splitSample(example1.A2)).range[0], example1.expectedRange[0]);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSample(example1.A1), splitSample(example1.A2)).range[1], example1.expectedRange[1]);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSample(example2.A1), splitSample(example2.A2)).range[0], example2.expectedRange[0]);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSample(example2.A1), splitSample(example2.A2)).range[1], example2.expectedRange[1]);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSample(example3.A1), splitSample(example3.A2)).range[0], example3.expectedRange[0]);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSample(example3.A1), splitSample(example3.A2)).range[1], example3.expectedRange[1]);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example1.A1, 1), splitSampleByIndices(example1.A2, 1)).range[0], example1.expectedRange[0]);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example1.A1, 1), splitSampleByIndices(example1.A2, 1)).range[1], example1.expectedRange[1]);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example2.A1, 1), splitSampleByIndices(example2.A2, 1)).range[0], example2.expectedRange[0]);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example2.A1, 1), splitSampleByIndices(example2.A2, 1)).range[1], example2.expectedRange[1]);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example3.A1, 1), splitSampleByIndices(example3.A2, 1)).range[0], example3.expectedRange[0]);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example3.A1, 1), splitSampleByIndices(example3.A2, 1)).range[1], example3.expectedRange[1]);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example1.A1, 0), splitSampleByIndices(example1.A2, 0)).range[0], example1.expectedRange[0]);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example1.A1, 0), splitSampleByIndices(example1.A2, 0)).range[1], example1.expectedRange[1]);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example2.A1, 0), splitSampleByIndices(example2.A2, 0)).range[0], example2.expectedRange[0]);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example2.A1, 0), splitSampleByIndices(example2.A2, 0)).range[1], example2.expectedRange[1]);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example3.A1, 0), splitSampleByIndices(example3.A2, 0)).range[0], example3.expectedRange[0]);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example3.A1, 0), splitSampleByIndices(example3.A2, 0)).range[1], example3.expectedRange[1]);


            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example1.A1, 1, 4), splitSampleByIndices(example1.A2, 1, 4)).range[0], example1.expectedRange[0]);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example1.A1, 1, 4), splitSampleByIndices(example1.A2, 1, 4)).range[1], example1.expectedRange[1]);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example2.A1, 1, 4), splitSampleByIndices(example2.A2, 1, 4)).range[0], example2.expectedRange[0]);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example2.A1, 1, 4), splitSampleByIndices(example2.A2, 1, 4)).range[1], example2.expectedRange[1]);

            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example3.A1, 1, 4), splitSampleByIndices(example3.A2, 1, 4)).range[0], example3.expectedRange[0]);
            assert.almostEqual(Statistics.probabilityRangeForWelchsTForMultipleSamples(splitSampleByIndices(example3.A1, 1, 4), splitSampleByIndices(example3.A2, 1, 4)).range[1], example3.expectedRange[1]);

        });
    });

    describe('movingAverage', function () {
        it('should return the origian values when both forward and backward window size is 0', function () {
            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 0, 0), [1, 2, 3, 4, 5]);
        });

        it('should find the moving average with a positive backward window', function () {
            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 1, 0),
                [1, (1 + 2) / 2, (2 + 3) / 2, (3 + 4) / 2, (4 + 5) / 2]);

            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 2, 0),
                [1, (1 + 2) / 2, (1 + 2 + 3) / 3, (2 + 3 + 4) / 3, (3 + 4 + 5) / 3]);

            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 3, 0),
                [1, (1 + 2) / 2, (1 + 2 + 3) / 3, (1 + 2 + 3 + 4) / 4, (2 + 3 + 4 + 5) / 4]);

            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 4, 0),
                [1, (1 + 2) / 2, (1 + 2 + 3) / 3, (1 + 2 + 3 + 4) / 4, (1 + 2 + 3 + 4 + 5) / 5]);

            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 5, 0),
                [1, (1 + 2) / 2, (1 + 2 + 3) / 3, (1 + 2 + 3 + 4) / 4, (1 + 2 + 3 + 4 + 5) / 5]);
        });

        it('should find the moving average with a positive forward window', function () {
            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 0, 1),
                [(1 + 2) / 2, (2 + 3) / 2, (3 + 4) / 2, (4 + 5) / 2, 5]);

            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 0, 2),
                [(1 + 2 + 3) / 3, (2 + 3 + 4) / 3, (3 + 4 + 5) / 3, (4 + 5) / 2, 5]);

            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 0, 3),
                [(1 + 2 + 3 + 4) / 4, (2 + 3 + 4 + 5) / 4, (3 + 4 + 5) / 3, (4 + 5) / 2, 5]);

            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 0, 4),
                [(1 + 2 + 3 + 4 + 5) / 5, (2 + 3 + 4 + 5) / 4, (3 + 4 + 5) / 3, (4 + 5) / 2, 5]);

            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 0, 5),
                [(1 + 2 + 3 + 4 + 5) / 5, (2 + 3 + 4 + 5) / 4, (3 + 4 + 5) / 3, (4 + 5) / 2, 5]);
        });

        it('should find the moving average when both backward and forward window sizes are specified', function () {
            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 1, 1),
                [(1 + 2) / 2, (1 + 2 + 3) / 3, (2 + 3 + 4) / 3, (3 + 4 + 5) / 3, (4 + 5) / 2]);

            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 1, 2),
                [(1 + 2 + 3) / 3, (1 + 2 + 3 + 4) / 4, (2 + 3 + 4 + 5) / 4, (3 + 4 + 5) / 3, (4 + 5) / 2]);

            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 2, 1),
                [(1 + 2) / 2, (1 + 2 + 3) / 3, (1 + 2 + 3 + 4) / 4, (2 + 3 + 4 + 5) / 4, (3 + 4 + 5) / 3]);

            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 2, 2),
                [(1 + 2 + 3) / 3, (1 + 2 + 3 + 4) / 4, (1 + 2 + 3 + 4 + 5) / 5, (2 + 3 + 4 + 5) / 4, (3 + 4 + 5) / 3]);

            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 2, 3),
                [(1 + 2 + 3 + 4) / 4, (1 + 2 + 3 + 4 + 5) / 5, (1 + 2 + 3 + 4 + 5) / 5, (2 + 3 + 4 + 5) / 4, (3 + 4 + 5) / 3]);

            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 3, 2),
                [(1 + 2 + 3) / 3, (1 + 2 + 3 + 4) / 4, (1 + 2 + 3 + 4 + 5) / 5, (1 + 2 + 3 + 4 + 5) / 5, (2 + 3 + 4 + 5) / 4]);

            assert.deepEqual(Statistics.movingAverage([1, 2, 3, 4, 5], 3, 3),
                [(1 + 2 + 3 + 4) / 4, (1 + 2 + 3 + 4 + 5) / 5, (1 + 2 + 3 + 4 + 5) / 5, (1 + 2 + 3 + 4 + 5) / 5, (2 + 3 + 4 + 5) / 4]);
        });
    });

    describe('cumulativeMovingAverage', function () {
        it('should find the cumulative moving average', function () {
            assert.deepEqual(Statistics.cumulativeMovingAverage([1, 2, 3, 4, 5]),
                [1, (1 + 2) / 2, (1 + 2 + 3) / 3, (1 + 2 + 3 + 4) / 4, (1 + 2 + 3 + 4 + 5) / 5]);

            assert.deepEqual(Statistics.cumulativeMovingAverage([-1, 7, 0, 8.5, 2]),
                [-1, (-1 + 7) / 2, (-1 + 7 + 0) / 3, (-1 + 7 + 0 + 8.5) / 4, (-1 + 7 + 0 + 8.5 + 2) / 5]);
        });
    });

    describe('exponentialMovingAverage', function () {
        it('should find the exponential moving average', function () {
            var averages = Statistics.exponentialMovingAverage([1, 2, 3, 4, 5], 0.2);
            assert.equal(averages[0], 1);
            assert.almostEqual(averages[1], 0.2 * 2 + 0.8 * averages[0]);
            assert.almostEqual(averages[2], 0.2 * 3 + 0.8 * averages[1]);
            assert.almostEqual(averages[3], 0.2 * 4 + 0.8 * averages[2]);
            assert.almostEqual(averages[4], 0.2 * 5 + 0.8 * averages[3]);

            averages = Statistics.exponentialMovingAverage([0.8, -0.2, 0.4, -0.3, 0.5], 0.1);
            assert.almostEqual(averages[0], 0.8);
            assert.almostEqual(averages[1], 0.1 * -0.2 + 0.9 * averages[0]);
            assert.almostEqual(averages[2], 0.1 * 0.4 + 0.9 * averages[1]);
            assert.almostEqual(averages[3], 0.1 * -0.3 + 0.9 * averages[2]);
            assert.almostEqual(averages[4], 0.1 * 0.5 + 0.9 * averages[3]);
        });
    });

    describe('segmentTimeSeriesGreedyWithStudentsTTest', function () {
        it('should segment time series', function () {
            assert.deepEqual(Statistics.segmentTimeSeriesGreedyWithStudentsTTest([1, 1, 1, 3, 3, 3], 1), [0, 2, 6]);
            assert.deepEqual(Statistics.segmentTimeSeriesGreedyWithStudentsTTest([1, 1.2, 0.9, 1.1, 1.5, 1.7, 1.8], 1), [0, 4, 7]);
        });
    });

    describe('segmentTimeSeriesByMaximizingSchwarzCriterion', function () {
        it('should segment time series of length 0 into a single segment', function () {
            var values = [];
            assert.deepEqual(Statistics.segmentTimeSeriesByMaximizingSchwarzCriterion(values), [0, 0]);
        });

        it('should not segment time series of length two into two pieces', function () {
            var values = [1, 2];
            assert.deepEqual(Statistics.segmentTimeSeriesByMaximizingSchwarzCriterion(values), [0, 2]);
        });

        it('should segment time series [1, 2, 3] into three pieces', function () {
            var values = [1, 2, 3];
            assert.deepEqual(Statistics.segmentTimeSeriesByMaximizingSchwarzCriterion(values), [0, 1, 3]);
        });

        it('should segment time series for platform=47 metric=4875 between 1453938553772 and 1454630903100 into two parts', function () {
            var values = [
                1546.5603, 1548.1536, 1563.5452, 1539.7823, 1546.4184, 1548.9299, 1532.5444, 1546.2800, 1547.1760, 1551.3507,
                1548.3277, 1544.7673, 1542.7157, 1538.1700, 1538.0948, 1543.0364, 1537.9737, 1542.2611, 1543.9685, 1546.4901,
                1544.4080, 1540.8671, 1537.3353, 1549.4331, 1541.4436, 1544.1299, 1550.1770, 1553.1872, 1549.3417, 1542.3788,
                1543.5094, 1541.7905, 1537.6625, 1547.3840, 1538.5185, 1549.6764, 1556.6138, 1552.0476, 1541.7629, 1544.7006,
                /* segments changes here */
                1587.1390, 1594.5451, 1586.2430, 1596.7310, 1548.1423];
            assert.deepEqual(Statistics.segmentTimeSeriesByMaximizingSchwarzCriterion(values), [0, 39, values.length]);
        });

        it('should segment time series for platform=51 metric=4565 betweeen 1452191332230 and 1454628206453 into two parts', function () {
            var values = [
                147243216, 147736350, 146670090, 146629723, 142749220, 148234161, 147303822, 145112097, 145852468, 147094741,
                147568897, 145160531, 148028242, 141272279, 144323236, 147492567, 146219156, 144895726, 144418925, 145455873,
                141924694, 141025833, 142082139, 144154698, 145312939, 148282554, 151852126, 149303740, 149431703, 150300257,
                148752468, 150449779, 150030118, 150553542, 151775421, 146666762, 149492535, 147143284, 150356837, 147799616,
                149889520,
                258634751, 147397840, 256106147, 261100534, 255903392, 259658019, 259501433, 257685682, 258460322, 255563633,
                259050663, 255567490, 253274911];
            assert.deepEqual(Statistics.segmentTimeSeriesByMaximizingSchwarzCriterion(values), [0, 40, values.length]);
        });

        it('should not segment time series for platform=51 metric=4817 betweeen 1453926047749 and 1454635479052 into multiple parts', function () {
            var values = [
                5761.3, 5729.4, 5733.49, 5727.4, 5726.56, 5727.48, 5716.79, 5721.23, 5682.5, 5735.71,
                5750.99, 5755.51, 5756.02, 5725.76, 5710.14, 5776.17, 5774.29, 5769.99, 5739.65, 5756.05,
                5722.87, 5726.8, 5779.23, 5772.2, 5763.1, 5807.05];
            assert.deepEqual(Statistics.segmentTimeSeriesByMaximizingSchwarzCriterion(values), [0, values.length]);
        });

        it('should not segment time series for platform=51 metric=4817 betweeen 1453926047749 and 1454635479052 into multiple parts', function () {
            var values = new Array(37);
            for (let i = 0; i < 37; i++)
                values[i] = 1;
            assert.deepEqual(Statistics.segmentTimeSeriesByMaximizingSchwarzCriterion(values), [ 0, 6, 16, 26, 37 ]);
        });
    });

    describe('findRangesForChangeDetectionsWithWelchsTTest', () => {
        it('should return an empty array if the value is empty list', () => {
            assert.deepEqual(Statistics.findRangesForChangeDetectionsWithWelchsTTest([], [], 0.975), []);
        });

        it('should return an empty array if segmentation is empty list', () => {
            assert.deepEqual(Statistics.findRangesForChangeDetectionsWithWelchsTTest([1,2,3], [], 0.975), []);
        });

        it('should return the range if computeWelchsT shows a significant change', () => {
            const values = [
                747.30337423744,
                731.47392585276,
                743.66763513161,
                738.02055323487,
                738.25426340842,
                742.38680046471,
                733.13921703284,
                739.22069966147,
                735.69295749633,
                743.01705472504,
                745.45778145306,
                731.04841157169,
                729.4372674973,
                735.4497416527,
                739.0230668644,
                730.91782989909,
                722.18725411279,
                731.96223451728,
                730.04119216192,
                730.78087646284,
                729.63155210365,
                730.17585200878,
                733.93766054706,
                740.74920717197,
                752.14718023647,
                764.49990164847,
                766.36100828473,
                756.2291883252,
                750.14522451097,
                749.57595092266,
                748.03624881866,
                769.41522176386,
                744.04660430456,
                751.17927808265,
                753.29996854062,
                757.01813756936,
                746.62413820741,
                742.64420062736,
                758.12726352772,
                778.2278439089,
                775.11818554541,
                775.11818554541];
            const segmentation = [{
                    seriesIndex: 0,
                    time: 1505176030671,
                    value: 736.5366704896555,
                    x: 370.4571789404566,
                    y: 185.52613334520248,
                },
                {
                    seriesIndex: 18,
                    time: 1515074391534,
                    value: 736.5366704896555,
                    x: 919.4183852714947,
                    y: 185.52613334520248
                },
                {
                    seriesIndex: 18,
                    time: 1515074391534,
                    value: 750.3483428383142,
                    x: 919.4183852714947,
                    y: 177.9710953409673,
                },
                {
                    seriesIndex: 41,
                    time: 1553851695869,
                    value: 750.3483428383142,
                    x: 3070.000290764446,
                    y: 177.9710953409673,
                }];
            assert.deepEqual(Statistics.findRangesForChangeDetectionsWithWelchsTTest(values, segmentation, 0.975), [
                {
                  "endIndex": 29,
                  "segmentationEndValue": 750.3483428383142,
                  "segmentationStartValue": 736.5366704896555,
                  "startIndex": 6
                }
            ]);
        })
    });
});
