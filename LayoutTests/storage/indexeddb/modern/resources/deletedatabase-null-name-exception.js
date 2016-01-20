description("This test calls deleteDatabase on window.indexedDB with a null database name, making sure there is an exception.");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

try {
	var request = window.indexedDB.deleteDatabase();
} catch (e) {
	debug(e);
}

done();
