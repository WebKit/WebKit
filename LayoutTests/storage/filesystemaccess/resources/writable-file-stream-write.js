if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("This test checks write command of FileSystemWritableFileStream");

const array = new Uint8Array([1,2,3,4]);
// Detach the ArrayBuffer by transferring it to a worker.
const worker = new Worker(URL.createObjectURL(new Blob([])));
worker.postMessage('', [array.buffer]);

var fileContent, undefinedObject;
var testCases = [
    { type:"String", input:"Test case one", expected:"Test case one" },
    { type:"ArrayBuffer", input:new TextEncoder().encode("Test case two"), expected:"Test case two" },
    { type:"Blob", input:new Blob(["Test case three"]), expected:"Test case three" },
    { type:"Undefined", input: undefinedObject },
    { type:"Number", input: new Number(1) },
    { type:"Detached ArrayBuffer", input: array },
    { type:"WriteParams with String", input:{ type:"write", data:"Test case four" }, expected:"Test case four" },
    { type:"WriteParams with ArrayBuffer", input:{ type:"write", data:new TextEncoder().encode("Test case five") }, expected:"Test case five" },
    { type:"WriteParams with Blob", input:{ type:"write", data:new Blob(["Test case six"]) }, expected:"Test case six" },
    { type:"WriteParams with position", input:{ type:"write", data:"Test case seven", position:4 }, expected:"\0\0\0\0Test case seven" },
    { type:"WriteParams with position (overwrite)", input:{ type:"write", data:"Test case seven" }, secondInput:{ type:"write", data:" CASE EIGHT", position:4 }, expected:"Test CASE EIGHT" }
];

async function runTest(testCase, fileHandle)
{
    stream = await fileHandle.createWritable();
    writer = stream.getWriter();

    // Input that is expected to fail.
    if (!testCase.expected) {
        try {
            await writer.write(testCase.input);
            testFailed("write() should fail, but succeeded");
            await writer.close();
        } catch(error) {
            testPassed("write() failed with error: " + error.name);
            // Wait until writer closed before creating new stream and writer.
            await writer.closed.then(() => { }, () => { });
        }
        return;
    }

    await writer.write(testCase.input);
    if (testCase.secondInput)
        await writer.write(testCase.secondInput);
    await writer.close();

    fileObject = await fileHandle.getFile();
    fileContent = await asyncReadFileAsText(fileObject);
    shouldBeEqualToString("fileContent", testCase.expected); 
}

async function test() 
{
    try {
        var rootHandle = await navigator.storage.getDirectory();
        await rootHandle.removeEntry("file-handle-writable-stream.txt").then(() => { }, () => { });
        var fileHandle = await rootHandle.getFileHandle("file-handle-writable-stream.txt", { "create" : true  });

        while (testCases.length) {
            testCase = testCases.shift();
            debug("Testing input data type: " + testCase.type);
            await runTest(testCase, fileHandle);
        }

        finishTest();
    } catch (error) {
        finishTest(error);
    }
}

test();
