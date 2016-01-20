description("This test calls deleteDatabase on window.indexedDB and verifies that an event is fired at the request.");


function done()
{
    finishJSTest();
}

var request = window.indexedDB.deleteDatabase("TestDatabase");

request.onsuccess = function()
{
	debug("success");
	done();
}
request.onerror = function(e)
{
	debug("error " + e);
	done();
}

debug(request);
