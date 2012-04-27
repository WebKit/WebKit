if (this.importScripts) {
    importScripts('fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('file-writer-utils.js');
}

description("Test that FileWriter can continue immediately after an abort.");

var sawWriteStart;
var sawAbort;
var sawWriteEnd;
var writer;
var expectedLength;
var truncateLength = 7;
var blobSize = 1100000;

var methodSet = [
  {  // Setup method set that writes, then aborts that write before completion.
    action : startWrite,
    verifyLength : 0,
    onwritestart : abortWrite,
    onwrite : onError,
    onabort : logAbort,
    onwriteend : checkLengthAndCallFollowOnAction
  },
  {  // Method set that does a complete write.
    action : startWrite,
    verifyLength : blobSize,
    onwritestart : nop,
    onwrite : nop,
    onabort : onError,
    onwriteend : checkLengthAndStartNextTest
  },
  {  // Setup method set that writes, then aborts that write before completion.
    action : startWrite,
    verifyLength : blobSize,  // Left over from the previous test.
    onwritestart : abortWrite,
    onwrite : onError,
    onabort : logAbort,
    onwriteend : checkLengthAndCallFollowOnAction
  },
  { // Method set that does a complete truncate.
    action : startTruncate,
    verifyLength : truncateLength,
    onwritestart : nop,
    onwrite : nop,
    onabort : onError,
    onwriteend : checkLengthAndCompleteTest
  }];

function nop() {
}

function tenXBlob(blob) {
    var bb = new WebKitBlobBuilder();
    for (var i = 0; i < 10; ++i) {
        bb.append(blob);
    }
    return bb.getBlob();
}

// These methods set up a write, abort it as soon as it starts, then initiate
// the follow on action.
function abortWrite(e) {
    testPassed("Calling abort");
    writer.abort();
}

function logAbort(e) {
    testPassed("Saw abort");
}

function checkLengthAndCallFollowOnAction(e) {
    testPassed("Saw writeend 0.");
    shouldBe("writer.length", "" + expectedLength);
    doFollowOnAction();
}

// For the second method set, verify completion and move on to the next test.
function checkLengthAndStartNextTest(e) {
    shouldBe("writer.length", "" + expectedLength);
    testPassed("Saw writeend 1.");
    runTest(2, 3);
}

function checkLengthAndCompleteTest(e) {
    shouldBe("writer.length", "" + expectedLength);
    testPassed("All tests complete.");
    cleanUp();
}

function startWrite() {
    // Let's make it about a megabyte.
    var bb = new WebKitBlobBuilder();
    bb.append("lorem ipsum");
    var blob = tenXBlob(bb.getBlob());
    blob = tenXBlob(blob);
    blob = tenXBlob(blob);
    blob = tenXBlob(blob);
    blob = tenXBlob(blob);
    var size = blob.size;
    shouldBe("" + size, "blobSize");
    writer.write(blob);
}

function startTruncate() {
    writer.truncate(truncateLength);
}

function setupWriter(methodSetIndex, writer) {
    writer.onerror = onError;

    var methods = methodSet[methodSetIndex];
    writer.onabort = methods.onabort;
    writer.onwritestart = methods.onwritestart;
    writer.onwrite = methods.onwrite;
    writer.onwriteend = methods.onwriteend;
    expectedLength = methods.verifyLength;
    methods.action();
}

function runTest(initIndex, testIndex) {
    followOnAction = testIndex;
    setupWriter(initIndex, writer);
}

function doFollowOnAction() {
    shouldBeTrue("followOnAction == 1 || followOnAction == 3");
    setupWriter(followOnAction, writer);
}

var jsTestIsAsync = true;
setupAndRunTest(2*1024*1024, 'file-writer-abort',
                function (fileEntry, fileWriter) {
                    fileEntryForCleanup = fileEntry;
                    writer = fileWriter;
                    runTest(0, 1);
                });
