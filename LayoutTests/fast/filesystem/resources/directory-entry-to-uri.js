if (this.importScripts) {
    importScripts('fs-worker-common.js');
    importScripts('fs-test-util.js');
}

description("Obtaining URI from DirectoryEntry.");

var fileSystem = null;
var testDirectoryName = 'testDirectory';
var testDirectoryURI = null;

function errorCallback(error) {
    testFailed("Error occured:" + error.code);
    finishJSTest();
}

function entryToURI(entry) {
    testDirectoryURI = entry.toURI();
    shouldBe("testDirectoryURI", "'filesystem:file:///temporary/testDirectory'");
    finishJSTest();
}

function createTestDirectory() {
    fileSystem.root.getDirectory(testDirectoryName, {create:true}, entryToURI, errorCallback);
}

function fileSystemCallback(fs) {
    fileSystem = fs;
    removeAllInDirectory(fileSystem.root, createTestDirectory, errorCallback);
}

var jsTestIsAsync = true;
requestFileSystem(TEMPORARY, 100, fileSystemCallback, errorCallback);
var successfullyParsed = true;
