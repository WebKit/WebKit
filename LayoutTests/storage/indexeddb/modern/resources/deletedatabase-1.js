description("This tests that if deleteDatabase is called while there is already an open connection to the database that the open connection gets the appropriate versionChange event.");

indexedDBTest(prepareDatabase, openSuccess);

function openSuccess() {
    debug("open db success");
}

var dbname;
function prepareDatabase(e)
{
    debug("Initial upgrade old version - " + e.oldVersion + " new version - " + e.newVersion);
    
    var versionTransaction = event.target.transaction;
    var database = event.target.result;
    dbname = database.name;
    var objectStore = database.createObjectStore("TestObjectStore");
    objectStore.put("This is a record", 1);
        
    event.target.transaction.oncomplete = function()
    {
        debug("Version change complete");
        database.onversionchange = function(e)
        {
            debug("First connection received versionchange event: oldVersion " + e.oldVersion + ", newVersion " + e.newVersion);
            database.close();
        }
        continueTest1();
    }
    event.target.transaction.onabort = function()
    {
        endTestWithLog("Version change unexpected abort");
    }
    event.target.transaction.onerror = function()
    {
        endTestWithLog("Version change unexpected error");
    }
}

function continueTest1()
{
    debug("Requesting deleteDatabase");
    var request = window.indexedDB.deleteDatabase(dbname);
    request.onsuccess = function(e)
    {
        debug("Delete database success: oldVersion " + e.oldVersion + ", newVersion " + e.newVersion);
        continueTest2();
    }
    request.onerror = function(e)
    {
        endTestWithLog("Delete database unexpected error");
    }
    request.onupgradeneeded = function(e)
    {
    	endTestWithLog("Delete database unexpected upgradeneeded");
    }
}

function continueTest2()
{
    debug("Recreating database to make sure it's new and empty");
    var request = window.indexedDB.open(dbname);

    request.onupgradeneeded = function(e)
    {
        debug("Second upgrade old version - " + e.oldVersion + " new version - " + e.newVersion);
        var versionTransaction = request.transaction;
        
        try {
            var objectStore = versionTransaction.objectStore("TestObjectStore");
        } catch(e) {
            debug("Unable to get object store in second upgrade transaction (which is correct because it should not be there)");
        }

        versionTransaction.oncomplete = function(e)
        {
            endTestWithLog("Second database upgrade success");
        }
        
        versionTransaction.onabort = function(e)
        {
            endTestWithLog("Second database upgrade unexpected abort");
        }
            
        versionTransaction.onerror = function(e)
        {
            endTestWithLog("Second database upgrade unexpected error");
        }
    }

    request.onsuccess = function(e)
    {
        endTestWithLog("Second database opening unexpected success");
    }
    
    request.onerror = function(e)
    {
        endTestWithLog("Second database opening unexpected error");
    }
}
