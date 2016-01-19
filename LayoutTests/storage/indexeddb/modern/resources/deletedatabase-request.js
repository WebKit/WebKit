description("This test calls deleteDatabase on window.indexedDB and verifies that an IDBOpenDBRequest object is returned.");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var request = window.indexedDB.deleteDatabase("TestDatabase");
debug("ALERT: " + request);

done();
