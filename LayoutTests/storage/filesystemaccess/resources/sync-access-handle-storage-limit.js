if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("This test checks FileSystemSyncAccessHandle returns error when limit is reached");

var accessHandle, fileHandle, quota;
async function test() 
{
    try {
        var rootHandle = await navigator.storage.getDirectory();
        // Create a new file for this test.
        await rootHandle.removeEntry("sync-access-handle-storage-limit.txt").then(() => { }, () => { });
        fileHandle = await rootHandle.getFileHandle("sync-access-handle-storage-limit.txt", { "create" : true  });
        accessHandle = await fileHandle.createSyncAccessHandle();

        shouldNotThrow("accessHandle.write(new Uint8Array(quota - 1).buffer)");
        shouldThrow("accessHandle.write(new Uint8Array(2).buffer)");
        accessHandle.close();

        finishTest();
    } catch (error) {
        finishTest(error);
    }
}

addEventListener('message', (event) => {
    quota = event.data;
    debug("quota is set to " + quota);
    test();
});
