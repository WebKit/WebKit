if (this.importScripts) {
    importScripts('../resources/fs-worker-common.js');
    importScripts('../resources/fs-test-util.js');
}

description("Tests using resolveLocalFileSystemURL to obtain an Entry from a URL");

var testFileName = '/testFile';
var fileSystem = null;
var expectedAndRunNext = function() { testPassed(""); cleanupAndRunNext(); };

var expectedPath = null;
var actualPath = null;

function errorCallback(error) {
    if (error && error.code)
        debug("Error occurred: " + error.code);
    testFailed("Bailing out early");
    finishJSTest();
}

function createTestFile(callback) {
    fileSystem.root.getFile(testFileName, {create:true}, callback, errorCallback);
}

function assertPathsMatch(expected, actual) {
    expectedPath = expected;
    actualPath = actual;
    shouldBe("expectedPath", "actualPath");
}

var isDirectory = false;
function assertIsDirectory(entry) {
    isDirectory = entry.isDirectory;
    shouldBeTrue("isDirectory");
}

var isFile = false;
function assertIsFile(entry) {
    isFile = entry.isFile;
    shouldBeTrue("isFile");
}

// Test body functions ----------------------------------------------------

function runBasicTest() {
    debug("* Resolving a generated URL.");
    createTestFile(function(entry) {
        resolveLocalFileSystemURL(entry.toURL(), function(e) {
            assertPathsMatch(entry.fullPath, e.fullPath);
            assertIsFile(e);
            cleanupAndRunNext();
        }, errorCallback);
    });
}

function runHandmadeURL() {
    debug("* Resolving test file by hand");
    createTestFile(function(entry) {
        resolveLocalFileSystemURL("filesystem:http://127.0.0.1:8000/temporary/testFile", function(e) {
            assertPathsMatch(testFileName, e.fullPath);
            assertIsFile(e);
            cleanupAndRunNext();
        }, errorCallback);
    });
}

function runBogusURL() {
    debug("* Resolving a completely bogus URL.");
    resolveLocalFileSystemURL("foo", errorCallback, expectedAndRunNext);
}

function runWrongProtocol() {
    debug("* Resolving a URL with the wrong protocol");
    resolveLocalFileSystemURL("http://127.0.0.1:8000/foo/bar/baz", errorCallback, expectedAndRunNext);
}

function runNotEnoughSlashes() {
    debug("* Resolving a URL with no slash between type and file");
    createTestFile(function(entry) {
        resolveLocalFileSystemURL("filesystem:http://127.0.0.1:8000/temporarytestFile", errorCallback, expectedAndRunNext);
    });
}

function runNotEnoughSlashes2() {
    debug("* Resolving a URL with no slash between protocol and type");
    createTestFile(function(entry) {
        resolveLocalFileSystemURL("filesystem:http://127.0.0.1:8000temporary/testFile", errorCallback, expectedAndRunNext);
    });
}

function runUseBackSlashes() {
    debug("* Resolve a path using backslashes");
    fileSystem.root.getDirectory("foo", {create:true}, function(entry) {
        entry.getFile("testFile", {create:true}, function(f) {
            resolveLocalFileSystemURL("filesystem:http://127.0.0.1:8000/temporary/foo\\testFile", function(e) {
                assertPathsMatch("/foo/testFile", e.fullPath);
                assertIsFile(e);
                cleanupAndRunNext();
            }, errorCallback);
        }, errorCallback);
    }, errorCallback);
}

function runDirectory() {
    debug("* Resolve a directory");
    fileSystem.root.getDirectory("foo", {create:true}, function(entry) {
        resolveLocalFileSystemURL("filesystem:http://127.0.0.1:8000/temporary/foo", function(e) {
            assertPathsMatch("/foo", e.fullPath);
            assertIsDirectory(e);
            cleanupAndRunNext();
        }, errorCallback);
    }, errorCallback);
}

function runWithTrailingSlash() {
    debug("* Resolve a path using a trailing slash");
    fileSystem.root.getDirectory("foo", {create:true}, function(entry) {
        resolveLocalFileSystemURL("filesystem:http://127.0.0.1:8000/temporary/foo/", function(e) {
            assertPathsMatch("/foo", e.fullPath);
            assertIsDirectory(e);
            cleanupAndRunNext();
        }, errorCallback);
    }, errorCallback);
}

function runPersistentTest() {
    debug("* Resolving a persistent URL.");
    requestFileSystem(PERSISTENT, 100, function(fs) {
        resolveLocalFileSystemURL(fs.root.toURL(), function(entry) {
            assertPathsMatch("/", entry.fullPath);
            assertIsDirectory(entry);
            cleanupAndRunNext();
        }, errorCallback);
    });
}

// End of test body functions ---------------------------------------------

var testsList = [
    runBasicTest,
    runHandmadeURL,
    runBogusURL,
    runWrongProtocol,
    runNotEnoughSlashes,
    runNotEnoughSlashes2,
    runUseBackSlashes,
    runDirectory,
    runWithTrailingSlash,
    runPersistentTest,
];
var testCounter = 0;

function runNextTest() {
    if (testCounter == testsList.length) {
        debug("Finished running tests.");
        finishJSTest();
    } else
        testsList[testCounter++]();
}

function cleanupAndRunNext() {
    removeAllInDirectory(fileSystem.root, runNextTest, runNextTest);
}

function fileSystemCallback(fs) {
    fileSystem = fs;
    cleanupAndRunNext();
}

if (this.requestFileSystem) {
    jsTestIsAsync = true;
    requestFileSystem(this.TEMPORARY, 100, fileSystemCallback, errorCallback);
} else
    debug("This test requires FileSystem API support.");

var successfullyParsed = true;
