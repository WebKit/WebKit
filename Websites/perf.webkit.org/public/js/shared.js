
function fileNameFromPlatformAndTest(platformId, testId) {
    return platformId + '-' + testId + '.json';
}

if (typeof module != 'undefined') {
    module.exports.fileNameFromPlatformAndTest = fileNameFromPlatformAndTest;
}
