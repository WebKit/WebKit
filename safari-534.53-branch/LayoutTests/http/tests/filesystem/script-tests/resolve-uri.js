if (this.importScripts) {
    importScripts('../resources/fs-worker-common.js');
    importScripts('../resources/fs-test-util.js');
}

description("Tests using webkitResolveLocalFileSystemURL to obtain an Entry from a URL");

var testFileName = '/testFile';
var fileSystem = null;
var expectedPath = null;
var actualPath = null;
var errorCode = null;

function errorCallback(error) {
    if (error && error.code)
        debug("Error occurred: " + error.code);
    testFailed("Bailing out early");
    finishJSTest();
}

function expectSecurityErrAndRunNext(error) {
    errorCode = error.code;
    shouldBe("FileError.SECURITY_ERR", "errorCode");
    cleanupAndRunNext();
}

function expectEncodingErrAndRunNext(error) {
    errorCode = error.code;
    shouldBe("FileError.ENCODING_ERR", "errorCode");
    cleanupAndRunNext();
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
        webkitResolveLocalFileSystemURL(entry.toURL(), function(e) {
            assertPathsMatch(entry.fullPath, e.fullPath);
            assertIsFile(e);
            cleanupAndRunNext();
        }, errorCallback);
    });
}

function runHandmadeURL() {
    debug("* Resolving test file by hand");
    createTestFile(function(entry) {
        webkitResolveLocalFileSystemURL("filesystem:http://127.0.0.1:8000/temporary/testFile", function(e) {
            assertPathsMatch(testFileName, e.fullPath);
            assertIsFile(e);
            cleanupAndRunNext();
        }, errorCallback);
    });
}

function runWrongDomain() {
    debug("* Resolving a URL with the wrong security origin (domain)");
    webkitResolveLocalFileSystemURL("filesystem:http://localhost:8000/temporary/foo", errorCallback, expectSecurityErrAndRunNext);
}

function runWrongPort() {
    debug("* Resolving a URL with the wrong security origin (port)");
    webkitResolveLocalFileSystemURL("filesystem:http://127.0.0.1:8080/temporary/foo", errorCallback, expectSecurityErrAndRunNext);
}

function runWrongScheme() {
    debug("* Resolving a URL with the wrong security origin (scheme)");
    webkitResolveLocalFileSystemURL("filesystem:https://127.0.0.1:8000/temporary/foo", errorCallback, expectSecurityErrAndRunNext);
}

function runBogusURL() {
    debug("* Resolving a completely bogus URL.");
    webkitResolveLocalFileSystemURL("foo", errorCallback, expectEncodingErrAndRunNext);
}

function runWrongProtocol() {
    debug("* Resolving a URL with the wrong protocol");
    webkitResolveLocalFileSystemURL("http://127.0.0.1:8000/foo/bar/baz", errorCallback, expectEncodingErrAndRunNext);
}

function runNotEnoughSlashes() {
    debug("* Resolving a URL with no slash between type and file");
    createTestFile(function(entry) {
        webkitResolveLocalFileSystemURL("filesystem:http://127.0.0.1:8000/temporarytestFile", errorCallback, expectEncodingErrAndRunNext);
    });
}

function runNotEnoughSlashes2() {
    debug("* Resolving a URL with no slash between protocol and type (bogus port)");
    createTestFile(function(entry) {
        webkitResolveLocalFileSystemURL("filesystem:http://127.0.0.1:8000temporary/testFile", errorCallback, expectSecurityErrAndRunNext);
    });
}

function runUseBackSlashes() {
    debug("* Resolve a path using backslashes");
    fileSystem.root.getDirectory("foo", {create:true}, function(entry) {
        entry.getFile("testFile", {create:true}, function(f) {
            webkitResolveLocalFileSystemURL("filesystem:http://127.0.0.1:8000/temporary/foo\\testFile", function(e) {
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
        webkitResolveLocalFileSystemURL("filesystem:http://127.0.0.1:8000/temporary/foo", function(e) {
            assertPathsMatch("/foo", e.fullPath);
            assertIsDirectory(e);
            cleanupAndRunNext();
        }, errorCallback);
    }, errorCallback);
}

function runWithTrailingSlash() {
    debug("* Resolve a path using a trailing slash");
    fileSystem.root.getDirectory("foo", {create:true}, function(entry) {
        webkitResolveLocalFileSystemURL("filesystem:http://127.0.0.1:8000/temporary/foo/", function(e) {
            assertPathsMatch("/foo", e.fullPath);
            assertIsDirectory(e);
            cleanupAndRunNext();
        }, errorCallback);
    }, errorCallback);
}

function runPersistentTest() {
    debug("* Resolving a persistent URL.");
    webkitRequestFileSystem(PERSISTENT, 100, function(fs) {
        webkitResolveLocalFileSystemURL(fs.root.toURL(), function(entry) {
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
    runWrongDomain,
    runWrongPort,
    runWrongScheme,
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

if (this.webkitRequestFileSystem) {
    jsTestIsAsync = true;
    webkitRequestFileSystem(this.TEMPORARY, 100, fileSystemCallback, errorCallback);
} else
    debug("This test requires FileSystem API support.");

var successfullyParsed = true;
