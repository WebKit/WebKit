/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

var checkout = checkout || {};

(function() {

var kWebKitTrunk = 'http://svn.webkit.org/repository/webkit/trunk/';

var g_haveSeenCheckoutAvailable = false;

function callIfCheckoutAvailable(callback, checkoutUnavailable)
{
    if (g_haveSeenCheckoutAvailable) {
        callback();
        return;
    }
    checkout.isAvailable(function(isAvailable) {
        if (isAvailable) {
            g_haveSeenCheckoutAvailable = true;
            callback();
            return;
        }
        if (checkoutUnavailable)
            checkoutUnavailable();
    });
}

checkout.subversionURLForTest = function(testName)
{
    return kWebKitTrunk + 'LayoutTests/' + testName;
};

checkout.isAvailable = function(callback)
{
    net.ajax({
        url: config.kLocalServerURL + '/ping',
        success: function() {
            callback(true);
        },
        error: function() {
            callback(false);
        },
    });
};

checkout.updateExpectations = function(failureInfoList, callback, checkoutUnavailable)
{
    callIfCheckoutAvailable(function() {
        net.post(config.kLocalServerURL + '/updateexpectations', JSON.stringify(failureInfoList), function() {
            callback();
        });
    }, checkoutUnavailable);
};

checkout.optimizeBaselines = function(testName, suffixes, callback, checkoutUnavailable)
{
    callIfCheckoutAvailable(function() {
        net.post(config.kLocalServerURL + '/optimizebaselines?' + $.param({
            'test': testName,
            'suffixes': suffixes,
        }), function() {
            callback();
        });
    }, checkoutUnavailable);
};

checkout.rollout = function(revision, reason, callback, checkoutUnavailable)
{
    callIfCheckoutAvailable(function() {
        net.post(config.kLocalServerURL + '/rollout?' + $.param({
            'revision': revision,
            'reason': reason
        }), function() {
            callback();
        });
    }, checkoutUnavailable);
};

checkout.rebaseline = function(failureInfoList, callback, progressCallback, checkoutUnavailable)
{
    callIfCheckoutAvailable(function() {
        base.callInSequence(function(failureInfo, callback) {
            var suffixes = base.uniquifyArray(base.flattenArray(failureInfo.failureInfoList.map(results.failureTypeToExtensionList))).join(',');

            net.post(config.kLocalServerURL + '/rebaseline?' + $.param({
                'builder': failureInfo.builderName,
                'test': failureInfo.testName,
                'suffixes': suffixes,
            }), function() {
                if (progressCallback)
                    progressCallback(failureInfo);
                callback();
            });
        }, failureInfoList, function() {
            var testNameList = base.uniquifyArray(failureInfoList.map(function(failureInfo) { return failureInfo.testName; }));
            var suffixes = base.uniquifyArray(base.flattenArray(
                failureInfoList.map(function(failureInfo) {
                    return base.flattenArray(failureInfo.failureInfoList.map(results.failureTypeToExtensionList))
                }))).join(',');
            base.callInSequence(function(testName, callback) { checkout.optimizeBaselines(testName, suffixes, callback)}, testNameList, callback);
        });
    }, checkoutUnavailable);
};

})();
