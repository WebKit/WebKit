if (this.importScripts) {
    importScripts('fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('file-writer-utils.js');
}

description("Test that FileWriter handles abort properly.");

var sawWriteStart;
var sawAbort;
var sawWriteEnd;
var writer;

function tenXBlob(blob) {
    var bb = new WebKitBlobBuilder();
    for (var i = 0; i < 10; ++i) {
        bb.append(blob);
    }
    return bb.getBlob();
}

function onWriteStart(e) {
    assert(writer);
    assert(writer.readyState == writer.WRITING);
    assert(e.type == "writestart");
    assert(!sawWriteStart);
    assert(!sawWriteEnd);
    assert(!e.loaded);
    sawWriteStart = true;
    testPassed("Calling abort");
    writer.abort();
}

// We should always abort before completion.
function onWrite(e) {
    testFailed("In onWrite.");
}

function onAbort(e) {
    assert(writer.readyState == writer.DONE);
    assert(writer.error.code == writer.error.ABORT_ERR);
    assert(sawWriteStart);
    assert(!sawWriteEnd);
    assert(!sawAbort);
    assert(e.type == "abort");
    sawAbort = true;
    testPassed("Saw abort");
}

function onWriteEnd(e) {
    assert(writer.readyState == writer.DONE);
    assert(writer.error.code == writer.error.ABORT_ERR);
    assert(sawWriteStart);
    assert(sawAbort);
    assert(!sawWriteEnd);
    assert(e.type == "writeend");
    sawWriteEnd = true;
    testPassed("Saw writeend.");
    writer.abort();  // Verify that this does nothing in readyState DONE.
    cleanUp();
}

function startWrite(fileWriter) {
    // Let's make it about a megabyte.
    var bb = new WebKitBlobBuilder();
    bb.append("lorem ipsum");
    var blob = tenXBlob(bb.getBlob());
    blob = tenXBlob(blob);
    blob = tenXBlob(blob);
    blob = tenXBlob(blob);
    blob = tenXBlob(blob);
    writer = fileWriter;
    fileWriter.onerror = onError;
    fileWriter.onabort = onAbort;
    fileWriter.onwritestart = onWriteStart;
    fileWriter.onwrite = onWrite;
    fileWriter.onwriteend = onWriteEnd;
    fileWriter.abort();  // Verify that this does nothing in readyState INIT.
    fileWriter.write(blob);
}

function runTest(unusedFileEntry, fileWriter) {
    startWrite(fileWriter);
}
var jsTestIsAsync = true;
setupAndRunTest(2*1024*1024, 'file-writer-abort', runTest);
