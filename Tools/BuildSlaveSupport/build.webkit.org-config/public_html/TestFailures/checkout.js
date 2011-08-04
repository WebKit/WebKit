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

checkout.subversionURLForTest = function(testName)
{
    return kWebKitTrunk + 'LayoutTests/' + testName;
}

checkout.updateExpectations = function(failureInfoList, callback)
{
    net.post('/updateexpectations', JSON.stringify(failureInfoList), function() {
        callback();
    });
};

checkout.optimizeBaselines = function(testName, callback)
{
    net.post('/optimizebaselines?' + $.param({
        'test': testName,
    }), function() {
        callback();
    });
};

checkout.rebaseline = function(failureInfoList, callback)
{
    base.callInSequence(function(failureInfo, callback) {
        var extensionList = Array.prototype.concat.apply([], failureInfo.failureTypeList.map(results.failureTypeToExtensionList));
        base.callInSequence(function(extension, callback) {
            net.post('/rebaseline?' + $.param({
                'builder': failureInfo.builderName,
                'test': failureInfo.testName,
                'extension': extension
            }), function() {
                callback();
            });
        }, extensionList, callback);
    }, failureInfoList, function() {
        var testNameList = base.uniquifyArray(failureInfoList.map(function(failureInfo) { return failureInfo.testName; }));
        base.callInSequence(checkout.optimizeBaselines, testNameList, callback);
    });
};

})();
