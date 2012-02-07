if (this.importScripts) {
    importScripts('fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('file-writer-utils.js');
}

description("Test that FileWriter produces proper progress events.");

var fileEntry;
var sawWriteStart;
var sawWrite;
var sawWriteEnd;
var sawProgress;
var writer;
var lastProgress = 0;
var toBeWritten;

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
    assert(!sawProgress);
    assert(!sawWrite);
    assert(!sawWriteEnd);
    assert(!e.loaded);
    assert(e.total == toBeWritten);
    sawWriteStart = true;
}

function onProgress(e) {
    assert(writer.readyState == writer.WRITING);
    assert(sawWriteStart);
    assert(!sawWrite);
    assert(!sawWriteEnd);
    assert(e.type == "progress");
    assert(e.loaded <= e.total);
    assert(lastProgress < e.loaded);
    assert(e.total == toBeWritten);
    lastProgress = e.loaded;
    sawProgress = true;
}

function onWrite(e) {
    assert(writer.readyState == writer.DONE);
    assert(sawWriteStart);
    assert(sawProgress);
    assert(lastProgress == e.total);
    assert(!sawWrite);
    assert(!sawWriteEnd);
    assert(e.type == "write");
    assert(e.loaded == e.total);
    assert(e.total == toBeWritten);
    sawWrite = true;
}

function onWriteEnd(e) {
    assert(writer.readyState == writer.DONE);
    assert(sawWriteStart);
    assert(sawProgress);
    assert(sawWrite);
    assert(!sawWriteEnd);
    assert(e.type == "writeend");
    assert(e.loaded == e.total);
    assert(e.total == toBeWritten);
    sawWriteEnd = true;
    testPassed("Saw all the right events.");
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
    toBeWritten = blob.size;
    writer = fileWriter;
    fileWriter.onerror = onError;
    fileWriter.onwritestart = onWriteStart;
    fileWriter.onprogress = onProgress;
    fileWriter.onwrite = onWrite;
    fileWriter.onwriteend = onWriteEnd;
    fileWriter.write(blob);
}

function runTest(unusedFileEntry, fileWriter) {
    startWrite(fileWriter);
}
var jsTestIsAsync = true;
setupAndRunTest(2*1024*1024, 'file-writer-events', runTest);
