description("Passing Flags parameter tests. This test checks if passing Flags parameters to DirectoryEntry.getFile does not crash and works as expected.");

var testFileName = '/non_existent_file';
var fileSystem = null;
var expectedCallbacksCount = 0;
var unexpectedCallbacksCount = 0;
var expected = function(e) { expectedCallbacksCount++; };
var unexpected = function(e) { unexpectedCallbacksCount++; };

var testsList = [
    'runObjectTest',
    'cleanupAndRunNext',
    'runJSONTest',
    'runJSONTestWithExclusive',
    'runNullTest',
    'runNonObjectTest'
];
var testCounter = 0;

function runNextTest(v) {
    if (testCounter == testsList.length) {
        debug("Finished running tests.");
        shouldBe('expectedCallbacksCount', '1');
        shouldBe('unexpectedCallbacksCount', '0');
        finishJSTest();
    } else
        this[testsList[testCounter++]]();
}

function errorCallback(error) {
    debug("Error occured during requesting Temporary FileSystem:" + error.code);
    finishJSTest();
}

// Test body functions ----------------------------------------------------
function runNullTest(v) {
    debug("* Passing null as a WebKitFlags parameter.");

    // This should be ok and we treat it as {false, false} Flags.
    fileSystem.root.getFile(testFileName, null, runNextTest, errorCallback);
}

function runNonObjectTest(v) {
    debug("* Passing a number as a WebKitFlags parameter.");

    // This should be ok and we treat it as {false, false} Flags.
    fileSystem.root.getFile(testFileName, 7, runNextTest, errorCallback);
}

function runObjectTest(v) {
    // WebKitFlags no longer has a visible constructor.
    if (window.WebKitFlags)
        throw "There should be no constructor for WebKitFlags!";
    runNextTest();
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

if (window.webkitRequestFileSystem) {
    window.jsTestIsAsync = true;
    webkitRequestFileSystem(window.TEMPORARY, 100, fileSystemCallback, errorCallback);
} else
    debug("This test requires FileSystem API support.");
