if (this.importScripts) {
    importScripts('fs-worker-common.js');
    importScripts('fs-test-util.js');
}

description("Obtaining URI from FileEntry.");

var fileSystem = null;
var testFileName = 'testFileEntry.txt';
var testFileURI = null;

function errorCallback(error) {
    testFailed("Error occured:" + error.code);
    finishJSTest();
}

function entryToURI(entry) {
    testFileURI = entry.toURI();
    shouldBe("testFileURI", "'filesystem:file:///temporary/testFileEntry.txt'");
    finishJSTest();
}

function createTestFile() {
    fileSystem.root.getFile(testFileName, {create:true}, entryToURI, errorCallback);
}

function fileSystemCallback(fs) {
    fileSystem = fs;
    removeAllInDirectory(fileSystem.root, createTestFile, errorCallback);
}

var jsTestIsAsync = true;
requestFileSystem(TEMPORARY, 100, fileSystemCallback, errorCallback);
var successfullyParsed = true;
