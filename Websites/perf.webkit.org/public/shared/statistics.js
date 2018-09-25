var Statistics = new (function () {

    this.min = function (values) {
        return Math.min.apply(Math, values);
    }

    this.max = function (values) {
        return Math.max.apply(Math, values);
    }

    this.sum = function (values) {
        return values.length ? values.reduce(function (a, b) { return a + b; }) : 0;
    }

    this.mean = function (values) {
        return this.sum(values) / values.length;
    }

    this.median = function (values) {
        return values.sort(function (a, b) { return a - b; })[Math.floor(values.length / 2)];
    }

    this.squareSum = function (values) {
        return values.length ? values.reduce(function (sum, value) { return sum + value * value;}, 0) : 0;
    }

    // With sum and sum of squares, we can compute the sample standard deviation in O(1).
    // See https://rniwa.com/2012-11-10/sample-standard-deviation-in-terms-of-sum-and-square-sum-of-samples/
    this.sampleStandardDeviation = function (numberOfSamples, sum, squareSum) {
        if (numberOfSamples < 2)
            return 0;
        return Math.sqrt(squareSum / (numberOfSamples - 1) - sum * sum / (numberOfSamples - 1) / numberOfSamples);
    }

    this.supportedConfidenceIntervalProbabilities = function () {
        const supportedProbabilities = [];
        for (const probability in tDistributionByOneSidedProbability)
            supportedProbabilities.push(oneSidedToTwoSidedProbability(probability).toFixed(2));
        return supportedProbabilities;
    }

    this.supportedOneSideTTestProbabilities = function () {
        return Object.keys(tDistributionByOneSidedProbability);
    }

    // Computes the delta d s.t. (mean - d, mean + d) is the confidence interval with the specified probability in O(1).
    this.confidenceIntervalDelta = function (probability, numberOfSamples, sum, squareSum) {
        var oneSidedProbability = twoSidedToOneSidedProbability(probability);
        if (!(oneSidedProbability in tDistributionByOneSidedProbability)) {
            throw 'We only support ' + this.supportedConfidenceIntervalProbabilities().map(function (probability)
            { return probability * 100 + '%'; } ).join(', ') + ' confidence intervals.';
        }
        if (numberOfSamples - 2 < 0)
            return NaN;
        // d = c * S/sqrt(numberOfSamples) where c ~ t-distribution(degreesOfFreedom) and S is the sample standard deviation.
        return this.minimumTForOneSidedProbability(oneSidedProbability, numberOfSamples - 1) * this.sampleStandardDeviation(numberOfSamples, sum, squareSum) / Math.sqrt(numberOfSamples);
    }

    this.confidenceInterval = function (values, probability) {
        var sum = this.sum(values);
        var mean = sum / values.length;
        var delta = this.confidenceIntervalDelta(probability || 0.95, values.length, sum, this.squareSum(values));
        return [mean - delta, mean + delta];
    }

    // Welch's t-test (http://en.wikipedia.org/wiki/Welch%27s_t_test)
    this.testWelchsT = function (values1, values2, probability) {
        return this.computeWelchsT(values1, 0, values1.length, values2, 0, values2.length, probability).significantlyDifferent;
    }

    function sampleMeanAndVarianceFromMultipleSamples(samples) {
        let sum = 0;
        let squareSum = 0;
        let size = 0;
        for (const sample of samples) {
            sum += sample.sum;
            squareSum += sample.squareSum;
            size += sample.sampleSize;
        }
        const mean = sum / size;
        const unbiasedSampleVariance = (squareSum - sum * sum / size) / (size - 1);
        return {
            mean,
            variance: unbiasedSampleVariance,
            size,
            degreesOfFreedom: size - 1,
        }
    }

    this.probabilityRangeForWelchsT = function (values1, values2) {
        var result = this.computeWelchsT(values1, 0, values1.length, values2, 0, values2.length);
        return this._determinetwoSidedProbabilityBoundaryForWelchsT(result.t, result.degreesOfFreedom);
    }

    this.probabilityRangeForWelchsTForMultipleSamples = function (sampleSet1, sampleSet2) {
        const stat1 = sampleMeanAndVarianceFromMultipleSamples(sampleSet1);
        const stat2 = sampleMeanAndVarianceFromMultipleSamples(sampleSet2);
        const combinedT = this._computeWelchsTFromStatistics(stat1, stat2);
        return this._determinetwoSidedProbabilityBoundaryForWelchsT(combinedT.t, combinedT.degreesOfFreedom)
    }

    this._determinetwoSidedProbabilityBoundaryForWelchsT = function(t, degreesOfFreedom) {
        if (isNaN(t) || isNaN(degreesOfFreedom))
            return {t: NaN, degreesOfFreedom:NaN, range: [null, null]};

        let lowerBound = null;
        let upperBound = null;
        for (const probability in tDistributionByOneSidedProbability) {
            const twoSidedProbability = oneSidedToTwoSidedProbability(probability);
            if (t > this.minimumTForOneSidedProbability(probability, Math.round(degreesOfFreedom)))
                lowerBound = twoSidedProbability;
            else if (lowerBound) {
                upperBound = twoSidedProbability;
                break;
            }
        }
        return {t, degreesOfFreedom, range: [lowerBound, upperBound]};
    };

    this.computeWelchsT = function (values1, startIndex1, length1, values2, startIndex2, length2, probability) {
        const stat1 = sampleMeanAndVarianceForValues(values1, startIndex1, length1);
        const stat2 = sampleMeanAndVarianceForValues(values2, startIndex2, length2);
        const {t, degreesOfFreedom} = this._computeWelchsTFromStatistics(stat1, stat2);
        const minT = this.minimumTForOneSidedProbability(twoSidedToOneSidedProbability(probability || 0.8), Math.round(degreesOfFreedom));
        return {t, degreesOfFreedom, significantlyDifferent: t > minT};
    };

    this._computeWelchsTFromStatistics = function(stat1, stat2) {
        const sumOfSampleVarianceOverSampleSize = stat1.variance / stat1.size + stat2.variance / stat2.size;
        const t = Math.abs((stat1.mean - stat2.mean) / Math.sqrt(sumOfSampleVarianceOverSampleSize));

        // http://en.wikipedia.org/wiki/Welch–Satterthwaite_equation
        const degreesOfFreedom = sumOfSampleVarianceOverSampleSize * sumOfSampleVarianceOverSampleSize
            / (stat1.variance * stat1.variance / stat1.size / stat1.size / stat1.degreesOfFreedom
                + stat2.variance * stat2.variance / stat2.size / stat2.size / stat2.degreesOfFreedom);
        return {t, degreesOfFreedom};
    }

    this.findRangesForChangeDetectionsWithWelchsTTest = function (values, segmentations, oneSidedPossibility=0.99) {
        if (!values.length)
            return [];

        const selectedRanges = [];
        const twoSidedFromOneSidedPossibility = 2 * oneSidedPossibility - 1;

        for (let i = 1; i + 2 < segmentations.length; i += 2) {
            let found = false;
            const previousMean = segmentations[i].value;
            const currentMean = segmentations[i + 1].value;
            console.assert(currentMean != previousMean);
            const currentChangePoint = segmentations[i].seriesIndex;
            const start = segmentations[i - 1].seriesIndex;
            const end = segmentations[i + 2].seriesIndex;

            for (let leftEdge = currentChangePoint - 2, rightEdge = currentChangePoint + 2; leftEdge >= start && rightEdge <= end; leftEdge--, rightEdge++) {
                const result = this.computeWelchsT(values, leftEdge, currentChangePoint - leftEdge, values, currentChangePoint, rightEdge - currentChangePoint, twoSidedFromOneSidedPossibility);
                if (result.significantlyDifferent) {
                    selectedRanges.push({
                        startIndex: leftEdge,
                        endIndex: rightEdge - 1,
                        segmentationStartValue: previousMean,
                        segmentationEndValue: currentMean
                    });
                    found = true;
                    break;
                }
            }
            if (!found && Statistics.debuggingTestingRangeNomination)
                console.log('Failed to find a testing range at', currentChangePoint, 'changing from', previousMean, 'to', currentMean);
        }

        return selectedRanges;
    };

    function sampleMeanAndVarianceForValues(values, startIndex, length) {
        var sum = 0;
        for (var i = 0; i < length; i++)
            sum += values[startIndex + i];
        var squareSum = 0;
        for (var i = 0; i < length; i++)
            squareSum += values[startIndex + i] * values[startIndex + i];
        var sampleMean = sum / length;
        // FIXME: Maybe we should be using the biased sample variance.
        var unbiasedSampleVariance = (squareSum - sum * sum / length) / (length - 1);
        return {
            mean: sampleMean,
            variance: unbiasedSampleVariance,
            size: length,
            degreesOfFreedom: length - 1,
        }
    }

    this.movingAverage = function (values, backwardWindowSize, forwardWindowSize) {
        var averages = new Array(values.length);
        // We use naive O(n^2) algorithm for simplicy as well as to avoid accumulating round-off errors.
        for (var i = 0; i < values.length; i++) {
            var sum = 0;
            var count = 0;
            for (var j = i - backwardWindowSize; j <= i + forwardWindowSize; j++) {
                if (j >= 0 && j < values.length) {
                    sum += values[j];
                    count++;
                }
            }
            averages[i] = sum / count;
        }
        return averages;
    }

    this.cumulativeMovingAverage = function (values) {
        var averages = new Array(values.length);
        var sum = 0;
        for (var i = 0; i < values.length; i++) {
            sum += values[i];
            averages[i] = sum / (i + 1);
        }
        return averages;
    }

    this.exponentialMovingAverage = function (values, smoothingFactor) {
        var averages = new Array(values.length);
        var movingAverage = values[0];
        averages[0] = movingAverage;
        for (var i = 1; i < values.length; i++) {
            movingAverage = smoothingFactor * values[i] + (1 - smoothingFactor) * movingAverage;
            averages[i] = movingAverage;
        }
        return averages;
    }

    // The return value is the starting indices of each segment.
    this.segmentTimeSeriesGreedyWithStudentsTTest = function (values, minLength) {
        if (values.length < 2)
            return [0];
        var segments = new Array;
        recursivelySplitIntoTwoSegmentsAtMaxTIfSignificantlyDifferent(values, 0, values.length, minLength, segments);
        segments.push(values.length);
        return segments;
    }

    this.debuggingSegmentation = false;
    this.segmentTimeSeriesByMaximizingSchwarzCriterion = function (values, segmentCountWeight, gridSize) {
        // Split the time series into grids since splitIntoSegmentsUntilGoodEnough is O(n^2).
        var gridLength = gridSize || 500;
        var totalSegmentation = [0];
        for (var gridCount = 0; gridCount < Math.ceil(values.length / gridLength); gridCount++) {
            var gridValues = values.slice(gridCount * gridLength, (gridCount + 1) * gridLength);
            var segmentation = splitIntoSegmentsUntilGoodEnough(gridValues, segmentCountWeight);

            if (Statistics.debuggingSegmentation)
                console.log('grid=' + gridCount, segmentation);

            for (var i = 1; i < segmentation.length - 1; i++)
                totalSegmentation.push(gridCount * gridLength + segmentation[i]);
        }

        if (Statistics.debuggingSegmentation)
            console.log('Final Segmentation', totalSegmentation);

        totalSegmentation.push(values.length);

        return totalSegmentation;
    }

    function recursivelySplitIntoTwoSegmentsAtMaxTIfSignificantlyDifferent(values, startIndex, length, minLength, segments) {
        var tMax = 0;
        var argTMax = null;
        for (var i = 1; i < length - 1; i++) {
            var firstLength = i;
            var secondLength = length - i;
            if (firstLength < minLength || secondLength < minLength)
                continue;
            var result = Statistics.computeWelchsT(values, startIndex, firstLength, values, startIndex + i, secondLength, 0.9);
            if (result.significantlyDifferent && result.t > tMax) {
                tMax = result.t;
                argTMax = i;
            }
        }
        if (!tMax) {
            segments.push(startIndex);
            return;
        }
        recursivelySplitIntoTwoSegmentsAtMaxTIfSignificantlyDifferent(values, startIndex, argTMax, minLength, segments);
        recursivelySplitIntoTwoSegmentsAtMaxTIfSignificantlyDifferent(values, startIndex + argTMax, length - argTMax, minLength, segments);
    }

    this.minimumTForOneSidedProbability = function(probability, degreesOfFreedom) {
        if (degreesOfFreedom < 1 || isNaN(degreesOfFreedom))
            return NaN;
        const tDistributionTableForProbability = tDistributionByOneSidedProbability[probability];
        if (degreesOfFreedom <= tDistributionTableForProbability.probabilityToTValue.length)
            return tDistributionTableForProbability.probabilityToTValue[degreesOfFreedom - 1];
        const tValuesSortedByProbability = tDistributionTableForProbability.tValuesSortedByProbability;

        let low = 0;
        let high = tValuesSortedByProbability.length;
        while (low < high) {
            const mid = low + Math.floor((high - low) / 2);
            const entry = tValuesSortedByProbability[mid];
            if (degreesOfFreedom <= entry.maxDF)
                high = mid;
            else
                low = mid + 1;
        }

        return tValuesSortedByProbability[low].value;
    }

    const tDistributionByOneSidedProbability = {
        0.9: {
            probabilityToTValue: [
                3.0777,  1.8856,  1.6377,  1.5332,  1.4759,  1.4398,  1.4149,  1.3968,  1.383,   1.3722,
                1.3634,  1.3562,  1.3502,  1.345,   1.3406,  1.3368,  1.3334,  1.3304,  1.3277,  1.3253,
                1.3232,  1.3212,  1.3195,  1.3178,  1.3163,  1.315,   1.3137,  1.3125,  1.3114,  1.3104,
                1.3095,  1.3086,  1.3077,  1.307,   1.3062,  1.3055,  1.3049,  1.3042,  1.3036,  1.3031,
                1.3025,  1.302,   1.3016,  1.3011,  1.3006,  1.3002,  1.2998,  1.2994,  1.2991,  1.2987,
                1.2984,  1.298,   1.2977,  1.2974,  1.2971,  1.2969,  1.2966,  1.2963,  1.2961,  1.2958,
                1.2956,  1.2954,  1.2951,  1.2949,  1.2947,  1.2945,  1.2943,  1.2941,  1.2939,  1.2938,
                1.2936,  1.2934,  1.2933,  1.2931,  1.2929,  1.2928,  1.2926,  1.2925,  1.2924,  1.2922,
                1.2921,  1.292,   1.2918,  1.2917,  1.2916,  1.2915,  1.2914,  1.2912,  1.2911,  1.291,
                1.2909,  1.2908,  1.2907,  1.2906,  1.2905,  1.2904,  1.2903,  1.2903,  1.2902,  1.2901
            ],
            tValuesSortedByProbability: [
                {maxDF: 101, value: 1.2900},  {maxDF: 102, value: 1.2899},  {maxDF: 103, value: 1.2898},  {maxDF: 105, value: 1.2897},
                {maxDF: 106, value: 1.2896},  {maxDF: 107, value: 1.2895},  {maxDF: 109, value: 1.2894},  {maxDF: 110, value: 1.2893},
                {maxDF: 112, value: 1.2892},  {maxDF: 113, value: 1.2891},  {maxDF: 115, value: 1.2890},  {maxDF: 116, value: 1.2889},
                {maxDF: 118, value: 1.2888},  {maxDF: 119, value: 1.2887},  {maxDF: 121, value: 1.2886},  {maxDF: 123, value: 1.2885},
                {maxDF: 125, value: 1.2884},  {maxDF: 127, value: 1.2883},  {maxDF: 128, value: 1.2882},  {maxDF: 130, value: 1.2881},
                {maxDF: 132, value: 1.2880},  {maxDF: 135, value: 1.2879},  {maxDF: 137, value: 1.2878},  {maxDF: 139, value: 1.2877},
                {maxDF: 141, value: 1.2876},  {maxDF: 144, value: 1.2875},  {maxDF: 146, value: 1.2874},  {maxDF: 149, value: 1.2873},
                {maxDF: 151, value: 1.2872},  {maxDF: 154, value: 1.2871},  {maxDF: 157, value: 1.2870},  {maxDF: 160, value: 1.2869},
                {maxDF: 163, value: 1.2868},  {maxDF: 166, value: 1.2867},  {maxDF: 170, value: 1.2866},  {maxDF: 173, value: 1.2865},
                {maxDF: 177, value: 1.2864},  {maxDF: 180, value: 1.2863},  {maxDF: 184, value: 1.2862},  {maxDF: 188, value: 1.2861},
                {maxDF: 193, value: 1.2860},  {maxDF: 197, value: 1.2859},  {maxDF: 202, value: 1.2858},  {maxDF: 207, value: 1.2857},
                {maxDF: 212, value: 1.2856},  {maxDF: 217, value: 1.2855},  {maxDF: 223, value: 1.2854},  {maxDF: 229, value: 1.2853},
                {maxDF: 235, value: 1.2852},  {maxDF: 242, value: 1.2851},  {maxDF: 249, value: 1.2850},  {maxDF: 257, value: 1.2849},
                {maxDF: 265, value: 1.2848},  {maxDF: 273, value: 1.2847},  {maxDF: 283, value: 1.2846},  {maxDF: 292, value: 1.2845},
                {maxDF: 303, value: 1.2844},  {maxDF: 314, value: 1.2843},  {maxDF: 326, value: 1.2842},  {maxDF: 339, value: 1.2841},
                {maxDF: 353, value: 1.2840},  {maxDF: 369, value: 1.2839},  {maxDF: 385, value: 1.2838},  {maxDF: 404, value: 1.2837},
                {maxDF: 424, value: 1.2836},  {maxDF: 446, value: 1.2835},  {maxDF: 471, value: 1.2834},  {maxDF: 499, value: 1.2833},
                {maxDF: 530, value: 1.2832},  {maxDF: 565, value: 1.2831},  {maxDF: 606, value: 1.2830},  {maxDF: 652, value: 1.2829},
                {maxDF: 707, value: 1.2828},  {maxDF: 771, value: 1.2827},  {maxDF: 848, value: 1.2826},  {maxDF: 942, value: 1.2825},
                {maxDF: 1060, value: 1.2824}, {maxDF: 1212, value: 1.2823}, {maxDF: 1415, value: 1.2822}, {maxDF: 1699, value: 1.2821},
                {maxDF: 2125, value: 1.2820}, {maxDF: 2837, value: 1.2819}, {maxDF: 4266, value: 1.2818}, {maxDF: 8601, value: 1.2817},
                {maxDF: Infinity, value: 1.2816}
            ]
        },
        0.95: {
            probabilityToTValue: [
                6.3138,  2.92,    2.3534,  2.1318,  2.015,   1.9432,  1.8946,  1.8595,  1.8331,  1.8125,
                1.7959,  1.7823,  1.7709,  1.7613,  1.7531,  1.7459,  1.7396,  1.7341,  1.7291,  1.7247,
                1.7207,  1.7171,  1.7139,  1.7109,  1.7081,  1.7056,  1.7033,  1.7011,  1.6991,  1.6973,
                1.6955,  1.6939,  1.6924,  1.6909,  1.6896,  1.6883,  1.6871,  1.686,   1.6849,  1.6839,
                1.6829,  1.682,   1.6811,  1.6802,  1.6794,  1.6787,  1.6779,  1.6772,  1.6766,  1.6759,
                1.6753,  1.6747,  1.6741,  1.6736,  1.673,   1.6725,  1.672,   1.6716,  1.6711,  1.6706,
                1.6702,  1.6698,  1.6694,  1.669,   1.6686,  1.6683,  1.6679,  1.6676,  1.6672,  1.6669,
                1.6666,  1.6663,  1.666,   1.6657,  1.6654,  1.6652,  1.6649,  1.6646,  1.6644,  1.6641,
                1.6639,  1.6636,  1.6634,  1.6632,  1.663,   1.6628,  1.6626,  1.6624,  1.6622,  1.662,
                1.6618,  1.6616,  1.6614,  1.6612,  1.6611,  1.6609,  1.6607,  1.6606,  1.6604,  1.6602,
                1.6601,  1.6599,  1.6598,  1.6596,  1.6595,  1.6594,  1.6592,  1.6591,  1.659,   1.6588,
                1.6587,  1.6586,  1.6585,  1.6583,  1.6582,  1.6581,  1.658,   1.6579,  1.6578,  1.6577
            ],
            tValuesSortedByProbability: [
                {maxDF: 121, value: 1.6575},  {maxDF: 122, value: 1.6574},   {maxDF: 123, value: 1.6573},  {maxDF: 124, value: 1.6572},
                {maxDF: 125, value: 1.6571},  {maxDF: 126, value: 1.6570},   {maxDF: 127, value: 1.6569},  {maxDF: 129, value: 1.6568},
                {maxDF: 130, value: 1.6567},  {maxDF: 131, value: 1.6566},   {maxDF: 132, value: 1.6565},  {maxDF: 133, value: 1.6564},
                {maxDF: 134, value: 1.6563},  {maxDF: 135, value: 1.6562},   {maxDF: 137, value: 1.6561},  {maxDF: 138, value: 1.6560},
                {maxDF: 139, value: 1.6559},  {maxDF: 140, value: 1.6558},   {maxDF: 142, value: 1.6557},  {maxDF: 143, value: 1.6556},
                {maxDF: 144, value: 1.6555},  {maxDF: 146, value: 1.6554},   {maxDF: 147, value: 1.6553},  {maxDF: 148, value: 1.6552},
                {maxDF: 150, value: 1.6551},  {maxDF: 151, value: 1.6550},   {maxDF: 153, value: 1.6549},  {maxDF: 154, value: 1.6548},
                {maxDF: 156, value: 1.6547},  {maxDF: 158, value: 1.6546},   {maxDF: 159, value: 1.6545},  {maxDF: 161, value: 1.6544},
                {maxDF: 163, value: 1.6543},  {maxDF: 164, value: 1.6542},   {maxDF: 166, value: 1.6541},  {maxDF: 168, value: 1.6540},
                {maxDF: 170, value: 1.6539},  {maxDF: 172, value: 1.6538},   {maxDF: 174, value: 1.6537},  {maxDF: 176, value: 1.6536},
                {maxDF: 178, value: 1.6535},  {maxDF: 180, value: 1.6534},   {maxDF: 182, value: 1.6533},  {maxDF: 184, value: 1.6532},
                {maxDF: 186, value: 1.6531},  {maxDF: 189, value: 1.6530},   {maxDF: 191, value: 1.6529},  {maxDF: 193, value: 1.6528},
                {maxDF: 196, value: 1.6527},  {maxDF: 198, value: 1.6526},   {maxDF: 201, value: 1.6525},  {maxDF: 204, value: 1.6524},
                {maxDF: 206, value: 1.6523},  {maxDF: 209, value: 1.6522},   {maxDF: 212, value: 1.6521},  {maxDF: 215, value: 1.6520},
                {maxDF: 218, value: 1.6519},  {maxDF: 221, value: 1.6518},   {maxDF: 225, value: 1.6517},  {maxDF: 228, value: 1.6516},
                {maxDF: 231, value: 1.6515},  {maxDF: 235, value: 1.6514},   {maxDF: 239, value: 1.6513},  {maxDF: 242, value: 1.6512},
                {maxDF: 246, value: 1.6511},  {maxDF: 250, value: 1.6510},   {maxDF: 255, value: 1.6509},  {maxDF: 259, value: 1.6508},
                {maxDF: 263, value: 1.6507},  {maxDF: 268, value: 1.6506},   {maxDF: 273, value: 1.6505},  {maxDF: 278, value: 1.6504},
                {maxDF: 283, value: 1.6503},  {maxDF: 288, value: 1.6502},   {maxDF: 294, value: 1.6501},  {maxDF: 299, value: 1.6500},
                {maxDF: 305, value: 1.6499},  {maxDF: 312, value: 1.6498},   {maxDF: 318, value: 1.6497},  {maxDF: 325, value: 1.6496},
                {maxDF: 332, value: 1.6495},  {maxDF: 339, value: 1.6494},   {maxDF: 347, value: 1.6493},  {maxDF: 355, value: 1.6492},
                {maxDF: 364, value: 1.6491},  {maxDF: 372, value: 1.6490},   {maxDF: 382, value: 1.6489},  {maxDF: 392, value: 1.6488},
                {maxDF: 402, value: 1.6487},  {maxDF: 413, value: 1.6486},   {maxDF: 424, value: 1.6485},  {maxDF: 436, value: 1.6484},
                {maxDF: 449, value: 1.6483},  {maxDF: 463, value: 1.6482},   {maxDF: 477, value: 1.6481},  {maxDF: 493, value: 1.6480},
                {maxDF: 509, value: 1.6479},  {maxDF: 527, value: 1.6478},   {maxDF: 545, value: 1.6477},  {maxDF: 566, value: 1.6476},
                {maxDF: 587, value: 1.6475},  {maxDF: 611, value: 1.6474},   {maxDF: 636, value: 1.6473},  {maxDF: 664, value: 1.6472},
                {maxDF: 694, value: 1.6471},  {maxDF: 727, value: 1.6470},   {maxDF: 764, value: 1.6469},  {maxDF: 804, value: 1.6468},
                {maxDF: 849, value: 1.6467},  {maxDF: 899, value: 1.6466},   {maxDF: 955, value: 1.6465},  {maxDF: 1019, value: 1.6464},
                {maxDF: 1092, value: 1.6463}, {maxDF: 1176, value: 1.6462},  {maxDF: 1274, value: 1.6461}, {maxDF: 1390, value: 1.6460},
                {maxDF: 1530, value: 1.6459}, {maxDF: 1700, value: 1.6458},  {maxDF: 1914, value: 1.6457}, {maxDF: 2189, value: 1.6456},
                {maxDF: 2555, value: 1.6455}, {maxDF: 3070, value: 1.6454},  {maxDF: 3845, value: 1.6453}, {maxDF: 5142, value: 1.6452},
                {maxDF: 7760, value: 1.6451}, {maxDF: 15812, value: 1.6450}, {maxDF: Infinity, value: 1.6449}
            ]
        },
        0.975: {
            probabilityToTValue: [
                12.7062,  4.3027,  3.1824,  2.7764,  2.5706,  2.4469,  2.3646,  2.306,   2.2622,  2.2281,
                2.201,    2.1788,  2.1604,  2.1448,  2.1314,  2.1199,  2.1098,  2.1009,  2.093,   2.086,
                2.0796,   2.0739,  2.0687,  2.0639,  2.0595,  2.0555,  2.0518,  2.0484,  2.0452,  2.0423,
                2.0395,   2.0369,  2.0345,  2.0322,  2.0301,  2.0281,  2.0262,  2.0244,  2.0227,  2.0211,
                2.0195,   2.0181,  2.0167,  2.0154,  2.0141,  2.0129,  2.0117,  2.0106,  2.0096,  2.0086,
                2.0076,   2.0066,  2.0057,  2.0049,  2.004,   2.0032,  2.0025,  2.0017,  2.001,   2.0003,
                1.9996,   1.999,   1.9983,  1.9977,  1.9971,  1.9966,  1.996,   1.9955,  1.9949,  1.9944,
                1.9939,   1.9935,  1.993,   1.9925,  1.9921,  1.9917,  1.9913,  1.9908,  1.9905,  1.9901,
                1.9897,   1.9893,  1.989,   1.9886,  1.9883,  1.9879,  1.9876,  1.9873,  1.987,   1.9867,
                1.9864,   1.9861,  1.9858,  1.9855,  1.9853,  1.985,   1.9847,  1.9845,  1.9842,  1.984,
                1.9837,   1.9835,  1.9833,  1.983,   1.9828,  1.9826,  1.9824,  1.9822,  1.982,   1.9818,
                1.9816,   1.9814,  1.9812,  1.981,   1.9808,  1.9806,  1.9804,  1.9803,  1.9801,  1.9799,
                1.9798,   1.9796,  1.9794,  1.9793,  1.9791,  1.979,   1.9788,  1.9787,  1.9785,  1.9784,
                1.9782,   1.9781,  1.978,   1.9778,  1.9777,  1.9776,  1.9774,  1.9773,  1.9772,  1.9771,
                1.9769,   1.9768,  1.9767,  1.9766,  1.9765,  1.9763,  1.9762,  1.9761,  1.976,   1.9759,
                1.9758,   1.9757,  1.9756,  1.9755,  1.9754,  1.9753,  1.9752,  1.9751,  1.975,   1.9749,
                1.9748,   1.9747,  1.9746,  1.9745
            ],
            tValuesSortedByProbability: [
                {maxDF: 166, value: 1.9744},  {maxDF: 167, value: 1.9743},  {maxDF: 168, value: 1.9742},   {maxDF: 169, value: 1.9741},
                {maxDF: 170, value: 1.9740},  {maxDF: 172, value: 1.9739},  {maxDF: 173, value: 1.9738},   {maxDF: 174, value: 1.9737},
                {maxDF: 175, value: 1.9736},  {maxDF: 177, value: 1.9735},  {maxDF: 178, value: 1.9734},   {maxDF: 179, value: 1.9733},
                {maxDF: 181, value: 1.9732},  {maxDF: 182, value: 1.9731},  {maxDF: 183, value: 1.9730},   {maxDF: 185, value: 1.9729},
                {maxDF: 186, value: 1.9728},  {maxDF: 188, value: 1.9727},  {maxDF: 189, value: 1.9726},   {maxDF: 191, value: 1.9725},
                {maxDF: 192, value: 1.9724},  {maxDF: 194, value: 1.9723},  {maxDF: 195, value: 1.9722},   {maxDF: 197, value: 1.9721},
                {maxDF: 199, value: 1.9720},  {maxDF: 200, value: 1.9719},  {maxDF: 202, value: 1.9718},   {maxDF: 204, value: 1.9717},
                {maxDF: 205, value: 1.9716},  {maxDF: 207, value: 1.9715},  {maxDF: 209, value: 1.9714},   {maxDF: 211, value: 1.9713},
                {maxDF: 213, value: 1.9712},  {maxDF: 215, value: 1.9711},  {maxDF: 217, value: 1.9710},   {maxDF: 219, value: 1.9709},
                {maxDF: 221, value: 1.9708},  {maxDF: 223, value: 1.9707},  {maxDF: 225, value: 1.9706},   {maxDF: 227, value: 1.9705},
                {maxDF: 229, value: 1.9704},  {maxDF: 231, value: 1.9703},  {maxDF: 234, value: 1.9702},   {maxDF: 236, value: 1.9701},
                {maxDF: 238, value: 1.9700},  {maxDF: 241, value: 1.9699},  {maxDF: 243, value: 1.9698},   {maxDF: 246, value: 1.9697},
                {maxDF: 248, value: 1.9696},  {maxDF: 251, value: 1.9695},  {maxDF: 253, value: 1.9694},   {maxDF: 256, value: 1.9693},
                {maxDF: 259, value: 1.9692},  {maxDF: 262, value: 1.9691},  {maxDF: 265, value: 1.9690},   {maxDF: 268, value: 1.9689},
                {maxDF: 271, value: 1.9688},  {maxDF: 274, value: 1.9687},  {maxDF: 277, value: 1.9686},   {maxDF: 280, value: 1.9685},
                {maxDF: 284, value: 1.9684},  {maxDF: 287, value: 1.9683},  {maxDF: 290, value: 1.9682},   {maxDF: 294, value: 1.9681},
                {maxDF: 298, value: 1.9680},  {maxDF: 302, value: 1.9679},  {maxDF: 305, value: 1.9678},   {maxDF: 309, value: 1.9677},
                {maxDF: 313, value: 1.9676},  {maxDF: 318, value: 1.9675},  {maxDF: 322, value: 1.9674},   {maxDF: 326, value: 1.9673},
                {maxDF: 331, value: 1.9672},  {maxDF: 335, value: 1.9671},  {maxDF: 340, value: 1.9670},   {maxDF: 345, value: 1.9669},
                {maxDF: 350, value: 1.9668},  {maxDF: 355, value: 1.9667},  {maxDF: 361, value: 1.9666},   {maxDF: 366, value: 1.9665},
                {maxDF: 372, value: 1.9664},  {maxDF: 378, value: 1.9663},  {maxDF: 384, value: 1.9662},   {maxDF: 390, value: 1.9661},
                {maxDF: 397, value: 1.9660},  {maxDF: 404, value: 1.9659},  {maxDF: 411, value: 1.9658},   {maxDF: 418, value: 1.9657},
                {maxDF: 425, value: 1.9656},  {maxDF: 433, value: 1.9655},  {maxDF: 441, value: 1.9654},   {maxDF: 449, value: 1.9653},
                {maxDF: 458, value: 1.9652},  {maxDF: 467, value: 1.9651},  {maxDF: 476, value: 1.9650},   {maxDF: 486, value: 1.9649},
                {maxDF: 496, value: 1.9648},  {maxDF: 507, value: 1.9647},  {maxDF: 518, value: 1.9646},   {maxDF: 530, value: 1.9645},
                {maxDF: 542, value: 1.9644},  {maxDF: 554, value: 1.9643},  {maxDF: 567, value: 1.9642},   {maxDF: 581, value: 1.9641},
                {maxDF: 596, value: 1.9640},  {maxDF: 611, value: 1.9639},  {maxDF: 627, value: 1.9638},   {maxDF: 644, value: 1.9637},
                {maxDF: 662, value: 1.9636},  {maxDF: 681, value: 1.9635},  {maxDF: 701, value: 1.9634},   {maxDF: 723, value: 1.9633},
                {maxDF: 745, value: 1.9632},  {maxDF: 769, value: 1.9631},  {maxDF: 795, value: 1.9630},   {maxDF: 823, value: 1.9629},
                {maxDF: 852, value: 1.9628},  {maxDF: 884, value: 1.9627},  {maxDF: 918, value: 1.9626},   {maxDF: 955, value: 1.9625},
                {maxDF: 995, value: 1.9624},  {maxDF: 1038, value: 1.9623}, {maxDF: 1086, value: 1.9622},  {maxDF: 1138, value: 1.9621},
                {maxDF: 1195, value: 1.9620}, {maxDF: 1259, value: 1.9619}, {maxDF: 1329, value: 1.9618},  {maxDF: 1408, value: 1.9617},
                {maxDF: 1496, value: 1.9616}, {maxDF: 1597, value: 1.9615}, {maxDF: 1712, value: 1.9614},  {maxDF: 1845, value: 1.9613},
                {maxDF: 2001, value: 1.9612}, {maxDF: 2185, value: 1.9611}, {maxDF: 2407, value: 1.9610},  {maxDF: 2678, value: 1.9609},
                {maxDF: 3019, value: 1.9608}, {maxDF: 3459, value: 1.9607}, {maxDF: 4049, value: 1.9606},  {maxDF: 4882, value: 1.9605},
                {maxDF: 6146, value: 1.9604}, {maxDF: 8295, value: 1.9603}, {maxDF: 12754, value: 1.9602}, {maxDF: 27580, value: 1.9601},
                {maxDF: Infinity, value: 1.9600}
            ]
        },
        0.99: {
            probabilityToTValue: [
                31.8205,  6.9646,  4.5407,  3.7469,  3.3649,  3.1427,  2.998,   2.8965,  2.8214,  2.7638,
                2.7181,   2.681,   2.6503,  2.6245,  2.6025,  2.5835,  2.5669,  2.5524,  2.5395,  2.528,
                2.5176,   2.5083,  2.4999,  2.4922,  2.4851,  2.4786,  2.4727,  2.4671,  2.462,   2.4573,
                2.4528,   2.4487,  2.4448,  2.4411,  2.4377,  2.4345,  2.4314,  2.4286,  2.4258,  2.4233,
                2.4208,   2.4185,  2.4163,  2.4141,  2.4121,  2.4102,  2.4083,  2.4066,  2.4049,  2.4033,
                2.4017,   2.4002,  2.3988,  2.3974,  2.3961,  2.3948,  2.3936,  2.3924,  2.3912,  2.3901,
                2.389,    2.388,   2.387,   2.386,   2.3851,  2.3842,  2.3833,  2.3824,  2.3816,  2.3808,
                2.38,     2.3793,  2.3785,  2.3778,  2.3771,  2.3764,  2.3758,  2.3751,  2.3745,  2.3739,
                2.3733,   2.3727,  2.3721,  2.3716,  2.371,   2.3705,  2.37,    2.3695,  2.369,   2.3685,
                2.368,    2.3676,  2.3671,  2.3667,  2.3662,  2.3658,  2.3654,  2.365,   2.3646,  2.3642,
                2.3638,   2.3635,  2.3631,  2.3627,  2.3624,  2.362,   2.3617,  2.3614,  2.361,   2.3607,
                2.3604,   2.3601,  2.3598,  2.3595,  2.3592,  2.3589,  2.3586,  2.3584,  2.3581,  2.3578,
                2.3576,   2.3573,  2.357,   2.3568,  2.3565,  2.3563,  2.3561,  2.3558,  2.3556,  2.3554,
                2.3552,   2.3549,  2.3547,  2.3545,  2.3543,  2.3541,  2.3539,  2.3537,  2.3535,  2.3533,
                2.3531,   2.3529,  2.3527,  2.3525,  2.3523,  2.3522,  2.352,   2.3518,  2.3516,  2.3515,
                2.3513,   2.3511,  2.351,   2.3508,  2.3506,  2.3505,  2.3503,  2.3502,  2.35,    2.3499,
                2.3497,   2.3496,  2.3494,  2.3493,  2.3492,  2.349,   2.3489,  2.3487,  2.3486,  2.3485,
                2.3484,   2.3482,  2.3481,  2.348,   2.3478,  2.3477,  2.3476,  2.3475,  2.3474,  2.3472,
                2.3471,   2.347,   2.3469,  2.3468,  2.3467,  2.3466,  2.3465,  2.3463,  2.3462,  2.3461,
                2.346,    2.3459,  2.3458,  2.3457,  2.3456,  2.3455,  2.3454,  2.3453,  2.3452,  2.3451
            ],
            tValuesSortedByProbability: [
                {maxDF: 201, value: 2.3450},   {maxDF: 203, value: 2.3449},   {maxDF: 204, value: 2.3448},     {maxDF: 205, value: 2.3447},
                {maxDF: 206, value: 2.3446},   {maxDF: 207, value: 2.3445},   {maxDF: 208, value: 2.3444},     {maxDF: 209, value: 2.3443},
                {maxDF: 211, value: 2.3442},   {maxDF: 212, value: 2.3441},   {maxDF: 213, value: 2.3440},     {maxDF: 214, value: 2.3439},
                {maxDF: 215, value: 2.3438},   {maxDF: 217, value: 2.3437},   {maxDF: 218, value: 2.3436},     {maxDF: 219, value: 2.3435},
                {maxDF: 220, value: 2.3434},   {maxDF: 222, value: 2.3433},   {maxDF: 223, value: 2.3432},     {maxDF: 224, value: 2.3431},
                {maxDF: 226, value: 2.3430},   {maxDF: 227, value: 2.3429},   {maxDF: 228, value: 2.3428},     {maxDF: 230, value: 2.3427},
                {maxDF: 231, value: 2.3426},   {maxDF: 233, value: 2.3425},   {maxDF: 234, value: 2.3424},     {maxDF: 236, value: 2.3423},
                {maxDF: 237, value: 2.3422},   {maxDF: 239, value: 2.3421},   {maxDF: 240, value: 2.3420},     {maxDF: 242, value: 2.3419},
                {maxDF: 243, value: 2.3418},   {maxDF: 245, value: 2.3417},   {maxDF: 246, value: 2.3416},     {maxDF: 248, value: 2.3415},
                {maxDF: 250, value: 2.3414},   {maxDF: 251, value: 2.3413},   {maxDF: 253, value: 2.3412},     {maxDF: 255, value: 2.3411},
                {maxDF: 256, value: 2.3410},   {maxDF: 258, value: 2.3409},   {maxDF: 260, value: 2.3408},     {maxDF: 262, value: 2.3407},
                {maxDF: 264, value: 2.3406},   {maxDF: 265, value: 2.3405},   {maxDF: 267, value: 2.3404},     {maxDF: 269, value: 2.3403},
                {maxDF: 271, value: 2.3402},   {maxDF: 273, value: 2.3401},   {maxDF: 275, value: 2.3400},     {maxDF: 277, value: 2.3399},
                {maxDF: 279, value: 2.3398},   {maxDF: 281, value: 2.3397},   {maxDF: 283, value: 2.3396},     {maxDF: 286, value: 2.3395},
                {maxDF: 288, value: 2.3394},   {maxDF: 290, value: 2.3393},   {maxDF: 292, value: 2.3392},     {maxDF: 295, value: 2.3391},
                {maxDF: 297, value: 2.3390},   {maxDF: 299, value: 2.3389},   {maxDF: 302, value: 2.3388},     {maxDF: 304, value: 2.3387},
                {maxDF: 307, value: 2.3386},   {maxDF: 309, value: 2.3385},   {maxDF: 312, value: 2.3384},     {maxDF: 314, value: 2.3383},
                {maxDF: 317, value: 2.3382},   {maxDF: 320, value: 2.3381},   {maxDF: 322, value: 2.3380},     {maxDF: 325, value: 2.3379},
                {maxDF: 328, value: 2.3378},   {maxDF: 331, value: 2.3377},   {maxDF: 334, value: 2.3376},     {maxDF: 337, value: 2.3375},
                {maxDF: 340, value: 2.3374},   {maxDF: 343, value: 2.3373},   {maxDF: 346, value: 2.3372},     {maxDF: 349, value: 2.3371},
                {maxDF: 353, value: 2.3370},   {maxDF: 356, value: 2.3369},   {maxDF: 360, value: 2.3368},     {maxDF: 363, value: 2.3367},
                {maxDF: 367, value: 2.3366},   {maxDF: 370, value: 2.3365},   {maxDF: 374, value: 2.3364},     {maxDF: 378, value: 2.3363},
                {maxDF: 381, value: 2.3362},   {maxDF: 385, value: 2.3361},   {maxDF: 389, value: 2.3360},     {maxDF: 393, value: 2.3359},
                {maxDF: 398, value: 2.3358},   {maxDF: 402, value: 2.3357},   {maxDF: 406, value: 2.3356},     {maxDF: 411, value: 2.3355},
                {maxDF: 415, value: 2.3354},   {maxDF: 420, value: 2.3353},   {maxDF: 425, value: 2.3352},     {maxDF: 430, value: 2.3351},
                {maxDF: 435, value: 2.3350},   {maxDF: 440, value: 2.3349},   {maxDF: 445, value: 2.3348},     {maxDF: 450, value: 2.3347},
                {maxDF: 456, value: 2.3346},   {maxDF: 461, value: 2.3345},   {maxDF: 467, value: 2.3344},     {maxDF: 473, value: 2.3343},
                {maxDF: 479, value: 2.3342},   {maxDF: 485, value: 2.3341},   {maxDF: 492, value: 2.3340},     {maxDF: 498, value: 2.3339},
                {maxDF: 505, value: 2.3338},   {maxDF: 512, value: 2.3337},   {maxDF: 519, value: 2.3336},     {maxDF: 526, value: 2.3335},
                {maxDF: 534, value: 2.3334},   {maxDF: 541, value: 2.3333},   {maxDF: 549, value: 2.3332},     {maxDF: 557, value: 2.3331},
                {maxDF: 566, value: 2.3330},   {maxDF: 575, value: 2.3329},   {maxDF: 584, value: 2.3328},     {maxDF: 593, value: 2.3327},
                {maxDF: 602, value: 2.3326},   {maxDF: 612, value: 2.3325},   {maxDF: 622, value: 2.3324},     {maxDF: 633, value: 2.3323},
                {maxDF: 644, value: 2.3322},   {maxDF: 655, value: 2.3321},   {maxDF: 667, value: 2.3320},     {maxDF: 679, value: 2.3319},
                {maxDF: 691, value: 2.3318},   {maxDF: 704, value: 2.3317},   {maxDF: 718, value: 2.3316},     {maxDF: 732, value: 2.3315},
                {maxDF: 747, value: 2.3314},   {maxDF: 762, value: 2.3313},   {maxDF: 778, value: 2.3312},     {maxDF: 794, value: 2.3311},
                {maxDF: 811, value: 2.3310},   {maxDF: 829, value: 2.3309},   {maxDF: 848, value: 2.3308},     {maxDF: 868, value: 2.3307},
                {maxDF: 888, value: 2.3306},   {maxDF: 910, value: 2.3305},   {maxDF: 933, value: 2.3304},     {maxDF: 957, value: 2.3303},
                {maxDF: 982, value: 2.3302},   {maxDF: 1008, value: 2.3301},  {maxDF: 1036, value: 2.3300},    {maxDF: 1066, value: 2.3299},
                {maxDF: 1097, value: 2.3298},  {maxDF: 1130, value: 2.3297},  {maxDF: 1166, value: 2.3296},    {maxDF: 1203, value: 2.3295},
                {maxDF: 1243, value: 2.3294},  {maxDF: 1286, value: 2.3293},  {maxDF: 1332, value: 2.3292},    {maxDF: 1381, value: 2.3291},
                {maxDF: 1434, value: 2.3290},  {maxDF: 1491, value: 2.3289},  {maxDF: 1553, value: 2.3288},    {maxDF: 1621, value: 2.3287},
                {maxDF: 1694, value: 2.3286},  {maxDF: 1775, value: 2.3285},  {maxDF: 1864, value: 2.3284},    {maxDF: 1962, value: 2.3283},
                {maxDF: 2070, value: 2.3282},  {maxDF: 2192, value: 2.3281},  {maxDF: 2329, value: 2.3280},    {maxDF: 2484, value: 2.3279},
                {maxDF: 2661, value: 2.3278},  {maxDF: 2865, value: 2.3277},  {maxDF: 3103, value: 2.3276},    {maxDF: 3385, value: 2.3275},
                {maxDF: 3722, value: 2.3274},  {maxDF: 4135, value: 2.3273},  {maxDF: 4650, value: 2.3272},    {maxDF: 5312, value: 2.3271},
                {maxDF: 6194, value: 2.3270},  {maxDF: 7428, value: 2.3269},  {maxDF: 9274, value: 2.3268},    {maxDF: 12344, value: 2.3267},
                {maxDF: 18450, value: 2.3266}, {maxDF: 36515, value: 2.3265}, {maxDF: 1754068, value: 2.3264}, {maxDF: Infinity, value: 2.3263}
            ]
        },
    };

    function oneSidedToTwoSidedProbability(probability) { return 2 * probability - 1; }
    function twoSidedToOneSidedProbability(probability) { return (1 - (1 - probability) / 2); }

    function splitIntoSegmentsUntilGoodEnough(values, BirgeAndMassartC) {
        if (values.length < 2)
            return [0, values.length];

        var matrix = new SampleVarianceUpperTriangularMatrix(values);

        var SchwarzCriterionBeta = Math.log1p(values.length - 1) / values.length;

        BirgeAndMassartC = BirgeAndMassartC || 2.5; // Suggested by the authors.
        var BirgeAndMassartPenalization = function (segmentCount) {
            return segmentCount * (1 + BirgeAndMassartC * Math.log1p(values.length / segmentCount - 1));
        }

        var segmentation;
        var minTotalCost = Infinity;
        var maxK = Math.min(50, values.length);

        for (var k = 1; k < maxK; k++) {
            var start = Date.now();
            var result = findOptimalSegmentation(values, matrix, k);
            var cost = result.cost / values.length;
            var penalty = SchwarzCriterionBeta * BirgeAndMassartPenalization(k);
            if (cost + penalty < minTotalCost) {
                minTotalCost = cost + penalty;
                segmentation = result.segmentation;
            } else
                maxK = Math.min(maxK, k + 3);
            if (Statistics.debuggingSegmentation)
                console.log('splitIntoSegmentsUntilGoodEnough', k, Date.now() - start, cost + penalty);
        }

        return segmentation;
    }

    function allocateCostUpperTriangularForSegmentation(values, segmentCount)
    {
        // Dynamic programming. cost[i][k] = The cost to segmenting values up to i into k segments.
        var cost = new Array(values.length);
        for (var segmentEnd = 0; segmentEnd < values.length; segmentEnd++)
            cost[segmentEnd] = new Float32Array(segmentCount + 1);
        return cost;
    }

    function allocatePreviousNodeForSegmentation(values, segmentCount)
    {
        // previousNode[i][k] = The start of the last segment in an optimal segmentation that ends at i with k segments.
        var previousNode = new Array(values.length);
        for (var i = 0; i < values.length; i++)
            previousNode[i] = new Array(segmentCount + 1);
        return previousNode;
    }

    function findOptimalSegmentationInternal(cost, previousNode, values, costMatrix, segmentCount)
    {
        cost[0] = [0]; // The cost of segmenting single value is always 0.
        previousNode[0] = [-1];
        for (var segmentStart = 0; segmentStart < values.length; segmentStart++) {
            var costOfOptimalSegmentationThatEndAtCurrentStart = cost[segmentStart];
            for (var k = 0; k < segmentCount; k++) {
                var noSegmentationOfLenghtKEndsAtCurrentStart = previousNode[segmentStart][k] === undefined;
                if (noSegmentationOfLenghtKEndsAtCurrentStart)
                    continue;
                for (var segmentEnd = segmentStart + 1; segmentEnd < values.length; segmentEnd++) {
                    var costOfOptimalSegmentationOfLengthK = costOfOptimalSegmentationThatEndAtCurrentStart[k];
                    var costOfCurrentSegment = costMatrix.costBetween(segmentStart, segmentEnd);
                    var totalCost = costOfOptimalSegmentationOfLengthK + costOfCurrentSegment;
                    if (previousNode[segmentEnd][k + 1] === undefined || totalCost < cost[segmentEnd][k + 1]) {
                        cost[segmentEnd][k + 1] = totalCost;
                        previousNode[segmentEnd][k + 1] = segmentStart;
                    }
                }
            }
        }
    }

    function findOptimalSegmentation(values, costMatrix, segmentCount) {
        var cost = allocateCostUpperTriangularForSegmentation(values, segmentCount);
        var previousNode = allocatePreviousNodeForSegmentation(values, segmentCount);

        findOptimalSegmentationInternal(cost, previousNode, values, costMatrix, segmentCount);

        if (Statistics.debuggingSegmentation) {
            console.log('findOptimalSegmentation with', segmentCount, 'segments');
            for (var end = 0; end < values.length; end++) {
                for (var k = 0; k <= segmentCount; k++) {
                    var start = previousNode[end][k];
                    if (start === undefined)
                        continue;
                    console.log(`C(segment=[${start}, ${end + 1}], segmentCount=${k})=${cost[end][k]}`);
                }
            }
        }

        var segmentEnd = values.length - 1;
        var segmentation = new Array(segmentCount + 1);
        segmentation[segmentCount] = values.length;
        for (var k = segmentCount; k > 0; k--) {
            segmentEnd = previousNode[segmentEnd][k];
            segmentation[k - 1] = segmentEnd;
        }
        var costOfOptimalSegmentation = cost[values.length - 1][segmentCount];

        if (Statistics.debuggingSegmentation)
            console.log('Optimal segmentation:', segmentation, 'with cost =', costOfOptimalSegmentation);

        return {segmentation: segmentation, cost: costOfOptimalSegmentation};
    }

    function SampleVarianceUpperTriangularMatrix(values) {
        // The cost of segment (i, j].
        var costMatrix = new Array(values.length - 1);
        for (var i = 0; i < values.length - 1; i++) {
            var remainingValueCount = values.length - i - 1;
            costMatrix[i] = new Float32Array(remainingValueCount);
            var sum = values[i];
            var squareSum = sum * sum;
            costMatrix[i][0] = 0;
            for (var j = i + 1; j < values.length; j++) {
                var currentValue = values[j];
                sum += currentValue;
                squareSum += currentValue * currentValue;
                var sampleSize = j - i + 1;
                var stdev = Statistics.sampleStandardDeviation(sampleSize, sum, squareSum);
                costMatrix[i][j - i - 1] = stdev > 0 ? sampleSize * Math.log(stdev * stdev) : 0;
            }
        }
        this.costMatrix = costMatrix;
    }

    SampleVarianceUpperTriangularMatrix.prototype.costBetween = function(from, to) {
        if (from >= this.costMatrix.length || from == to)
            return 0; // The cost of the segment that starts at the last data point is 0.
        return this.costMatrix[from][to - from - 1];
    }

})();

if (typeof module != 'undefined')
    module.exports = Statistics;
