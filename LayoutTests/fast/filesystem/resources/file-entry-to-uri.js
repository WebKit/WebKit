if (this.importScripts) {
    importScripts('fs-worker-common.js');
    importScripts('../../js/resources/js-test-pre.js');
    importScripts('fs-test-util.js');
}

description("Obtaining URL from FileEntry.");

var fileSystem = null;
var testFileName = 'testFileEntry.txt';
var testFileURI = null;

function errorCallback(error) {
    testFailed("Error occured:" + error.code);
    finishJSTest();
}

function entryToURL(entry) {
    testFileURL = entry.toURL();
    shouldBe("testFileURL", "'filesystem:file:///temporary/testFileEntry.txt'");
    finishJSTest();
}

function createTestFile() {
    fileSystem.root.getFile(testFileName, {create:true}, entryToURL, errorCallback);
}

function fileSystemCallback(fs) {
    fileSystem = fs;
    removeAllInDirectory(fileSystem.root, createTestFile, errorCallback);
}

var jsTestIsAsync = true;
webkitRequestFileSystem(TEMPORARY, 100, fileSystemCallback, errorCallback);
