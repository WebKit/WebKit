description("Obtaining File from FileEntry");

var fileSystem = null;
var testFileName = '/testFileEntry.txt';
var testFileEntry = null;
var testFile = null;

function errorCallback(error) {
    testFailed("Error occured:" + error.code);
    finishJSTest();
}

function fileCallback(file) {
    testFile = file;
    shouldBe("testFile.name", "testFileEntry.name");
    shouldBe("testFile.type", "'text/plain'");
    shouldBe("testFile.size", "0");
    finishJSTest();
}

function getFileFromEntry(entry) {
    testFileEntry = entry;
    entry.file(fileCallback, errorCallback);
}

function createTestFile() {
    fileSystem.root.getFile(testFileName, {create:true}, getFileFromEntry, errorCallback);
}

function fileSystemCallback(fs) {
    fileSystem = fs;
    removeAllInDirectory(fileSystem.root, createTestFile, errorCallback);
}

if (window.requestFileSystem) {
    window.jsTestIsAsync = true;
    requestFileSystem(window.TEMPORARY, 100, fileSystemCallback, errorCallback);
} else
    debug("This test requires FileSystem API support.");

window.successfullyParsed = true;
