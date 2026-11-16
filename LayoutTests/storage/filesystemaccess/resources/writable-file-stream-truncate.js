if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("This test checks truncate command of FileSystemWritableFileStream");

var fileContent;
async function checkFileContent(fileHandle, expectedContent)
{
    fileObject = await fileHandle.getFile();
    fileContent = await asyncReadFileAsText(fileObject);
    shouldBeEqualToString("fileContent", expectedContent);
}

async function runWriterTest()
{
    debug("Run writer tests:")
    var rootHandle = await navigator.storage.getDirectory();
    await rootHandle.removeEntry("writable-file-stream-truncate.txt").then(() => { }, () => { });
    var fileHandle = await rootHandle.getFileHandle("writable-file-stream-truncate.txt", { "create" : true  });

    stream = await fileHandle.createWritable();
    writer = stream.getWriter();
    await writer.write("abcdefghi");
    await writer.write({ type:"truncate", size:4 });
    await writer.write("xyz");
    await writer.write({ type:"truncate", size:6 });
    await writer.close();
    await checkFileContent(fileHandle, "abcdxy");

    stream = await fileHandle.createWritable({ keepExistingData:true });
    writer = stream.getWriter();
    // Not truncate.
    await writer.write({ type:"truncate", size:10 });
    await writer.close();

    await checkFileContent(fileHandle, "abcdxy\0\0\0\0");
}

async function runTest()
{
    debug("Run stream tests:")
    var rootHandle = await navigator.storage.getDirectory();
    await rootHandle.removeEntry("writable-file-stream-truncate.txt").then(() => { }, () => { });
    var fileHandle = await rootHandle.getFileHandle("writable-file-stream-truncate.txt", { "create" : true  });

    stream = await fileHandle.createWritable();
    await stream.write("abcdefghi");
    await stream.truncate(4);
    await stream.write("xyz");
    await stream.truncate(6);
    await stream.close();
    await checkFileContent(fileHandle, "abcdxy");

    stream = await fileHandle.createWritable({ keepExistingData:true });
    await stream.truncate(10);
    await stream.close();

    await checkFileContent(fileHandle, "abcdxy\0\0\0\0");
}

async function test() 
{
    try {
        await runWriterTest();
        await runTest();
        finishTest();
    } catch (error) {
        finishTest(error);
    }
}

test();
