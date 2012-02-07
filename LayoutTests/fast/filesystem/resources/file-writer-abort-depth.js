if (this.importScripts) {
    importScripts('fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('file-writer-utils.js');
}

description("Test that FileWriter defends against infinite recursion via abort.");

var sawWriteStart;
var sawAbort;
var sawWriteEnd;
var writer;
var bb = new WebKitBlobBuilder();
bb.append("lorem ipsum");
var blob = bb.getBlob();
var recursionDepth = 0;
var method;
var testsRun = 0;

function onWriteStart(e) {
    testPassed("Calling abort");
    ++recursionDepth;
    writer.abort();
}

// We should always abort before completion.
function onWrite(e) {
    testFailed("In onWrite.");
}

function onAbort(e) {
    testPassed("Saw abort");
    try {
      method();
    } catch (ex) {
      assert(ex.code == 2);  // Security error
      testPassed("Saw security error");
    }
}

function onWriteEnd(e) {
    --recursionDepth;
    testPassed("Saw writeend.");
    if (!recursionDepth) {
        ++testsRun;
        if (testsRun == 1) {
            method = function() {
                testPassed("Calling truncate.");
                writer.truncate(7);
            }
            setTimeout(method, 0);  // Invoke from the top level, so that we're not already in a handler.
        } else {
            cleanUp();
        }
    }
}

function runTest(unusedFileEntry, fileWriter) {
    writer = fileWriter;
    method = function () {
        testPassed("Calling write.");
        writer.write(blob);
    }
    fileWriter.onerror = onError;
    fileWriter.onabort = onAbort;
    fileWriter.onwritestart = onWriteStart;
    fileWriter.onwrite = onWrite;
    fileWriter.onwriteend = onWriteEnd;
    method();
}

var jsTestIsAsync = true;
setupAndRunTest(2*1024*1024, 'file-writer-abort-depth', runTest);
