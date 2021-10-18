if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
}

description("This test checks basic funtionalities of FileSystemDirectoryHandle.");

var rootHandle, dirHandle, fileHandle, isSameEntry, handleNames, createError;

function finishTest(error)
{
    if (error) {
        testFailed(error);
    }
    finishJSTest();
}

function getDirectory() {
    navigator.storage.getDirectory().then((handle) => {
        rootHandle = handle;
        shouldBeEqualToString("rootHandle.name", "");
        shouldBeEqualToString("rootHandle.kind", "directory");
        createDirectoryHandle(rootHandle);
    }).catch((error) => {
        finishTest(error);
    });
}

function createDirectoryHandle(fromHandle) {
    fromHandle.getDirectoryHandle("dir", { "create" : true }).then((handle) => {
        dirHandle = handle;
        shouldBeEqualToString("dirHandle.name", "dir");
        shouldBeEqualToString("dirHandle.kind", "directory");
        checkIfSameEntry(rootHandle, dirHandle);
    }).catch((error) => {
        finishTest(error);
    });
}

function checkIfSameEntry(handle1, handle2)
{
    handle1.isSameEntry(handle2).then((result) => {
        isSameEntry = result;
        shouldBeFalse("isSameEntry");
        createFileHandle(dirHandle, true);
    }).catch((error) => {
        finishTest(error);
    });
}

function createFileHandle(fromHandle, create) {
    let options = { "create" : create };
    fromHandle.getFileHandle("file", options).then((handle) => {
        fileHandle = handle;
        shouldBeEqualToString("fileHandle.name", "file");
        shouldBeEqualToString("fileHandle.kind", "file");
        resolvePath();
    }).catch((error) => {
        if (create) {
            finishTest(error);
            return;
        }
        createError = error;
        shouldBeEqualToString("createError.toString()", "NotFoundError: The object can not be found here.");
        finishTest();
    });
}

function resolvePath() {
    rootHandle.resolve(fileHandle).then((result) => {
        handleNames = result;
        shouldBe("handleNames.length", "2");
        shouldBeEqualToString("handleNames[0]", "dir");
        shouldBeEqualToString("handleNames[1]", "file");
        removeEntry(dirHandle, "file");
    }).catch((error) => {
        finishTest(error);
    });
}

function removeEntry(fromHandle, name) {
    fromHandle.removeEntry(name).then((result) => {
        // No error means it is removed.
        createFileHandle(dirHandle, false);
    }).catch((error) => {
        finishTest(error);
    });
}

getDirectory();
