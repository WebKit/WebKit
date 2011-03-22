if (this.importScripts) {
    importScripts('../../resources/fs-worker-common.js');
    importScripts('../../resources/fs-test-util.js');
}

description("Tests using resolveLocalFileSystemSyncURL to obtain an Entry from a URL");

var testFileName = '/testFile';
var fileSystem = null;
var expectedAndRunNext = function() { testPassed(""); cleanupAndRunNext(); };

var expectedPath = null;
var actualPath = null;

function createTestFile() {
    return fileSystem.root.getFile(testFileName, {create:true});
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
    entry = createTestFile();
    var e = resolveLocalFileSystemSyncURL(entry.toURL());
    assertPathsMatch(entry.fullPath, e.fullPath);
    assertIsFile(e);
}

function runHandmadeURL() {
    debug("* Resolving test file by hand");
    entry = createTestFile();
    var e = resolveLocalFileSystemSyncURL("filesystem:http://127.0.0.1:8000/temporary/testFile");
    assertPathsMatch(testFileName, e.fullPath);
    assertIsFile(e);
}

function runBogusURL() {
    debug("* Resolving a completely bogus URL.");
    try {
        resolveLocalFileSystemSyncURL("foo");
        testFailed();
    } catch (e) {
        testPassed("Caught exception");
    }
}

function runWrongProtocol() {
    debug("* Resolving a URL with the wrong protocol");
    try {
        resolveLocalFileSystemSyncURL("http://127.0.0.1:8000/foo/bar/baz");
        testFailed();
    } catch (e) {
        testPassed("Caught exception");
    }
}

function runNotEnoughSlashes() {
    debug("* Resolving a URL with no slash between type and file");
    entry = createTestFile();
    try {
        resolveLocalFileSystemSyncURL("filesystem:http://127.0.0.1:8000/temporarytestFile");
        testFailed();
    } catch (e) {
        testPassed("Caught exception");
    }
}

function runNotEnoughSlashes2() {
    debug("* Resolving a URL with no slash between protocol and type");
    entry = createTestFile();
    try {
        resolveLocalFileSystemSyncURL("filesystem:http://127.0.0.1:8000temporary/testFile");
    } catch (e) {
        testPassed("Caught exception");
    }
}

function runUseBackSlashes() {
    debug("* Resolve a path using backslashes");
    entry = fileSystem.root.getDirectory("foo", {create:true}).getFile("testFile", {create:true});
    var e = resolveLocalFileSystemSyncURL("filesystem:http://127.0.0.1:8000/temporary/foo\\testFile");
    assertPathsMatch("/foo/testFile", e.fullPath);
    assertIsFile(e);
}

function runDirectory() {
    debug("* Resolve a directory");
    entry = fileSystem.root.getDirectory("foo", {create:true});
    var e = resolveLocalFileSystemSyncURL("filesystem:http://127.0.0.1:8000/temporary/foo");
    assertPathsMatch("/foo", e.fullPath);
    assertIsDirectory(e);
}

function runWithTrailingSlash() {
    debug("* Resolve a path using a trailing slash");
    entry = fileSystem.root.getDirectory("foo", {create:true});
    var e = resolveLocalFileSystemSyncURL("filesystem:http://127.0.0.1:8000/temporary/foo/");
    assertPathsMatch("/foo", e.fullPath);
    assertIsDirectory(e);
}

function runPersistentTest() {
    debug("* Resolving a persistent URL.");
    var fs = requestFileSystemSync(PERSISTENT, 100);
    entry = resolveLocalFileSystemSyncURL(fs.root.toURL());
    assertPathsMatch("/", entry.fullPath);
    assertIsDirectory(entry);
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

if (this.requestFileSystem) {
    fileSystem = requestFileSystemSync(this.TEMPORARY, 100);
    for (var i = 0; i < testsList.length; ++i) {
      testsList[i]();
      removeAllInDirectorySync(fileSystem.root);
      entry = null;
    }
    debug("Finished running tests.");
    finishJSTest();
} else
    debug("This test requires FileSystem API support.");

var successfullyParsed = true;
