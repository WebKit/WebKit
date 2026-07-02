if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("This test checks close() of FileSystemSyncAccessHandle");

var accessHandle, fileHandle, error;
const buffer = new ArrayBuffer(1);
const options = { "at" : 0 };
var functions = [
    { name : "getSize" },
    { name : "flush" },
    { name : "read", args : [buffer, options] },
    { name : "write", args : [buffer, options] },
];

function testFunction(targetFunction)
{
    try {
        var result = accessHandle[targetFunction.name].apply(accessHandle, targetFunction.args);
        return null;
    } catch (error) {
        return error;
    }
}

function testFunctions()
{
    for (const func of functions) {
        debug("testing " + func.name);
        error = testFunction(func);
        shouldBeEqualToString("error.toString()", "InvalidStateError: AccessHandle is closed");
    }
}

async function testMultipleHandles()
{
    // Current limit of file descriptor count is 256.
    for (let i = 0; i < 512; i++) {
        try {
            accessHandle = await fileHandle.createSyncAccessHandle();
            accessHandle.close();
        } catch (err) {
            throw "Failed at No." + i + " handle: " + err.toString();
        }
    }
    debug("Create and close access handles successfully");
}

async function test() 
{
    try {
        var rootHandle = await navigator.storage.getDirectory();
        // Create a new file for this test.
        await rootHandle.removeEntry("sync-access-handle-close.txt").then(() => { }, () => { });
        fileHandle = await rootHandle.getFileHandle("sync-access-handle-close.txt", { "create" : true  });
        accessHandle = await fileHandle.createSyncAccessHandle();

        debug("test closing handle:");
        accessHandle.close();
        testFunctions();

        debug("test closing multiple handles:");
        await testMultipleHandles();

        finishTest();
    } catch (error) {
        finishTest(error);
    }
}

test();
