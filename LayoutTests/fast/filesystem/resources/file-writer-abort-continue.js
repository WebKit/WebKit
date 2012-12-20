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
var truncateLength;
var blobSize = 1100000;
var currentTest = 0;
var blob = getBlob();

// fileSystemOverhead is an estimate of extra size needed to save a full file
// system, it need to be large enough to avoid test failure due to file system
// quota limit.
var fileSystemOverhead = blobSize * 5 / 1000 + 1024;
var fileSystemSize = blobSize * 2 + fileSystemOverhead;

var methodSet = [
  {  // Setup method set that writes, then aborts that write before completion.
    action : startWrite,
    verifyLength : 0,
    onwritestart : abortWrite,
    onprogress : nop,
    onwrite : onError,
    onabort : logAbort,
    onwriteend : checkLengthAndStartNextTest
  },
  {  // Method set that does a complete write.
    action : startWrite,
    verifyLength : blobSize,
    onwritestart : nop,
    onprogress : nop,
    onwrite : nop,
    onabort : onError,
    onwriteend : checkLengthAndStartNextTest
  },
  { // Method set that does a complete truncate, just to clean up.
    action : startTruncate,
    truncateLength : 0,
    verifyLength : 0,
    onwritestart : nop,
    onprogress : nop,
    onwrite : nop,
    onabort : onError,
    onwriteend : checkLengthAndStartNextTest
  },
  {  // Setup method set that writes, then aborts that write just at completion.
    action : startWrite,
    verifyLength : blobSize,
    onwritestart : nop,
    onprogress : abortOnComplete,
    onwrite : nop,
    onabort : logAbort,
    onwriteend : checkLengthAndStartNextTest
  },
  {  // Method set that does a complete write.
    action : startWrite,
    verifyLength : blobSize * 2, // Add in leftovers from previous method.
    onwritestart : nop,
    onprogress : nop,
    onwrite : nop,
    onabort : onError,
    onwriteend : checkLengthAndStartNextTest
  },
  { // Method set that does a complete truncate, just to clean up.
    action : startTruncate,
    truncateLength : 0,
    verifyLength : 0,
    onwritestart : nop,
    onprogress : nop,
    onwrite : nop,
    onabort : onError,
    onwriteend : checkLengthAndStartNextTest
  },
  {  // Setup method set that writes, then aborts that write as it starts.
    action : startWrite,
    verifyLength : 0,
    onwritestart : abortWrite,
    onprogress : nop,
    onwrite : onError,
    onabort : logAbort,
    onwriteend : checkLengthAndStartNextTest
  },
  { // Method set that does a complete truncate.
    action : startTruncate,
    truncateLength : 7,
    verifyLength : 7,
    onwritestart : nop,
    onprogress : nop,
    onwrite : nop,
    onabort : onError,
    onwriteend : checkLengthAndStartNextTest
  }
];

function nop() {
}

function tenXBlob(blob) {
    var bb = [];
    for (var i = 0; i < 10; ++i) {
        bb.push(blob);
    }
    return new Blob(bb);
}

function getBlob() {
    // Let's make it about a megabyte.
    var blob = tenXBlob(new Blob(["lorem ipsum"]));
    blob = tenXBlob(blob);
    blob = tenXBlob(blob);
    blob = tenXBlob(blob);
    blob = tenXBlob(blob);
    var size = blob.size;
    shouldBe("" + size, "blobSize");
    return blob;
}

function abortWrite(e) {
    testPassed("Calling abort");
    writer.abort();
}

function abortOnComplete(e) {
    if (e.loaded == e.total) {
        testPassed("Calling abort at the end of the write");
        writer.abort();
    }
}

function logAbort(e) {
    testPassed("Saw abort");
}

function checkLengthAndStartNextTest(e) {
    shouldBe("writer.length", "" + expectedLength);
    testPassed("Saw writeend " + currentTest + ".");
    ++currentTest;
    if (currentTest < methodSet.length)
        runTest();
    else {
        testPassed("All tests complete.");
        cleanUp();
    }
}

function startWrite() {
    testPassed("Calling write.");
    writer.write(blob);
}

function startTruncate() {
    testPassed("Calling truncate.");
    writer.truncate(truncateLength);
}

function setupWriter(methodSetIndex, writer) {
    writer.onerror = onError;

    var methods = methodSet[methodSetIndex];
    writer.onabort = methods.onabort;
    writer.onwritestart = methods.onwritestart;
    writer.onprogress = methods.onprogress;
    writer.onwrite = methods.onwrite;
    writer.onwriteend = methods.onwriteend;
    expectedLength = methods.verifyLength;
    truncateLength = methods.truncateLength;
    methods.action();
}

function runTest() {
    setupWriter(currentTest, writer);
}

var jsTestIsAsync = true;
setupAndRunTest(fileSystemSize, 'file-writer-abort',
                function (fileEntry, fileWriter) {
                    fileEntryForCleanup = fileEntry;
                    writer = fileWriter;
                    runTest();
                });
