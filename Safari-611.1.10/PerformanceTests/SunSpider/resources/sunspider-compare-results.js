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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

function sunspiderCompareResults(output1, output2)
{
    var count1 = output1.length;
    var count2 = output2.length;

    var itemTotals1 = {};
    itemTotals1.length = count1;
    
    var total1 = 0;
    var categoryTotals1 = {};
    var testTotalsByCategory1 = {};
    
    var mean1 = 0;
    var categoryMeans1 = {};
    var testMeansByCategory1 = {};
    
    var stdDev1 = 0;
    var categoryStdDevs1 = {};
    var testStdDevsByCategory1 = {};
    
    var stdErr1 = 0;
    var categoryStdErrs1 = {};
    var testStdErrsByCategory1 = {};
    
    var itemTotals2 = {};
    itemTotals2.length = count2;
    
    var total2 = 0;
    var categoryTotals2 = {};
    var testTotalsByCategory2 = {};
    
    var mean2 = 0;
    var categoryMeans2 = {};
    var testMeansByCategory2 = {};
    
    var stdDev2 = 0;
    var categoryStdDevs2 = {};
    var testStdDevsByCategory2 = {};
    
    var stdErr2 = 0;
    var categoryStdErrs2 = {};
    var testStdErrsByCategory2 = {};
    
    function initialize()
    {
        itemTotals1 = {total: []};
        
        for (var i = 0; i < categories.length; i++) {
            var category = categories[i];
            itemTotals1[category] = [];
            categoryTotals1[category] = 0;
            testTotalsByCategory1[category] = {};
            categoryMeans1[category] = 0;
            testMeansByCategory1[category] = {};
            categoryStdDevs1[category] = 0;
            testStdDevsByCategory1[category] = {};
            categoryStdErrs1[category] = 0;
            testStdErrsByCategory1[category] = {};
        }
        
        for (var i = 0; i < tests.length; i++) {
            var test = tests[i];
            itemTotals1[test] = [];
            var category = test.replace(/-.*/, "");
            testTotalsByCategory1[category][test] = 0;
            testMeansByCategory1[category][test] = 0;
            testStdDevsByCategory1[category][test] = 0;
            testStdErrsByCategory1[category][test] = 0;
        }
        
        for (var i = 0; i < count1; i++) {
            itemTotals1["total"][i] = 0;
            for (var category in categoryTotals1) {
                itemTotals1[category][i] = 0;
                for (var test in testTotalsByCategory1[category]) {
                    itemTotals1[test][i] = 0;
                }
            }
        }
        
        itemTotals2 = {total: []};
        
        for (var i = 0; i < categories.length; i++) {
            var category = categories[i];
            itemTotals2[category] = [];
            categoryTotals2[category] = 0;
            testTotalsByCategory2[category] = {};
            categoryMeans2[category] = 0;
            testMeansByCategory2[category] = {};
            categoryStdDevs2[category] = 0;
            testStdDevsByCategory2[category] = {};
            categoryStdErrs2[category] = 0;
            testStdErrsByCategory2[category] = {};
        }
        
        for (var i = 0; i < tests.length; i++) {
            var test = tests[i];
            itemTotals2[test] = [];
            var category = test.replace(/-.*/, "");
            testTotalsByCategory2[category][test] = 0;
            testMeansByCategory2[category][test] = 0;
            testStdDevsByCategory2[category][test] = 0;
            testStdErrsByCategory2[category][test] = 0;
        }
        
        for (var i = 0; i < count2; i++) {
            itemTotals2["total"][i] = 0;
            for (var category in categoryTotals2) {
                itemTotals2[category][i] = 0;
                for (var test in testTotalsByCategory2[category]) {
                    itemTotals2[test][i] = 0;
                }
            }
        }
        
    }
    
    function computeItemTotals(output, itemTotals)
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
    
    function computeTotals(output, categoryTotals, testTotalsByCategory)
    {
        var total = 0;
        
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
        
        return total;
    }
    
    function computeMeans(count, total, categoryTotals, categoryMeans, testTotalsByCategory, testMeansByCategory)
    {
        var mean = total / count;
        for (var category in categoryTotals) {
            categoryMeans[category] = categoryTotals[category] / count;
            for (var test in testTotalsByCategory[category]) {
                testMeansByCategory[category][test] = testTotalsByCategory[category][test] / count;
            }
        }
        return mean;
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
    
    function computeStdDevs(mean, itemTotals, categoryStdDevs, categoryMeans, testStdDevsByCategory, testMeansByCategory)
    {
        var stdDev = standardDeviation(mean, itemTotals["total"]);
        for (var category in categoryStdDevs) {
            categoryStdDevs[category] = standardDeviation(categoryMeans[category], itemTotals[category]);
        }
        for (var category in categoryStdDevs) {
            for (var test in testStdDevsByCategory[category]) {
                testStdDevsByCategory[category][test] = standardDeviation(testMeansByCategory[category][test], itemTotals[test]);
            }
        }
        return stdDev;
    }
    
    function computeStdErrors(count, stdDev, categoryStdErrs, categoryStdDevs, testStdErrsByCategory, testStdDevsByCategory)
    {
        var sqrtCount = Math.sqrt(count);
        
        var stdErr = stdDev / sqrtCount;
        for (var category in categoryStdErrs) {
            categoryStdErrs[category] = categoryStdDevs[category] / sqrtCount;
        }
        for (var category in categoryStdDevs) {
            for (var test in testStdErrsByCategory[category]) {
                testStdErrsByCategory[category][test] = testStdDevsByCategory[category][test] / sqrtCount;
            }
        }
        
        return stdErr;
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
    
    
    function formatMean(meanWidth, mean, stdErr, count)
    {
        if (mean != mean) {
            var result = "  ERROR      ";
            for (var i = 0; i < meanWidth; ++i)
                result = " " + result;
            return result;
        }
        
        var meanString = mean.toFixed(1).toString();
        while (meanString.length < meanWidth) {
            meanString = " " + meanString;
        }
        
        var errString = ((tDist(count) * stdErr / mean) * 100).toFixed(1) + "%";
        while (errString.length < "99.9%".length)
            errString += " ";
        
        var error = "+/- " + errString + " ";
        
        return meanString + "ms " + error;
    }
    
    function computeLabelWidth()
    {
        var width = "Total".length;
        for (var category in categoryMeans1) {
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
    
    function computeMeanWidth(mean, categoryMeans, testMeansByCategory)
    {
        var width = mean.toFixed(1).toString().length;
        for (var category in categoryMeans) {
            var candidate = categoryMeans[category].toFixed(1).toString().length;
            if (candidate > width)
                width = candidate;
            for (var test in testMeansByCategory[category]) {
                var candidate = testMeansByCategory[category][test].toFixed(1).toString().length;
                if (candidate > width)
                    width = candidate;
            }
        }
        
        return width;
    }
    
    function pad(str, n)
    {
        while (str.length < n) {
            str += " ";
        }
        return str;
    }
    
    function resultLine(labelWidth, indent, label, meanWidth1, mean1, stdErr1, meanWidth2, mean2, stdErr2)
    {
        result = pad("", indent);    
        result += label + ": ";
        result = pad(result, labelWidth + 2);
        
        var diffSummary;
        var diffDetail;
        
        if (mean1 != mean1 || mean2 != mean2) {
            diffSummary = "??";
            diffDetail = "    invalid runs detected";
        } else {
            var t = (mean1 - mean2) / (Math.sqrt((stdErr1 * stdErr1) + (stdErr2 * stdErr2)));
            var df = count1 + count2 - 2;
            
            var statisticallySignificant = (Math.abs(t) > tDist(df+1));
            var diff = mean2 - mean1;
            var percentage = 100 * diff / mean1;
            var isFaster = diff < 0;
            var probablySame = (percentage < 0.1) && !statisticallySignificant;
            var ratio = isFaster ? (mean1 / mean2) : (mean2 / mean1);
            var fixedRatio = (ratio < 1.2) ? ratio.toFixed(3).toString() : ((ratio < 10) ? ratio.toFixed(2).toString() : ratio.toFixed(1).toString());
            var formattedRatio = isFaster ? fixedRatio + "x as fast" : "*" + fixedRatio + "x as slow*";

            if (probablySame) {
                diffSummary = "-";
                diffDetail = "";
            } else if (!statisticallySignificant) {
                diffSummary = "??";
                diffDetail =  "    not conclusive: might be " + formattedRatio;
            } else {
                diffSummary = formattedRatio;
                diffDetail = "    significant"; 
            }
        }
            
        return result + pad(diffSummary, 18) + formatMean(meanWidth1, mean1, stdErr1, count1) + "  " + formatMean(meanWidth2, mean2, stdErr2, count2) + diffDetail;
    }
    
    function printOutput()
    {
        var labelWidth = computeLabelWidth();
        var meanWidth1 = computeMeanWidth(mean1, categoryMeans1, testMeansByCategory1);
        var meanWidth2 = computeMeanWidth(mean2, categoryMeans2, testMeansByCategory2);
        
        print("\n");
        var header = "TEST";
        while (header.length < labelWidth)
            header += " ";
        header += "  COMPARISON               FROM                 TO             DETAILS";
        print(header);
        print("");
        print("===============================================================================");
        print("");
        print(resultLine(labelWidth, 0, "** TOTAL **", meanWidth1, mean1, stdErr1, meanWidth2, mean2, stdErr2));
        print("");
        print("===============================================================================");
        
        for (var category in categoryMeans1) {
            print("");
            print(resultLine(labelWidth, 2, category,
                             meanWidth1, categoryMeans1[category], categoryStdErrs1[category],
                             meanWidth2, categoryMeans2[category], categoryStdErrs2[category]));
            for (var test in testMeansByCategory1[category]) {
                var shortName = test.replace(/^[^-]*-/, "");
                print(resultLine(labelWidth, 4, shortName, 
                                 meanWidth1, testMeansByCategory1[category][test], testStdErrsByCategory1[category][test],
                                 meanWidth2, testMeansByCategory2[category][test], testStdErrsByCategory2[category][test]));
            }
        }
    }
    
    initialize();
    
    computeItemTotals(output1, itemTotals1);
    computeItemTotals(output2, itemTotals2);

    total1 = computeTotals(output1, categoryTotals1, testTotalsByCategory1);
    total2 = computeTotals(output2, categoryTotals2, testTotalsByCategory2);

    mean1 = computeMeans(count1, total1, categoryTotals1, categoryMeans1, testTotalsByCategory1, testMeansByCategory1);
    mean2 = computeMeans(count2, total2, categoryTotals2, categoryMeans2, testTotalsByCategory2, testMeansByCategory2);

    stdDev1 = computeStdDevs(mean1, itemTotals1, categoryStdDevs1, categoryMeans1, testStdDevsByCategory1, testMeansByCategory1);
    stdDev2 = computeStdDevs(mean2, itemTotals2, categoryStdDevs2, categoryMeans2, testStdDevsByCategory2, testMeansByCategory2);

    stdErr1 = computeStdErrors(count1, stdDev1, categoryStdErrs1, categoryStdDevs1, testStdErrsByCategory1, testStdDevsByCategory1);
    stdErr2 = computeStdErrors(count2, stdDev2, categoryStdErrs2, categoryStdDevs2, testStdErrsByCategory2, testStdDevsByCategory2);

    printOutput();
}
