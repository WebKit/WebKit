if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
}

description("This test checks move() of FileSystemHandle.");

var rootHandle, dirHandle1, dirHandle2, fileHandle, fileHandle2, testError, getFileHandleError, moveFileError;

function finishTest(error)
{
    if (error) {
        testFailed(error);
    }
    finishJSTest();
}

async function test() {
    try {
        var rootHandle = await navigator.storage.getDirectory();
        // Create two new folders for this test.
        await rootHandle.removeEntry("dir1", { "recursive" : true }).then(() => { }, () => { });
        await rootHandle.removeEntry("dir2", { "recursive" : true }).then(() => { }, () => { });
        dirHandle1 = await rootHandle.getDirectoryHandle("dir1", { "create" : true });
        dirHandle2 = await rootHandle.getDirectoryHandle("dir2", { "create" : true });

        debug("Test basic move:")
        fileHandle = await dirHandle1.getFileHandle("file", { "create" : true });
        shouldBeEqualToString("fileHandle.name", "file");
        await fileHandle.move(dirHandle2, "newFile");
        shouldBeEqualToString("fileHandle.name", "newFile");

        testError = null;
        await dirHandle1.getFileHandle("file", { "create" : false }).then((handle) => {
            testError = "Got file handle from dirHandle1 unexpectedly"
        }, (error) => {
            getFileHandleError = error;
            shouldBeEqualToString("getFileHandleError.toString()", "NotFoundError: The object can not be found here.");
        });

        if (testError) {
            return finishTest(testError);
        }

        fileHandle2 = await dirHandle2.getFileHandle("newFile", { "create" : false });
        shouldBeEqualToString("fileHandle2.name", "newFile");
        shouldBeEqualToString("fileHandle2.kind", "file");

        debug("Test move to a file handle:");
        testError = null;
        fileHandle3 = await dirHandle1.getFileHandle("secondFile", { "create" : true });
        await fileHandle.move(fileHandle3, fileHandle.name).then(() => {
            testError =  "Moved file to fileHandle3 unexpectedly"
        }, (error) => {
            moveFileError = error;
            shouldBeEqualToString("moveFileError.toString()", "TypeMismatchError: The type of an object was incompatible with the expected type of the parameter associated to the object.");
        });

        if (testError) {
            return finishTest(testError);
        }

        debug("Test move to a destination with existing file:");
        testError = null;
        await fileHandle.move(dirHandle1, "secondFile").then(() => {
            testError =  "Moved file to dirHandle1 unexpectedly";
        }, (error) => {
            moveFileError = error;
            shouldBeEqualToString("moveFileError.toString()", "UnknownError: The operation failed for an unknown transient reason (e.g. out of memory).");
        });

        if (testError) {
            return finishTest(testError);
        }

        debug("Test move with an invalid name:");
        testError = null;
        await fileHandle.move(dirHandle1, "..").then(() => {
            testError =  "Moved file to dirHandle1 with invalid name unexpectedly";
        }, (error) => {
            moveFileError = error;
            shouldBeEqualToString("moveFileError.toString()", "UnknownError: Name is invalid");
        });

        if (testError) {
            return finishTest(testError);
        }

        await fileHandle.move(dirHandle1, "root/file").then(() => {
            testError =  "Moved file to dirHandle1 with invalid name unexpectedly";
        }, (error) => {
            moveFileError = error;
            shouldBeEqualToString("moveFileError.toString()", "UnknownError: Name is invalid");
        });

        if (testError) {
            return finishTest(testError);
        }


        if (typeof WorkerGlobalScope !== 'undefined' && self instanceof WorkerGlobalScope) {
            debug("Test move with open access handle:");
            testError = null;
            accessHandle = await fileHandle.createSyncAccessHandle();
            await fileHandle.move(dirHandle1, "file").then(() => {
                testError =  "Moved file back to dirHandle1 unexpectedly";
            }, (error) => {
                moveFileError = error;
                shouldBeEqualToString("moveFileError.toString()", "InvalidStateError: The object is in an invalid state.");
            });

            if (testError) {
                return finishTest(testError);
            }

            accessHandle.close();
        }

        finishTest();
    } catch (error) {
        finishTest(error);
    }
}

test();
