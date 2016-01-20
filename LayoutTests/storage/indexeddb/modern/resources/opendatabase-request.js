description("This test calls open on window.indexedDB in various ways, verifying that IDBOpenDBRequest objects are returned when expected and exceptions are thrown when expected.");


if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var request = window.indexedDB.open("TestDatabase");
debug(request);

request = window.indexedDB.open("");
debug(request);

try {
	var request = window.indexedDB.open();
} catch (e) {
	debug(e);
}

try {
	var request = window.indexedDB.open("name", 0);
} catch (e) {
	debug(e);
}

done();

