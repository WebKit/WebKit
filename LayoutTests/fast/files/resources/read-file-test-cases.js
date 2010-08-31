var testCases = [
    "testReadingNonExistentFileAsBinaryString",
    "testReadingNonExistentFileAsText",
    "testReadingNonExistentFileAsDataURL",
    "testReadingEmptyFileAsBinaryString",
    "testReadingEmptyFileAsText",
    "testReadingEmptyFileAsDataURL",
    "testReadingUTF8EncodedFileAsBinaryString",
    "testReadingBinaryFileAsBinaryString",
    "testReadingUTF8EncodedFileAsText",
    "testReadingUTF16BEBOMEncodedFileAsText",
    "testReadingUTF16LEBOMEncodedFileAsText",
    "testReadingUTF8BOMEncodedFileAsText",
    "testReadingUTF16BEEncodedFileAsTextWithUTF16Encoding",
    "testReadingUTF16BEBOMEncodedFileAsTextWithUTF8Encoding",
    "testReadingUTF16BEBOMEncodedFileAsTextWithInvalidEncoding",
    "testReadingUTF8EncodedFileAsDataURL",
    "testMultipleReads",
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

function testReadingNonExistentFileAsBinaryString(testFiles)
{
    log("Test reading a non-existent file as binary string");
    readBlobAsBinaryString(testFiles, testFiles['non-existent']);
}

function testReadingNonExistentFileAsText(testFiles)
{
    log("Test reading a non-existent file as text");
    readBlobAsText(testFiles, testFiles['non-existent']);
}

function testReadingNonExistentFileAsDataURL(testFiles)
{
    log("Test reading a non-existent file as data URL");
    readBlobAsDataURL(testFiles, testFiles['non-existent']);
}

function testReadingEmptyFileAsBinaryString(testFiles)
{
    log("Test reading an empty file as binary string");
    readBlobAsBinaryString(testFiles, testFiles['empty-file']);
}

function testReadingEmptyFileAsText(testFiles)
{
    log("Test reading an empty file as text");
    readBlobAsText(testFiles, testFiles['empty-file']);
}

function testReadingEmptyFileAsDataURL(testFiles)
{
    log("Test reading an empty file as data URL");
    readBlobAsDataURL(testFiles, testFiles['empty-file']);
}

function testReadingUTF8EncodedFileAsBinaryString(testFiles)
{
    log("Test reading a UTF-8 file as binary string");
    readBlobAsBinaryString(testFiles, testFiles['UTF8-file']);
}

function testReadingBinaryFileAsBinaryString(testFiles)
{
    log("Test reading a binary file as binary string");
    readBlobAsBinaryString(testFiles, testFiles['binary-file']);
}

function testReadingUTF8EncodedFileAsText(testFiles)
{
    log("Test reading a UTF-8 file as text");
    readBlobAsText(testFiles, testFiles['UTF8-file']);
}

function testReadingUTF16BEBOMEncodedFileAsText(testFiles)
{
    log("Test reading a UTF-16BE BOM file as text");
    readBlobAsText(testFiles, testFiles['UTF16BE-BOM-file']);
}

function testReadingUTF16LEBOMEncodedFileAsText(testFiles)
{
    log("Test reading a UTF-16LE BOM file as text");
    readBlobAsText(testFiles, testFiles['UTF16LE-BOM-file']);
}

function testReadingUTF8BOMEncodedFileAsText(testFiles)
{
    log("Test reading a UTF-8 BOM file as text");
    readBlobAsText(testFiles, testFiles['UTF8-BOM-file']);
}

function testReadingUTF16BEEncodedFileAsTextWithUTF16Encoding(testFiles)
{
    log("Test reading a UTF-16BE file as text with UTF-16BE encoding");
    readBlobAsText(testFiles, testFiles['UTF16BE-file'], "UTF-16BE");
}

function testReadingUTF16BEBOMEncodedFileAsTextWithUTF8Encoding(testFiles)
{
    log("Test reading a UTF-16BE BOM file as text with UTF8 encoding");
    readBlobAsText(testFiles, testFiles['UTF16BE-BOM-file'], "UTF-8");
}

function testReadingUTF16BEBOMEncodedFileAsTextWithInvalidEncoding(testFiles)
{
    log("Test reading a UTF-16BE BOM file as text with invalid encoding");
    readBlobAsText(testFiles, testFiles['UTF16BE-BOM-file'], "AnyInvalidEncoding");
}

function testReadingUTF8EncodedFileAsDataURL(testFiles)
{
    log("Test reading a UTF-8 file as data URL");
    readBlobAsDataURL(testFiles, testFiles['UTF8-file']);
}

function testMultipleReads(testFiles)
{
    // This test case is only available for async reading.
    if (!isReadAsAsync()) {
        runNextTest(testFiles);
        return;
    }

    log("Test calling multiple read methods and only last one is processed");
    var reader = createReaderAsync();
    reader.readAsBinaryString(testFiles['UTF8-file']);
    reader.readAsText(testFiles['UTF8-file']);
    reader.readAsDataURL(testFiles['UTF8-file']);
}
