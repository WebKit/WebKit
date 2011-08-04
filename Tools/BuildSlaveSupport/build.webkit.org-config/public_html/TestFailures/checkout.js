var checkout = checkout || {};

(function() {

var kWebKitTrunk = 'http://svn.webkit.org/repository/webkit/trunk/';

function subversionURLAtRevision(subversionURL, revision)
{
    return subversionURL + '?r=' + revision;
}

checkout.subversionURLForTest = function(testName)
{
    return kWebKitTrunk + 'LayoutTests/' + testName;
}

checkout.existsAtRevision = function(subversionURL, revision, callback)
{
    net.ajax({
        method: 'HEAD',
        url: subversionURLAtRevision(subversionURL, revision), 
        success: function() {
            callback(true);
        },
        error: function() {
            callback(false);
        }
    });
};

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
