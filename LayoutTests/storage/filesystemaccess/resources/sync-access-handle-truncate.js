if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("This test checks truncate() method of FileSystemSyncAccessHandle");

var accessHandle, fileSize, writeBuffer, writeSize, readBuffer, readSize, readText;

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

function write(accessHandle, text)
{
    writeBuffer = stringToArrayBuffer(text);
    writeSize = accessHandle.write(writeBuffer);
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
        await rootHandle.removeEntry("sync-access-handle-truncate.txt").then(() => { }, () => { });
        var fileHandle = await rootHandle.getFileHandle("sync-access-handle-truncate.txt", { "create" : true  });
        accessHandle = await fileHandle.createSyncAccessHandle();
        fileSize = accessHandle.getSize();
        shouldBe("fileSize", "0");

        debug("Test: truncate size smaller than file size");
        write(accessHandle, "abcdefghi");
        accessHandle.truncate(4); // Write offset is updated to 4.
        write(accessHandle, "xyz");
        accessHandle.flush();
        fileSize = accessHandle.getSize();
        shouldBe("fileSize", "7");
        read(accessHandle, 0, fileSize, "abcdxyz");

        debug("Test: truncate size bigger than file size");
        write(accessHandle, "?");  // Write offset is 8.
        accessHandle.truncate(12); // Write offset should not be updated.
        write(accessHandle, "!");
        accessHandle.flush();
        fileSize = accessHandle.getSize();
        shouldBe("fileSize", "12");
        read(accessHandle, 0, fileSize, "abcdxyz?!\0\0\0");

        debug("Test: truncate size bigger than quota (1MB)");
        shouldThrow("accessHandle.truncate(1024 * 1024 + 1)");

        accessHandle.close();
        finishTest();
    } catch (error) {
        finishTest(error);
    }
}

console.log('error');
test();
