if (this.importScripts) {
    importScripts('fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('file-writer-utils.js');
}

description("Test of extending a file using truncate().");

function onTestSucceeded()
{
    testPassed("Truncate extension verified.");
    cleanUp();
}

function truncateToExtend(fileEntry, fileWriter, contents, onSuccess)
{
    var extension = 5;
    var lengthChecker = function() {
        verifyFileLength(fileEntry, contents.length + extension, onSuccess);
    };
    var extensionChecker = function() {
        verifyByteRangeIsZeroes(fileEntry, contents.length, extension, lengthChecker);
    };
    fileWriter.onwrite = function() {
        verifyByteRangeAsString(fileEntry, 0, contents, extensionChecker);
    };
    fileWriter.truncate(contents.length + extension);
}

function runTest(fileEntry, fileWriter) {
    var contents = "Lorem ipsum";
    writeString(fileEntry, fileWriter, 0, contents,
                function() {
                    truncateToExtend(fileEntry, fileWriter, contents, onTestSucceeded);
                });
}

var jsTestIsAsync = true;
setupAndRunTest(1024, 'file-writer-truncate-extend', runTest);
