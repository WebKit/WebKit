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

checkout.rebaseline = function(builderName, testName, failureTypeList, callback)
{
    var extensionList = Array.prototype.concat.apply([], failureTypeList.map(results.failureTypeToExtensionList));

    base.callInSequence(function(extension, callback) {
        $.post('/rebaseline?' + $.param({
            'builder': builderName,
            'test': testName,
            'extension': extension
        }), function() {
            callback();
        });
    }, extensionList, callback);
};

checkout.rebaselineAll = function(rebaselineTasks, callback)
{
    base.callInSequence(function(task, callback) {
        checkout.rebaseline(task.builderName, task.testName, task.failureTypeList, callback);
    }, rebaselineTasks, function() {
        var testNameList = base.uniquifyArray(rebaselineTasks.map(function(task) { return task.testName; }));
        base.callInSequence(checkout.optimizeBaselines, testNameList, callback);
    });
};

})();
