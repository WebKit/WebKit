if (this.importScripts) {
    importScripts('fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('file-writer-utils.js');
}

description("Test that a blob won't get garbage-collected while being written out by a FileWriter.");

var fileEntry;

function onTestSuccess() {
    testPassed("Successfully wrote blob.");
    cleanUp();
}

function tenXBlob(blob) {
    var bb = [];
    for (var i = 0; i < 10; ++i) {
        bb.push(blob);
    }
    return new Blob(bb);
}

function startWrite(writer) {
    // Let's make it about a megabyte.
    var blob = tenXBlob(new Blob(["lorem ipsum"]));
    blob = tenXBlob(blob);
    blob = tenXBlob(blob);
    blob = tenXBlob(blob);
    blob = tenXBlob(blob);
    writer.onerror = onError;
    writer.onwriteend = onTestSuccess;
    writer.write(blob);
}

function runTest(unusedFileEntry, fileWriter) {
    startWrite(fileWriter);
    gc();
}
var jsTestIsAsync = true;
setupAndRunTest(2*1024*1024, 'file-writer-gc-blob', runTest);
