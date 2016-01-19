description("This test calls open on window.indexedDB in various ways, verifying that IDBOpenDBRequest objects are returned when expected and exceptions are thrown when expected.");


if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

try {
    window.indexedDB.open("TestDatabase", 0);
} catch (e) {
    debug("ALERT: " + "Caught exception " + e);
}

try {
    window.indexedDB.open("TestDatabase", -1);
} catch (e) {
    debug("ALERT: " + "Caught exception " + e);
}

var request = window.indexedDB.open("TestDatabase");
debug("ALERT: " + request);

request.onsuccess = function()
{
	debug("ALERT: " + "success");
	done();
}
request.onerror = function(e)
{
	debug("ALERT: " + "error " + e);
	done();
}

request.onupgradeneeded = function(e)
{
    debug("ALERT: " + "upgradeneeded: old version - " + e.oldVersion + " new version - " + e.newVersion);
	done();
}

