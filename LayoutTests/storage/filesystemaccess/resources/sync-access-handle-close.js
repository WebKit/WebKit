if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
}

description("This test checks close() of FileSystemSyncAccessHandle");

var accessHandle, fileHandle, error;
const buffer = new ArrayBuffer(1);
const options = { "at" : 0 };
var functions = [
    { name : "getSize" },
    { name : "flush" },
    { name : "read", args : [buffer, options], sync : true },
    { name : "write", args : [buffer, options], sync : true },
];

function finishTest(error)
{
    if (error) {
        testFailed(error);
    }
    finishJSTest();
}

function testSyncFunction(currentFunction)
{
    try {
        var result = accessHandle[currentFunction.name].apply(accessHandle, currentFunction.args);
        return null;
    } catch (err) {
        return err;
    }
}

async function testAsyncFunction(func)
{
    var promise = accessHandle[func.name].apply(accessHandle, func.args);
    return promise.then((value) => {
        return func.name + " function should throw exception but didn't";
    }, (err) => {
        return err;
    });
}

async function testFunctions() 
{
    for (const func of functions) {
        debug("testing " + func.name);

        if (func.sync) {
            error = testSyncFunction(func);
        } else {
            error = await testAsyncFunction(func);
        }

        shouldBeEqualToString("error.toString()", "InvalidStateError: AccessHandle is closing or closed");
    }
}

async function testMultipleHandles()
{
    // Current limit of file descriptor count is 256.
    for (let i = 0; i < 512; i++) {
        try {
            accessHandle = await fileHandle.createSyncAccessHandle();
            await accessHandle.close();
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

        var closePromise = accessHandle.close();
        debug("test after invoking close():");
        await testFunctions();

        debug("test after close() is done:");
        await closePromise;
        await testFunctions();

        debug("test closing multiple handles:");
        await testMultipleHandles();

        finishTest();
    } catch (error) {
        finishTest(error);
    }
}

test();
