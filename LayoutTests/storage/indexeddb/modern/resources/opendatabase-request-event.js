description("This test calls open on window.indexedDB in various ways, verifying that IDBOpenDBRequest objects are returned when expected and exceptions are thrown when expected.");

function done()
{
    finishJSTest();
}

var dbname = setDBNameFromPath() + Date();

try {
    window.indexedDB.open(dbname, 0);
} catch (e) {
    debug("Caught exception " + e);
}

try {
    window.indexedDB.open(dbname, -1);
} catch (e) {
    debug("Caught exception " + e);
}

var request = window.indexedDB.open(dbname);
debug(request);

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

request.onupgradeneeded = function(e)
{
    debug("upgradeneeded: old version - " + e.oldVersion + " new version - " + e.newVersion);
	done();
}

