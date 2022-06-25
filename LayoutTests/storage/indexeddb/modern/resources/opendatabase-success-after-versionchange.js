description("This test verifies that: \
    - Opening a new database results in the onupgradeneeded handler being called on the IDBOpenDBRequest. \
    - The versionchange transaction representing the upgrade commits successfully. \
    - After that transaction commits, the onsuccess handler on the original IDBOpenDBRequest is called.");

indexedDBTest(prepareDatabase, openSuccess);


function done()
{
    finishJSTest();
}

function log(message)
{
    debug(message);
}

var request = window.indexedDB.open("OpenDatabaseSuccessAfterVersionChangeDatabase");

function openSuccess()
{
    debug("Got the onsuccess handler as expected.");
	done();
}

function prepareDatabase(e)
{
    debug("upgradeneeded: old version - " + e.oldVersion + " new version - " + e.newVersion);
    debug(e.target.transaction);
    
    e.target.transaction.oncomplete = function()
    {
        debug("Version change complete");
    }
    
    e.target.transaction.onabort = function()
    {
        debug("Version change unexpected abort");
        done();
    }
    
    e.target.transaction.onerror = function()
    {
        debug("Version change unexpected error");
        done();
    }
}


