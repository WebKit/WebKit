/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

var count = output.length;

var itemTotals = {};
itemTotals.length = count;

var total = 0;
var categoryTotals = {};
var testTotalsByCategory = {};

var mean = 0;
var categoryMeans = {};
var testMeansByCategory = {};

var stdDev = 0;
var categoryStdDevs = {};
var testStdDevsByCategory = {};

var stdErr = 0;
var categoryStdErrs = {};
var testStdErrsByCategory = {};

function initialize()
{
    itemTotals = {total: []};

    for (var i = 0; i < categories.length; i++) {
        var category = categories[i];
        itemTotals[category] = [];
        categoryTotals[category] = 0;
        testTotalsByCategory[category] = {};
        categoryMeans[category] = 0;
        testMeansByCategory[category] = {};
        categoryStdDevs[category] = 0;
        testStdDevsByCategory[category] = {};
        categoryStdErrs[category] = 0;
        testStdErrsByCategory[category] = {};
    }

    for (var i = 0; i < tests.length; i++) {
        var test = tests[i];
        itemTotals[test] = [];
        var category = test.replace(/-.*/, "");
        testTotalsByCategory[category][test] = 0;
        testMeansByCategory[category][test] = 0;
        testStdDevsByCategory[category][test] = 0;
        testStdErrsByCategory[category][test] = 0;
    }

    for (var i = 0; i < count; i++) {
        itemTotals["total"][i] = 0;
        for (var category in categoryTotals) {
            itemTotals[category][i] = 0;
            for (var test in testTotalsByCategory[category]) {
                itemTotals[test][i] = 0;
            }
        }
    }
}

function computeItemTotals()
{
    for (var i = 0; i < output.length; i++) {
        var result = output[i];
        for (var test in result) {
            var time = result[test];
            var category = test.replace(/-.*/, "");
            itemTotals["total"][i] += time;
            itemTotals[category][i] += time;
            itemTotals[test][i] += time;
        }
    }
}

function computeTotals()
{
    for (var i = 0; i < output.length; i++) {
        var result = output[i];
        for (var test in result) {
            var time = result[test];
            var category = test.replace(/-.*/, "");
            total += time;
            categoryTotals[category] += time;
            testTotalsByCategory[category][test] += time;
        }
    }
}

function computeMeans()
{
    mean = total / count;
    for (var category in categoryTotals) {
        categoryMeans[category] = categoryTotals[category] / count;
        for (var test in testTotalsByCategory[category]) {
            testMeansByCategory[category][test] = testTotalsByCategory[category][test] / count;
        }
    }
}

function standardDeviation(mean, items)
{
    var deltaSquaredSum = 0;
    for (var i = 0; i < items.length; i++) {
        var delta = items[i] - mean;
        deltaSquaredSum += delta * delta;
    }
    variance = deltaSquaredSum / (items.length - 1);
    return Math.sqrt(variance);
}

function computeStdDevs()
{
    stdDev = standardDeviation(mean, itemTotals["total"]);
    for (var category in categoryStdDevs) {
        categoryStdDevs[category] = standardDeviation(categoryMeans[category], itemTotals[category]);
    }
    for (var category in categoryStdDevs) {
        for (var test in testStdDevsByCategory[category]) {
            testStdDevsByCategory[category][test] = standardDeviation(testMeansByCategory[category][test], itemTotals[test]);
        }
    }
}

function computeStdErrors()
{
    var sqrtCount = Math.sqrt(count);

    stdErr = stdDev / sqrtCount;
    for (var category in categoryStdErrs) {
        categoryStdErrs[category] = categoryStdDevs[category] / sqrtCount;
    }
    for (var category in categoryStdDevs) {
        for (var test in testStdErrsByCategory[category]) {
            testStdErrsByCategory[category][test] = testStdDevsByCategory[category][test] / sqrtCount;
        }
    }

}

var tDistribution = [NaN, NaN, 12.71, 4.30, 3.18, 2.78, 2.57, 2.45, 2.36, 2.31, 2.26, 2.23, 2.20, 2.18, 2.16, 2.14, 2.13, 2.12, 2.11, 2.10, 2.09, 2.09, 2.08, 2.07, 2.07, 2.06, 2.06, 2.06, 2.05, 2.05, 2.05, 2.04, 2.04, 2.04, 2.03, 2.03, 2.03, 2.03, 2.03, 2.02, 2.02, 2.02, 2.02, 2.02, 2.02, 2.02, 2.01, 2.01, 2.01, 2.01, 2.01, 2.01, 2.01, 2.01, 2.01, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.99, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.98, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.97, 1.96];
var tMax = tDistribution.length;
var tLimit = 1.96;

function tDist(n)
{
    if (n > tMax)
        return tLimit;
    return tDistribution[n];
}


function formatResult(meanWidth, mean, stdErr, n)
{
    var meanString = mean.toFixed(1).toString();
    while (meanString.length < meanWidth) {
        meanString = " " + meanString;
    }

    if (n == 1)
        return meanString + "ms";

    return meanString + "ms +/- " + ((tDist(n) * stdErr / mean) * 100).toFixed(1) + "%";
}

function computeLabelWidth()
{
    var width = "Total".length;
    for (var category in categoryMeans) {
        if (category.length + 2 > width)
            width = category.length + 2;
    }
    for (var i = 0; i < tests.length; i++) {
        var shortName = tests[i].replace(/^[^-]*-/, "");
        if (shortName.length + 4 > width)
            width = shortName.length + 4;
    }

    return width;
}

function computeMeanWidth()
{
    var width = mean.toFixed(1).toString().length;
    for (var category in categoryMeans) {
        var candidate = categoryMeans[category].toFixed(2).toString().length;
        if (candidate > width)
            width = candidate;
        for (var test in testMeansByCategory[category]) {
            var candidate = testMeansByCategory[category][test].toFixed(2).toString().length;
            if (candidate > width)
                width = candidate;
        }
    }

    return width;
}

function resultLine(labelWidth, indent, label, meanWidth, mean, stdErr)
{
    var result = "";
    for (i = 0; i < indent; i++) {
        result += " ";
    }
    
    result += label + ": ";

    for (i = 0; i < (labelWidth - (label.length + indent)); i++) {
        result += " ";
    }
    
    return result + formatResult(meanWidth, mean, stdErr, count);
}

function printOutput()
{
    var labelWidth = computeLabelWidth();
    var meanWidth = computeMeanWidth();

    print("\n");
    print("============================================");
    if (count == 1)
        print("RESULTS");
    else
        print("RESULTS (means and 95% confidence intervals)");
    print("--------------------------------------------");
    print(resultLine(labelWidth, 0, "Total", meanWidth, mean, stdErr));
    print("--------------------------------------------");
    for (var category in categoryMeans) {
        print("");
        print(resultLine(labelWidth, 2, category, meanWidth, categoryMeans[category], categoryStdErrs[category]));
        for (var test in testMeansByCategory[category]) {
            var shortName = test.replace(/^[^-]*-/, "");
            print(resultLine(labelWidth, 4, shortName, meanWidth, testMeansByCategory[category][test], testStdErrsByCategory[category][test]));
        }
    }
}

initialize();
computeItemTotals();
computeTotals();
computeMeans();
computeStdDevs();
computeStdErrors();
printOutput();
