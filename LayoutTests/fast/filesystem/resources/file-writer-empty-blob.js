if (this.importScripts) {
    importScripts('fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('file-writer-utils.js');
}

description("Test that FileWriter works without crash when trying to write an empty blob.");

var fileEntry;

function onTestSuccess() {
    testPassed("Successfully wrote blob.");
    cleanUp();
}

function startWrite(writer) {
    var blob = new Blob([""]);
    writer.onerror = onError;
    writer.onwriteend = onTestSuccess;
    writer.write(blob);
}

function runTest(unusedFileEntry, fileWriter) {
    startWrite(fileWriter);
    gc();
}
var jsTestIsAsync = true;
setupAndRunTest(2 * 1024 * 1024, 'file-writer-empty-blob', runTest);

