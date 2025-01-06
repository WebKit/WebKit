if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("This test checks seek command of FileSystemWritableFileStream");

var fileContent;
async function test() 
{
    try {
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

        fileObject = await fileHandle.getFile();
        fileContent = await asyncReadFileAsText(fileObject);
        shouldBeEqualToString("fileContent", "abcdihgfe\0abcde");

        finishTest();
    } catch (error) {
        finishTest(error);
    }
}

test();
