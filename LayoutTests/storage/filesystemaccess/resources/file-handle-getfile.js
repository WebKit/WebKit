if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("This test checks getFile() of FileSystemFileHandle.");

var rootHandle, fileHandle, fileObject, writeSize, writeBuffer, fileContent;

async function read(file) 
{
    return new Promise((resolve, reject) => {
        var reader = new FileReader();
        reader.readAsText(file);
        reader.onload = (event) => {
            resolve(event.target.result); 
        }
        reader.onerror = (event) => {
            reject(event.target.error);
        }
    });
}

async function test() 
{
    try {
        var rootHandle = await navigator.storage.getDirectory();
        // Create a new file for this test.
        await rootHandle.removeEntry("file-handle-getfile.txt").then(() => { }, () => { });
        fileHandle = await rootHandle.getFileHandle("file-handle-getfile.txt", { "create" : true });
        fileContent = "";
    
        // Write file content in worker.
        if (typeof WorkerGlobalScope !== 'undefined' && self instanceof WorkerGlobalScope) {
            accessHandle = await fileHandle.createSyncAccessHandle();
            const encoder = new TextEncoder();
            fileContent = "This is a test.";
            writeBuffer = encoder.encode(fileContent);
            writeSize = accessHandle.write(writeBuffer, { "at" : 0 });
            shouldBe("writeSize", "writeBuffer.byteLength");
            accessHandle.close();
        }

        fileObject = await fileHandle.getFile();
        readText = await read(fileObject);
        shouldBe("readText", "fileContent");

        finishTest();
    } catch (error) {
        finishTest(error.toString());
    }
}

test();
