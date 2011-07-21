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

checkout.existsAtRevision = function (subversionURL, revision, callback)
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

checkout.rebaseline = function(builderName, testName, failureTypeList, callback)
{
    var extensionList = [];

    $.each(failureTypeList, function(index, failureType) {
        extensionList = extensionList.concat(results.failureTypeToExtensionList(failureType));
    });

    if (!extensionList.length)
        callback();

    var requestTracker = new base.RequestTracker(extensionList.length, callback);

    $.each(extensionList, function(index, extension) {
        $.post('/rebaseline?' + $.param({
            'builder': builderName,
            'test': testName,
            // FIXME: Rename "suffix" query parameter to "extension".
            'suffix': extension
        }), function() {
            requestTracker.requestComplete();
        });
    });
};

checkout.rebaselineAll = function(rebaselineTasks, callback)
{
    var nextIndex = 0;

    function rebaselineNext()
    {
        if (nextIndex >= rebaselineTasks.length) {
            callback();
            return;
        }
        var task = rebaselineTasks[nextIndex++];
        checkout.rebaseline(task.builderName, task.testName, task.failureTypeList, rebaselineNext);
    }

    rebaselineNext();
};

})();
