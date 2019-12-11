var testCases = [
    "testReadingNonExistentFileBlob",
    "testReadingNonExistentFileBlob2",
    "testReadingEmptyFileBlob",
    "testReadingEmptyTextBlob",
    "testReadingEmptyFileAndTextBlob",
    "testReadingSingleFileBlob",
    "testReadingSingleTextBlob",
    "testReadingSingleTextBlobAsDataURL",
    "testReadingSingleTextBlobAsDataURL2",
    "testReadingSingleArrayBufferBlob",
    "testReadingSlicedFileBlob",
    "testReadingSlicedTextBlob",
    "testReadingSlicedArrayBufferBlob",
    "testReadingMultipleFileBlob",
    "testReadingMultipleTextBlob",
    "testReadingMultipleArrayBufferBlob",
    "testReadingHybridBlob",
    "testReadingSlicedHybridBlob",
    "testReadingTripleSlicedHybridBlob",
];
var testIndex = 0;

function runNextTest(testFiles)
{
    if (testIndex < testCases.length) {
        testIndex++;
        self[testCases[testIndex - 1]](testFiles);
    } else {
        log("DONE");
    }
}

function testReadingNonExistentFileBlob(testFiles)
{
    log("Test reading a blob containing non-existent file");
    var blob = buildBlob([testFiles['non-existent']]);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingNonExistentFileBlob2(testFiles)
{
    log("Test reading a blob containing existent and non-existent file");
    var blob = buildBlob([testFiles['file1'], testFiles['non-existent'], testFiles['empty-file']]);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingEmptyFileBlob(testFiles)
{
    log("Test reading a blob containing empty file");
    var blob = buildBlob([testFiles['empty-file']]);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingEmptyTextBlob(testFiles)
{
    log("Test reading a blob containing empty text");
    var blob = buildBlob(['']);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingEmptyFileAndTextBlob(testFiles)
{
    log("Test reading a blob containing empty files and empty texts");
    var blob = buildBlob(['', testFiles['empty-file'], '', testFiles['empty-file']]);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingSingleFileBlob(testFiles)
{
    log("Test reading a blob containing single file");
    var blob = buildBlob([testFiles['file1']]);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingSingleTextBlob(testFiles)
{
    log("Test reading a blob containing single text");
    var blob = buildBlob(['First']);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingSingleTextBlobAsDataURL(testFiles)
{
    log("Test reading a blob containing single text as data URL");
    var blob = buildBlob(['First']);
    readBlobAsDataURL(testFiles, blob);
}

function testReadingSingleTextBlobAsDataURL2(testFiles)
{
    log("Test reading a blob containing single text as data URL (optional content type provided)");
    var blob = buildBlob(['First'], 'type/foo');
    readBlobAsDataURL(testFiles, blob);
}

function testReadingSingleArrayBufferBlob(testFiles)
{
    log("Test reading a blob containing single ArrayBuffer");
    var array = new Uint8Array([0, 1, 2, 128, 129, 130, 253, 254, 255]);
    var blob = buildBlob([array]);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingSlicedFileBlob(testFiles)
{
    log("Test reading a blob containing sliced file");
    var blob = buildBlob([testFiles['file2'].slice(1, 6)]);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingSlicedTextBlob(testFiles)
{
    log("Test reading a blob containing sliced text");
    var blob = buildBlob(['First'])
    blob = blob.slice(1, 11);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingSlicedArrayBufferBlob(testFiles)
{
    log("Test reading a blob containing sliced ArrayBuffer");
    var array = new Uint8Array([0, 1, 2, 128, 129, 130, 253, 254, 255]);
    var blob = buildBlob([array])
    blob = blob.slice(1, 11);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingMultipleFileBlob(testFiles)
{
    log("Test reading a blob containing multiple files");
    var blob = buildBlob([testFiles['file1'], testFiles['file2'], testFiles['file3']]);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingMultipleTextBlob(testFiles)
{
    log("Test reading a blob containing multiple texts");
    var blob = buildBlob(['First', 'Second', 'Third']);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingMultipleArrayBufferBlob(testFiles)
{
    log("Test reading a blob containing multiple ArrayBuffer");
    var array1 = new Uint8Array([0, 1, 2]);
    var array2 = new Uint8Array([128, 129, 130]);
    var array3 = new Uint8Array([253, 254, 255]);
    var blob = buildBlob([array1, array2, array3]);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingHybridBlob(testFiles)
{
    log("Test reading a hybrid blob");
    var array = new Uint8Array([48, 49, 50]);
    var blob = buildBlob(['First', testFiles['file1'], 'Second', testFiles['file2'], testFiles['file3'], 'Third', array]);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingSlicedHybridBlob(testFiles)
{
    log("Test reading a sliced hybrid blob");
    var array = new Uint8Array([48, 49, 50]);
    var blob = buildBlob(['First', testFiles['file1'], 'Second', testFiles['file2'], testFiles['file3'], 'Third', array]);
    var blob = blob.slice(7, 19);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingTripleSlicedHybridBlob(testFiles)
{
    log("Test reading a triple-sliced hybrid blob");
    var array = new Uint8Array([48, 49, 50]);
    var items = ['First', testFiles['file1'].slice(1, 11), testFiles['empty-file'], 'Second', testFiles['file2'], testFiles['file3'], 'Third', array];
    var blob = buildBlob(items);
    var blob = blob.slice(7, 19);
    var blob2 = buildBlob(items.concat(['Foo', blob, 'Bar']));
    var blob2 = blob2.slice(12, 42);
    readBlobAsBinaryString(testFiles, blob2);
}

