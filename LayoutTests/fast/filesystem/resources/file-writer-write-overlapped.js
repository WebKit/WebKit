if (this.importScripts) {
    importScripts('fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('file-writer-utils.js');
}

description("Test using FileWriter.seek to write overlapping existing data in a file.");

var fileEntry;
var fileWriter;
var testData = "Lorem ipsum";
var seekBackwardOffset = -4;
var seekForwardOffset = 4;

function onTestSucceeded()
{
    testPassed("All writes verified.");
    cleanUp();
}

function validateNonextendingWriteEnd() {
    verifyByteRangeAsString(fileEntry, testData.length + seekForwardOffset, testData.slice(seekForwardOffset), onTestSucceeded);
}

function validateNonextendingWriteStart() {
    testPassed("Nonextending write validated.");
    assert(fileWriter.length == testData.length * 2);
    verifyByteRangeAsString(fileEntry, 0, testData.slice(0, seekForwardOffset), validateNonextendingWriteEnd);
}

function testNonextendingWrite() {
    writeString(fileEntry, fileWriter, seekForwardOffset, testData, validateNonextendingWriteStart);
}

function setupNonextendingWrite() {
    testPassed("Positive seek validated.");
    setFileContents(fileEntry, fileWriter, testData + testData, testNonextendingWrite);
}

function validateSeekForward() {
    assert(fileWriter.length == testData.length + seekForwardOffset);
    verifyByteRangeAsString(fileEntry, 0, testData.slice(0, seekForwardOffset), setupNonextendingWrite);
}

function testSeekForward() {
    writeString(fileEntry, fileWriter, seekForwardOffset, testData, validateSeekForward);
}

function setupSeekForward() {
    testPassed("Negative seek validated.");
    setFileContents(fileEntry, fileWriter, testData, testSeekForward);
}

function validateSeekBackward() {
    assert(fileWriter.length == 2 * testData.length + seekBackwardOffset);
    verifyByteRangeAsString(fileEntry, 0, testData.slice(0, testData.length + seekBackwardOffset), setupSeekForward);
}

function testSeekBackward() {
    writeString(fileEntry, fileWriter, seekBackwardOffset, testData, validateSeekBackward);
}

// Each test has 3 phases: setup, execution, and validation.  In the setup phase, we make sure that there's a FileWriter pointing at a file with appropriate
// data for the test we want to run.  In the execution phase, we do a seek and a write, and the utility function that does those also validates that the data
// actually got written.  So in the validation phase, we only have to check that the rest of the file didn't get corrupted while we were writing our bit.
function runTest(fileEntryIn, fileWriterIn) {
    fileEntry = fileEntryIn;
    fileWriter = fileWriterIn;
    setFileContents(fileEntry, fileWriter, testData, testSeekBackward);
}

var jsTestIsAsync = true;
setupAndRunTest(1024, 'file-writer-truncate-extend', runTest);
