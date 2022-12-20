if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("This test checks read and write capabilities of FileSystemSyncAccessHandle.");

var accessHandle, fileSize, writeBuffer, writeSize, readBuffer, readSize, readText, readError;
var totalReadSize = 0, totalWriteSize = 0;

function arrayBufferToString(buffer)
{
    const decoder = new TextDecoder();
    var view = new Uint8Array(buffer);
    return decoder.decode(view);
}

function stringToArrayBuffer(string)
{
    const encoder = new TextEncoder();
    return encoder.encode(string);
}

function write(accessHandle, offset, text)
{
    writeBuffer = stringToArrayBuffer(text);
    if (offset == null)
        writeSize = accessHandle.write(writeBuffer);
    else
        writeSize = accessHandle.write(writeBuffer, { "at" : offset });
    shouldBe("writeSize", "writeBuffer.byteLength");
    return writeSize;
}

function read(accessHandle, offset, size, expectedString)
{
    readBuffer = new ArrayBuffer(size);
    if (offset == null)
        readSize = accessHandle.read(readBuffer);
    else
        readSize = accessHandle.read(readBuffer, { "at": offset });
    shouldBe("readSize", "readBuffer.byteLength");
    if (expectedString) {
        readText = arrayBufferToString(readBuffer);
        shouldBeEqualToString("readText", expectedString);
    }
    return readSize;
}

async function test() 
{
    try {
        var rootHandle = await navigator.storage.getDirectory();
        // Create a new file for this test.
        await rootHandle.removeEntry("sync-access-handle.txt").then(() => { }, () => { });
        var fileHandle = await rootHandle.getFileHandle("sync-access-handle.txt", { "create" : true  });
        accessHandle = await fileHandle.createSyncAccessHandle();
        fileSize = await accessHandle.getSize();
        shouldBe("fileSize", "0");

        debug("test read() and write():");
        totalWriteSize += write(accessHandle, 0, "This is first sentence.");
        totalReadSize += read(accessHandle, 0, totalWriteSize, "This is first sentence.");
        totalWriteSize += write(accessHandle, totalWriteSize, "This is second sentence.");
        read(accessHandle, totalReadSize, totalWriteSize - totalReadSize, "This is second sentence.");
        read(accessHandle, 0, totalWriteSize, "This is first sentence.This is second sentence.");

        // Wrong offset to read and write.
        totalReadSize = totalWriteSize;
        // File cursor should be updated by last read(), so the next write() should append to file.
        totalWriteSize += write(accessHandle, null, "This is third sentence.");
        read(accessHandle, 0, totalReadSize, "This is first sentence.This is second sentence.");
        read(accessHandle, null, totalWriteSize - totalReadSize, "This is third sentence.");
        shouldThrow("accessHandle.read(new ArrayBuffer(1), { \"at\" : Number.MAX_SAFE_INTEGER })");
        shouldThrow("accessHandle.write(new ArrayBuffer(1), { \"at\" : Number.MAX_SAFE_INTEGER })");

        debug("test flush():");
        await accessHandle.flush();
        fileSize = await accessHandle.getSize();
        shouldBe("fileSize", "totalWriteSize");

        debug("test truncate():");
        await accessHandle.truncate(4);
        await accessHandle.flush();
        fileSize = await accessHandle.getSize();
        shouldBe("fileSize", "4");

        debug("test write() with pending operation:");
        evalAndLog("accessHandle.truncate(0)");
        readBuffer = new ArrayBuffer(4);
        shouldThrow("accessHandle.read(readBuffer, { \"at\" : 0 })");

        await accessHandle.close();
        finishTest();
    } catch (error) {
        finishTest(error);
    }
}

test();
