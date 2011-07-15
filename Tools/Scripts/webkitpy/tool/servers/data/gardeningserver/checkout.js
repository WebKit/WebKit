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

    var requestsInFlight = extensionList.length;

    if (!requestsInFlight)
        callback();

    $.each(extensionList, function(index, extension) {
        $.post('/rebaseline?' + $.param({
            'builder': builderName,
            'test': testName,
            // FIXME: Rename "suffix" query parameter to "extension".
            'suffix': extension
        }), function() {
            --requestsInFlight;
            if (!requestsInFlight)
                callback();
        });
    });
};

})();
