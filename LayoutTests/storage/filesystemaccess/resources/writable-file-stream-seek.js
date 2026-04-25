if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("This test checks seek command of FileSystemWritableFileStream");

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
    await rootHandle.removeEntry("writable-file-stream-seek.txt").then(() => { }, () => { });
    var fileHandle = await rootHandle.getFileHandle("writable-file-stream-seek.txt", { "create" : true  });

    stream = await fileHandle.createWritable();
    writer = stream.getWriter();
    await writer.write("abcdefghi");
    await writer.write({ type:"seek", position:4 });
    await writer.write("ihgfe");
    await writer.write({ type:"seek", position:10 });
    await writer.write("abcde");
    await writer.close();

    await checkFileContent(fileHandle, "abcdihgfe\0abcde");
}

async function runTest()
{
    debug("Run stream tests:")
    var rootHandle = await navigator.storage.getDirectory();
    await rootHandle.removeEntry("writable-file-stream-seek.txt").then(() => { }, () => { });
    var fileHandle = await rootHandle.getFileHandle("writable-file-stream-seek.txt", { "create" : true  });

    stream = await fileHandle.createWritable();
    await stream.write("abcdefghi");
    await stream.seek(4);
    await stream.write("ihgfe");
    await stream.seek(10);
    await stream.write("abcde");
    await stream.close();

    await checkFileContent(fileHandle, "abcdihgfe\0abcde");
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
