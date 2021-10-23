if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
}

description("This test checks close() of FileSystemSyncAccessHandle");

var accessHandle, promise;

function finishTest(error)
{
    if (error) {
        testFailed(error);
    }
    finishJSTest();
}

async function testFunctions()
{
    shouldThrow("await accessHandle.close()");
    shouldThrow("await accessHandle.getSize()");
    shouldThrow("await accessHandle.flush()");
    shouldThrow("await accessHandle.read(new ArrayBuffer(1), { \"at\" : 0 })");
    shouldThrow("await accessHandle.write(new ArrayBuffer(1), { \"at\" : 0 })");
}

async function test() {
    try {
        var rootHandle = await navigator.storage.getDirectory();
        // Create a new file for this test.
        await rootHandle.removeEntry("sync-access-handle-close.txt").then(() => { }, () => { });
        var fileHandle = await rootHandle.getFileHandle("sync-access-handle-close.txt", { "create" : true  });
        accessHandle = await fileHandle.createSyncAccessHandle();

        var closePromise = accessHandle.close();
        debug("test after invoking close():");
        testFunctions();

        debug("test after close() is done:");
        await closePromise;
        testFunctions();

        finishTest();
    } catch (error) {
        finishTest(error);
    }
}

test();
