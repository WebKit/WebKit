var testCases = [
    "testReadingNonExistentFileAsArrayBuffer",
    "testReadingNonExistentFileAsBinaryString",
    "testReadingNonExistentFileAsText",
    "testReadingNonExistentFileAsDataURL",
    "testReadingEmptyFileAsArrayBuffer",
    "testReadingEmptyFileAsBinaryString",
    "testReadingEmptyFileAsText",
    "testReadingEmptyFileAsDataURL",
    "testReadingUTF8EncodedFileAsArrayBuffer",
    "testReadingUTF8EncodedFileAsBinaryString",
    "testReadingBinaryFileAsArrayBuffer",
    "testReadingBinaryFileAsBinaryString",
    "testReadingUTF8EncodedFileAsText",
    "testReadingUTF16BEBOMEncodedFileAsText",
    "testReadingUTF16LEBOMEncodedFileAsText",
    "testReadingUTF8BOMEncodedFileAsText",
    "testReadingUTF16BEEncodedFileAsTextWithUTF16Encoding",
    "testReadingUTF16BEBOMEncodedFileAsTextWithUTF8Encoding",
    "testReadingUTF16BEBOMEncodedFileAsTextWithInvalidEncoding",
    "testReadingUTF8EncodedFileAsDataURL",
];
var asyncTestCases = [
    "testMultipleReads",
    "testReadAgainAfterSuccessfulReadStep1",
    "testReadAgainAfterSuccessfulReadStep2",
    "testReadAgainAfterFailedReadStep1",
    "testReadAgainAfterFailedReadStep2",
    "testResultBeforeRead"
];
var testIndex = 0;
var initialized = false;

function ensureInitialized()
{
    if (initialized)
        return;
    initialized = true;
    if (isReadAsAsync())
        testCases = testCases.concat(asyncTestCases);
}

function runNextTest(testFiles)
{
    ensureInitialized();
    if (testIndex < testCases.length) {
        testIndex++;
        self[testCases[testIndex - 1]](testFiles);
    } else {
        log("DONE");
    }
}

function testReadingNonExistentFileAsArrayBuffer(testFiles)
{
    log("Test reading a non-existent file as array buffer");
    readBlobAsArrayBuffer(testFiles, testFiles['non-existent']);
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

function testReadingEmptyFileAsArrayBuffer(testFiles)
{
    log("Test reading an empty file as array buffer");
    readBlobAsArrayBuffer(testFiles, testFiles['empty-file']);
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

function testReadingUTF8EncodedFileAsArrayBuffer(testFiles)
{
    log("Test reading a UTF-8 file as array buffer");
    readBlobAsArrayBuffer(testFiles, testFiles['UTF8-file']);
}

function testReadingUTF8EncodedFileAsBinaryString(testFiles)
{
    log("Test reading a UTF-8 file as binary string");
    readBlobAsBinaryString(testFiles, testFiles['UTF8-file']);
}

function testReadingBinaryFileAsArrayBuffer(testFiles)
{
    log("Test reading a binary file as array buffer");
    readBlobAsArrayBuffer(testFiles, testFiles['binary-file']);
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
    log("Test calling multiple concurrent read methods");
    var reader = createReaderAsync(testFiles);
    reader.readAsDataURL(testFiles['UTF8-file']);
    try {
        reader.readAsArrayBuffer(testFiles['UTF8-file']);
    } catch (error) {
        log("Received exception " + error.code + ": " + error.name);
    }
    try {
        reader.readAsBinaryString(testFiles['UTF8-file']);
    } catch (error) {
        log("Received exception " + error.code + ": " + error.name);
    }
    try {
        reader.readAsText(testFiles['UTF8-file']);
    } catch (error) {
        log("Received exception " + error.code + ": " + error.name);
    }
    try {
        reader.readAsDataURL(testFiles['UTF8-file']);
    } catch (error) {
        log("Received exception " + error.code + ": " + error.name);
    }
}

var readerToTestReread;

function testReadAgainAfterSuccessfulReadStep1(testFiles)
{
    log("Test reading again after successful read");
    readerToTestReread = createReaderAsync(testFiles);
    readerToTestReread.readAsBinaryString(testFiles['UTF8-file']);
}

function testReadAgainAfterSuccessfulReadStep2(testFiles)
{
    readerToTestReread.readAsDataURL(testFiles['UTF8-file']);
    log("readyState after recalling read method: " + readerToTestReread.readyState);
    log("result after recalling read method: " + readerToTestReread.result);
    log("error after recalling read method: " + readerToTestReread.error);
}

function testReadAgainAfterFailedReadStep1(testFiles)
{
    log("Test reading again after failed read");
    readerToTestReread = createReaderAsync(testFiles);
    readerToTestReread.readAsBinaryString(testFiles['non-existent']);
}

function testReadAgainAfterFailedReadStep2(testFiles)
{
    readerToTestReread.readAsDataURL(testFiles['UTF8-file']);
    log("readyState after recalling read method: " + readerToTestReread.readyState);
    log("result after recalling read method: " + readerToTestReread.result);
    log("error after recalling read method: " + readerToTestReread.error);
}

function testResultBeforeRead(testFiles)
{
    log("Test result before reading method");
    var reader = createReaderAsync(testFiles);
    log("result before reading method: " + reader.result);
    reader.readAsBinaryString(testFiles['empty-file']);
}
