description("Passing Flags parameter tests. This test checks if passing Flags parameters (in Object format or in JSON format) to DirectoryEntry.getFile does not crash and works as expected.");

var testFileName = '/non_existent_file';
var fileSystem = null;
var expectedCallbacksCount = 0;
var unexpectedCallbacksCount = 0;
var expected = function(e) { expectedCallbacksCount++; };
var unexpected = function(e) { unexpectedCallbacksCount++; };

var testsList = [
    'runObjectTest',
    'runObjectTestWithExclusive',
    'cleanupAndRunNext',
    'runJSONTest',
    'runJSONTestWithExclusive'
];
var testCounter = 0;

function endTest() {
    debug("Finished running tests.");
    shouldBe('expectedCallbacksCount', '3');
    shouldBe('unexpectedCallbacksCount', '0');
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

function runNextTest(v) {
    if (testCounter == testsList.length)
        endTest();
    else
        this[testsList[testCounter++]]();
}

function errorCallback(error) {
    debug("Error occured during requesting Temporary FileSystem:" + error.code);

    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

// Test body functions ----------------------------------------------------
function runObjectTest(v) {
    debug("* Passing a Flags object.");
    var flags = new Flags();
    flags.create = false;

    fileSystem.root.getFile(testFileName, flags, unexpected, expected);

    debug("* Recycling the same Flags object.");

    fileSystem.root.getFile(testFileName, flags, unexpected, expected);

    flags.create = true;
    fileSystem.root.getFile(testFileName, flags, runNextTest, errorCallback);
}

function runObjectTestWithExclusive(v) {
    debug("* Passing a Flags object (with exclusive=true).");
    var flags = new Flags;
    flags.create = true;
    flags.exclusive = true;

    // This should fail.
    fileSystem.root.getFile(testFileName, flags, errorCallback, runNextTest);
}

function runJSONTest(v) {
    debug("* Passing JSON Flags object.");

    fileSystem.root.getFile(testFileName, {create:false}, unexpected, expected);
    fileSystem.root.getFile(testFileName, {create:true}, runNextTest, errorCallback);
}

function runJSONTestWithExclusive(v) {
    debug("* Passing JSON Flags object (with exclusive=true).");

    // This should fail.
    fileSystem.root.getFile(testFileName, {create:true, exclusive:true}, errorCallback, runNextTest);
}

// End of test body functions ---------------------------------------------

function cleanupAndRunNext(v) {
    fileSystem.root.getFile(testFileName, {create:false}, function(entry) {
        // Remove the entry before start testing.
        entry.remove(runNextTest, errorCallback);
    }, runNextTest);
}

function fileSystemCallback(fs) {
    fileSystem = fs;
    cleanupAndRunNext();
}

if (window.requestFileSystem) {
    if (window.layoutTestController)
        layoutTestController.waitUntilDone();

    requestFileSystem(window.TEMPORARY, 100, fileSystemCallback, errorCallback);
} else
    debug("This test requires FileSystem API support.");

var successfullyParsed = true;
