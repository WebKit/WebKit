description("This tests that if deleteDatabase is called while there is already an open connection to the database that the open connection gets the appropriate versionChange event.");

indexedDBTest(prepareDatabase, openSuccess);


function done()
{
    finishJSTest();
}

function log(message)
{
    debug(message);
}

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
        debug("Version change unexpected abort");
        done();
    }
    event.target.transaction.onerror = function()
    {
        debug("Version change unexpected error");
        done();
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
        debug("Delete database unexpected error");
        done();
    }
    request.onupgradeneeded = function(e)
    {
        debug("Delete database unexpected upgradeneeded");
    	done();
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
            debug("Second database upgrade success");
            done();
        }
        
        versionTransaction.onabort = function(e)
        {
            debug("Second database upgrade unexpected abort");
            done();
        }
            
        versionTransaction.onerror = function(e)
        {
            debug("Second database upgrade unexpected error");
            done();
        }
    }

    request.onsuccess = function(e)
    {
        debug("Second database opening unexpected success");
        done();
    }
    
    request.onerror = function(e)
    {
        debug("Second database opening unexpected error");
        done();
    }
}
