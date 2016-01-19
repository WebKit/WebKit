description("This test verifies that: \
    - Opening a new database results in the onupgradeneeded handler being called on the IDBOpenDBRequest. \
    - The versionchange transaction representing the upgrade commits successfully. \
    - After that transaction commits, the onsuccess handler on the original IDBOpenDBRequest is called.");
if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

function log(message)
{
    debug(message);
}

var request = window.indexedDB.open("OpenDatabaseSuccessAfterVersionChangeDatabase");

request.onsuccess = function()
{
    debug("Got the onsuccess handler as expected.");
	done();
}
request.onerror = function(e)
{
    debug("Unexpected onerror handler called");
	done();
}

request.onupgradeneeded = function(e)
{
    debug("upgradeneeded: old version - " + e.oldVersion + " new version - " + e.newVersion);
    debug(request.transaction);
    
    request.transaction.oncomplete = function()
    {
        debug("Version change complete");
    }
    
    request.transaction.onabort = function()
    {
        debug("Version change unexpected abort");
        done();
    }
    
    request.transaction.onerror = function()
    {
        debug("Version change unexpected error");
        done();
    }
}


