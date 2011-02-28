description("Tests using resolveLocalFileSystemURI to obtain an Entry from a URI");

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
    debug("* Resolving a generated URI.");
    createTestFile(function(entry) {
        resolveLocalFileSystemURI(entry.toURI(), function(e) {
            assertPathsMatch(entry.fullPath, e.fullPath);
            assertIsFile(e);
            cleanupAndRunNext();
        }, errorCallback);
    });
}

function runHandmadeURI() {
    debug("* Resolving test file by hand");
    createTestFile(function(entry) {
        resolveLocalFileSystemURI("filesystem:file:///temporary/testFile", function(e) {
            assertPathsMatch(testFileName, e.fullPath);
            assertIsFile(e);
            cleanupAndRunNext();
        }, errorCallback);
    });
}

function runBogusURI() {
    debug("* Resolving a completely bogus URI.");
    resolveLocalFileSystemURI("foo", errorCallback, expectedAndRunNext);
}

function runWrongProtocol() {
    debug("* Resolving a URI with the wrong protocol");
    resolveLocalFileSystemURI("file:///foo/bar/baz", errorCallback, expectedAndRunNext);
}

function runNotEnoughSlashes() {
    debug("* Resolving a URI with no slash between type and file");
    createTestFile(function(entry) {
        resolveLocalFileSystemURI("filesystem:file:///temporarytestFile", errorCallback, expectedAndRunNext);
    });
}

function runNotEnoughSlashes2() {
    debug("* Resolving a URI with no slash between protocol and type");
    createTestFile(function(entry) {
        resolveLocalFileSystemURI("filesystem:file://temporary/testFile", errorCallback, expectedAndRunNext);
    });
}

function runUseBackSlashes() {
    debug("* Resolve a path using backslashes");
    fileSystem.root.getDirectory("foo", {create:true}, function(entry) {
        entry.getFile("testFile", {create:true}, function(f) {
            resolveLocalFileSystemURI("filesystem:file:///temporary/foo\\testFile", function(e) {
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
        resolveLocalFileSystemURI("filesystem:file:///temporary/foo", function(e) {
            assertPathsMatch("/foo", e.fullPath);
            assertIsDirectory(e);
            cleanupAndRunNext();
        }, errorCallback);
    }, errorCallback);
}

function runWithTrailingSlash() {
    debug("* Resolve a path using a trailing slash");
    fileSystem.root.getDirectory("foo", {create:true}, function(entry) {
        resolveLocalFileSystemURI("filesystem:file:///temporary/foo/", function(e) {
            assertPathsMatch("/foo", e.fullPath);
            assertIsDirectory(e);
            cleanupAndRunNext();
        }, errorCallback);
    }, errorCallback);
}

function runPersistentTest() {
    debug("* Resolving a persistent URI.");
    requestFileSystem(PERSISTENT, 100, function(fs) {
        resolveLocalFileSystemURI(fs.root.toURI(), function(entry) {
            assertPathsMatch("/", entry.fullPath);
            assertIsDirectory(entry);
            cleanupAndRunNext();
        }, errorCallback);
    });
}

// End of test body functions ---------------------------------------------

var testsList = [
    runBasicTest,
    runHandmadeURI,
    runBogusURI,
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

if (window.requestFileSystem) {
    window.jsTestIsAsync = true;
    requestFileSystem(window.TEMPORARY, 100, fileSystemCallback, errorCallback);
} else
    debug("This test requires FileSystem API support.");

window.successfullyParsed = true;
