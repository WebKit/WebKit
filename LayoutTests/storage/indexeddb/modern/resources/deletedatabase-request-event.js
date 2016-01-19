description("This test calls deleteDatabase on window.indexedDB and verifies that an event is fired at the request.");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var request = window.indexedDB.deleteDatabase("TestDatabase");

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

debug("ALERT: " + request);
