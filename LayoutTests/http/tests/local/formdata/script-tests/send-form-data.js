description("Test for sending FormData via XMLHttpRequest.");

function runTest()
{
    debug("Sending FormData containing one string with empty name:");
    testSendingFormData([
        { 'type': 'string', 'name': '', 'value': 'foo' }
    ]);

    debug("Sending FormData containing one file with empty name:");
    testSendingFormData([
        { 'type': 'file', 'name': '', 'value': '../resources/file-for-drag-to-send.txt' }
    ]);

    debug("Sending FormData containing one string:");
    testSendingFormData([
        { 'type': 'string', 'name': 'string', 'value': 'foo' }
    ]);

    debug("Sending FormData containing one file:");
    testSendingFormData([
        { 'type': 'file', 'name': 'file', 'value': '../resources/file-for-drag-to-send.txt' }
    ]);

    debug("Sending FormData containing one string and one file:");
    testSendingFormData([
        { 'type': 'string', 'name': 'string1', 'value': 'foo' },
        { 'type': 'file', 'name': 'file1', 'value': '../resources/file-for-drag-to-send.txt' }
    ]);

    debug("Sending FormData containing two strings and two files:");
    testSendingFormData([
        { 'type': 'string', 'name': 'string1', 'value': 'foo' },
        { 'type': 'file', 'name': 'file1', 'value': '../resources/file-for-drag-to-send.txt' },
        { 'type': 'string', 'name': 'string2', 'value': 'bar' },
        { 'type': 'file', 'name': 'file2', 'value': '../resources/file-for-drag-to-send.txt' }
    ]);
}

if (window.eventSender) {
    runTest();
    formDataTestingCleanup();
} else {
    testFailed("This test is not interactive, please run using DumpRenderTree");
}

var successfullyParsed = true;
