
var StatisticsStrategies = {};

(function () {

StatisticsStrategies.MovingAverageStrategies = [
    {
        id: 1,
        label: 'Simple Moving Average',
        parameterList: [
            {label: "Backward window size", value: 8, min: 2, step: 1},
            {label: "Forward window size", value: 4, min: 0, step: 1}
        ],
        execute: function (backwardWindowSize, forwardWindowSize, values) {
            return Statistics.movingAverage(values, backwardWindowSize, forwardWindowSize);
        },
    },
    {
        id: 2,
        label: 'Cumulative Moving Average',
        execute: Statistics.cumulativeMovingAverage,
    },
    {
        id: 3,
        label: 'Exponential Moving Average',
        parameterList: [{label: "Smoothing factor", value: 0.1, min: 0.001, max: 0.9}],
        execute: function (smoothingFactor, values) {
            if (!values.length || typeof(smoothingFactor) !== "number")
                return null;
            return Statistics.exponentialMovingAverage(values, smoothingFactor);
        }
    },
    {
        id: 4,
        isSegmentation: true,
        label: 'Segmentation: Recursive t-test',
        description: "Recursively split values into two segments if Welch's t-test detects a statistically significant difference.",
        parameterList: [{label: "Minimum segment length", value: 20, min: 5}],
        execute: function (minLength, values) {
            return averagesFromSegments(values, Statistics.segmentTimeSeriesGreedyWithStudentsTTest(values, minLength));
        }
    },
    {
        id: 5,
        isSegmentation: true,
        label: 'Segmentation: Schwarz criterion',
        description: 'Adaptive algorithm that maximizes the Schwarz criterion (BIC).',
        // Based on Detection of Multiple Change–Points in Multivariate Time Series by Marc Lavielle (July 2006).
        execute: function (values) {
            return averagesFromSegments(values, Statistics.segmentTimeSeriesByMaximizingSchwarzCriterion(values));
        }
    },
];

function averagesFromSegments(values, segmentStartIndices) {
    var averages = new Array(values.length);
    var averageIndex = 0;
    for (var i = 0; i < segmentStartIndices.length; i++) {
        var segment = values.slice(segmentStartIndices[i - 1], segmentStartIndices[i]);
        var average = Statistics.sum(segment) / segment.length;
        for (var j = 0; j < segment.length; j++)
            averages[averageIndex++] = average;
    }
    return averages;
}


StatisticsStrategies.EnvelopingStrategies = [
    {
        id: 100,
        label: 'Average Difference',
        description: 'The average difference between consecutive values.',
        execute: function (values, movingAverages) {
            if (values.length < 1)
                return NaN;

            var diff = 0;
            for (var i = 1; i < values.length; i++)
                diff += Math.abs(values[i] - values[i - 1]);

            return diff / values.length;
        }
    },
    {
        id: 101,
        label: 'Moving Average Standard Deviation',
        description: 'The square root of the average deviation from the moving average with Bessel\'s correction.',
        execute: function (values, movingAverages) {
            if (values.length < 1)
                return NaN;

            var diffSquareSum = 0;
            for (var i = 1; i < values.length; i++) {
                var diff = (values[i] - movingAverages[i]);
                diffSquareSum += diff * diff;
            }

            return Math.sqrt(diffSquareSum / (values.length - 1));
        }
    },
];


StatisticsStrategies.TestRangeSelectionStrategies = [
    {
        id: 301,
        label: "t-test 99% significance",
        execute: function (values, segmentedValues) {
            if (!values.length)
                return [];

            var previousMean = segmentedValues[0];
            var selectedRanges = new Array;
            for (var i = 1; i < segmentedValues.length; i++) {
                var currentMean = segmentedValues[i];
                if (currentMean == previousMean)
                    continue;
                var found = false;
                for (var leftEdge = i - 2, rightEdge = i + 2; leftEdge >= 0 && rightEdge <= values.length; leftEdge--, rightEdge++) {
                    if (segmentedValues[leftEdge] != previousMean || segmentedValues[rightEdge - 1] != currentMean)
                        break;
                    var result = Statistics.computeWelchsT(values, leftEdge, i - leftEdge, values, i, rightEdge - i, 0.98);
                    if (result.significantlyDifferent) {
                        selectedRanges.push([leftEdge, rightEdge - 1]);
                        found = true;
                        break;
                    }
                }
                if (!found && Statistics.debuggingTestingRangeNomination)
                    console.log('Failed to find a testing range at', i, 'changing from', previousMean, 'to', currentMean);
                previousMean = currentMean;
            }
            return selectedRanges;
        }
    }
];



function createWesternElectricRule(windowSize, minOutlinerCount, limitFactor) {
    return function (values, movingAverages, deviation) {
        var results = new Array(values.length);
        var limit = limitFactor * deviation;
        for (var i = 0; i < values.length; i++)
            results[i] = countValuesOnSameSide(values, movingAverages, limit, i, windowSize) >= minOutlinerCount ? windowSize : 0;
        return results;
    }
}

function countValuesOnSameSide(values, movingAverages, limit, startIndex, windowSize) {
    var valuesAboveLimit = 0;
    var valuesBelowLimit = 0;
    var center = movingAverages[startIndex];
    for (var i = startIndex; i < startIndex + windowSize && i < values.length; i++) {
        var diff = values[i] - center;
        valuesAboveLimit += (diff > limit);
        valuesBelowLimit += (diff < -limit);
    }
    return Math.max(valuesAboveLimit, valuesBelowLimit);
}

StatisticsStrategies.AnomalyDetectionStrategy = [
    // Western Electric rules: http://en.wikipedia.org/wiki/Western_Electric_rules
    {
        id: 200,
        label: 'Western Electric: any point beyond 3σ',
        description: 'Any single point falls outside 3σ limit from the moving average',
        execute: createWesternElectricRule(1, 1, 3),
    },
    {
        id: 201,
        label: 'Western Electric: 2/3 points beyond 2σ',
        description: 'Two out of three consecutive points fall outside 2σ limit from the moving average on the same side',
        execute: createWesternElectricRule(3, 2, 2),
    },
    {
        id: 202,
        label: 'Western Electric: 4/5 points beyond σ',
        description: 'Four out of five consecutive points fall outside 2σ limit from the moving average on the same side',
        execute: createWesternElectricRule(5, 4, 1),
    },
    {
        id: 203,
        label: 'Western Electric: 9 points on same side',
        description: 'Nine consecutive points on the same side of the moving average',
        execute: createWesternElectricRule(9, 9, 0),
    },
    {
        id: 210,
        label: 'Mozilla: t-test 5 vs. 20 before that',
        description: "Use student's t-test to determine whether the mean of the last five data points differs from the mean of the twenty values before that",
        execute: function (values, movingAverages, deviation) {
            var results = new Array(values.length);
            var p = false;
            for (var i = 20; i < values.length - 5; i++)
                results[i] = Statistics.testWelchsT(values.slice(i - 20, i), values.slice(i, i + 5), 0.98) ? 5 : 0;
            return results;
        }
    },
]

StatisticsStrategies.executeStrategy = function (strategy, rawValues, additionalArguments)
{
    var parameters = (strategy.parameterList || []).map(function (param) {
        var parsed = parseFloat(param.value);
        return Math.min(param.max || Infinity, Math.max(param.min || -Infinity, isNaN(parsed) ? 0 : parsed));
    });
    parameters.push(rawValues);
    return strategy.execute.apply(strategy, parameters.concat(additionalArguments));
};

})();

if (typeof module != 'undefined') {
    for (var key in StatisticsStrategies)
        module.exports[key] = StatisticsStrategies[key];
}
