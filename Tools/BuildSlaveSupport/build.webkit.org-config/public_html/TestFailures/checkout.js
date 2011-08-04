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
    $.ajax({
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

checkout.optimizeBaselines = function(testName, callback)
{
    $.post('/optimizebaselines?' + $.param({
        'test': testName,
    }), function() {
        callback();
    });
};

checkout.rebaseline = function(rebaselineTasks, callback)
{
    base.callInSequence(function(task, callback) {
        var extensionList = Array.prototype.concat.apply([], task.failureTypeList.map(results.failureTypeToExtensionList));
        base.callInSequence(function(extension, callback) {
            $.post('/rebaseline?' + $.param({
                'builder': task.builderName,
                'test': task.testName,
                'extension': extension
            }), function() {
                callback();
            });
        }, extensionList, callback);
    }, rebaselineTasks, function() {
        var testNameList = base.uniquifyArray(rebaselineTasks.map(function(task) { return task.testName; }));
        base.callInSequence(checkout.optimizeBaselines, testNameList, callback);
    });
};

})();
