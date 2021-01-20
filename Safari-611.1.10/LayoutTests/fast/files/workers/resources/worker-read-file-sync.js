importScripts("../../resources/read-common.js", "../../resources/read-file-test-cases.js", "worker-read-common.js");

// Add specific test for FileReaderSync.
testCases.push("testMultipleReadsSync");

function testMultipleReadsSync(testFiles)
{
    log("Test calling multiple read methods in a row");
    var reader = createReaderSync();
    logResult(reader.readAsArrayBuffer(testFiles['UTF8-file']));
    logResult(reader.readAsBinaryString(testFiles['binary-file']));
    logResult(reader.readAsDataURL(testFiles['empty-file']));

    runNextTest(testFiles);
}

function isReadAsAsync()
{
    return false;
}
