var testCases = [
    "testReadingNonExistentFileBlob",
    "testReadingNonExistentFileBlob2",
    "testReadingEmptyFileBlob",
    "testReadingEmptyTextBlob",
    "testReadingEmptyFileAndTextBlob",
    "testReadingSingleFileBlob",
    "testReadingSingleTextBlob",
    "testReadingSlicedFileBlob",
    "testReadingSlicedTextBlob",
    "testReadingMultipleFileBlob",
    "testReadingMultipleTextBlob",
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

function testReadingSlicedFileBlob(testFiles)
{
    log("Test reading a blob containing sliced file");
    var blob = buildBlob([testFiles['file2'].slice(1, 5)]);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingSlicedTextBlob(testFiles)
{
    log("Test reading a blob containing sliced text");
    var blob = buildBlob(['First'])
    blob = blob.slice(1, 10);
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

function testReadingHybridBlob(testFiles)
{
    log("Test reading a hybrid blob");
    var blob = buildBlob(['First', testFiles['file1'], 'Second', testFiles['file2'], testFiles['file3'], 'Third']);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingSlicedHybridBlob(testFiles)
{
    log("Test reading a sliced hybrid blob");
    var blob = buildBlob(['First', testFiles['file1'], 'Second', testFiles['file2'], testFiles['file3'], 'Third']);
    var blob = blob.slice(7, 12);
    readBlobAsBinaryString(testFiles, blob);
}

function testReadingTripleSlicedHybridBlob(testFiles)
{
    log("Test reading a triple-sliced hybrid blob");
    var builder = new BlobBuilder();
    var blob = buildBlob(['First', testFiles['file1'].slice(1, 10), testFiles['empty-file'], 'Second', testFiles['file2'], testFiles['file3'], 'Third'], builder);
    var blob = blob.slice(7, 12);
    var blob2 = buildBlob(['Foo', blob, 'Bar'], builder);
    var blob2 = blob2.slice(12, 30);
    readBlobAsBinaryString(testFiles, blob2);
}

